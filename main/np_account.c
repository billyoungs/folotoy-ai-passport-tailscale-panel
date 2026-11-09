// main/np_account.c —— 把"有没有凭据/有没有成功过/有没有被拒"三个布尔归成一句人话。
#include "np_account.h"
#include <stdio.h>

np_login_state_t np_login_classify(bool has_credentials, bool auth_rejected,
                                   bool sync_ok)
{
    if (!has_credentials) return NP_LOGIN_NONE;
    if (auth_rejected) return NP_LOGIN_REJECTED;
    return sync_ok ? NP_LOGIN_ON : NP_LOGIN_PENDING;
}

void np_login_text(np_login_state_t state, char *buf, size_t cap)
{
    if (buf == NULL || cap == 0u) return;
    const char *text = "登录:状态未知";
    switch (state) {
    case NP_LOGIN_NONE:     text = "登录:未配置凭据"; break;
    case NP_LOGIN_ON:       text = "登录:已登录"; break;
    case NP_LOGIN_REJECTED: text = "登录:被拒绝,需重新配置"; break;
    case NP_LOGIN_PENDING:  text = "登录:等待首次同步"; break;
    }
    snprintf(buf, cap, "%s", text);
}

void np_account_text(const char *tailnet, char *buf, size_t cap)
{
    if (buf == NULL || cap == 0u) return;
    if (tailnet == NULL || tailnet[0] == '\0') {
        snprintf(buf, cap, "账号:未指定(用密钥默认尾网)");
        return;
    }
    snprintf(buf, cap, "账号:%s", tailnet);
}
