// tests/test_np_form.c —— urlencoded 表单解析与配对码校验测试。
#include <assert.h>
#include <string.h>
#include "np_form.h"

int main(void)
{
    char out[64];

    // 基本提取
    assert(np_form_get("ssid=home%20net&pass=a%2Bb", "ssid", out, sizeof(out)));
    assert(strcmp(out, "home net") == 0);
    assert(np_form_get("ssid=home%20net&pass=a%2Bb", "pass", out, sizeof(out)));
    assert(strcmp(out, "a+b") == 0);

    // 键缺失 → false 且空串;+ 号还原为空格
    assert(!np_form_get("a=1", "b", out, sizeof(out)));
    assert(out[0] == '\0');
    assert(np_form_get("k=x+y", "k", out, sizeof(out)));
    assert(strcmp(out, "x y") == 0);

    // 前缀相同的键不误匹配(key 精确比较)
    assert(!np_form_get("ssid2=1&ssid=2", "ssid", out, sizeof(out)) == false);
    assert(np_form_get("ssid2=1&ssid=2", "ssid", out, sizeof(out)));
    assert(strcmp(out, "2") == 0);

    // 非法 %-序列原样保留
    assert(np_form_get("k=%zz%41", "k", out, sizeof(out)));
    assert(strcmp(out, "%zzA") == 0);

    // 超长截断返回 false,但仍写出截断内容
    char small[8];
    assert(!np_form_get("k=0123456789", "k", small, sizeof(small)));
    assert(strlen(small) == 7u);

    // 空值与空 body
    assert(np_form_get("k=", "k", out, sizeof(out)));
    assert(out[0] == '\0');
    assert(!np_form_get("", "k", out, sizeof(out)));
    assert(!np_form_get(NULL, "k", out, sizeof(out)));
    assert(!np_form_get("k=1", "k", out, 0));

    // 配对码
    assert(np_pairing_code_valid("123456"));
    assert(!np_pairing_code_valid("12345"));
    assert(!np_pairing_code_valid("1234567"));
    assert(!np_pairing_code_valid("12345a"));
    assert(!np_pairing_code_valid(""));
    assert(!np_pairing_code_valid(NULL));
    return 0;
}
