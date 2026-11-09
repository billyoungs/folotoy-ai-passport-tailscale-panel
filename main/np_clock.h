// main/np_clock.h —— 校时门禁的节奏(纯逻辑,无平台依赖)。
// 为什么需要它:HTTPS 证书校验要知道"现在几点",而设备每次开机钟都停在 1970。
// 带着没校上的钟发请求,必然撞在"有效期还没开始"的证书上,那种失败不可能成功,
// 报出来只会把用户支去怀疑服务端的证书(np-0.1.22 实机反馈)。所以请求一律等钟
// 校上再发。但 lwIP 默认的 SNTP 同步间隔是十分钟:第一次交换被配置页扫描借走的
// 射频或被路由器拦掉的 UDP 123 弄丢一次,等待就会长到用户以为机器死了。本模块
// 只管"该不该重起一轮校时"和"等了多久该说实话",真正的 esp_sntp 调用与文案在
// netpanel_app.c,时间由调用方注入(uptime 毫秒)以便主机测试。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NP_CLOCK_RETRY_MS    30000LL  // 等待期间每隔这么久重起一轮 SNTP
#define NP_CLOCK_PATIENCE_MS 120000LL // 超过这么久还没回来,就把话说成 NTP 无响应

typedef enum {
    NP_CLOCK_GO = 0,     // 钟校上了,可以发请求
    NP_CLOCK_IDLE,       // 连链路都没有,校时无从谈起(由断网那句话负责)
    NP_CLOCK_WAIT,       // 继续等,不用做任何事
    NP_CLOCK_REARM,      // 该重起一轮 SNTP
    NP_CLOCK_STALLED,    // 耐心用完:NTP 一直没答(每轮等待只报一次)
} np_clock_action_t;

typedef struct {
    int64_t wait_start_ms;  // 本轮"链路已通、钟未校上"从什么时候开始算
    int64_t next_rearm_ms;  // 下一次重起 SNTP 的时刻
    uint32_t rearms;        // 本轮等待已重起过几轮(供日志)
    bool waiting;           // 正在等(用状态位而非 0 哨兵,开机第一拍也算数)
    bool patience_shown;    // 那句实话已经说过
} np_clock_t;

void np_clock_init(np_clock_t *c);

// 每个轮询节拍问一次。synced/link_up 由调用方当场测得,now_ms 是开机以来的毫秒。
// 校上或断网都会把本轮等待清零:断网期间的等待不作数,链路恢复后重新计时。
np_clock_action_t np_clock_tick(np_clock_t *c, bool link_up, bool synced,
                                int64_t now_ms);

bool np_clock_waiting(const np_clock_t *c);

// 那句实话说过之后一直保持为真,直到校上或断网重开一轮:上屏的话要说事实,
// 不能只在翻转那一拍对、之后又说不清到底还在不在等。
bool np_clock_stalled(const np_clock_t *c);
uint32_t np_clock_rearms(const np_clock_t *c);
