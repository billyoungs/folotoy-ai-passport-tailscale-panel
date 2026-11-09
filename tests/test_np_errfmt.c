// tests/test_np_errfmt.c —— 分层判定测试:每种低层信号必须落到不同的人话结论。
// 屏上只有一行,报"网络不通"而不说是外网/DNS/TLS/凭据,用户就没法自查。
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_errfmt.h"

#define CAUSE_CAP 48   // 与 netpanel_app.c 的 UI_ERR_CAP 一致
#define DETAIL_CAP 80  // 与 UI_ERR_DETAIL_CAP 一致

// 截断后必须仍是完整 UTF-8:半个码点在屏幕上是个占位框,比少一个字更糟。
static bool valid_utf8(const char *s)
{
    for (size_t i = 0; s[i]; ) {
        const unsigned char c = (unsigned char)s[i];
        size_t want = 1u;
        if ((c & 0xE0u) == 0xC0u) want = 2u;
        else if ((c & 0xF0u) == 0xE0u) want = 3u;
        else if ((c & 0xF8u) == 0xF0u) want = 4u;
        else if ((c & 0x80u) != 0u) return false;  // 裸连续字节开头 = 残缺
        for (size_t k = 1u; k < want; k++) {
            if (((unsigned char)s[i + k] & 0xC0u) != 0x80u) return false;
        }
        i += want;
    }
    return true;
}

static void cause(char *out, const np_err_src_t *src)
{
    np_errfmt_cause(out, CAUSE_CAP, src);
    assert(out[0] != '\0');
    assert(valid_utf8(out));
}

int main(void)
{
    char c[CAUSE_CAP];
    char d[DETAIL_CAP];

    // 空输入:不能崩,也不能留空串让屏幕显示"异常:"
    np_errfmt_cause(c, sizeof(c), NULL);
    assert(strcmp(c, "未知失败") == 0);
    np_errfmt_detail(d, sizeof(d), NULL);
    assert(d[0] == '\0');
    np_errfmt_cause(c, 0, NULL);  // cap 为 0 时只要求不写越界
    c[0] = 'x';
    cause(c, &(np_err_src_t){0});

    // ---------------------------------------------- 走到 HTTP:链路是通的
    cause(c, &(np_err_src_t){ .http_status = 401, .msg = "client is not authorized" });
    assert(strncmp(c, "凭据被拒:", 12) == 0);
    assert(strstr(c, "not authorized") != NULL);
    cause(c, &(np_err_src_t){ .http_status = 403 });
    assert(strcmp(c, "凭据被拒(HTTP 403)") == 0);
    cause(c, &(np_err_src_t){ .http_status = 404 });
    assert(strcmp(c, "找不到该 tailnet(404)") == 0);
    cause(c, &(np_err_src_t){ .http_status = 400 });
    assert(strcmp(c, "请求被拒(400)") == 0);
    cause(c, &(np_err_src_t){ .http_status = 422 });
    assert(strcmp(c, "请求被拒(422)") == 0);
    cause(c, &(np_err_src_t){ .http_status = 429 });
    assert(strcmp(c, "请求过于频繁(429)") == 0);
    cause(c, &(np_err_src_t){ .http_status = 503 });
    assert(strcmp(c, "Tailscale 服务异常(503)") == 0);
    // 门户热点:Wi-Fi 通了、DNS 也通了,却被 302 到登录页 —— 外网不通
    cause(c, &(np_err_src_t){ .http_status = 302 });
    assert(strcmp(c, "被门户拦截(302),外网不通") == 0);
    cause(c, &(np_err_src_t){ .http_status = 200 });
    assert(strcmp(c, "响应不是预期 JSON") == 0);
    cause(c, &(np_err_src_t){ .http_status = 200, .too_big = true });
    assert(strcmp(c, "设备列表过大") == 0);
    cause(c, &(np_err_src_t){ .http_status = 204 });
    assert(strcmp(c, "响应异常(HTTP 204)") == 0);

    // 关键契约:401 绝不能显示成"网络不通"——用户会去查路由器而不是查凭据。
    assert(strstr(c, "网络不通") == NULL);

    // ---------------------------------------------- 没走到 HTTP:哪段断报哪段
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x8001,
                              .host = "api.tailscale.com" });
    assert(strcmp(c, "解析不到 api.tailscale.com,外网不通") == 0);
    // host 缺失时回退,不能显示成"解析不到 ,外网不通"
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x8001, .host = "" });
    assert(strcmp(c, "解析不到 服务器,外网不通") == 0);
    // 设备没有 RTC:时钟停在 1970 时证书必然"还没生效",flag 0x200 是这一位。
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x801A,
                              .tls_code = 0x2700, .tls_flags = 0x200,
                              .clock_synced = false });
    assert(strcmp(c, "时钟未同步,证书校验不过") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .tls_flags = 0x200,
                              .clock_synced = true });
    assert(strcmp(c, "证书有效期不对(校过时钟?)") == 0);
    // flag 的每一位要说得出人话,不能都糊成"证书校验不过"。
    cause(c, &(np_err_src_t){ .transport = true, .tls_flags = 0x1,
                              .clock_synced = true });
    assert(strcmp(c, "证书有效期不对(校过时钟?)") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .tls_flags = 0x4 });
    assert(strcmp(c, "证书域名不匹配(疑被拦)") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .tls_flags = 0x8 });
    assert(strcmp(c, "根证书不认,链对不上") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .tls_flags = 0x40 });
    assert(strcmp(c, "没收到对端证书") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .tls_flags = 0x800 });
    assert(strcmp(c, "证书校验不过(链或有效期)") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x8006,
                              .host = "api.tailscale.com" });
    assert(strcmp(c, "连 api.tailscale.com:443 超时") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x8004 });
    assert(strcmp(c, "连不上 服务器:443") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x8002 });
    assert(strcmp(c, "连不上 服务器:443") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x8003 });
    assert(strcmp(c, "连不上 服务器:443") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x8009 });
    assert(strcmp(c, "TLS 握手超时(443 已连上)") == 0);

    // 走到 TLS 握手就说明 443 已经连上,再说"连不上外网"会把人支去查路由器。
    // esp-tls 存的是 -ret(正数),所以这里按正数喂。
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x801A,
                              .tls_code = 0x7F00 });
    assert(strcmp(c, "内存不足,TLS 握手中断") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x801A,
                              .tls_code = 0x7280 });
    assert(strcmp(c, "握手被掐断,疑网关拦 443") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x801A,
                              .tls_code = 0x7200 });
    assert(strcmp(c, "对端 TLS 报文不对,疑被拦") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .tls_code = 0x6800 });
    assert(strcmp(c, "TLS 握手超时(443 已连上)") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .tls_code = 0x6E80 });
    assert(strcmp(c, "TLS 版本谈不拢(443 已连上)") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .tls_code = 0x7100 });
    assert(strcmp(c, "本机 TLS 配置不对") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .tls_code = 0x7780 });
    assert(strcmp(c, "TLS 被对端拒绝(收到告警)") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .tls_code = 0x7680 });
    assert(strcmp(c, "没加载上根证书") == 0);
    // 认不出的 mbedTLS 码:承认卡在 TLS 且不谎报外网,把码留给细节行
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x801A,
                              .tls_code = 0x1234 });
    assert(strcmp(c, "TLS 握手失败(443 已连上)") == 0);
    cause(c, &(np_err_src_t){ .transport = true, .esp_err = 0x801A });
    assert(strcmp(c, "TLS 握手失败(443 已连上)") == 0);
    // 没走到 HTTP 又没有任何低层码:如实说不知道,别猜凭据
    cause(c, &(np_err_src_t){ .transport = true });
    assert(strcmp(c, "网络不通(没走到 HTTP)") == 0);
    // ts_api 的用法错误(凭据缺失、内存不足)自带原因文本
    cause(c, &(np_err_src_t){ .msg = "未配置 access token" });
    assert(strcmp(c, "未配置 access token") == 0);
    // 响应读到一半断了:此时 tls 码会显示成"握手失败"那类样子,
    // 但事实是链路已通、传输中断,原因文本必须压过低层码。
    cause(c, &(np_err_src_t){ .transport = true, .tls_code = -0x7200,
                              .msg = "响应中途断开" });
    assert(strcmp(c, "响应中途断开") == 0);
    cause(c, &(np_err_src_t){ .too_big = true });
    assert(strcmp(c, "设备列表过大") == 0);
    cause(c, &(np_err_src_t){ .http_status = 0 });  // 全零:只能报解析失败
    assert(strcmp(c, "响应不是预期 JSON") == 0);

    // ----------------------------------------------------------- 细节行
    // TLS 码按 `-0x%04X` 写,和 esp-tls 日志里的
    // "mbedtls_ssl_handshake returned -0x7F00" 是同一个数,可以直接对着查。
    np_errfmt_detail(d, sizeof(d),
                     &(np_err_src_t){ .esp_err = 0x8001, .tls_code = 0x2700,
                                      .tls_flags = 4, .sys_errno = 116,
                                      .http_status = 404 });
    assert(strcmp(d, "esp 0x8001 tls -0x2700 flag 0x4 sock 116 http 404") == 0);
    np_errfmt_detail(d, sizeof(d), &(np_err_src_t){ .http_status = 401 });
    assert(strcmp(d, "http 401") == 0);
    np_errfmt_detail(d, sizeof(d), &(np_err_src_t){ .clock_synced = true });
    assert(strcmp(d, "无低层错误码") == 0);
    // 内存不足和链路被掐在 esp_err 上长得一样,靠这两个数分开。
    np_errfmt_detail(d, sizeof(d),
                     &(np_err_src_t){ .esp_err = 0x801A, .tls_code = 0x7F00,
                                      .free_heap = 31204, .free_blk = 8192 });
    assert(strcmp(d, "esp 0x801A tls -0x7F00 heap 31204 blk 8192") == 0);
    // 没采集到堆信息(0)时不能写成 "heap 0",那会被读成"剩余为零"。
    np_errfmt_detail(d, sizeof(d), &(np_err_src_t){ .esp_err = 0x801A });
    assert(strcmp(d, "esp 0x801A") == 0);

    // 证书标志优先于握手码:同一个 0x8009,时钟没同步和 TLS 版本不兼容
    // 是两件完全不同的事,细节行必须把 flag 带出来。
    np_errfmt_detail(d, sizeof(d),
                     &(np_err_src_t){ .esp_err = 0x8009, .tls_flags = 0x80 });
    assert(strcmp(d, "esp 0x8009 flag 0x80") == 0);

    // -------------------------------------------------- 容量边界:不许半字
    // 取每一条中文结论,逐步收窄缓冲,验证截断结果永远是完整 UTF-8。
    static const char *samples[] = {
        "凭据被拒:client is not authorized",
        "找不到该 tailnet(404)",
        "被门户拦截(302),外网不通",
        "Tailscale 服务异常(503)",
        "解析不到 api.tailscale.com,外网不通",
        "时钟未同步,证书校验不过",
        "连 api.tailscale.com:443 超时",
        "未配置 access token",
        "内存不足,TLS 握手中断",
        "握手被掐断,疑网关拦 443",
        "对端 TLS 报文不对,疑被拦",
        "证书域名不匹配(疑被拦)",
        "建不了套接字(内存/句柄耗尽)",
    };
    const int ns = (int)(sizeof(samples) / sizeof(samples[0]));
    for (int i = 0; i < ns; i++) {
        // 走 transport+msg 这条路:整条中文结论原样进 put(),才测得到截断。
        const np_err_src_t src = { .transport = true, .msg = samples[i] };
        char small[8];
        for (size_t cap = 1u; cap < sizeof(small); cap++) {
            memset(small, 'A', sizeof(small));
            np_errfmt_cause(small, cap, &src);
            assert(strlen(small) + 1u <= cap);
            assert(valid_utf8(small));
        }
    }
    // 细节行同样受限,而且必须是纯 ASCII 数字,截断也不会出半字
    char small[12];
    for (size_t cap = 1u; cap < sizeof(small); cap++) {
        np_errfmt_detail(small, cap,
                         &(np_err_src_t){ .esp_err = 0x8001, .tls_code = 0x2700,
                                          .tls_flags = 4, .sys_errno = 116 });
        assert(strlen(small) + 1u <= cap);
        assert(valid_utf8(small));
    }

    // 原因行必须在 s_err[UI_ERR_CAP] 里不被截断(截断了等于把结论吞掉),
    // 加上"异常:"前缀后还要装得下状态页的 96 字节行缓冲。
    struct { np_err_src_t src; } fit[] = {
        { { .http_status = 404 } },
        { { .http_status = 503 } },
        { { .transport = true, .esp_err = 0x8001, .host = "api.tailscale.com" } },
        { { .transport = true, .tls_flags = 4, .clock_synced = true } },
        { { .transport = true, .esp_err = 0x801A, .tls_code = 0x7F00 } },
        { { .transport = true, .esp_err = 0x801A, .tls_code = 0x7200 } },
        { { .transport = true, .tls_code = 0x0042 } },
    };
    char line[96];  // 与 show_status_view() 的行缓冲一致
    for (size_t i = 0; i < sizeof(fit) / sizeof(fit[0]); i++) {
        cause(c, &fit[i].src);
        assert(strlen(c) < CAUSE_CAP);
        snprintf(line, sizeof(line), "异常:%s", c);
        assert(strlen(line) < sizeof(line));
    }
    // 满配细节行:六个字段全非零 + 堆两个数,仍然要装得下 80 字节缓冲和屏上一行。
    np_errfmt_detail(d, sizeof(d),
                     &(np_err_src_t){ .esp_err = 0x801A, .tls_code = 0x7F00,
                                      .tls_flags = 0x200, .sys_errno = 104,
                                      .http_status = 503, .free_heap = 156000,
                                      .free_blk = 60000 });
    assert(strlen(d) < DETAIL_CAP);
    snprintf(line, sizeof(line), "细节:%s", d);
    assert(strlen(line) < sizeof(line));

    printf("test_np_errfmt: ok\n");
    return 0;
}
