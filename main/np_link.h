// main/np_link.h —— 断链自愈的节奏(纯逻辑,无平台依赖)。
// 为什么需要它:驱动只在收到断开事件的那一刻自己重连一次。若那次请求被还在
// 进行的扫描吞掉,或撞上"正在连接"的返回码,链路就停在断开态 —— 驱动不再发
// 第二个事件,上层也永远不会再试,np-0.1.17 的现场表现正是"必须重走热点配网"。
// 本模块只回答两件事:现在该不该补发、该补发到哪一档。真正的 esp_wifi 调用在
// app_wifi.c,时间由调用方注入(毫秒)以便主机测试。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define NP_LINK_FIRST_DELAY_MS 3000u  // 断开后先让驱动自己试一轮
#define NP_LINK_MAX_DELAY_MS 30000u   // 补发封顶间隔,别把射频变成锤子
#define NP_LINK_FORGET_AT 4u          // 补发这么多轮仍无 link:先清连接状态
#define NP_LINK_REBUILD_AT 8u         // 再多就整栈重建(事件位已不可信)

typedef enum {
    NP_LINK_ACT_NONE = 0,   // 还没到点,或链路不该由这里管
    NP_LINK_ACT_CONNECT,    // 补发一次连接请求
    NP_LINK_ACT_FORGET,     // 先断开清掉卡住的连接状态,紧接着再连
    NP_LINK_ACT_REBUILD,    // 停栈重启:句柄与事件组可能已经错位
} np_link_action_t;

typedef struct {
    int64_t next_try_ms;    // 下一次补发的时刻(0 = 未武装)
    uint32_t delay_ms;      // 当前档间隔
    uint32_t tries;         // 本轮断开已补发的次数
    bool armed;             // 仅断开期间为真
} np_link_t;

void np_link_init(np_link_t *l);

// 关联成功:阶梯归零,下次断开从第一档重新开始。
void np_link_up(np_link_t *l);

// 刚断开:武装第一次补发。重复调用(驱动连发多个断开事件)不重置已经走到的档位,
// 只在没武装时才起第一档。
void np_link_down(np_link_t *l, int64_t now_ms);

// 每个节拍问一次。到点则返回动作并自动顺延,调用方照做即可。
np_link_action_t np_link_tick(np_link_t *l, int64_t now_ms);

// 供上层显示/日志:当前第几档、间隔多少。
uint32_t np_link_delay_ms(const np_link_t *l);
uint32_t np_link_tries(const np_link_t *l);

// 距离下一次补发还有多少毫秒:没武装、或已经到了点都是 0。断链期间屏幕上要说得出
// "有人在管",不能只留一句"重连后自动继续"。
uint32_t np_link_wait_ms(const np_link_t *l, int64_t now_ms);
