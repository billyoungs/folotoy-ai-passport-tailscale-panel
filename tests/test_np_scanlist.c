// tests/test_np_scanlist.c —— 扫描列表去重排序与响应渲染测试。
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "np_scanlist.h"

static np_scan_ap_t ap(const char *ssid, int8_t rssi, bool open)
{
    np_scan_ap_t a;
    memset(&a, 0, sizeof(a));
    snprintf(a.ssid, sizeof(a.ssid), "%s", ssid);
    a.rssi = rssi;
    a.open = open;
    return a;
}

int main(void)
{
    np_scan_ap_t in[8], out[NP_SCAN_KEEP];
    size_t n;

    // 同名留最强、空名丢弃、按信号降序
    in[0] = ap("home", -50, false);
    in[1] = ap("cafe", -40, true);
    in[2] = ap("home", -70, false);
    in[3] = ap("", -20, true);
    n = np_scanlist_dedupe(in, 4, out, NP_SCAN_KEEP);
    assert(n == 2u);
    assert(strcmp(out[0].ssid, "cafe") == 0 && out[0].rssi == -40 && out[0].open);
    assert(strcmp(out[1].ssid, "home") == 0 && out[1].rssi == -50);

    // 后到的同名强信号会顶掉弱信号并重新排位
    in[0] = ap("weak", -80, false);
    in[1] = ap("mid", -60, false);
    in[2] = ap("weak", -30, false);
    n = np_scanlist_dedupe(in, 3, out, NP_SCAN_KEEP);
    assert(n == 2u);
    assert(strcmp(out[0].ssid, "weak") == 0 && out[0].rssi == -30);
    assert(strcmp(out[1].ssid, "mid") == 0);

    // 同信号按名称升序,输出顺序稳定
    in[0] = ap("mm", -40, false);
    in[1] = ap("aa", -40, false);
    in[2] = ap("bb", -40, false);
    n = np_scanlist_dedupe(in, 3, out, NP_SCAN_KEEP);
    assert(n == 3u);
    assert(strcmp(out[0].ssid, "aa") == 0);
    assert(strcmp(out[1].ssid, "bb") == 0);
    assert(strcmp(out[2].ssid, "mm") == 0);

    // 满了以后只换掉最弱的一条;更弱的进不来
    in[0] = ap("x", -10, false);
    in[1] = ap("y", -20, false);
    in[2] = ap("z", -5, false);
    in[3] = ap("w", -90, false);
    n = np_scanlist_dedupe(in, 4, out, 2);
    assert(n == 2u);
    assert(strcmp(out[0].ssid, "z") == 0);
    assert(strcmp(out[1].ssid, "x") == 0);

    // 非法参数
    assert(np_scanlist_dedupe(NULL, 1, out, 2) == 0u);
    assert(np_scanlist_dedupe(in, 1, NULL, 2) == 0u);
    assert(np_scanlist_dedupe(in, 1, out, 0) == 0u);

    // 渲染:顺序、字段与布尔值
    out[0] = ap("cafe", -40, true);
    out[1] = ap("home", -50, false);
    char json[256];
    assert(np_scanlist_json(out, 2, "home", json, sizeof(json)));
    assert(strcmp(json,
        "{\"aps\":[{\"ssid\":\"cafe\",\"rssi\":-40,\"open\":true},"
        "{\"ssid\":\"home\",\"rssi\":-50,\"open\":false}],\"saved\":\"home\"}") == 0);

    // 无已配置网络时 saved 是空串;空列表也能渲染
    assert(np_scanlist_json(out, 2, NULL, json, sizeof(json)));
    assert(strstr(json, "\"saved\":\"\"") != NULL);
    assert(np_scanlist_json(NULL, 0, "", json, sizeof(json)));
    assert(strcmp(json, "{\"aps\":[],\"saved\":\"\"}") == 0);

    // 名称里的引号必须转义,不能提前闭合 JSON
    out[0] = ap("a\"b", -40, false);
    assert(np_scanlist_json(out, 1, NULL, json, sizeof(json)));
    assert(strcmp(json, "{\"aps\":[{\"ssid\":\"a\\\"b\",\"rssi\":-40,"
                        "\"open\":false}],\"saved\":\"\"}") == 0);

    // 容量不足时整条判失败,不会发出半截 JSON
    char tiny[40];
    assert(!np_scanlist_json(out, 1, "home", tiny, sizeof(tiny)));
    assert(!np_scanlist_json(NULL, 0, "home", tiny, 4));

    return 0;
}
