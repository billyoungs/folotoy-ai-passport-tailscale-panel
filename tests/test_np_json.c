// tests/test_np_json.c —— JSON 字符串转义测试。
#include <assert.h>
#include <string.h>
#include "np_json.h"

int main(void)
{
    char out[64];
    size_t u;

    // 原样写入,不额外加引号
    u = 0;
    assert(np_json_str(out, sizeof(out), &u, "home"));
    assert(strcmp(out, "home") == 0);
    assert(u == 4u);

    // 已有前缀时续写
    memcpy(out, "{\"s\":\"", 6);
    u = 6;
    assert(np_json_str(out, sizeof(out), &u, "a\"b"));
    assert(strcmp(out, "{\"s\":\"a\\\"b") == 0);

    // 反斜杠自身
    u = 0;
    assert(np_json_str(out, sizeof(out), &u, "a\\b"));
    assert(strcmp(out, "a\\\\b") == 0);

    // 控制字符走十六进制转义
    u = 0;
    assert(np_json_str(out, sizeof(out), &u, "x\ny"));
    assert(strcmp(out, "x\\u000ay") == 0);

    // 制表符与 NUL 终止:0x1f 是最小的受控字符
    u = 0;
    assert(np_json_str(out, sizeof(out), &u, "\t\x1f"));
    assert(strcmp(out, "\\u0009\\u001f") == 0);

    // UTF-8 多字节不拆,按字节原样透传
    u = 0;
    assert(np_json_str(out, sizeof(out), &u, "网吧"));
    assert(strcmp(out, "网吧") == 0);

    // 空串成功:长度不变,只在当前位置补一个终止符
    memcpy(out, "abc", 3);
    out[3] = '#';
    u = 3;
    assert(np_json_str(out, sizeof(out), &u, ""));
    assert(u == 3u && memcmp(out, "abc", 3) == 0 && out[3] == '\0');

    // 容量不足:返回 false 且 *used 保持原值,调用方不会发出半截 JSON
    char small[8];
    memcpy(small, "123456", 7);
    u = 3;
    assert(!np_json_str(small, sizeof(small), &u, "abcdefgh"));
    assert(u == 3u);

    // 一个引号吃掉两个字节后放不下
    u = 0;
    assert(!np_json_str(small, sizeof(small), &u, "abc\"defgh"));
    assert(u == 0u);

    // 参数非法
    u = 0;
    assert(!np_json_str(NULL, sizeof(out), &u, "x"));
    assert(!np_json_str(out, sizeof(out), NULL, "x"));
    assert(!np_json_str(out, sizeof(out), &u, NULL));
    u = sizeof(out);
    assert(!np_json_str(out, sizeof(out), &u, "x"));

    // 字面量追加
    u = 0;
    assert(np_json_lit(out, sizeof(out), &u, "{\"a\":"));
    assert(strcmp(out, "{\"a\":") == 0 && u == 5u);
    u = sizeof(out) - 1u;
    assert(!np_json_lit(out, sizeof(out), &u, "ab"));
    assert(u == sizeof(out) - 1u);

    // 整数追加:负数与极值都不越界
    u = 0;
    assert(np_json_int(out, sizeof(out), &u, -47));
    assert(strcmp(out, "-47") == 0);
    u = 0;
    assert(np_json_int(out, sizeof(out), &u, 9223372036854775807LL));
    assert(strcmp(out, "9223372036854775807") == 0);
    char narrow[20];
    u = 14;
    assert(!np_json_int(narrow, sizeof(narrow), &u, 9223372036854775807LL));
    assert(u == 14u);

    return 0;
}
