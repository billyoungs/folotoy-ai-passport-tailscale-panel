// tests/test_np_prompt.c —— 局域网配置提示页文案测试(0.0.0.0 绝不外泄)。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "np_prompt.h"

static void fit_len(const np_prompt_t *p)
{
    for (int i = 0; i < NP_PROMPT_LINES; i++) {
        assert(strlen(p->line[i]) < NP_PROMPT_LINE);
    }
}

// 屏幕上任何一行都不许出现全零地址:它看着像成功,点开是死链。
static void no_zero_ip(const np_prompt_t *p)
{
    for (int i = 0; i < NP_PROMPT_LINES; i++) {
        assert(strstr(p->line[i], "0.0.0.0") == NULL);
    }
}

int main(void)
{
    np_prompt_t p;

    // 正常路径:第一行交代 Wi-Fi 结果,第二行交代"换设备录入",第三行给
    // 可点开的地址,第四行配对码
    np_prompt_lan(&p, "Yang", "192.168.1.5", "123456");
    assert(strcmp(p.line[0], "Wi-Fi 已连:Yang") == 0);
    assert(strcmp(p.line[1], "用其他设备录入 Tailscale 凭据") == 0);
    assert(strcmp(p.line[2], "打开 http://192.168.1.5") == 0);
    assert(strcmp(p.line[3], "配对码 123456 长按中止") == 0);
    fit_len(&p);
    no_zero_ip(&p);

    // DHCP 还没给出地址:退化成明示"未取得 IP",而不是印一个 0.0.0.0
    np_prompt_lan(&p, "Yang", "", "123456");
    assert(strcmp(p.line[2], "未取得 IP,长按确定重试") == 0);
    assert(strstr(p.line[2], "http") == NULL);
    no_zero_ip(&p);

    // 调用方即使把全零地址传进来也拦得住(np-0.1.10 的实测症状)
    np_prompt_lan(&p, "Yang", "0.0.0.0", "123456");
    assert(strcmp(p.line[2], "未取得 IP,长按确定重试") == 0);
    no_zero_ip(&p);

    //  ssid/配对码缺失:各行退化成不依赖该信息的提示,不留空行
    np_prompt_lan(&p, "", "10.0.0.2", "");
    assert(strcmp(p.line[0], "Wi-Fi 已连接") == 0);
    assert(strcmp(p.line[2], "打开 http://10.0.0.2") == 0);
    assert(strcmp(p.line[3], "长按确定 = 中止配置") == 0);
    fit_len(&p);

    np_prompt_lan(&p, "yeung", "10.0.0.3", NULL);
    assert(strcmp(p.line[3], "长按确定 = 中止配置") == 0);
    np_prompt_lan(&p, NULL, NULL, NULL);
    assert(strcmp(p.line[0], "Wi-Fi 已连接") == 0);
    assert(strcmp(p.line[2], "未取得 IP,长按确定重试") == 0);

    // 超长 ASCII SSID(32 字符)压满一行仍要留 NUL,前缀不能被挤掉
    np_prompt_lan(&p, "01234567890123456789012345678901", "172.16.0.9", "654321");
    assert(strlen(p.line[0]) == 39u);
    assert(strncmp(p.line[0], "Wi-Fi 已连:", 13u) == 0);
    fit_len(&p);
    no_zero_ip(&p);

    // 含中文的 SSID 截到一半时必须退到码点边界,不给 LVGL 留半个字符
    np_prompt_lan(&p, "客客客客客客客客客客客客", "172.16.0.9", "654321");
    assert(strlen(p.line[0]) == 37u);   // 13 前缀 + 8 个三字字符,第 9 个被完整退回
    assert(memcmp(p.line[0] + 13, "客客客客客客客客", 24u) == 0);
    fit_len(&p);

    // 超长配对码同样收敛,不能顶破缓冲区
    np_prompt_lan(&p, "Yang", "192.168.1.5", "123456789012345678901234567890");
    fit_len(&p);
    assert(strncmp(p.line[3], "配对码 1234567890123456", 21u) == 0);

    np_prompt_lan(NULL, "Yang", "1.2.3.4", "123456");  // 空指针只是不动作

    // ---- np_prompt_lan_update:配对码页每秒复查,只有真变了才要求重画
    np_prompt_t cur;
    np_prompt_lan(&cur, "Yang", "192.168.1.5", "123456");

    // 地址没变:四行逐字相同,不动 io、不重画
    assert(np_prompt_lan_update(&cur, "Yang", "192.168.1.5", "123456") == false);
    assert(strcmp(cur.line[2], "打开 http://192.168.1.5") == 0);

    // DHCP 换租约:地址行变,其余三行(含配对码)原样保留
    assert(np_prompt_lan_update(&cur, "Yang", "192.168.1.77", "123456") == true);
    assert(strcmp(cur.line[2], "打开 http://192.168.1.77") == 0);
    assert(strcmp(cur.line[3], "配对码 123456 长按中止") == 0);
    fit_len(&cur);
    no_zero_ip(&cur);

    // 链路掉了、地址回空:明说"未取得 IP",照样不许漏出 0.0.0.0
    assert(np_prompt_lan_update(&cur, "Yang", "", "123456") == true);
    assert(strcmp(cur.line[2], "未取得 IP,长按确定重试") == 0);
    no_zero_ip(&cur);

    // 地址没动但换了 AP:第一行跟着变,同样得重画
    assert(np_prompt_lan_update(&cur, "office", "", "123456") == true);
    assert(strcmp(cur.line[0], "Wi-Fi 已连:office") == 0);

    // 空指针同样只是不动作
    assert(np_prompt_lan_update(NULL, "Yang", "1.2.3.4", "123456") == false);

    printf("np_prompt: ok\n");
    return 0;
}
