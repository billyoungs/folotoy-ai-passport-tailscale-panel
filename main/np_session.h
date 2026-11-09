// main/np_session.h —— 配置会话窗口的计时(纯逻辑,可主机测试)。
// 窗口由两条线界定:距最后一次请求的空闲时长,以及自会话启动起的绝对上限。
// 只走空闲会被浏览器每 5 秒的状态轮询无限续命,所以绝对上限必须保留。
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int64_t started_us;
    int64_t last_req_us;
    int64_t idle_us;
    int64_t hard_us;
    int64_t close_at_us;   // 0 = 没排关闭;非 0 = 到点就该收尾
} np_session_t;

// 绝对上限为 0 表示不设上限。空闲计时从 now_us 起算。
void np_session_start(np_session_t *s, int64_t now_us, int64_t idle_us,
                      int64_t hard_us);

// 记录一次请求。续命不会越过绝对上限,过期后的请求也不会把会话复活。
void np_session_touch(np_session_t *s, int64_t now_us);

bool np_session_expired(const np_session_t *s, int64_t now_us);

// 网页点了"保存并联网":给它一段宽限让浏览器读完响应,再排期关闭。已经排过就不改期,
// 否则浏览器每 5 秒的状态轮询会把关闭一直往后推,会话反而永不收尾。
void np_session_request_close(np_session_t *s, int64_t now_us, int64_t grace_us);

// 排到的关闭到点没有。过期判定独立于它:绝对上限到点照样立刻成立,不等宽限。
bool np_session_close_due(const np_session_t *s, int64_t now_us);
