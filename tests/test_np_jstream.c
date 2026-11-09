// tests/test_np_jstream.c —— 设备列表切片测试(重点是"内存不跟 tailnet 一起长")。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "np_jstream.h"

#define CAP 128

typedef struct {
    char got[8][CAP];    // 收回来的片段
    int n;               // 收回几个
    int incomplete;      // 被判定装不下的几个
} sink_t;

static void collect(void *user, const char *json, bool complete)
{
    sink_t *s = user;
    if (!complete) { s->incomplete++; return; }
    if (s->n < 8) {
        snprintf(s->got[s->n], CAP, "%s", json);
        s->n++;
    }
}

// 整包喂一次。
static void run(const char *body, sink_t *out, char *obj, size_t cap,
                uint32_t *total, bool stop_after_zero)
{
    np_jstream_t st;
    np_jstream_init(&st, obj, cap);
    if (stop_after_zero) np_jstream_stop(&st);
    np_jstream_feed(&st, body, strlen(body), collect, out);
    if (total) *total = np_jstream_total(&st);
}

int main(void)
{
    char obj[CAP];
    sink_t sink;
    uint32_t total;

    // ---------------------------------------------------------- 正常两台
    memset(&sink, 0, sizeof(sink));
    run("{\"devices\":[{\"hostname\":\"a\",\"n\":1},{\"hostname\":\"b\"}]}",
        &sink, obj, sizeof(obj), &total, false);
    assert(sink.n == 2);
    assert(total == 2u);
    assert(strcmp(sink.got[0], "{\"hostname\":\"a\",\"n\":1}") == 0);
    assert(strcmp(sink.got[1], "{\"hostname\":\"b\"}") == 0);
    // 片段本身就是合法 JSON 对象文本:首尾花括号都在,没被截
    assert(sink.got[0][0] == '{');
    assert(sink.got[0][strlen(sink.got[0]) - 1] == '}');

    // ------------------------------------- 根对象里还有别的键,顺序也不帮忙
    memset(&sink, 0, sizeof(sink));
    run("{\"meta\":{\"devices\":[1,2],\"count\":2},\"devices\":[{\"id\":\"7\"}]}",
        &sink, obj, sizeof(obj), &total, false);
    // 嵌套对象里同名的键不算:depth==1 这一条把它挡住了,否则会把 [1,2] 当列表。
    assert(sink.n == 1);
    assert(strcmp(sink.got[0], "{\"id\":\"7\"}") == 0);

    // --------------------------------------------- 空白、换行、缩进都得吃下
    memset(&sink, 0, sizeof(sink));
    run("{\n  \"devices\" : [ {\"a\":1},\n  {\"b\":2} ]\n}",
        &sink, obj, sizeof(obj), &total, false);
    assert(sink.n == 2);

    // --------------------------------------- 字符串里的花括号与转义引号不作数
    memset(&sink, 0, sizeof(sink));
    run("{\"devices\":[{\"hostname\":\"weird } { \\\"quoted\\\"\",\"os\":\"linux\"},"
        "{\"hostname\":\"tail }\"}]}",
        &sink, obj, sizeof(obj), &total, false);
    assert(sink.n == 2);
    assert(strstr(sink.got[0], "weird } { \\\"quoted\\\"") != NULL);
    assert(strcmp(sink.got[1], "{\"hostname\":\"tail }\"}") == 0);

    // 值里正好叫 devices 也不能骗到人:它后面跟的是 ',' 而不是 ':'
    memset(&sink, 0, sizeof(sink));
    run("{\"title\":\"devices\",\"devices\":[{\"x\":1}]}",
        &sink, obj, sizeof(obj), &total, false);
    assert(sink.n == 1);

    // ---------------------------------------------------- 嵌套对象要整台切全
    memset(&sink, 0, sizeof(sink));
    run("{\"devices\":[{\"a\":{\"b\":{\"c\":[]}},\"d\":\"e\"},{\"z\":0}]}",
        &sink, obj, sizeof(obj), &total, false);
    assert(sink.n == 2);
    assert(strcmp(sink.got[0], "{\"a\":{\"b\":{\"c\":[]}},\"d\":\"e\"}") == 0);

    // ------------------------------------------- 空数组 / devices 为 null
    memset(&sink, 0, sizeof(sink));
    run("{\"devices\":[]}", &sink, obj, sizeof(obj), &total, false);
    assert(sink.n == 0 && total == 0u);
    memset(&sink, 0, sizeof(sink));
    run("{\"devices\":null}", &sink, obj, sizeof(obj), &total, false);
    assert(sink.n == 0);   // 不是数组就认不出来,调用方按解析失败处理

    // 根对象没有 devices:整包读完,一个也不给
    memset(&sink, 0, sizeof(sink));
    run("{\"error\":{\"message\":\"nope\"}}", &sink, obj, sizeof(obj), &total, false);
    assert(sink.n == 0 && total == 0u);

    // ----------------------------------------------- 一块一片:切在任意字节上
    // 真实 body 是 1 KB 一块送进来的,状态机必须跨块存活。逐字节喂与整包喂结果一致。
    {
        static const char *body =
            "{\"devices\":[{\"hostname\":\"aa\",\"tags\":[\"tag:x\",\"tag:y\"]},"
            "{\"hostname\":\"bb\"},{\"hostname\":\"cc\"}]}";
        sink_t whole, bytewise;
        char o1[CAP], o2[CAP];
        memset(&whole, 0, sizeof(whole));
        memset(&bytewise, 0, sizeof(bytewise));
        uint32_t t1 = 0, t2 = 0;
        run(body, &whole, o1, sizeof(o1), &t1, false);
        np_jstream_t st;
        np_jstream_init(&st, o2, sizeof(o2));
        for (size_t i = 0; body[i] != '\0'; i++) {
            np_jstream_feed(&st, body + i, 1, collect, &bytewise);
        }
        t2 = np_jstream_total(&st);
        assert(t1 == 3u && t2 == 3u);
        assert(bytewise.n == whole.n);
        for (int i = 0; i < whole.n; i++) {
            assert(strcmp(bytewise.got[i], whole.got[i]) == 0);
        }
    }

    // ---------------------------------------------- 单台装不下:计数照走,别错位
    // 缓冲区只够第一条的一半:第一条报"不完整",后面的仍然按整台切出来。
    memset(&sink, 0, sizeof(sink));
    {
        np_jstream_t st;
        char small[24];
        np_jstream_init(&st, small, sizeof(small));
        const char *body = "{\"devices\":[{\"hostname\":\"0123456789abcdef0123\"},"
                           "{\"b\":2}]}";
        np_jstream_feed(&st, body, strlen(body), collect, &sink);
        assert(sink.n == 1);                 // 只有第二条装得下
        assert(strcmp(sink.got[0], "{\"b\":2}") == 0);
        assert(sink.incomplete == 1);        // 第一条被截断,调用方不要拿去解析
        assert(np_jstream_total(&st) == 2u); // 但个数没丢
    }

    // --------------------------------------- stop:列表满了之后只数不吐
    memset(&sink, 0, sizeof(sink));
    run("{\"devices\":[{\"a\":1},{\"b\":2},{\"c\":3}]}",
        &sink, obj, CAP, &total, true);
    assert(sink.n == 0 && sink.incomplete == 0);
    assert(total == 3u);

    // finished:数组闭合后调用方可以不再读 body
    {
        np_jstream_t st;
        char o[CAP];
        np_jstream_init(&st, o, sizeof(o));
        const char *body = "{\"devices\":[{\"a\":1}]}   ";
        sink_t s2;
        memset(&s2, 0, sizeof(s2));
        np_jstream_feed(&st, body, strlen(body), collect, &s2);
        assert(np_jstream_finished(&st));
        assert(s2.n == 1);
    }

    // 乱输入不许把状态机带沟里:闭合之后再喂什么都不发生
    {
        np_jstream_t st;
        char o[CAP];
        const char *body = "]}[{\"devices\":[{\"a\":1}]}";
        np_jstream_init(&st, o, sizeof(o));
        sink_t s2;
        memset(&s2, 0, sizeof(s2));
        np_jstream_feed(&st, body, strlen(body), collect, &s2);
        assert(s2.n == 0);
    }

    // 空指针与零容量:不崩、不吐片段
    {
        char o[CAP];
        np_jstream_init(NULL, o, sizeof(o));
        np_jstream_t st;
        np_jstream_init(&st, NULL, 0);
        np_jstream_feed(&st, "{\"devices\":[{\"a\":1}]}", 22, collect, NULL);
        np_jstream_feed(NULL, "x", 1, NULL, NULL);
        np_jstream_stop(NULL);
        assert(!np_jstream_finished(NULL));
        assert(np_jstream_total(NULL) == 0u);
    }

    return 0;
}
