// main/np_errfmt.c —— 分层判定:先看有没有走到 HTTP,再按 esp-tls/mbedTLS 码归类。
#include "np_errfmt.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// esp-tls 的错误码(见 IDF 5.5 components/esp-tls/esp_tls_errors.h,
// ESP_ERR_ESP_TLS_BASE=0x8000)。这里自带一份数值,为的是本模块能在主机上编译,
// 不引 IDF 头;ts_api.c 原样把数字带进细节行,对不上时一眼能看出来。
#define NP_TLS_CANNOT_RESOLVE  0x8001
#define NP_TLS_CANNOT_SOCKET   0x8002
#define NP_TLS_PROTO_FAMILY    0x8003
#define NP_TLS_CONNECT_FAILED  0x8004
#define NP_TLS_CONN_TIMEOUT    0x8006
#define NP_TLS_HANDSHAKE_TO    0x8009
#define NP_TLS_MBEDTLS_FAILED  0x801A  // mbedtls 自己回了错(握手没做成)

// mbedTLS X509 校验标志(见 mbedtls x509.h)。esp-tls 在校验失败时把原值交上来,
// 每一位就是"为什么不过",不必让用户去猜 0x8 和 0x200 的区别。
#define NP_CERT_EXPIRED        0x0001
#define NP_CERT_CN_MISMATCH    0x0004
#define NP_CERT_NOT_TRUSTED    0x0008
#define NP_CERT_MISSING        0x0040
#define NP_CERT_FUTURE         0x0200

// mbedTLS 错误码的绝对值。esp_tls_mbedtls.c 抓到的是 `-ret`(正数),
// 串口日志按 `-0x%04X` 打印,细节行也用同样的写法,两边能直接对。
#define NP_MTLS_INVALID_DATE      0x2400  // X509 日期解析/区间不对
#define NP_MTLS_CERT_UNKNOWN      0x2780  // X509 证书格式读不了
#define NP_MTLS_X509_VERIFY       0x2700  // X509 校验不过
#define NP_MTLS_NET_SOCKET        0x0042  // 套接字建不出来
#define NP_MTLS_NET_CONNECT       0x0044
#define NP_MTLS_NET_INVALID_CTX   0x0045
#define NP_MTLS_NET_RECV          0x004C
#define NP_MTLS_NET_SEND          0x004E
#define NP_MTLS_NET_CONN_RESET    0x0050
#define NP_MTLS_BAD_PROTOCOL      0x6E80  // 版本谈不拢
#define NP_MTLS_UNEXPECTED_REC    0x6700
#define NP_MTLS_ILLEGAL_PARAM     0x6600
#define NP_MTLS_TIMEOUT           0x6800
#define NP_MTLS_DECODE_ERROR      0x7300
#define NP_MTLS_INVALID_RECORD    0x7200
#define NP_MTLS_CONN_EOF          0x7280
#define NP_MTLS_UNEXPECTED_MSG    0x7700
#define NP_MTLS_FATAL_ALERT       0x7780
#define NP_MTLS_BAD_CERTIFICATE   0x7A00
#define NP_MTLS_CA_CHAIN_REQ      0x7680  // 根证书没加载上
#define NP_MTLS_NO_RNG            0x7400
#define NP_MTLS_BAD_INPUT         0x7100
#define NP_MTLS_BAD_CONFIG        0x5E80
#define NP_MTLS_ALLOC_FAILED      0x7F00

// 按字节截断会把多字节字符切成半截,屏幕上只会是占位框,不如整字丢掉。
// 返回去掉尾部不完整序列之后的长度(dst 此前必须是完整序列)。
static size_t complete_len(const char *dst, size_t len)
{
    size_t i = len;
    while (i > 0u && ((unsigned char)dst[i - 1] & 0xC0u) == 0x80u) i--;
    if (i == 0u) return len;
    const unsigned char lead = (unsigned char)dst[i - 1];
    size_t want = 1u;
    if ((lead & 0xE0u) == 0xC0u) want = 2u;
    else if ((lead & 0xF0u) == 0xE0u) want = 3u;
    else if ((lead & 0xF8u) == 0xF0u) want = 4u;
    return (len - (i - 1u) >= want) ? len : i - 1u;
}

static void put(char *dst, size_t cap, size_t *used, const char *fmt, ...)
{
    if (*used + 1u >= cap) return;
    char frag[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(frag, sizeof(frag), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    const size_t left = cap - *used - 1u;
    if ((size_t)n > left) n = (int)left;
    memcpy(dst + *used, frag, (size_t)n);
    *used = complete_len(dst, *used + (size_t)n);
    dst[*used] = '\0';
}

// 证书没验过:每一位标志都对应一个可行动的结论。
static const char *cert_reason(const np_err_src_t *src)
{
    const int f = src->tls_flags;
    if (f & (NP_CERT_FUTURE | NP_CERT_EXPIRED)) {
        // "还没生效"或"已过期":这块板没有 RTC,时钟停在 1970 时必然如此。
        return src->clock_synced ? "证书有效期不对(校过时钟?)"
                                 : "时钟未同步,证书校验不过";
    }
    if (f & NP_CERT_CN_MISMATCH) return "证书域名不匹配(疑被拦)";
    if (f & NP_CERT_NOT_TRUSTED) return "根证书不认,链对不上";
    if (f & NP_CERT_MISSING) return "没收到对端证书";
    return "证书校验不过(链或有效期)";
}

// 握手期 mbedTLS 的码:esp_err 只能说"卡在 TLS",这一层才说清卡在哪一步。
// 认不出来的返回 NULL,由调用方给笼统结论。
static const char *tls_reason(const np_err_src_t *src)
{
    switch (src->tls_code) {
    case NP_MTLS_ALLOC_FAILED:
        return "内存不足,TLS 握手中断";
    case NP_MTLS_NET_SOCKET:
        return "建不了套接字(内存/句柄耗尽)";
    case NP_MTLS_TIMEOUT:
        return "TLS 握手超时(443 已连上)";
    case NP_MTLS_CONN_EOF:
    case NP_MTLS_NET_CONN_RESET:
    case NP_MTLS_NET_SEND:
    case NP_MTLS_NET_RECV:
    case NP_MTLS_NET_CONNECT:
    case NP_MTLS_NET_INVALID_CTX:
        return "握手被掐断,疑网关拦 443";
    case NP_MTLS_FATAL_ALERT:
        return "TLS 被对端拒绝(收到告警)";
    case NP_MTLS_INVALID_RECORD:
    case NP_MTLS_UNEXPECTED_REC:
    case NP_MTLS_UNEXPECTED_MSG:
    case NP_MTLS_DECODE_ERROR:
    case NP_MTLS_ILLEGAL_PARAM:
        return "对端 TLS 报文不对,疑被拦";
    case NP_MTLS_BAD_CERTIFICATE:
    case NP_MTLS_CERT_UNKNOWN:
        return "对端证书读不了";
    case NP_MTLS_INVALID_DATE:
    case NP_MTLS_X509_VERIFY:
        return src->clock_synced ? "证书有效期不对(校过时钟?)"
                                 : "时钟未同步,证书校验不过";
    case NP_MTLS_BAD_PROTOCOL:
        return "TLS 版本谈不拢(443 已连上)";
    case NP_MTLS_CA_CHAIN_REQ:
        return "没加载上根证书";
    case NP_MTLS_NO_RNG:
        return "随机数发生器不可用";
    case NP_MTLS_BAD_INPUT:
    case NP_MTLS_BAD_CONFIG:
        return "本机 TLS 配置不对";
    default:
        return NULL;
    }
}

void np_errfmt_cause(char *dst, size_t cap, const np_err_src_t *src)
{
    if (dst == NULL || cap == 0u) return;
    dst[0] = '\0';
    size_t n = 0u;
    if (src == NULL) {
        put(dst, cap, &n, "未知失败");
        return;
    }
    const char *host = (src->host && src->host[0]) ? src->host : "服务器";
    const char *msg = src->msg ? src->msg : "";

    if (src->http_status > 0) {
        // 走到 HTTP 了:链路是通的,问题在协议或服务端,别再报"网络不通"。
        switch (src->http_status) {
        case 401: case 403:
            if (msg[0] != '\0') put(dst, cap, &n, "凭据被拒:%s", msg);
            else put(dst, cap, &n, "凭据被拒(HTTP %d)", src->http_status);
            break;
        case 404:
            put(dst, cap, &n, "找不到该 tailnet(404)");
            break;
        case 400: case 422:
            put(dst, cap, &n, "请求被拒(%d)", src->http_status);
            break;
        case 429:
            put(dst, cap, &n, "请求过于频繁(429)");
            break;
        default:
            if (src->http_status >= 500) {
                put(dst, cap, &n, "Tailscale 服务异常(%d)", src->http_status);
            } else if (src->http_status >= 300) {
                // 门户热点会把请求 302 到登录页:Wi-Fi 通,外网不通。
                put(dst, cap, &n, "被门户拦截(%d),外网不通", src->http_status);
            } else if (src->too_big) {
                put(dst, cap, &n, "设备列表过大");
            } else if (src->http_status == 200) {
                put(dst, cap, &n, "响应不是预期 JSON");
            } else {
                put(dst, cap, &n, "响应异常(HTTP %d)", src->http_status);
            }
            break;
        }
        return;
    }

    // 没走到 HTTP:按连接层哪一段断的开药。ts_api 交来的原因文本优先,
    // 它说的是"中途断了/没配凭据"这类低层码解释不了的事。
    if (src->too_big) {
        put(dst, cap, &n, "设备列表过大");
    } else if (msg[0] != '\0') {
        put(dst, cap, &n, "%s", msg);
    } else if (src->esp_err == NP_TLS_CANNOT_RESOLVE) {
        put(dst, cap, &n, "解析不到 %s,外网不通", host);
    } else if (src->tls_flags != 0) {
        put(dst, cap, &n, "%s", cert_reason(src));
    } else if (src->esp_err == NP_TLS_CONN_TIMEOUT) {
        put(dst, cap, &n, "连 %s:443 超时", host);
    } else if (src->esp_err == NP_TLS_CONNECT_FAILED ||
               src->esp_err == NP_TLS_CANNOT_SOCKET ||
               src->esp_err == NP_TLS_PROTO_FAMILY) {
        put(dst, cap, &n, "连不上 %s:443", host);
    } else if (src->esp_err == NP_TLS_HANDSHAKE_TO) {
        // 能走到握手说明 TCP 已经连上 443,别再说"连不上外网"。
        put(dst, cap, &n, "TLS 握手超时(443 已连上)");
    } else if (tls_reason(src) != NULL) {
        put(dst, cap, &n, "%s", tls_reason(src));
    } else if (src->tls_code != 0 || src->esp_err == NP_TLS_MBEDTLS_FAILED) {
        put(dst, cap, &n, "TLS 握手失败(443 已连上)");
    } else if (src->transport) {
        // 一个低层数都没有:如实说不知道,别猜凭据。
        put(dst, cap, &n, "网络不通(没走到 HTTP)");
    } else {
        put(dst, cap, &n, "响应不是预期 JSON");
    }
}

void np_errfmt_detail(char *dst, size_t cap, const np_err_src_t *src)
{
    if (dst == NULL || cap == 0u) return;
    dst[0] = '\0';
    if (src == NULL) return;
    size_t n = 0u;

    // 只报非零项,顺序按诊断价值:分类码 → TLS 码 → 证书标志 → errno → 状态码 → 堆。
    // TLS 码按 `-0x%04X` 写,和 esp-tls 自己那行
    // "mbedtls_ssl_handshake returned -0xXXXX" 对得上。
    if (src->esp_err != 0) put(dst, cap, &n, "esp 0x%X", (unsigned)src->esp_err);
    if (src->tls_code != 0) {
        if (src->tls_code > 0) put(dst, cap, &n, "%stls -0x%04X", n ? " " : "",
                                  (unsigned)src->tls_code);
        else put(dst, cap, &n, "%stls %d", n ? " " : "", src->tls_code);
    }
    if (src->tls_flags != 0) put(dst, cap, &n, "%sflag 0x%X", n ? " " : "",
                                 (unsigned)src->tls_flags);
    if (src->sys_errno != 0) put(dst, cap, &n, "%ssock %d", n ? " " : "",
                                 src->sys_errno);
    if (src->http_status != 0) put(dst, cap, &n, "%shttp %d", n ? " " : "",
                                   src->http_status);
    // 剩余堆与最大可分配块:TLS 握手要连续几十 KB,C3 上"握手失败"常常就是这里缺。
    if (src->free_heap > 0) put(dst, cap, &n, "%sheap %d", n ? " " : "",
                                src->free_heap);
    if (src->free_blk > 0) put(dst, cap, &n, "%sblk %d", n ? " " : "",
                               src->free_blk);
    if (n == 0u) put(dst, cap, &n, "无低层错误码");
}
