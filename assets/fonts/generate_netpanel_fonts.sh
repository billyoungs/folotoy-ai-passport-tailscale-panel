#!/usr/bin/env bash
# assets/fonts/generate_netpanel_fonts.sh —— 复现 np_font_14/np_font_20 的生成过程。
#
# 依赖:Node.js(npx lv_font_conv@1.5.3)、Python venv(fonttools)。
# 源字体:系统安装的 Noto Sans CJK 可变字体(OFL-1.1,允许再分发子集)。
#
# 四步流水线,顺序不能换:
#   1) instancer 取 SC Regular 静态实例(CFF2)。整族实例化要十几分钟,产物按
#      $STATIC_OTF 缓存;这一步必须在子集化之前 —— 对 CFF2 可变字体先子集化
#      会让 fontTools 的 instancer 抛 TypeError。
#   2) pyftsubset 裁到清表码点。
#   3) CFF2 → CFF。lv_font_conv 只认 TrueType/CFF 轮廓;convertCFF2ToCFF 要求
#      TopDict 已展开,所以先强制访问 FDSelect/CharStrings/Private,并关掉
#      recalcBBoxes。
#   4) lv_font_conv 逐字号出 LVGL C 字体。
#
# 用法(仓库根目录):
#   FONT_TTC=/path/to/NotoSansCJK-VF.ttc FONT_INDEX=2 \
#     ./assets/fonts/generate_netpanel_fonts.sh <应用源码列表...>
set -euo pipefail
cd "$(dirname "$0")/../.."

: "${FONT_TTC:=/usr/share/fonts/google-noto-sans-cjk-vf-fonts/NotoSansCJK-VF.ttc}"
: "${FONT_INDEX:=2}"          # 0=JP 1=KR 2=SC 3=TC 4=HK
: "${FONT_ENV:=/tmp/fontenv}" # python venv,需含 fonttools
STATIC_OTF=${STATIC_OTF:-/tmp/NotoSansCJKsc-Regular.otf}
WORK=/tmp/np_font_work
mkdir -p "$WORK"

CODES="$(python3 -c 'import sys
cs=set(int(l[2:],16) for l in open("assets/fonts/netpanel_chars.txt") if l.strip())
cs.update(range(0x20,0x7F))
print(",".join("U+%04X"%c for c in sorted(cs)))')"

SYMBOLS="$(python3 -c 'import sys
print("".join(chr(int(l[2:],16)) for l in open("assets/fonts/netpanel_chars.txt") if l.strip()), end="")')"

# 1) 清表必须与源码一致(单一事实来源)。
python3 assets/fonts/extract_chars.py --check "$@" >/dev/null 2>&1 || {
    echo "np_inventory.c 与源文件不一致:先不带 --check 重跑 extract_chars.py" >&2
    exit 1
}

# 2) 可变字体 → SC Regular 静态实例。
if [ ! -f "$STATIC_OTF" ]; then
    echo "实例化整族字体(可能要十几分钟)……"
    "$FONT_ENV/bin/python" - "$FONT_TTC" "$FONT_INDEX" "$STATIC_OTF" <<'PY'
import sys
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
ttc, idx, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
f = TTFont(ttc, fontNumber=idx)
instancer.instantiateVariableFont(f, {'wght': 400}).save(out)
PY
fi

# 3) 静态实例 → 清表子集。
"$FONT_ENV/bin/python" - "$STATIC_OTF" "$CODES" "$WORK/sc_subset.otf" <<'PY'
import sys
from fontTools.ttLib import TTFont
from fontTools.subset import Subsetter, Options

src, codes, out = sys.argv[1], sys.argv[2], sys.argv[3]
f = TTFont(src, recalcBBoxes=False, recalcTimestamp=False)
opt = Options()
opt.layout_features = []
opt.hinting = False
opt.name_IDs = ['*']
opt.notdef_outline = True
s = Subsetter(options=opt)
s.populate(unicodes=[int(c[2:], 16) for c in codes.split(',')])
s.subset(f)
f.save(out)
print("subset glyphs:", len(f.getGlyphOrder()))
PY

# 4) CFF2 → CFF,并确认源字体真的覆盖了每个码点。
"$FONT_ENV/bin/python" - "$WORK/sc_subset.otf" "$WORK/sc_cff.otf" "$CODES" <<'PY'
import sys
from fontTools.ttLib import TTFont
from fontTools.cffLib.CFF2ToCFF import convertCFF2ToCFF

src, dst, codes = sys.argv[1], sys.argv[2], sys.argv[3]
f = TTFont(src, recalcBBoxes=False, recalcTimestamp=False)
top = f['CFF2'].cff.topDictIndex[0]
_ = top.FDSelect            # 强制展开:转换会先把 cff2GetGlyphOrder 置 None
_ = top.CharStrings
for fd in top.FDArray:
    _ = fd.Private
convertCFF2ToCFF(f)
want = [int(c[2:], 16) for c in codes.split(',')]
have = f.getBestCmap()
missing = [hex(c) for c in want if c not in have]
if missing:
    raise SystemExit("源字体缺字形: %s" % missing)
f.save(dst)
print("cff ok, glyphs cover", len(want), "code points")
PY

# 5) 逐字号生成子集。
for SIZE in 14 20; do
    npx -y lv_font_conv@1.5.3 \
        --font "$WORK/sc_cff.otf" \
        --range 0x20-0x7E \
        --symbols "$SYMBOLS" \
        --size "$SIZE" --bpp 4 --format lvgl --no-compress \
        --lv-font-name "np_font_${SIZE}" --lv-include lvgl.h \
        --output "assets/fonts/np_font_${SIZE}.c"
done
echo "已生成 assets/fonts/np_font_14.c 与 np_font_20.c"
