<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# icons/

The operating-system marks shown in front of device names in the network
panel, and the script that turns them into C data.

| File | Role |
| --- | --- |
| [`generate_os_icons.py`](generate_os_icons.py) | Source of truth: every glyph as an explicit 12-character ASCII grid, plus `--preview` and the emit step. |
| [`np_os_icons.c`](np_os_icons.c) | Generated tables (`np_icon_codes`, `np_icon_bits`) compiled into the firmware via `main/CMakeLists.txt`. Do not hand-edit. |
| [`tailscale-favicon.svg`](tailscale-favicon.svg) | Verbatim copy of the official Tailscale mark, fetched from `https://tailscale.com/favicon.svg`. Reference only: the geometry is transcribed into `main/np_logo.c`, and the file is not compiled into the firmware. |

## Format and integration

- One glyph is a 12 × 12 grid, one byte per pixel, alpha only 0 / 90 / 150 /
  255 (`.` `~` `+` `#`). At this size a partial edge either reads or does not,
  so there is no anti-aliasing step to invent.
- `main/np_icons.c` renders the tables through a callback `lv_font_t`:
  `LV_FONT_GLYPH_FORMAT_A8` with `static_bitmap = 1`, which is the combination
  LVGL 9.6's software renderer blends directly instead of copying into a draw
  buffer. The list renders six rows at a time, so this costs no heap beyond the
  labels those rows already had.
- Codepoints are private-use `U+E000`-`U+E004`; `np_model.h` names them
  (`NP_GLYPH_LINUX` and friends) and `np_os_glyph()` maps the control plane's
  `os` string to one. The chain is `np_font_14 → np_icon_font → montserrat_14`,
  installed on runtime copies of the subset font descriptors.
- `tests/test_icon_contract.py` pins the glyph count, the private-use range
  (the three-byte UTF-8 encoding assumes it), the pixel count, the four-value
  alpha alphabet, and that the `NP_GLYPH_*` constants match the generated
  table order.

These codepoints stay out of the glyph inventory that
[`../fonts/extract_chars.py`](../fonts/extract_chars.py) builds: they are
shapes, not text, and the generator's own comments contain characters that must
never become glyphs. Consequence for callers — an icon byte has to be appended
after `np_text_sanitize()`, or the inventory filter replaces it with `?`.

## Regenerate

From the repository root:

```sh
python3 assets/icons/generate_os_icons.py --preview   # look at the shapes
python3 assets/icons/generate_os_icons.py             # rewrite np_os_icons.c
```

No external dependency, no font conversion. Commit the generated file with any
change to the grids; `tools/validate.sh` does not rebuild it.

## Origin and license

**Operating-system marks** — original artwork: the grids were drawn by hand for
this repository, at 12 px, in the plain ASCII form above so they can be reviewed
without an image tool. None of them is extracted from, traced from, or bundled
with any vendor asset file, and no third-party icon set is redistributed here.
The marks stand for the operating systems named by `os` values returned by the
Tailscale control plane; any trademarks involved belong to their owners and are
not licensed by this repository.

**The Tailscale mark** — `tailscale-favicon.svg` is the opposite case and is
labelled as such: it is the official file, byte for byte apart from the header
comment that records where and when it was fetched, and `main/np_logo.c` traces
its coordinates. Tailscale and its mark are trademarks of Tailscale Inc.; they
are kept here only to identify the service this device monitors, and nothing in
this repository is affiliated with or endorsed by Tailscale Inc. If their brand
guidance ever calls for removal, delete the SVG together with `main/np_logo.c`,
`main/np_logo.h`, `tests/test_np_logo.c`, `tests/test_logo_contract.py` and the
`status_logo()` block in `main/netpanel_ui.c`; the rest of the status view does
not depend on them.
