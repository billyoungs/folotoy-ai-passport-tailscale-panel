// main/ts_api.h —— Tailscale Admin API v2 客户端:凭据换取 + 设备列表拉取。
// 响应里的 machineKey/nodeKey 一律丢弃,绝不落入任何存储。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "np_model.h"

typedef enum {
    TS_OK = 0,
    TS_ERR_PARAM,        // 凭据缺失等,调用方用法错误
    TS_ERR_NETWORK,      // DNS/TCP/TLS 层失败(可退避重试)
    TS_ERR_TIMEOUT,
    TS_ERR_SERVER,       // 5xx(可退避重试)
    TS_ERR_RATE_LIMITED, // 429(可退避重试)
    TS_ERR_AUTH,         // 401/403:凭据无效,持续退避到重新配置为止
    TS_ERR_PARSE,        // 响应不是预期 JSON(可退避重试)
    TS_ERR_TOO_BIG,      // 响应超过缓冲上限
} ts_result_t;

// 一次失败的低层信号,供屏幕分成"原因"与"细节"两行显示。
// 数字原样带出,含义由 np_errfmt 判定;没走到 HTTP 时 http_status 为 0。
// msg 只取服务端 JSON 的 message 字段,已按字体清表过滤,绝不含凭据。
typedef struct {
    int http_status;
    int esp_err;    // esp-tls/http client 的 esp_err
    int tls_code;   // mbedTLS 码的绝对值(esp-tls 存的是 -ret)
    int tls_flags;  // 证书校验标志位
    int sys_errno;  // socket errno
    int free_heap;  // 失败时剩余堆(字节)
    int free_blk;   // 失败时最大可分配块(字节)
    char host[32];
    char msg[64];
    bool transport; // 失败发生在连接层
    bool too_big;   // 响应超过缓冲上限
} ts_fail_t;

// 拉取设备列表填入 list(覆盖旧内容),返回结果码。
// 会按需获取/复用 OAuth JWT,阻塞执行,只允许在专用任务里调用。
ts_result_t ts_api_fetch_devices(np_list_t *list);

// 单独拉一台设备的扩展信息(GET /device/{id}?fields=all):子网路由、DERP 时延与
// 候选直连端点、发行版、SSH 开关 —— 列表用的默认字段集里没有这些。
// 一次一台,响应缓冲远小于整表,所以只在详情页需要时才多跑一趟。
// dev_id 为空返回 TS_ERR_PARAM。失败细节同样落在 ts_api_last_failure()。
ts_result_t ts_api_fetch_device(const char *dev_id, np_detail_t *out);

// 最近一次失败的细节(只在失败后有意义;成功路径不覆盖它)。
const ts_fail_t *ts_api_last_failure(void);

// 纯逻辑:去掉 Tailscale 用户名前缀(如 "login-noreply_"),空则回退 alt,
// 再做清表过滤。写入 dst 并返回 dst。
#include "np_text.h"
char *ts_api_display_name(char *dst, size_t cap, const char *host, const char *alt);
