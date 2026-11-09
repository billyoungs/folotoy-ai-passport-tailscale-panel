// tests/test_np_model.c —— 列表排序/统计与轮询退避状态机测试。
#include <assert.h>
#include <string.h>
#include "np_model.h"

static void add(np_list_t *l, const char *name, bool online)
{
    np_device_t *d = &l->items[l->count++];
    memset(d, 0, sizeof(*d));
    strncpy(d->name, name, NP_NAME_LEN - 1u);
    d->online = online;
}

int main(void)
{
    np_list_t list;
    memset(&list, 0, sizeof(list));
    list.version = 3u;

    add(&list, "nas", true);
    add(&list, "alpha", false);
    add(&list, "laptop", true);
    np_list_sort(&list);
    // 在线组按名字序在前,离线随后
    assert(strcmp(list.items[0].name, "laptop") == 0);
    assert(strcmp(list.items[1].name, "nas") == 0);
    assert(strcmp(list.items[2].name, "alpha") == 0);
    assert(list.version == 3u); // 排序不改版本号
    assert(np_list_online_count(&list) == 2u);

    np_list_clear(&list);
    assert(list.count == 0u);

    // ---- 轮询控制器 ----
    np_poll_t p;
    np_poll_init(&p, 300u, 3600u);
    // 网络未就绪永不到点
    assert(!np_poll_due(&p, 1000));
    np_poll_set_net(&p, true);
    assert(np_poll_due(&p, 1000));       // 立即到点并顺延
    assert(!np_poll_due(&p, 1299));
    assert(np_poll_due(&p, 1300));

    // 失败指数退避: 300→600→1200,封顶 3600
    np_poll_on_fail(&p, 2000);
    assert(p.current_delay_s == 600u);
    np_poll_on_fail(&p, 2600);
    assert(p.current_delay_s == 1200u);
    for (int i = 0; i < 10; i++) np_poll_on_fail(&p, 0);
    assert(p.current_delay_s == 3600u);

    // 成功回到基准
    np_poll_on_success(&p, 10000);
    assert(p.current_delay_s == 300u);
    assert(p.next_run_s == 10300);

    // 断网恢复且处于退避: 立刻重试
    np_poll_on_fail(&p, 20000);
    np_poll_set_net(&p, false);
    assert(!np_poll_due(&p, 20001));
    np_poll_set_net(&p, true);
    assert(np_poll_due(&p, 20001));

    // ---- lastSeen 折算:在线节点有好几种长相,都不能判成离线 ----
    const int64_t now = 1762300000;
    // 字段缺失 / 解析失败 → 传 0 进来
    assert(np_seen_disconnect_at(now, 0) == 0);
    // Go 零值时间 0001-01-01T00:00:00Z 是能解析成功的,必须不当断线时刻
    assert(np_seen_disconnect_at(now, -62135596800LL) == 0);
    assert(np_seen_disconnect_at(now, 1) == 0);
    // 下界本身算有效时刻
    assert(np_seen_disconnect_at(now, NP_TIME_FLOOR_EPOCH) == NP_TIME_FLOOR_EPOCH);
    // 宽限期内(含比 now 还新的时钟偏差)都算还连着
    assert(np_seen_disconnect_at(now, now - NP_SEEN_GRACE_S) == 0);
    assert(np_seen_disconnect_at(now, now - 1) == 0);
    assert(np_seen_disconnect_at(now, now + 3600) == 0);
    // 出了宽限期才真的离线,原时刻留给"离线于"那一行
    assert(np_seen_disconnect_at(now, now - NP_SEEN_GRACE_S - 1) == now - NP_SEEN_GRACE_S - 1);
    assert(np_seen_disconnect_at(now, now - 20 * 3600) == now - 20 * 3600);
    // 时钟没同步时一个都不判
    assert(np_seen_disconnect_at(0, now - 7200) == 0);
    assert(np_seen_disconnect_at(-5, now - 7200) == 0);

    // ---- Wi-Fi 分档:0 段就是"未连网" ----
    assert(np_signal_bars(0) == 0);
    assert(np_signal_bars(-40) == 3);
    assert(np_signal_bars(-60) == 3);   // 边界归上一档
    assert(np_signal_bars(-61) == 2);
    assert(np_signal_bars(-75) == 2);
    assert(np_signal_bars(-76) == 1);
    assert(np_signal_bars(-110) == 1);

    // ---- 在线判定:控制面说了话就不必猜 ----
    // connectedToControl 在场时以它为准,哪怕 lastSeen 折算说"还连着"。
    assert(np_device_is_online(1, 0) == true);
    assert(np_device_is_online(1, 12345) == true);   // 折算出断线时刻也不算离线
    assert(np_device_is_online(0, 0) == false);      // 控制面说断了,没有时刻也算断了
    assert(np_device_is_online(0, 12345) == false);
    // 字段缺失(-1)才退回 lastSeen 折算
    assert(np_device_is_online(-1, 0) == true);
    assert(np_device_is_online(-1, 12345) == false);

    // ---- 系统图标:认得出的给标志,认不出的给通用机器,空的什么都不画 ----
    assert(np_os_glyph("linux") == NP_GLYPH_LINUX);
    assert(np_os_glyph("Linux") == NP_GLYPH_LINUX);      // 大小写不敏感
    assert(np_os_glyph("windows") == NP_GLYPH_WINDOWS);
    assert(np_os_glyph("darwin") == NP_GLYPH_APPLE);
    assert(np_os_glyph("macOS") == NP_GLYPH_APPLE);
    assert(np_os_glyph("ios") == NP_GLYPH_APPLE);
    assert(np_os_glyph("android") == NP_GLYPH_ANDROID);
    assert(np_os_glyph("synology") == NP_GLYPH_OTHER);
    assert(np_os_glyph("openwrt") == NP_GLYPH_OTHER);
    assert(np_os_glyph("") == 0u);
    assert(np_os_glyph(NULL) == 0u);

    // ---- 扩展信息缓冲:清空后时延必须是"没测到",不是 0ms ----
    np_detail_t d;
    d.ssh = true;
    d.endpoints = 9;
    np_detail_clear(&d);
    assert(d.derp_ms == -1 && d.n_en == 0u && d.n_adv == 0u && !d.ssh);
    assert(d.derp[0] == '\0' && d.distro[0] == '\0' && !d.have_conn);

    // 双缓冲:写 back、swap 后 new 变 active,old 降级为 back 供下次覆写。
    np_lists_init();
    np_list_t *buf_a = np_lists_back();
    buf_a->count = 3;
    buf_a->version = 42;
    np_lists_swap();
    assert(np_lists_active()->count == 3u && np_lists_active()->version == 42u);
    assert(np_lists_back() != np_lists_active());

    // ---- 快照新旧:断链即旧,合法退避不算旧 ----
    // 本轮开机还没同步成功过就没有"现在"的数据可言。
    assert(!np_snapshot_fresh(1000000, 0, true, 300u));
    // 链路一断,上一份 online 立刻只是过去,哪怕刚刚才抓的。
    assert(!np_snapshot_fresh(1000000, 999999, false, 300u));
    // 链路在:窗口 = NP_SNAPSHOT_GRACE_MS + 2×退避。基准 300 秒时窗口 660 秒。
    assert(np_snapshot_fresh(1000000 + NP_SNAPSHOT_GRACE_MS - 1, 1000000, true, 0u));
    assert(!np_snapshot_fresh(1000000 + NP_SNAPSHOT_GRACE_MS + 1, 1000000, true, 0u));
    assert(np_snapshot_fresh(1000000 + 660000, 1000000, true, 300u));
    assert(!np_snapshot_fresh(1000000 + 660001, 1000000, true, 300u));
    // 退避到上限(1 小时)也不该被判成过期,否则每次断网屏幕上都在撒谎。
    assert(np_snapshot_fresh(1000000 + 7260000, 1000000, true, 3600u));
    assert(!np_snapshot_fresh(1000000 + 7260001, 1000000, true, 3600u));
    return 0;
}
