<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# fonts/

Reusable font material and generated LVGL font sources.

## Network panel subset fonts (`np_font_14.c`, `np_font_20.c`)

| Item | Value |
| --- | --- |
| Source | `NotoSansCJK-VF.ttc` instance `Noto Sans CJK SC` weight 400 (system package `google-noto-sans-cjk-vf-fonts`) |
| License | SIL Open Font License 1.1 — redistribution of subsets permitted |
| Coverage | printable ASCII (`0x20-0x7E`) plus every code point listed in `netpanel_chars.txt` |
| Character inventory | `main/np_inventory.c` and `netpanel_chars.txt`, generated from application sources by `extract_chars.py` |
| Conversion | `lv_font_conv@1.5.3`, `--bpp 4 --format lvgl --no-compress`, sizes 14 and 20 |
| Destination | compiled into the `main` component (see `main/CMakeLists.txt` `target_sources`), used by `netpanel_ui.c` with a Montserrat 14 fallback for missing symbols |

The `.ttc` is a CFF2 variable collection, which `lv_font_conv` cannot read. The
script therefore runs a fixed pipeline, always in this order:

1. `fontTools.varLib.instancer` pins `wght=400`, producing a static CFF2 `.otf`
   (cached in `$STATIC_OTF`; the whole 65k-glyph family takes over ten minutes).
   This must happen *before* subsetting — running the instancer on a subsetted
   CFF2 font makes fontTools raise `TypeError`.
2. `pyftsubset` trims the static instance down to the inventory code points.
3. `fontTools.cffLib.CFF2ToCFF` rewrites the CFF2 outlines as CFF. The converter
   needs an already-decompiled TopDict, so `FDSelect`, `CharStrings` and every
   `Private` dict are touched before conversion, and `recalcBBoxes` is off.
4. `lv_font_conv` emits one `.c` per size.

Regenerate end to end from the repository root:

```bash
python3 assets/fonts/extract_chars.py <application sources...>   # refresh the inventory
FONT_ENV=/path/to/venv-with-fonttools \
  ./assets/fonts/generate_netpanel_fonts.sh <application sources...>
```

`extract_chars.py --check` (wired into `tools/validate.sh`) fails when tracked
sources contain characters missing from the generated inventory or fonts; the
device also self-checks glyph coverage at UI startup and logs missing glyphs.
Re-run the pipeline whenever the inventory grows, and confirm every new code
point appears in the generated `unicode_list` before committing.

## Conventions

- Generated `.c` fonts are committed so builds are reproducible without the source `.ttc` on the build machine.
- Do not commit the full Noto `.otf`/`.ttc` binaries here; the system package is the upstream.
- Keep `netpanel_chars.txt` and `main/np_inventory.c` in the same commit as any UI text change.
- The panel's row icons are **not** part of this pipeline: they live in [`../icons/README.md`](../icons/README.md), use private-use codepoints, and are deliberately absent from `netpanel_chars.txt` because they are shapes rather than text.
