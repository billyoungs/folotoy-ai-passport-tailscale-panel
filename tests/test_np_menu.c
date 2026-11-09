// tests/test_np_menu.c —— 状态页菜单的下标与页脚测试。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "np_menu.h"

// 截断只落在码点边界:半个 UTF-8 序列到屏幕上就是乱码方块。
static void no_half_glyph(const char *s)
{
    const size_t n = strlen(s);
    for (size_t i = 0; i < n; i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 0x80u) continue;
        size_t want = 0u;
        if (c >= 0xF0u) want = 4u;
        else if (c >= 0xE0u) want = 3u;
        else if (c >= 0xC0u) want = 2u;
        else { assert(want != 0u); }   // 不该有孤立的后续字节
        assert(i + want <= n);
        i += want - 1u;
    }
}

int main(void)
{
    // ---- 夹紧:菜单只有 4 项,越界的下标不能穿到数组外
    assert(np_menu_clamp(0u) == 0u);
    assert(np_menu_clamp(3u) == 3u);
    assert(np_menu_clamp(4u) == 3u);
    assert(np_menu_clamp(9999u) == 3u);

    // ---- 移动:到边界停住,不环绕(np-0.1.22 之前根本没有这一档运算)
    assert(np_menu_move(0u, -1) == 0u);
    assert(np_menu_move(0u, 1) == 1u);
    assert(np_menu_move(3u, 1) == 3u);
    assert(np_menu_move(3u, -1) == 2u);
    assert(np_menu_move(7u, 1) == 3u);      // 越界的旧选中项先夹紧再走
    assert(np_menu_move(7u, -1) == 2u);     // 负方向也从夹紧后的位置出发
    assert(np_menu_move(1u, 2) == 3u);
    assert(np_menu_move(3u, -3) == 0u);
    assert(np_menu_move(1u, 99) == 3u);
    assert(np_menu_move(1u, -99) == 0u);
    assert(np_menu_move(2u, 0) == 2u);

    // ---- 分发:下标到动作一一对应,和屏幕上 ACTS[] 的书写顺序是同一份契约
    assert(np_menu_activate(0u) == NP_MENU_SYNC);
    assert(np_menu_activate(1u) == NP_MENU_LAN);
    assert(np_menu_activate(2u) == NP_MENU_AP);
    assert(np_menu_activate(3u) == NP_MENU_CLEAR);
    assert(np_menu_activate(8u) == NP_MENU_CLEAR);   // 越界落到最后一项,不误触刷新

    // ---- 页脚:常规档给出菜单入口和同步时刻
    char foot[160];
    np_menu_footer(foot, sizeof(foot), 8u, 12u, 12u, "15:30");
    assert(strcmp(foot, "8/12 在线  长按菜单  同步 15:30") == 0);
    assert(strlen(foot) < sizeof(foot));
    no_half_glyph(foot);

    // 同步时刻还没来(本轮没同步成功/钟没校上):退回纯提示档
    np_menu_footer(foot, sizeof(foot), 8u, 12u, 12u, "");
    assert(strcmp(foot, "8/12 在线  确定详情 长按菜单") == 0);
    np_menu_footer(foot, sizeof(foot), 8u, 12u, 12u, NULL);
    assert(strcmp(foot, "8/12 在线  确定详情 长按菜单") == 0);

    // 溢出档:未显示的台数优先,这一行放不下按键说明和同步时刻
    np_menu_footer(foot, sizeof(foot), 8u, 24u, 30u, "15:30");
    assert(strcmp(foot, "8/24 在线  还有 6 台未显示") == 0);
    np_menu_footer(foot, sizeof(foot), 0u, 0u, 0u, "");
    assert(strcmp(foot, "0/0 在线  确定详情 长按菜单") == 0);

    // 大数也不能顶破缓冲区,且不能留下半个字符
    np_menu_footer(foot, sizeof(foot), 4294967295u, 4294967294u, 4294967295u,
                   "15:30");
    assert(strlen(foot) < sizeof(foot));
    no_half_glyph(foot);

    // 小缓冲:宁可整段收敛,也不能写出界、不能留半个码点
    char tiny[13];
    np_menu_footer(tiny, sizeof(tiny), 8u, 24u, 30u, "15:30");
    assert(strlen(tiny) < sizeof(tiny));
    no_half_glyph(tiny);
    np_menu_footer(tiny, sizeof(tiny), 8u, 12u, 12u, "15:30");
    assert(strlen(tiny) < sizeof(tiny));
    no_half_glyph(tiny);
    np_menu_footer(tiny, 1u, 8u, 24u, 30u, "15:30");
    assert(tiny[0] == '\0');
    np_menu_footer(NULL, 160u, 1u, 1u, 1u, "15:30");   // 空指针只是不动作

    printf("np_menu: ok\n");
    return 0;
}
