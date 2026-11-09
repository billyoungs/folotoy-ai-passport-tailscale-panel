// main/app_prov.c —— esp_http_server 双模式配置服务。
// 安全约定:LAN 模式所有写接口必须携带与屏幕一致的 6 位配对码;凭据只写不回显;
// 服务生命周期 = 会话窗口(距最后一次请求空闲 10 分钟,自启动起绝对上限 30 分钟),
// 点"保存并联网"或窗口到点才关闭。
// 页面按常量片段拼一次,运行时不再格式化,免得把半截 JS 发出去。
#include "app_prov.h"

#include "app_cfg.h"
#include "app_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "np_dns_server.h"
#include "np_form.h"
#include "np_json.h"
#include "np_mem.h"
#include "np_scanlist.h"
#include "np_session.h"
#include "nvs.h"

#include "lwip/ip4_addr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "app_prov";

#define PROV_IDLE_US (600LL * 1000 * 1000) // 距最后一次请求空闲 10 分钟
#define PROV_HARD_US (1800LL * 1000 * 1000) // 自启动起绝对上限 30 分钟
// 页面上点了收尾类的按钮之后,给浏览器读完这一次响应的时间。再多没有意义,
// 再少就是让用户的页面停在一个发不出请求的服务上。
#define PROV_CLOSE_GRACE_US (1500LL * 1000)
#define BODY_CAP 2048
#define PAGE_CAP 8192      // 拼装后的整页 HTML(含脚本)
#define SCAN_JSON_CAP 1536 // 12 个网络加已配置名的余量
#define AP_SSID_CAP 33

static np_prov_mode_t s_mode;
static httpd_handle_t s_hd;
static esp_netif_t *s_ap_netif, *s_sta_netif;
static np_session_t s_session;
static volatile uint32_t s_req_seq; // httpd 任务写,np_app 读
static uint32_t s_seq_seen;         // 上次取走的序号;只有 np_app 读写
// 网页点了收尾按钮:跨越任务的只传这一个标志,int64 的时刻由 np_app 自己盖,
// 免得 httpd 那四个线程里谁都可能去动同一组 64 位字段。
static volatile bool s_close_requested;
static bool s_close_scheduled;      // np_app 已把这个请求折算成关闭时刻
static bool s_wifi_applied, s_ts_applied;
static char s_pairing[8];
static char s_ap_ssid[AP_SSID_CAP]; // 扫描时滤掉设备自己的热点
// 引导热点名缓存:Wi-Fi 保存成功之前,热点重开沿用同一个名字 —— 手机此刻
// 连着的就是它,换个新随机值等于把它正在用的热点抽掉。热点无密码,没有
// 第二段要缓存。
static char s_ap_cred_ssid[AP_SSID_CAP];
static bool s_ap_cred_cached, s_ap_cred_stale;
static char s_html[PAGE_CAP];
static char s_scan_json[SCAN_JSON_CAP];
static volatile bool s_sync_request;

static void copy_text(char *dst, size_t cap, const char *src);

// ---------------------------------------------------------------- 页面模板
// 双语注释规矩:本文件中文只出现在注释与页面/JSON 文案里。
static const char HTML_HEAD[] =
"<!doctype html><html lang=zh-CN><meta charset=utf-8>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>网络面板配置</title>"
"<style>body{background:#0b1220;color:#e2e8f0;font:15px/1.6 system-ui,"
"Microsoft YaHei,sans-serif;margin:24px auto;max-width:640px}"
"h1{font-size:19px}section{background:#101a2d;border:1px solid #1e293b;"
"border-radius:10px;padding:14px;margin:14px 0}"
"label{display:block;margin:8px 0;font-size:13px;color:#94a3b8}"
"input,select{width:100%;padding:8px;box-sizing:border-box;background:#0b1220;"
"color:#e2e8f0;border:1px solid #334155;border-radius:6px}"
// 勾选框不是文本框:上面那条 width:100% 会把它撑满整行,文字被挤到下一行去,
// 看着就像两个不相干的东西。它要的是"框在左、字在右"贴在一起。
"input[type=checkbox]{width:auto;padding:0;margin:0 6px 0 0}"
"label.chk{display:flex;align-items:center}"
"button{padding:10px 14px;margin:4px 6px 0 0;border:0;border-radius:6px;"
"background:#2563eb;color:#fff;font-size:15px}"
"button.warn{background:#b91c1c}summary{color:#94a3b8;margin:6px 0}"
"#st,#wst{white-space:pre-wrap;font-size:13px;color:#94a3b8;margin:0}"
"#msg{color:#22c55e;min-height:22px;font-size:14px}"
"</style><h1>网络面板配置</h1><p id=dev>";

static const char HTML_DEV_AP[] =
"首次引导:选一个网络、填密码,点\"保存并联网\"即可。"
"Tailscale 凭据可以等设备联网之后再补。";
static const char HTML_DEV_LAN[] =
"日常修改(闲置 10 分钟后自动关闭),改完点\"保存\"。";

static const char HTML_STATUS[] =
"<section><b>设备状态</b><p id=st>读取中…</p></section>";

// 下拉只负责"选中即填入网络名",提交仍走输入框,扫描结果与手动输入共用一条路径。
// 两种模式都不自动扫描,占位统一为"点「重新扫描」列出附近网络":开页就扫会把
// 射频借走约两秒,正在关联的手机会连不上,页面首屏也开不出来。
static const char HTML_WIFI_AP[] =
"<section><b>Wi-Fi</b>"
"<label>附近网络<select id=wl "
"onchange=\"if(this.value)q('ssid').value=this.value\">"
"<option value=\"\">点「重新扫描」列出附近网络</option></select></label>"
"<button onclick=scan()>重新扫描</button><p id=wst></p>"
"<label>网络名<input id=ssid maxlength=31 autocomplete=off></label>"
"<label>密码<input id=psk type=password maxlength=63 autocomplete=off></label>"
"<label class=chk><input type=checkbox id=showp "
"onclick=\"q('psk').type=q('showp').checked?'text':'password'\">显示密码"
"</label></section>";

static const char HTML_WIFI_LAN[] =
"<section><b>Wi-Fi</b>"
"<label>附近网络<select id=wl "
"onchange=\"if(this.value)q('ssid').value=this.value\">"
"<option value=\"\">点「重新扫描」列出附近网络</option></select></label>"
"<button onclick=scan()>重新扫描</button><p id=wst>改 Wi-Fi 前先想清楚:扫描期间设备会短暂掉线。</p>"
"<label>网络名<input id=ssid maxlength=31 autocomplete=off></label>"
"<label>密码<input id=psk type=password maxlength=63 autocomplete=off></label>"
"<label class=chk><input type=checkbox id=showp "
"onclick=\"q('psk').type=q('showp').checked?'text':'password'\">显示密码"
"</label></section>";

// 局域网模式默认展开(就是来改凭据的);引导页不再渲染这段 —— Tailscale 凭据
// 整体搬到设备联网之后的局域网地址上录,手机热点页只留 Wi-Fi(np-0.2.1 起)。
static const char HTML_ADV_LAN[] =
"<details open><summary>Tailscale 凭据与轮询</summary>";

// Tailscale 凭据输入按明文显示:填的人是机主,打点反而看不出填对没有。
// 状态行里的 token_mask 仍然只回显后 4 位。
static const char HTML_CREDENTIALS[] =
"<section><b>Tailscale 凭据</b>"
"<label>类型<select id=atype><option value=0>API 访问令牌</option>"
"<option value=1>OAuth 客户端</option></select></label>"
"<label>访问令牌(tskey-…)<input id=token autocomplete=off></label>"
"<label>OAuth Client ID<input id=oid autocomplete=off></label>"
"<label>OAuth 密钥<input id=osec autocomplete=off></label>"
"<label>Tailnet(留空=默认)<input id=tailnet autocomplete=off></label></section>"
"<section><b>轮询与显示</b>"
"<label>基准间隔(秒)<input id=base value=120></label>"
"<label>退避上限(秒)<input id=cap value=3600></label>"
"<label>时区偏移(小时,-14 到 14)<input id=tz value=8></label></section>";

// 引导页没有折叠段要关了:操作段不再以 </details> 开头,凭据栏整体不渲染。
static const char HTML_ACTIONS_AP[] =
"<section><b>操作</b><button onclick=go(1)>保存并联网</button>"
"<p>Tailscale 凭据之后用局域网模式补填。</p></section>";

static const char HTML_ACTIONS_LAN[] =
"</details><section><b>操作</b><button onclick=go(0)>保存</button> "
"<button onclick=sync()>立即同步一次</button> "
"<button class=warn onclick=clr()>清除 Tailscale 凭据</button></section>";

static const char HTML_SCRIPT[] =
"<p id=msg></p><script>"
"function q(i){return document.getElementById(i)}"
"function esc(s){return s.replace(/[&<>\"]/g,function(c){"
"return {'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;'}[c]})}"
"var P0={},FIRST=1;"
// 浏览器侧的失败(设备已关服务、掉线、槽位占满)不会落进 j.error,只会 reject;
// 不接 catch 就是点了没反应,所以每条请求都要留一句可见的话。
"function dead(){q('msg').textContent="
"'请求失败:设备可能已关闭配置服务,请重连热点后刷新页面'}"
"function post(u,p){var c=q('code');p.append('code',c?c.value.trim():'');"
"q('msg').textContent='正在提交…';var T=0;"
"function drop(){if(!T)return;clearTimeout(T);T=0}"
"function slow(){if(!T)return;T=0;q('msg').textContent="
"'设备 20 秒内没应答。刷新页面看是否已保存,先别重复提交'}"
"T=setTimeout(slow,20000);"
"return fetch(u,{method:'POST',body:new URLSearchParams(p)})"
".then(function(r){return r.json()}).then(function(j){"
"drop();if(j.error){q('msg').textContent=j.error;return}"
"q('msg').textContent=j.msg?j.msg:'设备已受理';refresh()})"
".catch(function(){drop();dead()})}"
// 只送改动过的字段:回填的原值再送回去会被当成一次真实变更。这段 JS 两页共用,
// 凭据与轮询段只在局域网页渲染,热点页没有那些元素,取值前先判空 —— 不判就在
// 收集字段时出错,保存按钮装死(np-0.2.4 实测:"保存并联网"点了没反应)。
"function go(fin){var p=new URLSearchParams();"
"var ss=q('ssid').value.trim(),pk=q('psk').value;"
"if(ss&&ss!==P0.ssid)p.append('ssid',ss);if(pk)p.append('psk',pk);"
"if(q('token')&&(q('token').value||q('oid').value||q('osec').value)){"
"p.append('auth',q('atype').value);p.append('a',q('token').value.trim());"
"p.append('b',q('osec').value.trim());p.append('oid',q('oid').value.trim());"
"p.append('tailnet',q('tailnet').value.trim())}"
"if(q('base')&&q('base').value!=P0.base)p.append('base',q('base').value);"
"if(q('cap')&&q('cap').value!=P0.cap)p.append('cap',q('cap').value);"
"if(q('tz')&&q('tz').value!=P0.tz)p.append('tz',q('tz').value);"
"p.append('finish',fin?'1':'0');post('/api/apply',p)}"
"function sync(){post('/api/sync',new URLSearchParams())}"
"function clr(){post('/api/clear',new URLSearchParams())}"
"function scan(){q('wl').innerHTML='<option value=\"\">扫描中…</option>';"
"q('wst').textContent='正在搜索附近网络,约 2 秒';"
"fetch('/api/scan').then(function(r){return r.json()}).then(function(j){"
"if(j.error){q('wst').textContent=j.error;q('ssid').focus();return}"
"var a=j.aps||[],sv=j.saved||'',o=['<option value=\"\">— 手动输入 —</option>'];"
"for(var i=0;i<a.length;i++){o.push('<option value=\"'+esc(a[i].ssid)+'\"'"
"+(a[i].ssid===sv?' selected':'')+'>'+esc(a[i].ssid)+' '+a[i].rssi+'dBm'"
"+(a[i].open?' 开放':'')+'</option>')}"
"q('wl').innerHTML=o.join('');"
"q('wst').textContent='找到 '+a.length+' 个网络'"
"+(sv?',已选中当前配置':'');"
"if(sv)q('ssid').value=sv;q('psk').focus()})"
".catch(function(){q('wst').textContent='扫描请求失败,可手动输入网络名'})}"
"function refresh(){fetch('/api/status').then(function(r){return r.json()})"
".then(function(j){var t=[];t.push('当前:'+esc(j.mode));"
"t.push('设备:'+esc(j.info));t.push('Wi-Fi:'+(j.wifi?'已配置 '+esc(j.ssid):'未配置'));"
"t.push('Tailscale:'+(j.has_ts?'已配置('+esc(j.auth)+' '+esc(j.token_mask)+')':'未配置'));"
"if(j.tailnet)t.push('Tailnet:'+esc(j.tailnet));if(j.poll)t.push('轮询:'+j.poll);"
"if(FIRST){FIRST=0;P0.base=j.poll_base;P0.cap=j.poll_cap;P0.tz=j.tz;P0.ssid=j.ssid||'';"
"if(q('base')){q('base').value=P0.base;q('cap').value=P0.cap;q('tz').value=P0.tz}"
"if(P0.ssid&&!q('ssid').value)q('ssid').value=P0.ssid}"
"q('st').textContent=t.join('\\n')})"
".catch(function(){q('st').textContent='设备已断开或配置会话已结束'})}"
"refresh();";

// 扫描只由页面上的「重新扫描」按钮发起,np-0.2.2 起引导热点也一样:自动扫描
// 会在手机正在关联、首屏正在加载时把射频借走约两秒,"连不上"和"页面打不开"
// 两个实测症状都出在这里。收尾一段负责定时器和 </script>,顺序在 render_html 里拼。
static const char HTML_SCRIPT_TAIL[] = "setInterval(refresh,5000)</script>";

// 配对码只渲染在 LAN 页:凭据录入阶段的写闸门(np-0.2.3 起热点页不再有此栏)。
static const char HTML_CODE_SECTION[] =
"<section><b>配对</b><label>屏幕上的 6 位配对码<input id=code maxlength=6></label></section>";

// ---------------------------------------------------------------- 小工具
static bool html_put(char **cur, char *end, const char *text)
{
    const size_t n = strlen(text);
    if (*cur + n >= end) return false;
    memcpy(*cur, text, n);
    *cur += n;
    **cur = '\0';
    return true;
}

static void copy_text(char *dst, size_t cap, const char *src)
{
    if (dst == NULL || cap == 0u) return;
    size_t i = 0;
    for (; src && src[i] && i + 1u < cap; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static esp_err_t ok_json(httpd_req_t *r, const char *body_json)
{
    httpd_resp_set_type(r, "application/json");
    return httpd_resp_sendstr(r, body_json);
}

// 网页上点了收尾类的按钮:只留一个标志,关闭时刻由 np_app 在它的下一拍折算。
static void schedule_close(void)
{
    s_close_requested = true;
}

// 每个请求记一笔:填表途中不该因为"会话启动了 10 分钟"而掉线。
// 跨任务只传 32 位序号,RV32 上 int64 读写不是原子的,时钟读数留在 np_app 那一拍。
static void session_touch(void)
{
    s_req_seq++;
}

// 读完整 body 一次,再由调用方对多个字段反复解析。
static bool read_body(httpd_req_t *r, char *buf, size_t cap)
{
    const int len = httpd_req_recv(r, buf, (int)cap - 1);
    if (len <= 0) return false;
    buf[len] = '\0';
    return true;
}

// 写接口鉴权(对已读入的 body 取 code 字段)。配对码只守凭据录入阶段:设备
// 已连上 Wi-Fi、网页经局域网访问时,改配置必须输入设备屏上的配对码 —— 能改
// 配置的人,必须看得到设备屏幕。热点阶段不设码:那是有人盯着设备的引导流程,
// 接入与填 Wi-Fi 都自由(np-0.2.3 撤回 np-0.2.2 扩到热点的闸门)。
static bool pairing_ok(const char *body)
{
    if (s_mode != NP_PROV_LAN) return true;
    char code[8] = "";
    if (!np_form_get(body, "code", code, sizeof(code))) return false;
    return np_pairing_code_valid(code) && strcmp(code, s_pairing) == 0;
}

// ---------------------------------------------------------------- 路由
static esp_err_t h_page(httpd_req_t *r)
{
    session_touch();
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_sendstr(r, s_html);
    return ESP_OK;
}

#define STATUS_CAP 768
#define STATUS_TOO_LONG "{\"error\":\"状态内容过长\"}"

// 状态里唯一不回显的是凭据本体。SSID 与 Tailnet 名称按原样返回:它们本来就显示在
// 设备屏幕上,页面还要靠原值判断"用户有没有改动"。
static esp_err_t h_status(httpd_req_t *r)
{
    session_touch();
    char out[STATUS_CAP];
    size_t u = 0;
    char mask[16];
    const bool softap = s_mode == NP_PROV_SOFTAP;

    if (!np_json_lit(out, sizeof(out), &u, "{\"mode\":\"") ||
        !np_json_str(out, sizeof(out), &u, softap ? "引导(热点)" : "修改(局域网)") ||
        !np_json_lit(out, sizeof(out), &u, "\",\"info\":\"") ||
        !np_json_str(out, sizeof(out), &u, softap
            ? "页面没自动弹出时,在浏览器地址栏输入设备屏幕上的热点网关地址"
            : "http://netpanel.local 配对码见设备屏幕") ||
        !np_json_lit(out, sizeof(out), &u, "\",\"wifi\":") ||
        !np_json_lit(out, sizeof(out), &u, app_cfg_has_wifi() ? "true" : "false") ||
        !np_json_lit(out, sizeof(out), &u, ",\"ssid\":\"") ||
        !np_json_str(out, sizeof(out), &u, app_cfg_ssid()) ||
        !np_json_lit(out, sizeof(out), &u, "\",\"has_ts\":") ||
        !np_json_lit(out, sizeof(out), &u, app_cfg_has_tailscale() ? "true" : "false") ||
        !np_json_lit(out, sizeof(out), &u, ",\"auth\":") ||
        !np_json_lit(out, sizeof(out), &u,
                     app_cfg_auth_type() == NP_AUTH_OAUTH ? "\"OAuth\"" : "\"令牌\"") ||
        !np_json_lit(out, sizeof(out), &u, ",\"token_mask\":\"")) {
        return ok_json(r, STATUS_TOO_LONG);
    }

    app_cfg_mask(app_cfg_auth_type() == NP_AUTH_OAUTH ? app_cfg_oauth_id()
                                                      : app_cfg_access_token(),
                 mask, sizeof(mask));
    // 表单与状态都用小时,内部仍按秒存(此前回秒会让输入框显示 28800)。
    if (!np_json_str(out, sizeof(out), &u, mask) ||
        !np_json_lit(out, sizeof(out), &u, "\",\"tailnet\":\"") ||
        !np_json_str(out, sizeof(out), &u,
                     app_cfg_tailnet()[0] ? app_cfg_tailnet() : "-(默认)") ||
        !np_json_lit(out, sizeof(out), &u, "\",\"poll\":") ||
        !np_json_int(out, sizeof(out), &u, (long long)app_cfg_poll_base_s()) ||
        !np_json_lit(out, sizeof(out), &u, ",\"poll_base\":") ||
        !np_json_int(out, sizeof(out), &u, (long long)app_cfg_poll_base_s()) ||
        !np_json_lit(out, sizeof(out), &u, ",\"poll_cap\":") ||
        !np_json_int(out, sizeof(out), &u, (long long)app_cfg_poll_cap_s()) ||
        !np_json_lit(out, sizeof(out), &u, ",\"tz\":") ||
        !np_json_int(out, sizeof(out), &u,
                     (long long)(app_cfg_tz_offset_s() / 3600)) ||
        !np_json_lit(out, sizeof(out), &u, "}")) {
        return ok_json(r, STATUS_TOO_LONG);
    }
    return ok_json(r, out);
}

// 台架读数口:第二步到底把 LVGL 池、Wi-Fi 缓冲调到哪一档,得看实机高水位
// (np_mem.h 里那个 65536 是被一个失效的 Kconfig 键骗出来的教训,不能再靠猜)。
// 纯读、不调 session_touch —— 拿 curl 连盯几个小时也不该把配网会话吊着不散,
// 会话期里 /api/apply 可是开着的。内容只有字节数,没有任何凭据。
#define MEM_CAP 384

static esp_err_t h_mem(httpd_req_t *r)
{
    np_mem_snap_t s;
    np_mem_last(&s);   // 应用层每 20 秒采一次;这里最坏滞后一个采样周期
    char out[MEM_CAP];
    size_t u = 0;
    if (!np_json_lit(out, sizeof(out), &u, "{\"free_heap\":") ||
        !np_json_int(out, sizeof(out), &u, s.free_heap) ||
        !np_json_lit(out, sizeof(out), &u, ",\"largest_blk\":") ||
        !np_json_int(out, sizeof(out), &u, s.largest_blk) ||
        !np_json_lit(out, sizeof(out), &u, ",\"dma_free\":") ||
        !np_json_int(out, sizeof(out), &u, s.dma_free) ||
        !np_json_lit(out, sizeof(out), &u, ",\"dma_blk\":") ||
        !np_json_int(out, sizeof(out), &u, s.dma_blk) ||
        !np_json_lit(out, sizeof(out), &u, ",\"lv_measured\":") ||
        !np_json_lit(out, sizeof(out), &u, s.lv_total != 0u ? "true" : "false") ||
        !np_json_lit(out, sizeof(out), &u, ",\"lv_total\":") ||
        !np_json_int(out, sizeof(out), &u, (long long)s.lv_total) ||
        !np_json_lit(out, sizeof(out), &u, ",\"lv_free\":") ||
        !np_json_int(out, sizeof(out), &u, (long long)s.lv_free) ||
        !np_json_lit(out, sizeof(out), &u, ",\"lv_biggest\":") ||
        !np_json_int(out, sizeof(out), &u, (long long)s.lv_biggest) ||
        !np_json_lit(out, sizeof(out), &u, ",\"lv_max_used\":") ||
        !np_json_int(out, sizeof(out), &u, (long long)s.lv_max_used) ||
        !np_json_lit(out, sizeof(out), &u, ",\"lv_used_pct\":") ||
        !np_json_int(out, sizeof(out), &u, (long long)s.lv_used_pct) ||
        !np_json_lit(out, sizeof(out), &u, ",\"lv_frag_pct\":") ||
        !np_json_int(out, sizeof(out), &u, (long long)s.lv_frag_pct) ||
        !np_json_lit(out, sizeof(out), &u, "}")) {
        return ok_json(r, "{\"error\":\"内存读数过长\"}");
    }
    return ok_json(r, out);
}

static esp_err_t h_scan(httpd_req_t *r)
{
    session_touch();
    np_scan_ap_t aps[NP_SCAN_KEEP];
    size_t n = 0;
    if (app_wifi_scan(s_mode == NP_PROV_SOFTAP ? s_ap_ssid : NULL,
                      aps, NP_SCAN_KEEP, &n) != ESP_OK) {
        return ok_json(r, "{\"error\":\"扫描失败,请直接手动输入网络名\"}");
    }
    if (!np_scanlist_json(aps, n, app_cfg_ssid(), s_scan_json, sizeof(s_scan_json))) {
        return ok_json(r, "{\"error\":\"扫描结果过长,请手动输入网络名\"}");
    }
    return ok_json(r, s_scan_json);
}

// 唯一的写接口:一次收齐所有变更,再由 finish 决定是否结束会话。
// 空字段一律视为"不修改",同一张页面才能既管首次引导又管日常修改。
static esp_err_t h_apply(httpd_req_t *r)
{
    session_touch();
    char body[BODY_CAP];
    if (!read_body(r, body, sizeof(body))) return ok_json(r, "{\"error\":\"空请求\"}");
    if (!pairing_ok(body)) return ok_json(r, "{\"error\":\"配对码缺失或错误\"}");

    char ssid[40] = "", psk[72] = "", auth[8] = "", token[256] = "",
         osec[256] = "", oid[128] = "", tailnet[64] = "", fin[8] = "";
    np_form_get(body, "ssid", ssid, sizeof(ssid));
    np_form_get(body, "psk", psk, sizeof(psk));
    np_form_get(body, "auth", auth, sizeof(auth));
    np_form_get(body, "a", token, sizeof(token));   // 令牌槽位
    np_form_get(body, "b", osec, sizeof(osec));     // OAuth secret 槽位
    np_form_get(body, "oid", oid, sizeof(oid));
    np_form_get(body, "tailnet", tailnet, sizeof(tailnet));
    np_form_get(body, "finish", fin, sizeof(fin));

    bool wifi_changed = false, creds_changed = false, poll_changed = false;
    if (ssid[0] || psk[0]) {
        if (ssid[0] && !app_cfg_set_ssid(ssid)) {
            return ok_json(r, "{\"error\":\"网络名不合法\"}");
        }
        if (psk[0] && !app_cfg_set_psk(psk)) {
            return ok_json(r, "{\"error\":\"密码长度不合法(8-63)\"}");
        }
        wifi_changed = true;
    }
    if (token[0] || oid[0] || osec[0]) {
        const np_auth_type_t type = (uint8_t)atoi(auth) ? NP_AUTH_OAUTH
                                                         : NP_AUTH_ACCESS_TOKEN;
        const bool ok = (type == NP_AUTH_ACCESS_TOKEN)
                            ? app_cfg_set_auth(type, token, NULL)
                            : app_cfg_set_auth(type, oid, osec);
        if (!ok) return ok_json(r, "{\"error\":\"凭据不完整或超长\"}");
        creds_changed = true;
    }
    if (tailnet[0]) {
        if (!app_cfg_set_tailnet(tailnet)) {
            return ok_json(r, "{\"error\":\"Tailnet 名称不合法\"}");
        }
        creds_changed = true;
    }

    char base_s[16] = "", cap_s[16] = "", tz_s[16] = "";
    const bool has_base = np_form_get(body, "base", base_s, sizeof(base_s));
    const bool has_cap = np_form_get(body, "cap", cap_s, sizeof(cap_s));
    const bool has_tz = np_form_get(body, "tz", tz_s, sizeof(tz_s));
    if (has_base || has_cap) {
        const long b = has_base ? atol(base_s) : (long)app_cfg_poll_base_s();
        const long c = has_cap ? atol(cap_s) : (long)app_cfg_poll_cap_s();
        if (b < 30 || b > 86400 || c < b || c > 86400) {
            return ok_json(r, "{\"error\":\"间隔应在 30 到 86400 秒,上限不小于基准\"}");
        }
        app_cfg_set_poll((uint32_t)b, (uint32_t)c);
        poll_changed = true;
    }
    if (has_tz) {
        const long tz = atol(tz_s);
        if (tz < -14 || tz > 14) {
            return ok_json(r, "{\"error\":\"时区偏移应在 -14 到 14 小时\"}");
        }
        app_cfg_set_tz_offset((int32_t)tz * 3600);
        poll_changed = true;
    }

    if (!wifi_changed && !creds_changed && !poll_changed) {
        return ok_json(r, "{\"error\":\"没有可保存的内容\"}");
    }
    if (app_cfg_save() != ESP_OK) return ok_json(r, "{\"error\":\"保存失败\"}");
    if (wifi_changed) {
        s_wifi_applied = true;
        // Wi-Fi 落盘成功:引导热点的使命完成,下次重开换一个新名字。
        s_ap_cred_stale = true;
    }
    if (creds_changed || poll_changed) s_ts_applied = true;

    if (atoi(fin)) {
        if (!app_cfg_has_wifi()) {
            return ok_json(r, "{\"error\":\"请先选择网络并填写密码\"}");
        }
        schedule_close();
        return ok_json(r, app_cfg_has_tailscale()
                  ? "{\"ok\":true,\"msg\":\"已保存,设备开始联网,手机可退回原 Wi-Fi\"}"
                  : "{\"ok\":true,\"msg\":\"Wi-Fi 已保存,设备开始联网;凭据可稍后补\"}");
    }
    // 局域网模式改完 Tailscale 凭据就收尾:设备随即自己起同步,屏幕给出结果。
    // 不关会话的话,页面停在"已保存"而用户无从判断设备到底连上不连上。
    if (creds_changed) {
        schedule_close();
        return ok_json(r,
                       "{\"ok\":true,\"msg\":\"凭据已保存,设备开始同步,页面可以关掉了\"}");
    }
    return ok_json(r, "{\"ok\":true,\"msg\":\"已保存\"}");
}

static esp_err_t h_sync(httpd_req_t *r)
{
    session_touch();
    char body[BODY_CAP];
    if (!read_body(r, body, sizeof(body))) return ok_json(r, "{\"error\":\"空请求\"}");
    if (!pairing_ok(body)) return ok_json(r, "{\"error\":\"配对码缺失或错误\"}");
    s_sync_request = true;
    return ok_json(r, "{\"ok\":true}");
}

static esp_err_t h_clear(httpd_req_t *r)
{
    session_touch();
    char body[BODY_CAP];
    if (!read_body(r, body, sizeof(body))) return ok_json(r, "{\"error\":\"空请求\"}");
    if (!pairing_ok(body)) return ok_json(r, "{\"error\":\"配对码缺失或错误\"}");
    app_cfg_clear_tailscale();
    if (app_cfg_save() != ESP_OK) return ok_json(r, "{\"error\":\"保存失败\"}");
    s_ts_applied = true;
    return ok_json(r, "{\"ok\":true}");
}

// SoftAP 模式的门户探测:任何未知 GET 都回到根路径,根路径本身就是配置页。
static esp_err_t h_any(httpd_req_t *r)
{
    session_touch();
    if (strcmp(r->uri, "/favicon.ico") == 0) {
        httpd_resp_set_status(r, "404 Not Found");
        httpd_resp_send(r, NULL, 0);
        return ESP_OK;
    }
    if (s_mode == NP_PROV_SOFTAP) {
        esp_netif_ip_info_t info = { 0 };
        if (s_ap_netif && esp_netif_get_ip_info(s_ap_netif, &info) == ESP_OK) {
            char loc[32];
            ip4_addr_t gw = { .addr = info.ip.addr };
            snprintf(loc, sizeof(loc), "http://" IPSTR "/", IP2STR(&gw));
            httpd_resp_set_status(r, "302 Found");
            httpd_resp_set_hdr(r, "Location", loc);
            httpd_resp_send(r, NULL, 0);
            return ESP_OK;
        }
    }
    httpd_resp_set_status(r, "404 Not Found");
    httpd_resp_sendstr(r, "open the device address in your browser");
    return ESP_OK;
}

// ---------------------------------------------------------------- 看门狗
// 手机侧"连不上热点"要能分清是没关联、关联后掉线、还是会话已经关掉,所以逐笔记账。
static esp_event_handler_instance_t s_ap_h[2];

static void on_ap_sta(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    const uint8_t *mac = ((wifi_event_ap_staconnected_t *)data)->mac;
    ESP_LOGI(TAG, "热点%s %02x:%02x:%02x:%02x:%02x:%02x",
             id == WIFI_EVENT_AP_STACONNECTED ? "接入" : "离开",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void ap_sta_watch(bool on)
{
    const int32_t ids[2] = { WIFI_EVENT_AP_STACONNECTED,
                             WIFI_EVENT_AP_STADISCONNECTED };
    for (int i = 0; i < 2; i++) {
        if (on) {
            esp_event_handler_instance_register(WIFI_EVENT, ids[i], on_ap_sta,
                                                NULL, &s_ap_h[i]);
        } else if (s_ap_h[i]) {
            esp_event_handler_instance_unregister(WIFI_EVENT, ids[i], s_ap_h[i]);
            s_ap_h[i] = NULL;
        }
    }
}

// ---------------------------------------------------------------- 生命周期
// 页面是启动时拼装一次的静态缓冲;拼不下就报启动失败,不能发半截 JS。
static bool render_html(void)
{
    char *cur = s_html;
    char *end = s_html + sizeof(s_html);
    const bool lan = s_mode == NP_PROV_LAN;
    const char *pieces[] = {
        HTML_HEAD,
        lan ? HTML_DEV_LAN : HTML_DEV_AP,
        "</p>",
        lan ? HTML_CODE_SECTION : "",
        HTML_STATUS,
        lan ? HTML_WIFI_LAN : HTML_WIFI_AP,
        lan ? HTML_ADV_LAN : "",
        lan ? HTML_CREDENTIALS : "",
        lan ? HTML_ACTIONS_LAN : HTML_ACTIONS_AP,
        HTML_SCRIPT,
        HTML_SCRIPT_TAIL,
    };
    for (size_t i = 0; i < sizeof(pieces) / sizeof(pieces[0]); i++) {
        if (!html_put(&cur, end, pieces[i])) return false;
    }
    ESP_LOGI(TAG, "配置页 %u 字节(缓冲 %u)", (unsigned)strlen(s_html),
             (unsigned)sizeof(s_html));
    return true;
}

static esp_err_t prov_httpd_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.task_priority = 6;
    // 扫描在 httpd 任务里同步执行,要留出原始记录数组与驱动调用的余量。
    cfg.stack_size = 8192;
    cfg.server_port = 80;
    cfg.max_uri_handlers = 9;
    cfg.max_open_sockets = 4;
    // 手机浏览器会攥着 keep-alive 连接,加上页面每 5 秒轮询,4 个槽位很容易被占满;
    // 不启用 LRU 回收时后续请求只能排队,表现就是"点保存没反应"。
    cfg.lru_purge_enable = true;
    // 通配匹配,让 "/*" 兜住门户探测打来的任意路径。
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    esp_err_t err = httpd_start(&s_hd, &cfg);
    if (err != ESP_OK) return err;

    const httpd_uri_t handlers[] = {
        { .uri = (char *)"/",           .method = HTTP_GET,  .handler = h_page },
        { .uri = (char *)"/prov",       .method = HTTP_GET,  .handler = h_page },
        { .uri = (char *)"/api/status", .method = HTTP_GET,  .handler = h_status },
        { .uri = (char *)"/api/scan",   .method = HTTP_GET,  .handler = h_scan },
        { .uri = (char *)"/api/mem",    .method = HTTP_GET,  .handler = h_mem },
        { .uri = (char *)"/api/apply",  .method = HTTP_POST, .handler = h_apply },
        { .uri = (char *)"/api/sync",   .method = HTTP_POST, .handler = h_sync },
        { .uri = (char *)"/api/clear",  .method = HTTP_POST, .handler = h_clear },
        { .uri = (char *)"/*",          .method = HTTP_GET,  .handler = h_any },
    };
    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
        err = httpd_register_uri_handler(s_hd, &handlers[i]);
        if (err != ESP_OK) {
            httpd_stop(s_hd);
            s_hd = NULL;
            return err;
        }
    }
    return ESP_OK;
}

static void teardown_softap(void)
{
    ap_sta_watch(false);
    np_dns_stop();
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
    if (s_sta_netif) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }
    s_ap_ssid[0] = '\0';
}

// 引导热点带一个 STA 接口:驱动只允许 STA 或 APSTA 模式扫描,纯 AP 会拒绝。
// STA 只挂网不连接,页面才可能列出附近网络。热点开放无密码(np-0.2.2):手机
// 按名字缓存的旧密码不再存在,"输对密码也连不上"不再发生;热点阶段的页面
// 写操作不设配对码,凭据录入的 LAN 页才由 pairing_ok 把关(np-0.2.3)。
static esp_err_t start_ap_with_scan(char *ssid_out, size_t cap)
{
    s_ap_netif = esp_netif_create_default_wifi_ap();
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_ap_netif || !s_sta_netif) return ESP_ERR_NO_MEM;

    const wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&wcfg);
    if (err != ESP_OK) return err;

    // 热点名只在"从没生成过"或"Wi-Fi 已保存成功"时重新随机。手机此刻连着的是
    // 旧名字,换个新随机值等于把它正在用的热点抽掉。
    if (!s_ap_cred_cached || s_ap_cred_stale) {
        const uint32_t rnd = esp_random();
        snprintf(s_ap_cred_ssid, sizeof(s_ap_cred_ssid), "Passport-%04X",
                 (unsigned)(rnd & 0xFFFFu));
        s_ap_cred_cached = true;
        s_ap_cred_stale = false;
        ESP_LOGI(TAG, "引导热点生成新名字");
    } else {
        ESP_LOGI(TAG, "引导热点沿用上次名字(Wi-Fi 尚未保存成功)");
    }
    copy_text(s_ap_ssid, sizeof(s_ap_ssid), s_ap_cred_ssid);

    wifi_config_t wc = { 0 };
    copy_text((char *)wc.ap.ssid, sizeof(wc.ap.ssid), s_ap_ssid);
    wc.ap.ssid_len = (uint8_t)strlen(s_ap_ssid);
    wc.ap.channel = 6;
    wc.ap.max_connection = 3;
    wc.ap.authmode = WIFI_AUTH_OPEN;

    err = esp_wifi_set_config(WIFI_IF_AP, &wc);
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) return err;

    copy_text(ssid_out, cap, s_ap_ssid);
    return ESP_OK;
}

esp_err_t app_prov_start(np_prov_mode_t mode, char *ap_ssid_out, size_t cap,
                         char *pairing_out, size_t pairing_cap)
{
    // 上一轮会话没拆干净就是真的编程错误:现在只有 np_app 一处开关会话,
    // 不再有一个还在睡的看门狗需要等。
    if (s_mode != NP_PROV_OFF) return ESP_ERR_INVALID_STATE;
    if (mode != NP_PROV_SOFTAP && mode != NP_PROV_LAN) return ESP_ERR_INVALID_ARG;

    s_wifi_applied = s_ts_applied = false;
    s_sync_request = false;
    s_close_requested = false;
    s_close_scheduled = false;
    s_req_seq = 0;
    s_seq_seen = 0;
    np_session_start(&s_session, esp_timer_get_time(), PROV_IDLE_US,
                     PROV_HARD_US);
    s_mode = mode;
    // 出参一律先清空再回填:调用方靠空串判断"没有拿到",不能残留上一轮的值。
    if (ap_ssid_out && cap) ap_ssid_out[0] = '\0';
    if (pairing_out && pairing_cap) pairing_out[0] = '\0';
    if (mode == NP_PROV_LAN) {
        // 配对码只在凭据录入阶段生成:热点页没有这一栏,也不校验它。
        const uint32_t rnd = esp_random() % 1000000u;
        snprintf(s_pairing, sizeof(s_pairing), "%06u", (unsigned)rnd);
        copy_text(pairing_out, pairing_cap, s_pairing);
    } else {
        // 防旧会话残留:热点阶段的 pairing_ok 不看它,留着没有意义。
        s_pairing[0] = '\0';
    }

    esp_err_t err = ESP_OK;
    if (mode == NP_PROV_SOFTAP) {
        // 让出射频:若 STA 栈还在,先彻底拆掉再按热点重建。
        esp_wifi_deinit();
        err = start_ap_with_scan(ap_ssid_out, cap);
        if (err != ESP_OK) goto out;
        ap_sta_watch(true);

        // 门户劫持起不来也只是退化成"手动输地址",不影响配置流程。
        if (!np_dns_start("WIFI_AP_DEF")) {
            ESP_LOGW(TAG, "门户 DNS 启动失败,需手动访问热点网关地址");
        }
    } else {
        err = mdns_init();
        if (err == ESP_OK) err = mdns_hostname_set("netpanel");
    }
    if (err != ESP_OK) goto out;

    if (!render_html()) { err = ESP_ERR_NO_MEM; goto out; }
    err = prov_httpd_start();
    if (err != ESP_OK) goto out;
    ESP_LOGI(TAG, "配置服务已启动 mode=%s", mode == NP_PROV_SOFTAP ? "softap" : "lan");
    return ESP_OK;

out:
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "配置服务启动失败: %s", esp_err_to_name(err));
        if (s_hd) { httpd_stop(s_hd); s_hd = NULL; }
        if (mode == NP_PROV_SOFTAP) teardown_softap();
        else mdns_service_remove_all();
        s_mode = NP_PROV_OFF;
    }
    return err;
}

void app_prov_stop(void)
{
    if (s_mode == NP_PROV_OFF) return;
    const np_prov_mode_t mode = s_mode;
    s_mode = NP_PROV_OFF;
    if (s_hd) {
        httpd_stop(s_hd);
        s_hd = NULL;
    }
    if (mode == NP_PROV_LAN) {
        mdns_service_remove_all();
        mdns_free();
    } else {
        teardown_softap();
    }
    // 纯拆除,不等任何任务:会话的开关现在只由 np_app 一处做,结构上就不会有
    // 另一个任务在睡醒之后把调用方刚打开的新会话拆掉。当年那两处等待,
    // 等的是这个已经不会发生的场景。
    s_sync_request = false;
    s_close_requested = false;
    s_close_scheduled = false;
    s_seq_seen = s_req_seq;
}

np_prov_mode_t app_prov_mode(void) { return s_mode; }

bool app_prov_wifi_applied(void) { return s_wifi_applied; }
bool app_prov_ts_applied(void) { return s_ts_applied; }

// 会话窗口由这里折算:请求过就把空闲计时往后推,空闲满或绝对上限到点即报该关。
// 只报不拆 —— 拆由调用方在它自己的上下文里做,免掉跨任务等待与嵌套持锁。
bool app_prov_take_close(int64_t now_us)
{
    if (s_mode == NP_PROV_OFF) return false;
    const uint32_t seq = s_req_seq;
    if (seq != s_seq_seen) {
        s_seq_seen = seq;
        np_session_touch(&s_session, now_us);
    }
    if (s_close_requested && !s_close_scheduled) {
        // 关闭时刻在这里盖,不在 httpd 那四个线程里盖:宽限从"我们看见请求"起算,
        // 而响应就在这一拍之后发出去,1.5 秒足够浏览器读完它。
        np_session_request_close(&s_session, now_us, PROV_CLOSE_GRACE_US);
        s_close_scheduled = true;
    }
    if (np_session_close_due(&s_session, now_us)) {
        ESP_LOGI(TAG, "配置服务收尾:页面上点过保存");
        return true;
    }
    if (np_session_expired(&s_session, now_us)) {
        ESP_LOGI(TAG, "配置服务收尾:空闲满 10 分钟或会话满 30 分钟");
        return true;
    }
    return false;
}

// 上层轮询"立即同步"按钮。
bool app_prov_take_sync_request(void)
{
    if (!s_sync_request) return false;
    s_sync_request = false;
    return true;
}
