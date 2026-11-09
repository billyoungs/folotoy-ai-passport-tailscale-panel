// main/np_errfmt.h —— 把一次同步失败的底层信号翻译成"到底哪一层不通"(纯逻辑)。
// 契约:原因行只说结论(外网/DNS/TCP/TLS/凭据/tailnet/服务端),细节行只放数字,
// 两者都必须能在小屏一行内读完;没走到 HTTP 层时绝不猜"凭据有问题"。
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int http_status;   // 已收到响应时的状态码;0 = 没走到 HTTP 层
    int esp_err;       // esp-tls/http client 的 esp_err(0 = 无)
    int tls_code;      // mbedTLS 码的绝对值(esp-tls 存的是 -ret,正数)
    int tls_flags;     // 证书校验标志位(非 0 = 证书没通过校验)
    int sys_errno;     // socket errno(0 = 无)
    int free_heap;     // 失败时剩余堆(0 = 未采集)
    int free_blk;      // 失败时最大可分配块(0 = 未采集)
    const char *host;  // 出错的域名,可为 NULL
    const char *msg;   // 服务端返回的 message 文本,可为 NULL
    bool clock_synced; // 发起那次请求**那一刻**设备时钟是否已校时(未校时会让证书校验必然失败)
    bool transport;    // 失败发生在连接层(完全没走到 HTTP)
    bool too_big;      // 响应超过缓冲上限
} np_err_src_t;

// 原因行:一句人话。dst 始终 NUL 结尾;放不下时按整个码点丢弃尾部,不留半字。
void np_errfmt_cause(char *dst, size_t cap, const np_err_src_t *src);

// 细节行:只放数字与两个关键词,便于对着串口日志核对。
void np_errfmt_detail(char *dst, size_t cap, const np_err_src_t *src);
