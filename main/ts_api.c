// main/ts_api.c —— esp_http_client(HTTPS, 内置 ISRG Root X1)+ cJSON。
#include "ts_api.h"

#include "app_cfg.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "np_jstream.h"
#include "np_mem.h"
#include "np_time.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "ts_api";

#define TS_API_BASE "https://api.tailscale.com/api/v2"
// 单次读一块 1 KB 的 body 交给切片器;整张设备表不再进内存(见 http_stream)。
// 其余接口(令牌、单台详情)响应有定,缓冲按 4 KB 起步、按需长大:一次性攥住
// 大块会让 TLS 握手和读响应抢同一块内部堆。
#define TS_RESP_START 4096u
#define TS_TOKEN_CAP 1600u
#define TS_OAUTH_TOKEN_URL "https://login.tailscale.com/api/v2/oauth/token"

// ISRG Root X1(Let's Encrypt 根,有效期到 2035-06-04)。api.tailscale.com 与
// login.tailscale.com 的证书链由其交叉签发覆盖;仅信任锚,不含凭据。
static const char TS_ROOT_CA[] =
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n"
    ;

static char s_jwt[TS_TOKEN_CAP];
static time_t s_jwt_expiry;
static ts_fail_t s_fail;

const ts_fail_t *ts_api_last_failure(void) { return &s_fail; }

// URL → 主机名(截到 '/' 或 ':' 为止)。失败时要说清是哪一台连不上。
static void url_host(const char *url, char *out, size_t cap)
{
    if (out == NULL || cap == 0u) return;
    out[0] = '\0';
    const char *p = strstr(url, "://");
    p = p ? p + 3u : url;
    size_t i = 0u;
    while (p[i] != '\0' && p[i] != '/' && p[i] != ':' && i + 1u < cap) {
        out[i] = p[i];
        i++;
    }
    out[i] = '\0';
}

// 把低层三个数原样抄下来:esp_err(esp-tls 的分类)、TLS 码/证书校验标志、errno。
// 判定留给 np_errfmt,这里不解读,免得猜错平台差异。why 非空时用它覆盖原因文本
// (读到一半断了这种低层码解释不了的情况,只能自己说)。
static void fill_fail(esp_http_client_handle_t c, const char *url, int esp_errno,
                      int status, const char *why)
{
    s_fail.http_status = status;
    s_fail.esp_err = esp_errno;
    s_fail.transport = status == 0;
    int code = 0, flags = 0;
    const int tls = esp_http_client_get_and_clear_last_tls_error(c, &code, &flags);
    s_fail.tls_code = code;
    s_fail.tls_flags = flags;
    // esp_http_client 只交 ESP_ERR_HTTP_CONNECT 这种笼统码,真正分段
    // (DNS/TCP/握手)的结论在 esp-tls 的 0x8000 段里(数值见 np_errfmt.c)。
    if (s_fail.esp_err == 0 || (tls >= 0x8000 && tls <= 0x80FF)) {
        s_fail.esp_err = tls;
    }
    s_fail.sys_errno = esp_http_client_get_errno(c);
    // 内存不足和"网络掐断"在 esp_err 上长得一样,把堆的两个数带出来才分得开:
    // 握手要一块几十 KB 的连续内存,剩余总量够而最大块不够照样会失败。
    s_fail.free_heap = (int)esp_get_free_heap_size();
    s_fail.free_blk = (int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    // 同一行再补 DMA 区与池子的账(np-0.1.26):TLS 记录缓冲走普通内部堆,显示
    // 那条路走 DMA,而 LVGL 池是另一本账 —— 三种"内存不足"缺的不是同一块。
    // 这里不采 lv_*:它要在 LVGL 锁里遍历池子,而 API 层不该去碰那把锁;
    // 应用层每 20 秒那条 "mem ..." 行里已经有,时间戳能对上。
    np_mem_snap_t mem;
    memset(&mem, 0, sizeof(mem));
    mem.free_heap = s_fail.free_heap;
    mem.largest_blk = s_fail.free_blk;
    mem.dma_free = (int)heap_caps_get_free_size(MALLOC_CAP_DMA);
    mem.dma_blk = (int)heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    char memline[NP_MEM_LINE_CAP];
    np_mem_format(&mem, memline, sizeof(memline));
    url_host(url, s_fail.host, sizeof(s_fail.host));
    if (why != NULL) strlcpy(s_fail.msg, why, sizeof(s_fail.msg));
    ESP_LOGW(TAG, "失败 host=%s http=%d esp=%d(%s) tls=0x%04X flag=0x%X sock=%d %s",
             s_fail.host, status, s_fail.esp_err, esp_err_to_name(s_fail.esp_err),
             (unsigned)s_fail.tls_code, (unsigned)s_fail.tls_flags,
             s_fail.sys_errno, memline);
}

// 非网络原因的早退(凭据缺失/超长):把话说清楚,别让屏幕报"网络不通"。
static void record_reason(const char *why)
{
    s_fail.transport = true;
    s_fail.http_status = 0;
    strlcpy(s_fail.msg, why, sizeof(s_fail.msg));
}

static ts_result_t param_fail(const char *why)
{
    record_reason(why);
    return TS_ERR_PARAM;
}

// 非 2xx 时把服务端 message 字段抄进失败记录(只取文本,已按清表过滤)。
static void record_api_message(const char *body)
{
    s_fail.msg[0] = '\0';
    cJSON *root = cJSON_Parse(body);
    if (!root) return;
    const cJSON *m = cJSON_GetObjectItemCaseSensitive(root, "message");
    if (cJSON_IsString(m) && m->valuestring[0] != '\0') {
        np_text_sanitize(s_fail.msg, sizeof(s_fail.msg), m->valuestring);
    }
    cJSON_Delete(root);
}

char *ts_api_display_name(char *dst, size_t cap, const char *host, const char *alt)
{
    if (dst == NULL || cap == 0u) return dst;
    dst[0] = '\0';
    const char *src = (host && host[0]) ? host : (alt ? alt : "");
    const char *p = src;
    static const char NOREPLY[] = "login-noreply_";
    if (strncmp(p, NOREPLY, sizeof(NOREPLY) - 1u) == 0) p += sizeof(NOREPLY) - 1u;
    if (*p == '\0') p = (alt && alt[0]) ? alt : "";
    np_text_sanitize(dst, cap, p);
    return dst;
}

static void copy_bounded(char *dst, size_t cap, const char *src)
{
    if (cap == 0u) return;
    size_t i = 0;
    for (; src && src[i] != '\0' && i + 1u < cap; i++) dst[i] = src[i];
    dst[i] = '\0';
}

// x-www-form-urlencoded 转义(保留 '%' 自身)。失败返回 false。
static bool form_encode(const char *src, char *out, size_t cap)
{
    static const char HEX[] = "0123456789ABCDEF";
    size_t used = 0;
    for (const unsigned char *s = (const unsigned char *)src; *s; s++) {
        char buf[3];
        size_t n = 1;
        if (*s == ' ') { buf[0] = '+'; }
        else if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') ||
                 (*s >= '0' && *s <= '9') || strchr("-._~", *s) != NULL) {
            buf[0] = (char)*s;
        } else {
            n = 3;
            buf[0] = '%'; buf[1] = HEX[*s >> 4]; buf[2] = HEX[*s & 0xF];
        }
        if (used + n >= cap) return false;
        memcpy(out + used, buf, n);
        used += n;
    }
    out[used] = '\0';
    return true;
}

static ts_result_t http_exchange(const char *url, const char *auth_header,
                                 const char *post_body, char **buf_out, size_t cap,
                                 size_t *out_len)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .cert_pem = TS_ROOT_CA,
        .timeout_ms = 15000,
        .buffer_size = 4096,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        record_reason("内存不足(建不了请求)");
        url_host(url, s_fail.host, sizeof(s_fail.host));
        return TS_ERR_NETWORK;
    }

    ts_result_t result;
    const int body_len = post_body ? (int)strlen(post_body) : 0;
    if (auth_header) esp_http_client_set_header(c, "Authorization", auth_header);
    if (post_body) {
        esp_http_client_set_method(c, HTTP_METHOD_POST);
        esp_http_client_set_header(c, "Content-Type",
                                   "application/x-www-form-urlencoded");
        esp_http_client_set_post_field(c, post_body, body_len);
    } else {
        esp_http_client_set_method(c, HTTP_METHOD_GET);
    }

    const esp_err_t oerr = esp_http_client_open(c, body_len);
    if (oerr != ESP_OK) {
        // DNS/TCP/TLS 都在这一步分家,填完细节再放句柄(cleanup 后就取不到了)。
        fill_fail(c, url, (int)oerr, 0, NULL);
        esp_http_client_cleanup(c);
        return TS_ERR_NETWORK;
    }
    if (post_body) {
        (void)esp_http_client_write(c, post_body, body_len);
    }
    (void)esp_http_client_fetch_headers(c);
    const int status = esp_http_client_get_status_code(c);

    // 响应缓冲与 TLS 握手要的是同一块内部堆,握手之前先攥着大块实测会直接
    // -0x7F00 分配失败。所以握手做完、连接内存已经拿到之后,再从 4 KB 起
    // 按需要长大。调用方自备缓冲(栈上的)时一律不动,只在自家这块上 realloc。
    size_t used = 0;
    bool too_big = false;
    char *buf = (buf_out && *buf_out) ? *buf_out : NULL;
    bool owned = false;
    size_t own_cap = cap;
    if (buf == NULL && cap > 0u) {
        own_cap = cap < TS_RESP_START ? cap : TS_RESP_START;
        buf = malloc(own_cap);
        if (!buf) {
            result = TS_ERR_NETWORK;
            fill_fail(c, url, 0, 0, "内存不足(装不下响应)");
            goto out;
        }
        owned = true;
        if (buf_out) *buf_out = buf;
    }

    for (;;) {
        int r = esp_http_client_read_response(c, buf + used, (int)(own_cap - used - 1u));
        if (r < 0) {
            result = TS_ERR_NETWORK;
            // 状态码已经拿到,链路却是通的,别再按 HTTP 分支解释:
            // 报"中途断开"比报"响应不是预期 JSON"更接近事实。
            fill_fail(c, url, (int)ESP_FAIL, 0, "响应中途断开");
            goto out;
        }
        if (r == 0) break;
        used += (size_t)r;
        if (used + 1u >= own_cap) {
            if (!owned || own_cap >= cap) { too_big = true; break; }
            const size_t next = (own_cap * 2u > cap) ? cap : own_cap * 2u;
            char *bigger = realloc(buf, next);
            if (!bigger) {
                // 长不动是内存问题,不是服务端响应太大,原样保留已分配的这块。
                result = TS_ERR_NETWORK;
                fill_fail(c, url, 0, 0, "内存不足(装不下响应)");
                goto out;
            }
            buf = bigger;
            own_cap = next;
            if (buf_out) *buf_out = buf;
        }
    }
    buf[used] = '\0';
    if (too_big) {
        result = TS_ERR_TOO_BIG;
        fill_fail(c, url, 0, status, NULL);
        s_fail.too_big = true;
        goto out;
    }

    switch (status) {
    case 200: result = used > 0u ? TS_OK : TS_ERR_PARSE; break;
    case 401: case 403: result = TS_ERR_AUTH; break;
    case 429: result = TS_ERR_RATE_LIMITED; break;
    default: result = (status >= 500) ? TS_ERR_SERVER : TS_ERR_PARSE; break;
    }
    if (result != TS_OK) {
        fill_fail(c, url, 0, status, NULL);
        record_api_message(buf);   // 服务端 message 里往往直接写着为什么被拒
    }
out:
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    if (result != TS_OK) ESP_LOGW(TAG, "HTTP %d url=%s err=%d", status, url, (int)result);
    if (out_len) *out_len = used;
    return result;
}

// GET 并把 body 边读边交给 np_jstream 切片:整张设备表专用。
// 为什么不能再走 http_exchange:那条路要先把整个 body 读进一块连续缓冲(实测
// 4 KB 起、倍到 24 KB 就长不动了),再让 cJSON 在上面建一棵 2.5~4 倍大的树。
// 主机一多,两步都撑不住,报出来的就是"内存不足,装不下响应"。这里一次只攥
// 一块 1 KB 读缓冲和一台设备的片段,峰值内存由"最大的一台"决定,不由整个
// tailnet 决定 —— 服务端可以给 500 台,板子还是那点内存。
// 返回值含义与 http_exchange 一致;失败时的细节(状态码、服务端 message)同样
// 落在 s_fail 里。
#define TS_DEV_CHUNK    1024u  // 一次从 TLS 读多少,读完就交给切片器
#define TS_ERR_BODY_CAP 256u   // 非 200 时只为抄一句服务端 message

static ts_result_t http_stream(const char *url, const char *auth_header,
                               np_jstream_t *js, np_js_fn cb, void *user)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .cert_pem = TS_ROOT_CA,
        .timeout_ms = 15000,
        .buffer_size = 4096,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        record_reason("内存不足(建不了请求)");
        url_host(url, s_fail.host, sizeof(s_fail.host));
        return TS_ERR_NETWORK;
    }
    esp_http_client_set_method(c, HTTP_METHOD_GET);
    if (auth_header) esp_http_client_set_header(c, "Authorization", auth_header);

    const esp_err_t oerr = esp_http_client_open(c, 0);
    if (oerr != ESP_OK) {
        // DNS/TCP/TLS 都在这一步分家,填完细节再放句柄(cleanup 后就取不到了)。
        fill_fail(c, url, (int)oerr, 0, NULL);
        esp_http_client_cleanup(c);
        return TS_ERR_NETWORK;
    }
    (void)esp_http_client_fetch_headers(c);
    const int status = esp_http_client_get_status_code(c);

    ts_result_t result;
    char *chunk = malloc(TS_DEV_CHUNK);
    if (!chunk) {
        result = TS_ERR_NETWORK;
        fill_fail(c, url, 0, 0, "内存不足(读不动响应)");
        goto out;
    }
    // 非 200 时 body 是错误 JSON,不是设备表:攒一小段就够抄那句 message。
    char err_body[TS_ERR_BODY_CAP];
    err_body[0] = '\0';
    size_t err_used = 0;

    for (;;) {
        const int r = esp_http_client_read_response(c, chunk, (int)TS_DEV_CHUNK - 1);
        if (r < 0) {
            result = TS_ERR_NETWORK;
            // 状态码已经拿到,链路却是通的,别再按 HTTP 分支解释。
            fill_fail(c, url, (int)ESP_FAIL, 0, "响应中途断开");
            goto out;
        }
        if (r == 0) break;
        if (status == 200) {
            np_jstream_feed(js, chunk, (size_t)r, cb, user);
            if (np_jstream_finished(js)) break;  // 数组闭合了,后面不必再读
        } else if (err_used + 1u < sizeof(err_body)) {
            const size_t room = sizeof(err_body) - err_used - 1u;
            const size_t n = ((size_t)r < room) ? (size_t)r : room;
            memcpy(err_body + err_used, chunk, n);
            err_used += n;
            err_body[err_used] = '\0';
        }
    }

    switch (status) {
    case 200:
        // 切片器没等到 devices 数组(响应形状不对或中途断)就是解析失败,
        // 不能当成"0 台设备":那会在屏幕上变成一个诚实的空列表。
        result = (js->phase == NP_JS_DONE) ? TS_OK : TS_ERR_PARSE;
        break;
    case 401: case 403: result = TS_ERR_AUTH; break;
    case 429: result = TS_ERR_RATE_LIMITED; break;
    default: result = (status >= 500) ? TS_ERR_SERVER : TS_ERR_PARSE; break;
    }
    if (result != TS_OK) {
        fill_fail(c, url, 0, status, NULL);
        if (status != 200) record_api_message(err_body);
    }
    free(chunk);
out:
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    if (result != TS_OK) ESP_LOGW(TAG, "HTTP %d url=%s err=%d", status, url, (int)result);
    return result;
}

// OAuth client_credentials → JWT(access token 模式直接拼 Bearer 头)。
static ts_result_t build_auth_header(char *out, size_t cap)
{
    if (app_cfg_auth_type() == NP_AUTH_ACCESS_TOKEN) {
        const char *t = app_cfg_access_token();
        if (t[0] == '\0') return param_fail("未配置 access token");
        if (snprintf(out, cap, "Bearer %s", t) >= (int)cap) return param_fail("access token 过长");
        return TS_OK;
    }

    const char *id = app_cfg_oauth_id();
    const char *sec = app_cfg_oauth_secret();
    if (id[0] == '\0' || sec[0] == '\0') return param_fail("未配置 OAuth client 凭据");

    const time_t now = time(NULL);
    if (s_jwt[0] != '\0' && now < s_jwt_expiry - 60) {
        if (snprintf(out, cap, "Bearer %s", s_jwt) >= (int)cap) return param_fail("JWT 过长");
        return TS_OK;
    }

    char enc_id[256], enc_sec[384];
    if (!form_encode(id, enc_id, sizeof(enc_id)) ||
        !form_encode(sec, enc_sec, sizeof(enc_sec))) return param_fail("OAuth 凭据过长");
    char body[768];
    if (snprintf(body, sizeof(body),
                 "grant_type=client_credentials&client_id=%s&client_secret=%s&scope=all%%3Aread",
                 enc_id, enc_sec) >= (int)sizeof(body)) {
        return param_fail("OAuth 请求过长");
    }

    char resp[2048];
    char *resp_p = resp;   // 栈上缓冲,http_exchange 不会再自己 malloc
    ts_result_t r = http_exchange(TS_OAUTH_TOKEN_URL, NULL, body,
                                  &resp_p, sizeof(resp), NULL);
    if (r != TS_OK) return r;

    cJSON *root = cJSON_Parse(resp);
    if (!root) {
        record_reason("令牌响应无法解析");
        return TS_ERR_PARSE;
    }
    const cJSON *jt = cJSON_GetObjectItemCaseSensitive(root, "access_token");
    const cJSON *ex = cJSON_GetObjectItemCaseSensitive(root, "expires_in");
    r = TS_ERR_PARSE;
    if (cJSON_IsString(jt) && strlen(jt->valuestring) < TS_TOKEN_CAP) {
        copy_bounded(s_jwt, sizeof(s_jwt), jt->valuestring);
        // 文档给 3600;缺字段时保守按 5 分钟。
        s_jwt_expiry = time(NULL) + (cJSON_IsNumber(ex) ? (time_t)ex->valuedouble
                                                        : 300);
        r = TS_OK;
    }
    if (r != TS_OK) record_reason("响应里没有 access_token");
    cJSON_Delete(root);
    if (r == TS_OK) ESP_LOGI(TAG, "OAuth JWT 已更新");
    return r;
}

static void parse_device(const cJSON *item, np_device_t *dev, int64_t now)
{
    memset(dev, 0, sizeof(*dev));
    dev->ctrl = -1;   // 没读到字段就是"控制面没说",不能当成"没连着"
    char name[64];
    const cJSON *hn = cJSON_GetObjectItemCaseSensitive(item, "hostname");
    const cJSON *nm = cJSON_GetObjectItemCaseSensitive(item, "name");
    ts_api_display_name(name, sizeof(name),
                        cJSON_IsString(hn) ? hn->valuestring : NULL,
                        cJSON_IsString(nm) ? nm->valuestring : NULL);
    copy_bounded(dev->name, sizeof(dev->name), name);

    const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
    // id 是详情接口的寻址键。截断后的串照样能发请求,只是必然 404 —— 不如不收。
    if (cJSON_IsString(id) && strlen(id->valuestring) < sizeof(dev->id))
        copy_bounded(dev->id, sizeof(dev->id), id->valuestring);

    const cJSON *addrs = cJSON_GetObjectItemCaseSensitive(item, "addresses");
    if (cJSON_IsArray(addrs)) {
        const cJSON *a0 = cJSON_GetArrayItem(addrs, 0);
        if (cJSON_IsString(a0)) copy_bounded(dev->addr, sizeof(dev->addr),
                                             a0->valuestring);
    }
    const cJSON *os = cJSON_GetObjectItemCaseSensitive(item, "os");
    if (cJSON_IsString(os)) copy_bounded(dev->os, sizeof(dev->os), os->valuestring);
    const cJSON *cv = cJSON_GetObjectItemCaseSensitive(item, "clientVersion");
    if (cJSON_IsString(cv)) copy_bounded(dev->ver, sizeof(dev->ver), cv->valuestring);

    const cJSON *user = cJSON_GetObjectItemCaseSensitive(item, "user");
    if (cJSON_IsString(user)) {
        char owner[NP_OWNER_LEN];
        np_text_sanitize(owner, sizeof(owner), user->valuestring);
        copy_bounded(dev->owner, sizeof(dev->owner), owner);
    }

    // lastSeen 有三种长相:没字段、Go 零值时间(0001-01-01)、真断线时刻。
    // 零值时间能被解析成功,所以不能只按"有没有字段"判在线,交给模型折算。
    const cJSON *ls = cJSON_GetObjectItemCaseSensitive(item, "lastSeen");
    int64_t seen = 0;
    if (!(cJSON_IsString(ls) && np_time_parse_rfc3339(ls->valuestring, &seen))) seen = 0;
    dev->last_seen = np_seen_disconnect_at(now, seen);
    const cJSON *ct = cJSON_GetObjectItemCaseSensitive(item, "created");
    if (cJSON_IsString(ct)) {
        int64_t cr = 0;
        if (np_time_parse_rfc3339(ct->valuestring, &cr) && cr >= NP_TIME_FLOOR_EPOCH) {
            dev->created = cr;
        }
    }
    const cJSON *ex = cJSON_GetObjectItemCaseSensitive(item, "expires");
    // expires 也可能是零值时间(禁用了密钥过期的节点),同一个坑,同样折成"没有"。
    int64_t exp = 0;
    if (cJSON_IsString(ex) && np_time_parse_rfc3339(ex->valuestring, &exp) &&
        exp >= NP_TIME_FLOOR_EPOCH) dev->expires = exp;

    const cJSON *auth = cJSON_GetObjectItemCaseSensitive(item, "authorized");
    dev->authorized = cJSON_IsBool(auth) ? cJSON_IsTrue(auth) : true;
    const cJSON *ext = cJSON_GetObjectItemCaseSensitive(item, "isExternal");
    dev->external = cJSON_IsBool(ext) && cJSON_IsTrue(ext);
    const cJSON *upd = cJSON_GetObjectItemCaseSensitive(item, "updateAvailable");
    dev->update_available = cJSON_IsBool(upd) && cJSON_IsTrue(upd);
    const cJSON *keoff = cJSON_GetObjectItemCaseSensitive(item, "keyExpiryDisabled");
    dev->key_expiry_off = cJSON_IsBool(keoff) && cJSON_IsTrue(keoff);
    const cJSON *blk = cJSON_GetObjectItemCaseSensitive(item, "blocksIncomingConnections");
    dev->blocks_incoming = cJSON_IsBool(blk) && cJSON_IsTrue(blk);
    const cJSON *eph = cJSON_GetObjectItemCaseSensitive(item, "isEphemeral");
    dev->ephemeral = cJSON_IsBool(eph) && cJSON_IsTrue(eph);

    // connectedToControl 才是"这台现在活着"的直接答案:文档说明节点连着控制面时
    // lastSeen 会被整个省略,所以只看 lastSeen 会把长连接的机器判成离线。
    const cJSON *ctc = cJSON_GetObjectItemCaseSensitive(item, "connectedToControl");
    if (cJSON_IsBool(ctc)) dev->ctrl = cJSON_IsTrue(ctc) ? 1 : 0;
    dev->online = np_device_is_online(dev->ctrl, dev->last_seen);

    // 标签合并成一行,只保留能放进缓冲区的部分。
    const cJSON *tags = cJSON_GetObjectItemCaseSensitive(item, "tags");
    if (cJSON_IsArray(tags)) {
        size_t used = 0;
        const cJSON *t = NULL;
        cJSON_ArrayForEach(t, tags) {
            if (!cJSON_IsString(t)) continue;
            char clean[NP_TAGS_LEN];
            np_text_sanitize(clean, sizeof(clean), t->valuestring);
            if (clean[0] == '\0') continue;
            const int n = snprintf(dev->tags + used, sizeof(dev->tags) - used,
                                   used ? " %s" : "%s", clean);
            if (n < 0 || (size_t)n >= sizeof(dev->tags) - used) break;
            used += (size_t)n;
        }
    }
}

// 流式解析时一次只攒一台设备的原始 JSON。3 KB 是按"最肥的一台"留的(几十个
// IP 地址 + 一串标签),不是按整张表留的 —— 这是本文件里唯一一处与 tailnet 规模
// 无关的缓冲。真装不下就跳过那一台并记日志,其余照常上屏。
#define TS_DEV_OBJ_CAP 3072u

typedef struct {
    np_jstream_t *js;
    np_list_t *list;
    int64_t now;
    uint32_t dropped;   // 片段太大或解析不了的台数(只进日志)
} dev_ctx_t;

// 切片器交出完整一台设备:建一棵小 cJSON 树、填进快照、立刻删掉。
// 满了(24 台)就请切片器停止回调,后面只数个数。
static void dev_object(void *user, const char *json, bool complete)
{
    dev_ctx_t *d = user;
    if (!complete) {
        d->dropped++;
        return;
    }
    cJSON *item = cJSON_Parse(json);
    if (item == NULL) {
        d->dropped++;
        return;
    }
    if (d->list->count < NP_MAX_DEVICES) {
        parse_device(item, &d->list->items[d->list->count], d->now);
        d->list->count++;
        if (d->list->count >= NP_MAX_DEVICES) np_jstream_stop(d->js);
    }
    cJSON_Delete(item);
}

ts_result_t ts_api_fetch_devices(np_list_t *list)
{
    if (list == NULL) return TS_ERR_PARAM;
    memset(&s_fail, 0, sizeof(s_fail));   // 每轮从干净的理由开始
    char auth[TS_TOKEN_CAP + 16];
    ts_result_t r = build_auth_header(auth, sizeof(auth));
    if (r != TS_OK) return r;

    const char *tailnet = app_cfg_tailnet();
    char url[256];
    if (snprintf(url, sizeof(url),
                 TS_API_BASE "/tailnet/%s/devices?fields=default",
                 tailnet[0] ? tailnet : "-") >= (int)sizeof(url)) {
        return param_fail("tailnet 名过长");
    }

    char *obj = malloc(TS_DEV_OBJ_CAP);
    if (!obj) {
        record_reason("内存不足(装不下一台设备)");
        return TS_ERR_NETWORK;
    }
    np_jstream_t js;
    np_jstream_init(&js, obj, TS_DEV_OBJ_CAP);
    // 备用快照:UI 读的是另一块,这里清空不影响正在显示的画面。
    np_list_clear(list);
    dev_ctx_t ctx = { .js = &js, .list = list, .now = (int64_t)time(NULL),
                      .dropped = 0u };
    r = http_stream(url, auth, &js, dev_object, &ctx);
    free(obj);
    if (r == TS_OK) {
        list->total = np_jstream_total(&js);
        np_list_sort(list);
        list->version++;
        if (ctx.dropped > 0u) {
            ESP_LOGW(TAG, "%u 台设备信息过长,未列入(共 %u 台)",
                     (unsigned)ctx.dropped, (unsigned)list->total);
        }
    }
    return r;
}

// 路由数组取前 NP_ROUTE_MAX 条(CIDR 都是 ASCII,不必过字形清表)。
static void copy_routes(const cJSON *arr, char out[NP_ROUTE_MAX][NP_ROUTE_LEN],
                        uint8_t *n)
{
    *n = 0u;
    if (!cJSON_IsArray(arr)) return;
    const cJSON *it = NULL;
    cJSON_ArrayForEach(it, arr) {
        if (*n >= NP_ROUTE_MAX || !cJSON_IsString(it)) continue;
        copy_bounded(out[*n], NP_ROUTE_LEN, it->valuestring);
        if (out[*n][0] != '\0') (*n)++;
    }
}

#define TS_DETAIL_CAP 6144u

ts_result_t ts_api_fetch_device(const char *dev_id, np_detail_t *out)
{
    if (dev_id == NULL || dev_id[0] == '\0' || out == NULL) {
        record_reason("缺少设备编号");
        return TS_ERR_PARAM;
    }
    np_detail_clear(out);
    memset(&s_fail, 0, sizeof(s_fail));

    char auth[TS_TOKEN_CAP + 16];
    ts_result_t r = build_auth_header(auth, sizeof(auth));
    if (r != TS_OK) return r;

    char url[256];
    if (snprintf(url, sizeof(url), TS_API_BASE "/device/%s?fields=all",
                 dev_id) >= (int)sizeof(url)) {
        return param_fail("设备编号过长");
    }

    char *resp = NULL;
    r = http_exchange(url, auth, NULL, &resp, TS_DETAIL_CAP, NULL);
    if (r != TS_OK) { free(resp); return r; }

    cJSON *root = cJSON_Parse(resp);
    free(resp);
    if (!root) return TS_ERR_PARSE;

    // 发行版三段拼一行:debian 12 bookworm。缺任何一段就跳过那一段。
    const cJSON *dis = cJSON_GetObjectItemCaseSensitive(root, "distro");
    if (cJSON_IsObject(dis)) {
        const cJSON *parts[3] = {
            cJSON_GetObjectItemCaseSensitive(dis, "name"),
            cJSON_GetObjectItemCaseSensitive(dis, "version"),
            cJSON_GetObjectItemCaseSensitive(dis, "codeName"),
        };
        char buf[sizeof(out->distro)];
        size_t used = 0u;
        buf[0] = '\0';
        for (int i = 0; i < 3; i++) {
            if (!cJSON_IsString(parts[i]) || parts[i]->valuestring[0] == '\0') continue;
            char clean[32];
            np_text_sanitize(clean, sizeof(clean), parts[i]->valuestring);
            const int n = snprintf(buf + used, sizeof(buf) - used,
                                   used ? " %s" : "%s", clean);
            if (n < 0 || (size_t)n >= sizeof(buf) - used) break;
            used += (size_t)n;
        }
        copy_bounded(out->distro, sizeof(out->distro), buf);
    }
    const cJSON *ssh = cJSON_GetObjectItemCaseSensitive(root, "sshEnabled");
    out->ssh = cJSON_IsBool(ssh) && cJSON_IsTrue(ssh);
    copy_routes(cJSON_GetObjectItemCaseSensitive(root, "enabledRoutes"),
                out->routes_en, &out->n_en);
    copy_routes(cJSON_GetObjectItemCaseSensitive(root, "advertisedRoutes"),
                out->routes_adv, &out->n_adv);

    const cJSON *cc = cJSON_GetObjectItemCaseSensitive(root, "clientConnectivity");
    if (cJSON_IsObject(cc)) {
        out->have_conn = true;
        const cJSON *eps = cJSON_GetObjectItemCaseSensitive(cc, "endpoints");
        if (cJSON_IsArray(eps)) {
            const long n = cJSON_GetArraySize(eps);
            out->endpoints = (n > 0) ? (uint16_t)((n > 65535) ? 65535 : n) : 0u;
        }
        const cJSON *mv = cJSON_GetObjectItemCaseSensitive(cc, "mappingVariesByDestIP");
        out->mapping_varies = cJSON_IsBool(mv) && cJSON_IsTrue(mv);
        // latency 以 DERP 区域号为键。时延可能报 -1(没测到),那种不算首选。
        const cJSON *lat = cJSON_GetObjectItemCaseSensitive(cc, "latency");
        if (cJSON_IsObject(lat)) {
            int best_ms = -1;
            char best[8] = "";
            const cJSON *reg = NULL;
            cJSON_ArrayForEach(reg, lat) {
                if (!cJSON_IsObject(reg)) continue;
                const cJSON *pref = cJSON_GetObjectItemCaseSensitive(reg, "preferred");
                const cJSON *ms = cJSON_GetObjectItemCaseSensitive(reg, "latencyMs");
                if (!cJSON_IsTrue(pref) || !cJSON_IsNumber(ms)) continue;
                const long v = (long)(ms->valuedouble + 0.5);
                if (v < 0) continue;
                if (best_ms < 0 || v < best_ms) {
                    best_ms = (int)v;
                    copy_bounded(best, sizeof(best), reg->string);
                }
            }
            if (best_ms >= 0) {
                out->derp_ms = (int16_t)best_ms;
                copy_bounded(out->derp, sizeof(out->derp), best);
            }
        }
    }
    cJSON_Delete(root);
    return TS_OK;
}
