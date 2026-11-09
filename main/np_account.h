// main/np_account.h —— 登录状态归类与状态页文案(纯逻辑,可主机测试)。
#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    NP_LOGIN_NONE = 0,   // 没填凭据,谈不上登录
    NP_LOGIN_PENDING,    // 填了凭据,但本次开机还没同步成功过
    NP_LOGIN_ON,         // 已登录:最近一次同步成功
    NP_LOGIN_REJECTED    // 服务端拒了凭据,要重新配置
} np_login_state_t;

// 归类顺序:先问有没有凭据,再问是不是被拒(被拒时哪怕先前成功过也不许报
// "已登录"),最后才看本次开机有没有成功同步过。
np_login_state_t np_login_classify(bool has_credentials, bool auth_rejected,
                                   bool sync_ok);

// 输出"登录:…"整行(含前缀),buf 始终 NUL 结尾。
void np_login_text(np_login_state_t state, char *buf, size_t cap);

// 输出"账号:…"整行。tailnet 为空表示请求走的是那个 "-" 默认尾网(密钥自己
// 所属的尾网),屏幕上不能只留一个连字符,给一句说明。
void np_account_text(const char *tailnet, char *buf, size_t cap);
