// main/np_menu.c —— 菜单下标夹紧与页脚组装。
#include "np_menu.h"

#include <stdio.h>
#include <string.h>

size_t np_menu_clamp(size_t cur)
{
    return cur < NP_MENU_COUNT ? cur : NP_MENU_COUNT - 1u;
}

size_t np_menu_move(size_t cur, int delta)
{
    const size_t from = np_menu_clamp(cur);
    if (delta > 0) {
        const size_t to = from + (size_t)delta;
        return to < NP_MENU_COUNT ? to : NP_MENU_COUNT - 1u;
    }
    const size_t back = (size_t)(-delta);
    return back > from ? 0u : from - back;
}

np_menu_action_t np_menu_activate(size_t cur)
{
    switch (np_menu_clamp(cur)) {
    case 0u: return NP_MENU_SYNC;
    case 1u: return NP_MENU_LAN;
    case 2u: return NP_MENU_AP;
    default: return NP_MENU_CLEAR;
    }
}

void np_menu_footer(char *out, size_t cap, uint32_t online, uint32_t shown,
                    uint32_t total, const char *sync_hm)
{
    if (out == NULL || cap == 0u) return;
    out[0] = '\0';
    if (total > shown) {
        snprintf(out, cap, "%u/%u 在线  还有 %u 台未显示", (unsigned)online,
                 (unsigned)shown, (unsigned)(total - shown));
    } else if (sync_hm != NULL && sync_hm[0] != '\0') {
        snprintf(out, cap, "%u/%u 在线  长按菜单  同步 %s", (unsigned)online,
                 (unsigned)shown, sync_hm);
    } else {
        snprintf(out, cap, "%u/%u 在线  确定详情 长按菜单", (unsigned)online,
                 (unsigned)shown);
    }
    // snprintf 可能正好切在一个多字节序列中间,退到码点边界:宁可少一个字,
    // 不给 LVGL 留半个字符(渲染成乱码方块)。
    size_t i = strlen(out);
    while (i > 0u && ((unsigned char)out[i - 1u] & 0xC0u) == 0x80u) i--;
    if (i > 0u) {
        const unsigned char head = (unsigned char)out[i - 1u];
        const size_t want = head >= 0xF0u ? 4u : head >= 0xE0u ? 3u : 2u;
        if (head >= 0xC0u && strlen(out) - i + 1u < want) out[i - 1u] = '\0';
    }
}
