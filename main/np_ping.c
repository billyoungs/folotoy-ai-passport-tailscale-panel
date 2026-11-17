// main/np_ping.c —— ICMP echo(用 lwIP raw PCB)一个域名,只报通/不通。
// 任务里跑;LWIP_ICMP 默认启用,lwIP 的 core locking 保证从任务调 raw_* 安全。
#include "np_ping.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/raw.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/icmp.h"
#include "lwip/prot/ip.h"
#include "lwip/def.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include <string.h>

static const char *TAG = "np_ping";

static volatile np_ping_state_t s_state = NP_PING_IDLE;
static volatile bool s_reply;
static char s_host[64];
static bool s_task_running;

// 收到 ICMP echo reply(type 0)就置位。回调在 lwIP 线程里,只写一个 flag。
static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p,
                      const ip_addr_t *addr)
{
    (void)arg; (void)pcb; (void)addr;
    if (p->tot_len >= sizeof(struct icmp_echo_hdr)) {
        struct icmp_echo_hdr *icmp = (struct icmp_echo_hdr *)p->payload;
        if (icmp->type == ICMP_ER) s_reply = true;
    }
    pbuf_free(p);
    return 1;   // 已消费
}

// 域名 → IPv4。走 lwIP 的 DNS(getaddrinfo 内部就是 DNS 查询)。
static bool resolve_ipv4(const char *host, ip_addr_t *out)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, "80", &hints, &res) != 0 || res == NULL) return false;
    const struct sockaddr_in *sin = (const struct sockaddr_in *)res->ai_addr;
    const uint8_t *o = (const uint8_t *)&sin->sin_addr;
    IP_ADDR4(out, o[0], o[1], o[2], o[3]);
    freeaddrinfo(res);
    return true;
}

static void ping_task(void *arg)
{
    (void)arg;
    ip_addr_t dest;
    if (!resolve_ipv4(s_host, &dest)) {
        ESP_LOGW(TAG, "解析失败:%s", s_host);
        s_state = NP_PING_NOHOST;
        goto done;
    }
    struct raw_pcb *pcb = raw_new(IP_PROTO_ICMP);
    if (pcb == NULL) {
        s_state = NP_PING_FAIL;
        goto done;
    }
    raw_recv(pcb, ping_recv, NULL);

    const u16_t LEN = sizeof(struct icmp_echo_hdr) + 16;   // 8 头 + 16 负载
    struct pbuf *pb = pbuf_alloc(PBUF_IP, LEN, PBUF_RAM);
    if (pb == NULL) {
        raw_remove(pcb);
        s_state = NP_PING_FAIL;
        goto done;
    }
    struct icmp_echo_hdr *icmp = (struct icmp_echo_hdr *)pb->payload;
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->chksum = 0;
    icmp->id = lwip_htons(0x1234);
    icmp->seqno = lwip_htons(1);
    memset((u8_t *)pb->payload + sizeof(struct icmp_echo_hdr), 0,
           LEN - sizeof(struct icmp_echo_hdr));
    icmp->chksum = inet_chksum(icmp, LEN);

    s_reply = false;
    const err_t e = raw_sendto(pcb, pb, &dest);
    pbuf_free(pb);
    if (e != ERR_OK) {
        raw_remove(pcb);
        s_state = NP_PING_FAIL;
        goto done;
    }
    // 最多等 1500 ms(30 × 50 ms)收一个 echo reply。
    for (int i = 0; i < 30 && !s_reply; i++) vTaskDelay(pdMS_TO_TICKS(50));
    raw_remove(pcb);
    s_state = s_reply ? NP_PING_OK : NP_PING_FAIL;
    ESP_LOGI(TAG, "ping %s -> %s", s_host, s_reply ? "ok" : "fail");

done:
    s_task_running = false;
    vTaskDelete(NULL);
}

bool np_ping_start(const char *host)
{
    if (host == NULL || host[0] == '\0') return false;
    if (s_state == NP_PING_BUSY && s_task_running) return false;
    strncpy(s_host, host, sizeof(s_host) - 1);
    s_host[sizeof(s_host) - 1] = '\0';
    s_state = NP_PING_BUSY;
    s_task_running = true;
    if (xTaskCreate(ping_task, "np_ping", 4096, NULL, 4, NULL) != pdPASS) {
        s_task_running = false;
        s_state = NP_PING_FAIL;
        return false;
    }
    return true;
}

np_ping_state_t np_ping_state(void)
{
    return s_state;
}
