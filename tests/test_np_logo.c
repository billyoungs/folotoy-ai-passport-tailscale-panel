// tests/test_np_logo.c —— 状态页 logo 几何表的测试(跨文件对账在
// tests/test_logo_contract.py,这里只钉表自身的约定)。
#include <assert.h>
#include <stdio.h>

#include "np_logo.h"

int main(void)
{
    int solid = 0, faded = 0;
    for (int i = 0; i < NP_LOGO_DOTS; i++) {
        const np_logo_dot_t *d = &np_logo_dots[i];
        // 圆心的整个圆盘都得在绘制盒里:越界的那半截会被父对象裁掉。
        assert(d->cx >= NP_LOGO_R && d->cx + NP_LOGO_R <= NP_LOGO_BOX);
        assert(d->cy >= NP_LOGO_R && d->cy + NP_LOGO_R <= NP_LOGO_BOX);
        if (d->faded) faded++;
        else solid++;
    }
    // 官方文件里是 4 个前景点(<g id="t">)+ 5 个背景点(<g id="grid">)。
    assert(solid == 4);
    assert(faded == 5);

    // 前景是"中间一行三点 + 正中下方一点":少了这一档,画出来的就不是那个 mark。
    assert(np_logo_dots[0].cx == 3  && np_logo_dots[0].cy == 12 && !np_logo_dots[0].faded);
    assert(np_logo_dots[1].cx == 12 && np_logo_dots[1].cy == 12 && !np_logo_dots[1].faded);
    assert(np_logo_dots[2].cx == 21 && np_logo_dots[2].cy == 12 && !np_logo_dots[2].faded);
    assert(np_logo_dots[3].cx == 12 && np_logo_dots[3].cy == 21 && !np_logo_dots[3].faded);

    // 九个点位落在 3×3 网格的整除间距上(盒 24 → 圆心 3/12/21)。
    for (int i = 0; i < NP_LOGO_DOTS; i++) {
        assert(np_logo_dots[i].cx % 9 == 3);
        assert(np_logo_dots[i].cy % 9 == 3);
    }

    printf("np_logo: ok\n");
    return 0;
}
