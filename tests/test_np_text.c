// tests/test_np_text.c —— UTF-8 解码与字形清表过滤的纯逻辑测试。
#include <assert.h>
#include <string.h>
#include "np_text.h"

int main(void)
{
    size_t adv;
    uint32_t cp;

    // ASCII
    assert(np_utf8_next("A", 1, &adv, &cp) == 1 && cp == 'A' && adv == 1);
    // 3 字节 CJK(在=U+5728,清表内字符,来自 UI 文案"在线")
    assert(np_utf8_next("\xE5\x9C\xA8", 3, &adv, &cp) == 1 && cp == 0x5728 && adv == 3);
    // 非法首字节 / 截断 / 过长编码
    assert(np_utf8_next("\xFF", 1, &adv, &cp) == -1);
    assert(np_utf8_next("\xE5\x9C", 2, &adv, &cp) == -1);
    assert(np_utf8_next("\xC0\x80", 2, &adv, &cp) == -1);
    // 代理区
    assert(np_utf8_next("\xED\xA0\x80", 3, &adv, &cp) == -1);

    // 清表命中与未命中
    assert(np_text_supported('x'));
    assert(np_text_supported(0x5728u));      // 在
    // 负例:扩展 B 区生僻字,任何源文件注释里都不会出现,保证不在清表
    assert(!np_text_supported(0x2A6B2u));
    assert(!np_text_supported(0x01u));

    // sanitize:支持字符原样保留,不支持字符以 '?' 替换并计数
    char out[64];
    assert(np_text_sanitize(out, sizeof(out), "abc") == 0 && strcmp(out, "abc") == 0);
    assert(np_text_sanitize(out, sizeof(out), "\xE5\x9C\xA8\xE7\xBA\xBF") == 0 &&
           strcmp(out, "\xE5\x9C\xA8\xE7\xBA\xBF") == 0);  // 在线(均在清表)
    assert(np_text_sanitize(out, sizeof(out), "a\xF0\x9F\x98\x80""b") == 1 &&
           strcmp(out, "a?b") == 0);                        // emoji 被替换
    assert(np_text_sanitize(out, sizeof(out), "") == 0 && out[0] == '\0');

    // 缓冲区边界:超长截断且始终 NUL 结尾;cap=0 返回 -1
    char small[4];
    assert(np_text_sanitize(small, sizeof(small), "abcdefg") == 0);
    assert(strlen(small) == 3u);
    assert(np_text_sanitize(small, 0, "x") == -1);

    // 非法字节流不会死循环:每个坏字节恰好消费一次
    assert(np_text_sanitize(out, sizeof(out), "\xC0\xC1\xC2") == 3);
    return 0;
}
