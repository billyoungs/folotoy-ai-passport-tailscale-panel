// tests/test_np_wifi_store.c —— 多网络记忆:插入/置顶/淘汰/选网。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "np_wifi_store.h"

static const char *ssid_at(const np_wifi_store_t *st, size_t i)
{
    return st->slot[i].ssid;
}

static bool has_ssid(const np_wifi_store_t *st, const char *ssid)
{
    for (size_t i = 0; i < st->count; i++) {
        if (strcmp(st->slot[i].ssid, ssid) == 0) return true;
    }
    return false;
}

int main(void)
{
    np_wifi_store_t st;
    np_wifi_store_init(&st);
    assert(st.count == 0u);

    // ---- 插入顺序即 MRU 顺序,最新在 0 号
    np_wifi_store_upsert(&st, "home", "aaaa");
    np_wifi_store_upsert(&st, "office", "bbbb");
    np_wifi_store_upsert(&st, "lab", "");
    assert(st.count == 3u);
    assert(strcmp(ssid_at(&st, 0), "lab") == 0);
    assert(st.slot[0].psk[0] == '\0');
    assert(strcmp(ssid_at(&st, 2), "home") == 0);

    // ---- 重复配置同一网络:更新密码并置顶,不新增槽位
    np_wifi_store_upsert(&st, "home", "new-psk");
    assert(st.count == 3u);
    assert(strcmp(ssid_at(&st, 0), "home") == 0);
    assert(strcmp(st.slot[0].psk, "new-psk") == 0);
    assert(strcmp(ssid_at(&st, 2), "office") == 0);

    // ---- 空名不入库;psk 空指针按开放网络处理
    np_wifi_store_upsert(&st, "", "x");
    assert(st.count == 3u);
    np_wifi_store_upsert(&st, "open", NULL);
    assert(st.count == 4u);
    assert(strcmp(ssid_at(&st, 0), "open") == 0);
    assert(st.slot[0].psk[0] == '\0');

    // ---- 满员后新网络挤掉最末尾(最久没用)的
    // 现存顺序:open a home lab office → 再加 a、b、c:
    np_wifi_store_upsert(&st, "a", "1");   // a open home lab office
    np_wifi_store_upsert(&st, "b", "2");   // b a open home lab office(满)
    np_wifi_store_upsert(&st, "c", "3");   // 挤掉 office → c b a open home lab
    assert(st.count == NP_WIFI_MAX);
    assert(strcmp(ssid_at(&st, 0), "c") == 0);
    assert(!has_ssid(&st, "office"));
    np_wifi_store_upsert(&st, "d", "4");   // 挤掉 lab → d c b a open home
    assert(st.count == NP_WIFI_MAX);
    assert(strcmp(ssid_at(&st, 0), "d") == 0);
    assert(!has_ssid(&st, "lab"));
    // home 因中途被重新配置过而存活:淘汰看的确实是"最近使用"。
    assert(has_ssid(&st, "home"));

    // ---- select:取出并把命中的置顶
    assert(np_wifi_store_select(&st, 4u));           // open → 0 号
    assert(strcmp(ssid_at(&st, 0), "open") == 0);
    assert(strcmp(ssid_at(&st, 1), "d") == 0);
    assert(np_wifi_store_select(&st, 0u));           // 已在顶上,原地不动
    assert(strcmp(ssid_at(&st, 0), "open") == 0);
    assert(!np_wifi_store_select(&st, NP_WIFI_MAX)); // 越界
    assert(!np_wifi_store_select(&st, 999u));

    np_wifi_store_t empty;
    np_wifi_store_init(&empty);
    assert(!np_wifi_store_select(&empty, 0u));       // 空库无可选
    assert(np_wifi_store_pick(&empty, NULL, 0u) == -1);

    // ---- pick:最强信号优先(扫描结果已按 rssi 降序),不是先扫到谁算谁
    // 此刻顺序:open d c b a home(select 置顶后的顺序)
    const np_scan_ap_t strong_old[] = {
        {"home", -40, false}, {"d", -50, false},
    };
    // home 信号最强但排在记忆末位:按信号选它,而不是按槽位选 d。
    assert(np_wifi_store_pick(&st, strong_old, 2u) == 5);
    const np_scan_ap_t mixed[] = {
        {"cafe", -40, true}, {"b", -50, false}, {"nowhere", -60, false},
    };
    assert(np_wifi_store_pick(&st, mixed, 3u) == 3); // b 在记忆里排 3
    const np_scan_ap_t none[] = { {"cafe", -40, true} };
    assert(np_wifi_store_pick(&st, none, 1u) == -1); // 附近一个都没有
    assert(np_wifi_store_pick(&st, none, 0u) == -1);
    assert(np_wifi_store_pick(&st, NULL, 1u) == -1);
    assert(np_wifi_store_pick(NULL, none, 1u) == -1);

    // ---- 超长输入截断而不是越界(上游 set_ssid 会拦,这里保自身健壮)
    np_wifi_store_t tr;
    np_wifi_store_init(&tr);
    char long_ssid[NP_SCAN_SSID_MAX + 8];
    for (size_t i = 0; i < sizeof(long_ssid) - 1u; i++) long_ssid[i] = 'x';
    long_ssid[sizeof(long_ssid) - 1u] = '\0';
    np_wifi_store_upsert(&tr, long_ssid, "psk");
    assert(tr.count == 1u);
    assert(strlen(tr.slot[0].ssid) == NP_SCAN_SSID_MAX);
    assert(tr.slot[0].ssid[NP_SCAN_SSID_MAX] == '\0');

    printf("np_wifi_store: ok\n");
    return 0;
}
