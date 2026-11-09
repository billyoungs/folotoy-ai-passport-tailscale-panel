// tests/test_np_account.c —— 登录状态归类与状态页两行文案测试。
#include <assert.h>
#include <string.h>
#include "np_account.h"

int main(void)
{
    char buf[96];

    // 归类:没凭据时另两个布尔不许改变结论(屏幕上"未配置"最要紧)。
    assert(np_login_classify(false, false, false) == NP_LOGIN_NONE);
    assert(np_login_classify(false, true, true) == NP_LOGIN_NONE);
    // 被拒优先于"成功过":换了密钥或撤销授权后,上一次成功不能继续报"已登录"。
    assert(np_login_classify(true, true, true) == NP_LOGIN_REJECTED);
    assert(np_login_classify(true, true, false) == NP_LOGIN_REJECTED);
    assert(np_login_classify(true, false, true) == NP_LOGIN_ON);
    assert(np_login_classify(true, false, false) == NP_LOGIN_PENDING);

    np_login_text(NP_LOGIN_NONE, buf, sizeof(buf));
    assert(strcmp(buf, "登录:未配置凭据") == 0);
    np_login_text(NP_LOGIN_PENDING, buf, sizeof(buf));
    assert(strcmp(buf, "登录:等待首次同步") == 0);
    np_login_text(NP_LOGIN_ON, buf, sizeof(buf));
    assert(strcmp(buf, "登录:已登录") == 0);
    np_login_text(NP_LOGIN_REJECTED, buf, sizeof(buf));
    assert(strcmp(buf, "登录:被拒绝,需重新配置") == 0);
    // 枚举之外的值(内存写坏、以后加状态忘了改文案)也不许留未初始化缓冲。
    np_login_text((np_login_state_t)99, buf, sizeof(buf));
    assert(strcmp(buf, "登录:状态未知") == 0);

    // 文案要装进状态页那一行:212 px 宽、14 px 字体 ≈ 15 个汉字,行缓冲 96 字节。
    for (int s = 0; s <= 3; s++) {
        np_login_text((np_login_state_t)s, buf, sizeof(buf));
        assert(strlen(buf) < 96u);
    }

    // 账号行:填了尾网就照原样讲,没填要说清用的是密钥默认那个尾网。
    np_account_text("example.ts.net", buf, sizeof(buf));
    assert(strcmp(buf, "账号:example.ts.net") == 0);
    np_account_text("", buf, sizeof(buf));
    assert(strcmp(buf, "账号:未指定(用密钥默认尾网)") == 0);
    np_account_text(NULL, buf, sizeof(buf));
    assert(strcmp(buf, "账号:未指定(用密钥默认尾网)") == 0);
    // 尾网名最长 64 字节(app_cfg 的 s_tailnet),加前缀 7 字节仍在 96 的行缓冲里。
    char long_name[65];
    memset(long_name, 'a', sizeof(long_name) - 1u);
    long_name[sizeof(long_name) - 1u] = '\0';
    np_account_text(long_name, buf, sizeof(buf));
    assert(strlen(buf) == 71u);
    // 缓冲装不下时只能截断,不许越界:前缀本身就 7 字节,给 4 字节试试。
    memset(buf, 'Z', sizeof(buf));
    np_account_text(long_name, buf, 4u);
    assert(strlen(buf) == 3u && buf[3] == '\0' && buf[4] == 'Z');

    // 空缓冲与空指针都不许写。
    np_login_text(NP_LOGIN_ON, NULL, 96u);
    np_login_text(NP_LOGIN_ON, buf, 0u);
    np_account_text("x", buf, 0u);
    np_account_text(NULL, NULL, 96u);
    return 0;
}
