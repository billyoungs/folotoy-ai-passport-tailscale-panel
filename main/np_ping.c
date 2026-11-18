// main/np_ping.c —— 互联网连通性探测:TCP 连接 www.baidu.com:80,只报通/不通。
// 不走 raw ICMP:运营商和上游防火墙常丢弃 ICMP,而 TCP 握手成功才是"能上网"
// 的真实判据;sockets 也是应用 HTTP 栈在用的同一套 lwIP 机制。
// 任务里跑,结果用 volatile 状态交给 GUI 轮询。
#include "np_ping.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>

static const char *TAG = "np_ping";

static volatile np_ping_state_t s_state = NP_PING_IDLE;
static char s_host[64];
static bool s_task_running;

// 域名 → sockaddr_in。getaddrinfo 内部走 lwIP DNS。
static bool resolve_ipv4(const char *host, struct sockaddr_in *out)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, "80", &hints, &res) != 0 || res == NULL) return false;
    memcpy(out, res->ai_addr, sizeof(*out));
    freeaddrinfo(res);
    return true;
}

static void ping_task(void *arg)
{
    (void)arg;
    struct sockaddr_in addr;
    if (!resolve_ipv4(s_host, &addr)) {
        ESP_LOGW(TAG, "解析失败:%s", s_host);
        s_state = NP_PING_NOHOST;
        goto done;
    }

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        s_state = NP_PING_FAIL;
        goto done;
    }
    const int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) (void)fcntl(fd, F_SETFL, fl | O_NONBLOCK);

    bool ok = false;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        ok = true;   // 非阻塞下立即成功(本机已有通路)
    } else if (errno == EINPROGRESS) {
        for (int i = 0; i < 60; i++) {          // 上限 3 s(60 × 50 ms)
            fd_set wr, ex;
            FD_ZERO(&wr);
            FD_SET(fd, &wr);
            FD_ZERO(&ex);
            FD_SET(fd, &ex);
            struct timeval tv = { .tv_sec = 0, .tv_usec = 50 * 1000 };
            const int r = select(fd + 1, NULL, &wr, &ex, &tv);
            if (r > 0) {
                if (FD_ISSET(fd, &ex)) break;   // 选中的是异常:判失败
                int soerr = 0;
                socklen_t sl = sizeof(soerr);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl);
                ok = (soerr == 0);
                break;
            }
            if (r < 0 && errno != EINTR) break;
        }
    }
    close(fd);
    s_state = ok ? NP_PING_OK : NP_PING_FAIL;
    ESP_LOGI(TAG, "connect %s -> %s", s_host, ok ? "ok" : "fail");

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
