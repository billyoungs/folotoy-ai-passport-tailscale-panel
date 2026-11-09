// main/np_dns_server.c —— 门户 DNS 劫持。
// 报文解析改自 ESP-IDF 示例 examples/protocols/http_server/captive_portal/components/dns_server
// (SPDX-License-Identifier: Unlicense OR CC0-1.0),按本应用做了三点收敛:
// 只回答一个 netif 的地址、句柄静态化(无 PSRAM 上少一次 malloc)、
// 停止时发一个回环包唤醒阻塞中的 recvfrom,而不是从别的任务 vTaskDelete 掉它。
#include "np_dns_server.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"

#include <errno.h>
#include <string.h>

static const char *TAG = "np_dns";

#define DNS_PORT 53
#define DNS_MAX_LEN 256
#define OPCODE_MASK 0x7800
#define QR_FLAG (1u << 7)
#define QD_TYPE_A 0x0001
#define ANS_TTL_SEC 300

typedef struct __attribute__((__packed__)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

typedef struct {
    uint16_t type;
    uint16_t class;
} dns_question_t;

typedef struct __attribute__((__packed__)) {
    uint16_t ptr_offset;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t addr_len;
    uint32_t ip_addr;
} dns_answer_t;

static volatile bool s_run;
static volatile bool s_live;
static const char *s_if_key;
static TaskHandle_t s_task;

// 把 DNS 线格式名字(长度前缀分段)展成点分名,返回名字之后的指针。
static char *parse_dns_name(char *raw, char *out, size_t out_cap)
{
    char *label = raw;
    char *itr = out;
    int len = 0;
    do {
        const int sub = *label;
        len += sub + 1;
        if (len > (int)out_cap) return NULL;
        memcpy(itr, label + 1, sub);
        itr[sub] = '.';
        itr += sub + 1;
        label += sub + 1;
    } while (*label != 0);
    out[len - 1] = '\0';
    return label + 1;
}

static esp_ip4_addr_t local_ip(void)
{
    esp_ip4_addr_t ip = { .addr = IPADDR_ANY };
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(s_if_key);
    esp_netif_ip_info_t info = { 0 };
    if (netif && esp_netif_get_ip_info(netif, &info) == ESP_OK) {
        ip.addr = info.ip.addr;
    }
    return ip;
}

static int build_reply(char *req, size_t req_len, char *reply, size_t reply_cap)
{
    if (req_len > reply_cap) return -1;
    memset(reply, 0, reply_cap);
    memcpy(reply, req, req_len);

    dns_header_t *hdr = (dns_header_t *)reply;
    if ((hdr->flags & OPCODE_MASK) != 0) return 0;  // 非标准查询,不回答
    hdr->flags |= QR_FLAG;
    const uint16_t qd_count = ntohs(hdr->qd_count);

    // 容量按最坏情形预检(A 型问题每个都要一条应答),实际长度循环后按写入算。
    if ((int)(qd_count * sizeof(dns_answer_t) + req_len) > (int)reply_cap) {
        return -1;
    }

    const esp_ip4_addr_t ip = local_ip();
    if (ip.addr == IPADDR_ANY) return -1;

    // 只给 A 型问题写应答体,an_count 按实际写入回填:手机做门户探测时常同时
    // 查 A 和 AAAA,报文里声明有应答却缺应答体属于畸形,探测方会把它当无效
    // 数据丢掉,门户就开不出来(np-0.2.2 实测)。AAAA 由此得到合法的空应答
    // (NoError、0 条应答),探测得以继续。
    uint16_t answers = 0;
    char *ans = reply + req_len;
    char *qd = reply + sizeof(dns_header_t);
    char name[96];
    for (uint16_t i = 0; i < qd_count; i++) {
        char *after = parse_dns_name(qd, name, sizeof(name));
        if (!after) return -1;
        dns_question_t *q = (dns_question_t *)after;
        if (ntohs(q->type) == QD_TYPE_A) {
            dns_answer_t *a = (dns_answer_t *)ans;
            a->ptr_offset = htons((uint16_t)(0xC000 | (qd - reply)));
            a->type = q->type;
            a->class = q->class;
            a->ttl = htonl(ANS_TTL_SEC);
            a->addr_len = htons(sizeof(ip.addr));
            a->ip_addr = ip.addr;
            ans += sizeof(dns_answer_t);
            answers++;
        }
        qd = after + sizeof(dns_question_t);
    }
    hdr->an_count = htons(answers);
    return (int)(ans - reply);
}

static void dns_task(void *arg)
{
    (void)arg;
    while (s_run) {
        struct sockaddr_in dst = { 0 };
        dst.sin_addr.s_addr = htonl(INADDR_ANY);
        dst.sin_family = AF_INET;
        dst.sin_port = htons(DNS_PORT);

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "DNS socket 创建失败 errno=%d", errno);
            break;
        }
        if (bind(sock, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
            ESP_LOGE(TAG, "DNS bind 失败 errno=%d", errno);
            close(sock);
            break;
        }
        while (s_run) {
            char rx[128];
            struct sockaddr_in6 from;
            socklen_t from_len = sizeof(from);
            const int len = recvfrom(sock, rx, sizeof(rx) - 1, 0,
                                     (struct sockaddr *)&from, &from_len);
            if (len < 0) {
                ESP_LOGW(TAG, "recvfrom 失败 errno=%d", errno);
                break;
            }
            rx[len] = '\0';
            char reply[DNS_MAX_LEN];
            const int n = build_reply(rx, (size_t)len, reply, sizeof(reply));
            if (n > 0) {
                (void)sendto(sock, reply, (size_t)n, 0,
                             (struct sockaddr *)&from, from_len);
            }
        }
        shutdown(sock, 0);
        close(sock);
    }
    s_live = false;
    vTaskDelete(NULL);
}

bool np_dns_running(void)
{
    return s_live;
}

bool np_dns_start(const char *if_key)
{
    if (s_live || if_key == NULL) return false;
    s_if_key = if_key;
    s_run = true;
    s_live = true;
    if (xTaskCreate(dns_task, "np_dns", 4096, NULL, 5, &s_task) != pdPASS) {
        s_run = false;
        s_live = false;
        return false;
    }
    return true;
}

void np_dns_stop(void)
{
    if (!s_live) return;
    s_run = false;
    // 唤醒阻塞中的 recvfrom:给自己发一个空包,任务随即退出循环。
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock >= 0) {
        struct sockaddr_in dst = { 0 };
        dst.sin_family = AF_INET;
        dst.sin_port = htons(DNS_PORT);
        dst.sin_addr.s_addr = inet_addr("127.0.0.1");
        (void)sendto(sock, "", 1, 0, (struct sockaddr *)&dst, sizeof(dst));
        close(sock);
    }
    for (int i = 0; i < 50 && s_live; i++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (s_live) {
        ESP_LOGW(TAG, "DNS 任务未在 1 秒内退出,交给看门狗兜底");
    }
    (void)s_task;
}
