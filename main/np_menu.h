// main/np_menu.h —— 状态页菜单的选项运算与页脚文案(纯逻辑,主机可测)。
// 契约:选中项只在 0..NP_MENU_COUNT-1 内夹紧、不环绕;动作由下标唯一决定,
// 屏幕上的文字顺序和这里的分发顺序必须是同一份契约(np-0.1.22 实测:第二拍
// 落到 0 号,于是"再按一次确定清除凭据"变成"立即刷新")。
#pragma once

#include <stddef.h>
#include <stdint.h>

#define NP_MENU_COUNT 4

typedef enum {
    NP_MENU_NONE = 0,
    NP_MENU_SYNC,     // 立即刷新
    NP_MENU_LAN,      // 局域网配置
    NP_MENU_AP,       // 引导热点配网
    NP_MENU_CLEAR,    // 清除凭据
} np_menu_action_t;

size_t np_menu_clamp(size_t cur);
size_t np_menu_move(size_t cur, int delta);   // 越界停住,不环绕
np_menu_action_t np_menu_activate(size_t cur);

// 列表页页脚:未显示的机器必须写在脸上,那一档放不下按键说明。
// total > shown 时只剩"还有 K 台未显示";否则给菜单入口,数据同步时刻
// (sync_hm 非空的 "HH:MM")一并写上 —— 在线是抓取那一刻的事实,没有时刻
// 就说不清这屏信息多新。sync_hm 为空退回纯提示。
void np_menu_footer(char *out, size_t cap, uint32_t online, uint32_t shown,
                    uint32_t total, const char *sync_hm);
