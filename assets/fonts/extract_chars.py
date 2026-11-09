#!/usr/bin/env python3
"""从应用源文件提取中文字形清表,生成 np_inventory.c 与 chars.txt。

单一事实来源:UI 源码里的每个 CJK/全角字符必须先进入清表,再由
generate_netpanel_fonts.sh 用同一清单生成 LVGL 字体子集。
主机测试与运行时字形自检读取同一份 np_inventory.c。

用法(仓库根目录):
  python3 assets/fonts/extract_chars.py [--check] srcfile...
--check: 仅校验现有 np_inventory.c 与源文件一致(用于 validate.sh),不写文件。
"""
import sys
import pathlib

def collect(paths):
    cps = set()
    for p in paths:
        text = pathlib.Path(p).read_text(encoding="utf-8")
        for ch in text:
            cp = ord(ch)
            if cp < 0x80:
                continue
            # 排除注释之外的判断不做:注释里的中文也可能要显示,宁可多收录。
            cps.add(cp)
    # 固定追加标点兜底(表单/错误文案常用),避免漏字
    for extra in "，。：；！？…—·％（）「」":
        cps.add(ord(extra))
    return sorted(cps)

def emit_c(cps, out_path):
    lines = [
        "// main/np_inventory.c —— 由 assets/fonts/extract_chars.py 生成,请勿手改。",
        "// 重新生成: python3 assets/fonts/extract_chars.py main/*.c netpanel 源文件列表见脚本头部",
        "#include \"np_inventory.h\"",
        "",
        "const uint32_t np_inventory[] = {",
    ]
    row = []
    for cp in cps:
        row.append(f"0x{cp:04X}u,")
        if len(row) == 8:
            lines.append("    " + " ".join(row))
            row = []
    if row:
        lines.append("    " + " ".join(row))
    lines += ["};", "", f"const size_t np_inventory_count = {len(cps)}u;", ""]
    new = "\n".join(lines)
    old = out_path.read_text(encoding="utf-8") if out_path.exists() else ""
    return new, old

def main(argv):
    check = "--check" in argv
    paths = [a for a in argv if not a.startswith("--")]
    if not paths:
        print("用法: extract_chars.py [--check] srcfile...", file=sys.stderr)
        return 2
    root = pathlib.Path(".")
    cps = collect(paths)
    c_path = root / "main/np_inventory.c"
    txt_path = root / "assets/fonts/netpanel_chars.txt"
    new_c, old_c = emit_c(cps, c_path)
    new_txt = "\n".join(f"U+{cp:04X}" for cp in cps) + "\n"
    if check:
        ok = (new_c == old_c)
        if not ok:
            print("FAIL: np_inventory.c 与源文件不一致,重新运行 extract_chars.py", file=sys.stderr)
        return 0 if ok else 1
    c_path.write_text(new_c, encoding="utf-8")
    txt_path.write_text(new_txt, encoding="utf-8")
    print(f"写入 {c_path} 与 {txt_path}: {len(cps)} 个码点")
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
