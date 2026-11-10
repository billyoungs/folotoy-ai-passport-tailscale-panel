<p align="right"><a href="tailscale-panel-history.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Network panel — delivery history

Per-version delivery record for the network panel application (`np-*` firmware
versions). Every entry is titled like a Conventional Commits subject so it can
seed the commit message when that version is committed, and the body keeps what
the version decided, what it traded away, the gate result and the exact image
identity. Entries are chronological — append new versions at the end. An entry
describes the firmware **as of its version** and is never rewritten
retroactively; the current specification lives in
[tailscale-panel.md](tailscale-panel.md).

Device status marks, as recorded at the time: **Flashed** = the user reported
results from hardware; **Awaiting a device flash** = gate-verified but never
reported from hardware. Entries before `np-0.1.8` quote the old fixed image
name; those files are gone from `build/` and survive only in the
content-addressed `build/firmware/<sha256>/` bundles.

## np-0.1.2 — feat(panel): baseline panel flashed; its field report opened the work

- Flashed by the user. The report that drove the round: several separate save
  buttons, SSID typed by hand, and `http://192.168.4.1` not opening unless the
  portal jumped there itself.

## np-0.1.4 — build(panel): first gate-verified image

- Built and gate-verified (sha256
  `b56e178b02cb1526bbb96c87bc5a86e728bb6fb6f396d7675a97979f3ed9f530`), never
  flashed: superseded the same day by `np-0.1.5`.

## np-0.1.5 — fix(prov): provisioning no longer dims the screen; page errors surface

- Code complete, `sdkconfig.defaults` pinned, `--static` green (host tests +
  font inventory, fonts regenerated). The first `--firmware` run failed on a
  wrong event struct name (`wifi_event_ap_staconnected_t`); after the fix the
  merged image passed the gate (sha256
  `856f0cb50d86fc599d9429923f8312de41981f6ad5eaef5ec7a4e403fa9b1e6b`). Never
  flashed: superseded straight away by `np-0.1.6`.
- Fixed here but not yet device-tested: no screen dimming while a provisioning
  session runs; `lru_purge_enable` so one phone holding keep-alive sockets
  cannot starve the four httpd slots; every page request has a `.catch()` so a
  dead session says so instead of looking inert; AP join/leave and
  session-close reasons are logged.
- Open device report (cause then unconfirmed): hotspot unreachable after a
  while, Save appearing to do nothing. Leading hypothesis: the 10-minute
  session window counted from service start could expire mid-form, and the
  hotspot then restarted with a *new* random SSID and password that a dimmed
  screen made unreadable.

## np-0.1.6 — fix(prov): session window is idle-based; hotspot credentials stable; watchdog hand-back

- The window is 10 minutes since the last request plus a 30-minute absolute cap
  (the page polls status every 5 s, so idle-only would never expire); timing
  moved into the pure module `main/np_session.c` with `tests/test_np_session.c`,
  which takes the clock as an argument; httpd and the watchdog exchange only a
  32-bit request counter because 64-bit accesses are not atomic on RV32.
- The bootstrap hotspot reuses its SSID and password until a Wi-Fi save lands
  (`s_ap_cred_*` in `app_prov.c`).
- Harder root cause fixed: the watchdog never cleared `s_watchdog` before
  deleting itself, so `app_prov_start()` kept returning `ESP_ERR_INVALID_STATE`
  and a second provisioning session could never start ("hotspot start failed").
  The handle is handed back before the APPLIED notification; starting waits up
  to 2.5 s for the retiring watchdog.
- Font inventory grew to 702 code points, subsets regenerated. Image sha256
  `8aa3337f00bc9e6c4c866c2a701c070a93272315a97b61507949ecb0a90291c1`.
  **Flashed** — the device report it produced is the `np-0.1.7` entry.

## np-0.1.7 — fix(ui): provisioning exit rebuilds the list screen

- Device report from `np-0.1.6`: green "device is connecting" on the phone
  while the board stayed on the provisioning screen with the keys dead. Cause:
  `handle_prov_done()` called `np_ui_list_exit()` first, deleting the list
  screen along with the rest, and the hotspot branch never rebuilt it — the
  panel kept showing the last frame of a deleted page with nowhere to report.
- `reenter_list()` (exit + enter under the LVGL lock, then render) now
  terminates both `enter_boot()` states and the LAN branch; `ST_WIFI_RETRY`
  renders the list as the retry page with the failure in its footer. Keys
  staying inert during `ST_PROV` is by design: only the OK long-press abort
  passes.
- Font inventory 705 code points, subsets regenerated. Image sha256
  `dba98d29be5a406ec13f07627bad4312a922bc3848b330e404214b03c7046162`.
  **Flashed**: provisioning no longer sticks on the last frame; Wi-Fi
  connection confirmed. Exposed the empty-list dead end fixed in `np-0.1.8`.

## np-0.1.8 — fix(ui): OK on an empty list opens the status view

- "Add the Tailscale credentials later" had nowhere to go: with no credentials
  the list is empty, `NP_KEY_OPEN_DETAIL` did nothing, and the status view —
  and therefore the LAN config server — could never be reached once Wi-Fi was
  saved and connecting. OK on an empty list now opens the status view directly
  through `show_status_view()`, which both branches share.
- From this build the merged image carries the versioned name (sha256
  `336a88df1a64513501cc2040e984498e81b479fcd565a41fd751cc09e1a10952`, 1789232
  bytes); delivering it deletes older `build/FoloToy-AI-Passport-full*.bin`,
  and the CI artifact and release patterns moved to the versioned pattern.
  **Awaiting a device flash.**

## np-0.1.9 — fix(panel): auto prompt without credentials; submit watchdog; dim timer reset

- Three fixes from the `np-0.1.8` device report: (1) the screen went dark the
  moment a form was submitted — any hand-back of the screen now calls
  `idle_reset()`, and the first frame at boot counts as an interaction; (2)
  `enter_boot()` starts a LAN config session by itself when Wi-Fi is up without
  credentials (`s_prompt_dismissed` suppresses further auto-starts for the
  boot); (3) a 20-second submit watchdog plus acknowledged responses for
  `/api/sync` and `/api/clear`; saving credentials over LAN closes the session
  and triggers an immediate sync.
- Lesson recorded: fixing (3) introduced a brace imbalance that would have
  deadened every button and still passed the gate — no host test parses the
  page script. Residual risk, still open.
- Font inventory 713 code points. Image sha256
  `977441c7327640b5ad6e5b2316c9ebab42b518cf05b015c9eb5f09f6c5fdf82c`, 1792144
  bytes. **Awaiting a device flash.**

## np-0.1.10 — feat(cfg): bench credentials behind `NP_DEV_WIFI` (removed in np-0.2.0)

- Development scaffolding, not a device fix: the `NP_DEV_WIFI` switch and a
  hard-coded two-network table (`Yang`, `yeung`) applied only while NVS holds
  no SSID; the footer names the network that failed last; the gate prints a
  warning while the switch is on.
- Font inventory 718 code points. Image sha256
  `620609d009b0f2404e296deba9e6c84bb69443882d1de7d25131ecd3ad0e6faf`, 1794288
  bytes. **Flashed**: joined one network with no provisioning, but the
  pairing-code prompt then printed `http://0.0.0.0` — fixed in `np-0.1.11`.

## np-0.1.11 — fix(wifi): connected means "has an IP"; a zero address can never reach the screen

- `app_wifi_connect()` returned on association while DHCP was still in flight,
  and the LAN page rendered whatever the netif held. Association no longer
  counts as connected: a `IP_EVENT_STA_GOT_IP` handler filtered to this
  component's STA netif sets a third event bit; the connect waits up to 8 s for
  it and reports `APP_ERR_WIFI_NO_IP` separately; `app_wifi_ip_str()` yields an
  empty string instead of `0.0.0.0`; the LAN session only opens with an address
  in hand.
- Prompt wording moved into the pure module `main/np_prompt.c` with
  `tests/test_np_prompt.c`: the zero-address regression, missing-ssid/missing-code
  degradation, and truncation on codepoint boundaries so a CJK name never
  reaches LVGL as half a UTF-8 sequence.
- Font inventory 744 code points. Image sha256
  `9b61faac9c1c64032e2decec8ec40735a667ae91c0a02165d594a986675efa27`, 1803504
  bytes. **Awaiting a device flash.**

## np-0.1.12 — feat(diag): layered Tailscale failure reporting; auto-dimming removed

- `ts_fail_t` + `ts_api_last_failure()` capture the esp-tls classification and
  mbedTLS codes, the socket errno, the HTTP status, the contacted host and the
  server's `message` field; the pure `main/np_errfmt.c` turns them into a
  conclusion line and a detail line, every branch pinned by
  `tests/test_np_errfmt.c`. Judgement calls recorded: the real classification
  comes from the esp-tls range `0x8000`–`0x80FF` because
  `esp_http_client_open()` only ever returns the generic connect error, and a
  text reason outranks a TLS code (a response cut mid-read carries a plausible
  handshake number while the truth is the link dropped).
- No RTC on the board: until SNTP answers, every certificate check fails.
  Polling waits up to 20 s for the clock, says so, and then tries anyway so the
  certificate flags name a router blocking UDP 123.
- Auto-dimming removed entirely (`idle_reset()`, `s_last_key_ms`, backlight
  steps) at the user's request; supersedes the `np-0.1.9` dim-timer fix.
- Font inventory 770 code points. Image sha256
  `65a8e3244bcb0ea574f64430442e7970c77dae08d421f8e9e0a1251ec6e262b0`, 1814720
  bytes. **Awaiting a device flash.**

## np-0.1.13 — fix(diag): decode handshake failures; heap numbers at the failure moment

- esp-tls stores the mbedTLS return as its absolute value and the
  classification as `0x801A`; `np_errfmt.c` decodes that into allocation
  failure, timeout, reset/EOF, malformed record, peer alert, bad certificate,
  version mismatch, missing root, bad local config, and reads the certificate
  flags per bit instead of collapsing them into one message.
- `ts_api.c` captures free heap and the largest free block at the moment of
  failure: a TLS handshake wants a contiguous tens-of-KiB allocation and the C3
  has no PSRAM — total heap can look healthy while the largest block is far too
  small. The detail line prints `-0x7F00`-style hex to match the serial log.
- Font inventory 781 code points. Image sha256
  `791fab97126cbe8caf27f28c620e5a143426b62fbd671d213968f2989fba8a17`, 1819600
  bytes. **Awaiting a device flash.**

## np-0.1.14 — fix(mem): allocate the HTTP response buffer after the TLS handshake

- The device answered `heap 30544 blk 17408` with `tls -0x7F00` =
  `MBEDTLS_ERR_SSL_ALLOC_FAILED`: the caller owned the whole 24 KiB response
  buffer before `esp_http_client_open()` — the call where the handshake
  happens — leaving mbedTLS no room for its record buffers.
- `http_exchange()` now owns the buffer: it allocates only after the handshake
  has completed, starts at 4 KiB and doubles toward the 24 KiB ceiling as the
  body needs; caller-supplied buffers are never grown; ownership transfers to
  the caller on the failure paths too. Loop arithmetic checked against a
  standalone model of the same expressions.
- Font inventory 785 code points. Image sha256
  `af9b373770d43f2976a4c9c91cff110109df17edabbb41ca057941371ea1afbe`, 1820944
  bytes. **Awaiting a device flash.**

## np-0.1.15 — fix(model): Go zero-time lastSeen no longer reads as years offline

- A connected node's `lastSeen` is Go's zero time (`0001-01-01T00:00:00Z`,
  verified against `tailscale status --json` on the same tailnet); it parsed
  fine and then `np_time_format_ago()` computed ~740000 days of age.
  `np_seen_disconnect_at()` in `main/np_model.c` is now the single decider
  (zero time, unparseable values, unset board clock, 15-minute grace window);
  the detail row prints the actual `MM-DD HH:MM`; `expires` gets the same
  floor test.
- Image sha256
  `afbb80f3fb34911db25981e3b0c2fcd3743f8e77144bcad6258ed026c376ff4a`, 1821824
  bytes. **Flashed**: the first flash of this image came up with a blank panel
  and the second worked — online detection confirmed fixed. Nothing in the
  images distinguishes the two attempts (identical DRAM segment, entry point
  and partition table; DROM +664 bytes = two glyphs); with no serial log the
  flash tool and cable stay the suspects. The audit also turned up two
  pre-existing LVGL-lock risks that later fed `np-0.1.27`.

## np-0.1.16 — feat(ui): Wi-Fi arc glyph, card list, plain-text credential inputs

- Name label and status dot share one midline (`LEFT_MID`); the RSSI number
  left the header for a drawn Wi-Fi glyph (three concentric `lv_arc` domes plus
  a dot), with banding in `np_signal_bands()` in `main/np_model.c` and every
  boundary pinned by tests; arcs because the subset font has no Wi-Fi symbol
  and drawing costs no code points.
- The list moved to 32 px cards with accent selection and muted offline names;
  the Tailscale credential inputs became plain text (whoever types them is the
  owner; the status line still masks, and the Wi-Fi password field keeps its
  show/hide checkbox).
- No new code points: comments were rewritten with existing characters so the
  asset did not grow by 21 glyphs. Font inventory stays at 787. Image sha256
  `feb936d9486994c4a3040835ee71299c9332bc608eb10fed9879c1f9e060b427`, 1823184
  bytes. `NP_DEV_WIFI=1` still set — bench image. **Awaiting a device flash.**

## np-0.1.17 — fix(wifi): retry auth-class disconnects; waiting becomes a note channel

- "The very first power-on says the Wi-Fi password is wrong": cold-radio
  auth-class disconnects (`AUTH_FAIL`, 4-way/handshake timeout, `MIC_FAILURE`)
  are now retried up to `AUTH_TRY_MAX = 3` times inside one
  `app_wifi_connect()`; `CONNECT_TIMEOUT_MS` went 12 s → 20 s so three rounds
  actually fit. A genuinely wrong key still fails fast.
- "The wait for SNTP is reported on the error line": a second status channel
  `s_note` alongside `s_err` — faults in red, waits in cyan; clock-not-synced,
  no-IP-yet, link-dropped-and-reconnecting and no-credentials moved to the note
  channel. `np-0.1.16` (never flashed) ships here too.
- Font inventory 801 code points (14 from comments). Image sha256
  `aac6be46afff870ec829b8bf0a1d7e410d947851ed072b38d4e4e67209c3426a`, 1827680
  bytes. **Awaiting a device flash** — the cold-boot cause is inferred from the
  symptom pattern; the board was flashed over the web tool, so there is no
  serial record of the failing attempt.

## np-0.1.18 — feat(ui): detail enrichment, OS icons, battery gauge; LAN scans on button only

- "Going into network configuration breaks Wi-Fi": the config page scanned on
  load in every mode; the all-channel sweep swallowed the driver's single
  reconnect and nothing re-armed the STA. LAN mode now scans only when *Rescan*
  is pressed, `app_wifi_scan()` re-kicks a link it broke, and the poll task
  runs the backoff ladder (`main/np_link.c`, `tests/test_np_link.c`): 3 s
  doubling to 30 s, disconnect-before-connect from the 4th round, full stack
  rebuild from the 8th — that round ends the re-provisioning dead end.
  Recorded as a code-reading inference; the observable predictions are in the
  original entry.
- The detail page fetches `fields=all` per device (never all 24 at once) into a
  static `np_detail_t` and publishes under the LVGL mutex; ten rows: address,
  OS, client, owner, tags, key expiry, control-plane report, joined date,
  relay, subnet routes. Latent bug fixed en route: `NP_ID_LEN` was 16 while
  Tailscale ids run 19–20 digits — it is 24 now and half-stored ids are dropped.
- OS glyphs are hand-placed pixel grids (ellipse compositing produced blobs at
  12 px), with `--preview` for terminal review; the battery became a drawn
  gauge at the right edge; the status dot gained a hollow/filled distinction.
- Font inventory 868 code points; `main/np_icons.c` joined the inventory source
  list while the generated icon sources stay out of it. Image sha256
  `28ae08089b6e4f414abe251a52375a8132956089eca7f864bfc3980b897da62b`, 1877968
  bytes. `NP_DEV_WIFI=1` — bench image. **Awaiting a device flash.**

## np-0.1.19 — fix(ui): detail wrapping and scroll, list geometry, address follows the link

- Detail rows printed on top of each other: values folded under `LONG_DOT`
  without growing the object. Values now wrap explicitly and the row advance is
  measured with `lv_text_get_size()`; the ten rows live in a vertically
  scrollable container (one 22 px pitch per step), and the late extra-info
  repaint restores the scroll offset — which needs `lv_obj_update_layout()`
  first, or the restore silently lands at the top.
- List rows hung off the right edge: the theme wraps containers in card
  padding. Padding is cleared on the list container and rows; geometry lives in
  the 12..228 band (`ROW_*` constants); names are pinned to one line with an
  ellipsis; scroll-into-view was added (past the seventh row the cursor had
  been moving on invisible rows).
- The pairing-code page address now follows the link, via the pure
  `np_prompt_lan_update()`; tests pin no-repaint, renewed lease, address
  dropping to empty, and an AP change that keeps the address.
- Font inventory 882 code points (14 from comments). Image sha256
  `9b0de968dd49929516303fe977a2a57b0a6ee6ec99faa8b858faa4af561d6399`, 1883072
  bytes. `NP_DEV_WIFI=1` — bench image. **Awaiting a device flash.**

## np-0.1.20 — refactor(ui): the detail view leaves only via OK long press

- The edge-exit gesture from `np-0.1.19` (press the same direction again at the
  edge) never reached a device and was withdrawn — mistiming one key while
  reading a middle row threw away the whole page. Up/Down only scrolls and
  clamps; the OK long press is the only way out. Font inventory unchanged at
  882 (subsets not regenerated). Image sha256
  `2ceab3d4b4804d0b3318da186ee87587586ce65269081662ad4b94719cfd98af`, 1883024
  bytes. `NP_DEV_WIFI=1` — bench image. **Awaiting a device flash.**

## np-0.1.21 — fix(panel): checkbox row, signing-in note, clock-grace rework

- The show-password checkbox stretched across the whole line (`input { width:
  100% }`) and pushed its label away; checkboxes got their own exception and a
  flex label row (`class=chk`), in both templates.
- After submitting credentials the screen stayed frozen: `handle_prov_done()`
  now writes "credentials saved, signing in" on the cyan note channel until the
  first sync resolves (cleared on success, red fault on failure, waiting note
  when genuinely stuck).
- The 20-second clock grace was measured from the first IP at boot and had
  always expired by the time credentials were submitted — the first login went
  out with a 1970 clock and hit a certificate whose validity had not started.
  The clock snapshot is taken at request time; `clock_rearm()` pushes the grace
  anchor; an unsynced-clock certificate failure is treated as transient (cyan
  note, retry when the window opens, honest report only after 3 stalled
  rounds). The pure `np_errfmt_clock_wait()` and its tests pin the whole rule.
- Font inventory 890 code points. Image sha256
  `9675e2983741824b0abca7496ba94705d6f7bb99a306446da372512bb9a5c5f0`, 1886432
  bytes. `NP_DEV_WIFI=1` — bench image; carries the unflashed np-0.1.19/0.1.20
  checks too. **Awaiting a device flash.**

## np-0.1.22 — feat(ui): header clock band, account/login rows, side-profile penguin

- Battery and Wi-Fi now share one 14..26 visual band (midline 20) aligned by
  ink rather than baseline; the band width is derived from the Wi-Fi arc radii
  and the battery shell hangs off `ICON_TOP`.
- Header: date `M/D` on the left, `HH:MM` centred (`np_time_format_md()` /
  `np_time_format_hm()`, sharing `civil_of()`); `--:--` while the clock is
  unsynced; "no network" and "waiting for time sync" merged into one cell's
  priority order — with the link down SNTP has no result either, so the two
  states share one root.
- Status page gained account and login-state rows: the account is the tailnet
  name from the config (no extra round trip; nothing in the repo supports an
  account display name), the login state is classified by the pure
  `np_account.c` — not configured > rejected > signed in > waiting for first
  sync; a rejection must outrank an old success. `s_last_sync` is tracked and
  reset on resubmit/clear; five rows at 22 px pitch.
- The Linux penguin became a side profile: front-facing was unrecognisable at
  12 px, and a 1 px transparent eye split the head into two pieces, so there is
  no eye. Reviewed version by version with `--preview`.
- Font inventory 906 code points; `np_account.c/.h` joined the inventory source
  list. Image sha256
  `8f590d2b98cef0cc2cf76af4f6353a29ef29f93938c04d2b87dc1f7e4b0869ca`, 1891344
  bytes. `NP_DEV_WIFI=1` — bench image; carries np-0.1.19–0.1.21 checks.
  **Awaiting a device flash.**

## np-0.1.23 — fix(mem): stream the device list; clock gate without grace

- The device refuted the grace period: after flashing, the very first
  credential submission again reported the certificate fault. Replaced by a
  gate with no grace — a request goes out only when the link **has an IPv4 and
  the clock is genuinely synced**; while it does not hold nothing is sent and
  the backoff ladder stays frozen. The waiting cadence moved into the pure
  `np_clock.c` (`np_clock_tick()` → `GO/IDLE/WAIT/REARM/STALLED`, tested): SNTP
  re-arms every 30 s, after 120 s the footer blames UDP 123 in cyan, NTP
  servers went from two to three, waiting text repaints only when the reason
  changes (`note_kind_t`), and `np_errfmt_clock_wait()` was deleted as
  unreachable.
- "Not enough memory" scaled with tailnet size: the whole table was collected
  into one buffer and handed to cJSON, which builds a tree 2.5–4x the text.
  The list now streams: 1 KiB reads feed the character-level slicer
  `np_jstream.c`, which cuts top-level objects out of the `devices` array one
  at a time, each parsed in its own small tree freed as soon as the row is
  filled — peak memory is "the largest single device plus a 1 KiB chunk".
  Tests pin nested same-name keys, escaped quotes, byte-by-byte feeding,
  truncated devices, and the state machine completing its walk while
  overflowing. `np_list_t` gained `total`; the footer reports "K more not
  shown" instead of letting the list read as complete.
- Font inventory 935 code points. Image sha256
  `5fc6584568fd1839055d32c3f576f40c0ccd08299d71b40b5eeec7c602531533`, 1902352
  bytes. `NP_DEV_WIFI=1` — bench image, device-test only; carries
  np-0.1.19–0.1.22 checks. **Awaiting a device flash.**

## np-0.1.24 — feat(ui): sessions reopen; the status view became a four-item menu

- "Hotspot failed to start" while already on Wi-Fi was an unopened exclusion:
  neither entry point closed the session that was already running, and
  `app_prov_start()` refuses while `s_mode` is not OFF. Both entry points now
  close first, and the failure screen prints the `esp_err_t` name (pure ASCII,
  no new glyphs).
- `app_prov_stop()` waits for the retiring watchdog (up to 25×40 ms) instead of
  returning while it could still tear down the session that had just been
  opened; the 2.5 s wait in `app_prov_start()` stays as backstop. A stale
  "configuration applied" event from a deliberately closed session is judged
  against the session state at hand-off.
- The status view became the menu (sync now / LAN configuration / start the
  guide hotspot / clear credentials); the geometry is the pure `np_menu.c`
  (`np_menu_move/clamp/activate/footer`, pinned by `tests/test_np_menu.c`), the
  selected item paints like a list row.
- "Clear credentials could not be reached": the two-press confirm rebuilt the
  screen and reset the selection, so the second press landed on sync now. The
  hint row rewrites in place (no rebuild, cursor unmoved) and pointers are
  handed back on exit. Long-press OK on the list opens the menu;
  `np_ui_handle_key()` runs under the LVGL lock.
- Font inventory 945 code points. Image sha256
  `7584aadbc5971fc0c1078ee832038f07226b08d9a80e0588950a1de3a4be5035`, 1906672
  bytes. `NP_DEV_WIFI=1` — bench image. **Awaiting a device flash.** The SNTP
  mechanism problem was explicitly left out of this version.

## np-0.1.25 — feat(ui): Passport hotspot name; clear hands over the address; Tailscale mark head

- `NETPANEL-%04X` became `Passport-%04X`; the scan list hides the device's own
  AP by comparing the SSID string it is actually running, so the rename needed
  nothing there.
- "What does clear credentials clear": the Tailscale triple only (access token
  + OAuth client id and secret, then a save so the NVS copy goes with it);
  stored Wi-Fi belongs to `app_cfg_clear_wifi()`, which no menu entry calls.
- Clearing now ends in `open_lan_prov()` — the screen prints the SSID,
  `netpanel.local`, the gateway IP and the pairing code; the dismissed-prompt
  flag resets; with no IP or a refused start the status view says which
  happened.
- The status head carries the product name, version and the Tailscale mark:
  nine `lv_obj` discs traced point by point from
  `assets/icons/tailscale-favicon.svg` into the pure table `main/np_logo.c`;
  `tests/test_logo_contract.py` re-derives the geometry from the SVG so the
  drawing cannot drift; a pixel-font icon was the wrong alternative (circles
  turn to mud at 12 px, per its own generator).
- Font inventory 945 → 957 code points. Image sha256
  `6632e01d8694892de172dca7b5bd1a8f45c4ddb8d5dba6838eeaf0fb175cf5e1`, 1910624
  bytes. `NP_DEV_WIFI=1` — bench only. **Awaiting a device flash.** The SNTP
  mechanism problem still deferred.

## np-0.1.26 — fix(mem): honest LVGL pool key, dynamic TLS buffers, memory telemetry

- `CONFIG_LV_MEM_SIZE_KILOBYTES=24` had never done anything — LVGL 9.6 consumes
  `CONFIG_LV_MEM_SIZE` and keeps the KB spelling as an inert `default 0`.
  `sdkconfig.defaults` now states `CONFIG_LV_MEM_SIZE=65536`: the same 64 KiB
  the map has always shown, written in the key that is read. Sizing the pool
  from a measured `lv_max` is step two.
- `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y`: TLS buffers are right-sized per record and
  shrink back to a 16-byte cache instead of a resident 16 KiB RX + 4 KiB TX per
  connection (20 KiB per poll, twice that with a detail fetch).
  `MBEDTLS_SSL_IN_CONTENT_LEN` was deliberately NOT lowered: nothing advertises
  `max_fragment_length`, so a lowered ceiling returns `-0x7200` on a
  legitimate 16 KiB record.
- Telemetry through three channels that already existed: `main/np_mem.c` (pure,
  ASCII so telemetry cannot push glyphs), a 20 s poll-loop INFO line, the
  handshake-failure warning (heap/blk + DMA figures — separates "out of memory"
  from "out of contiguous memory"), and read-only `GET /api/mem`
  (`max_uri_handlers` 8 → 9). Pool collection lives in the app layer under the
  LVGL lock with a 200 ms budget.
- The failure class became checks: a static assertion ties `CONFIG_LV_MEM_SIZE`
  to `NP_LV_POOL_BYTES`; `check_memory_knobs()` rejects the inert key and
  requires both live knobs; the delivered map is the grep target.
- Font inventory 957 → 973. Image sha256
  `798c90657c1e1970bcc66c40f7e3c5dfa42d5503691ac33e35c412393034173a`, 1921664
  bytes. Map evidence: `work_mem_int` still `0x10000` (the pool did not move),
  `.dram0.bss` `0x1b318` → `0x1b340` (+40 bytes, the snapshot struct).
  `NP_DEV_WIFI=1` — bench only. **Awaiting a device flash and the user's
  measurement authorisation**: steady `heap/blk` over thirty ordinary polls
  with no `-0x7F00`; five consecutive detail fetches; the browse+poll collision
  moment via `GET /api/mem`; one run each of the OAuth and token paths. Step
  two (pool to `lv_max + ~30%`, radio buffers, draw buffer lines, BLE, demo
  sources, `s_html`) waits on those numbers.

## np-0.1.27 — fix(panel): keys stop freezing; snapshots admit staleness; scan and ladder make peace

- Four independent causes behind "the buttons freeze after I open LAN
  configuration": (1) the key task ran blocking reconnects every 30 s —
  periodic reconnection is the 500 ms ladder's alone now; (2) opening the page
  waited on the session watchdog inside the key task — the task is deleted,
  httpd workers leave only flags, `app_prov_take_close()` decides (≈3 KB of
  task stack back on the heap); (3) a busy LVGL lock swallowed presses — events
  re-post up to twice with post time and retry count, and any key older than
  300 ms when handled is logged; (4) the snapshot never admitted it was old —
  `s_snapshot_ms` + `np_snapshot_fresh()` draw hollow dots with muted names and
  the ladder countdown ("reconnecting, next try in N s").
- Also: `link_matches_config()` stops blocking reconnects from tearing down a
  healthy link; the ladder and the foreground scan no longer interrupt each
  other (`app_wifi_scan_busy()` + `heal_after_scan()`); `ACT_REBUILD` degrades
  to a plain kick while a LAN session holds the netif; a config-page exit sets
  `ST_RUN`/`ST_WIFI_RETRY` from the actual link; the pairing-code and hotspot
  pages state their own key semantics on the fifth row; the dev-Wi-Fi rotation
  moved onto the ladder's `FORGET` rung.
- Glyph inventory stayed at 973 (subsets untouched — on-screen sentences reuse
  existing characters). `--static` and the full gate green. Image sha256
  `dbc685b6137dfbdc2882349db3c1cf9d0e13cc459f9a37db3daa20013c71b69f`, 1922848
  bytes (+1184); map `.dram0.bss` `0x1b348` (+8), `work_mem_int` `0x10000`.
  `NP_DEV_WIFI=1` — bench only. **Awaiting a device flash** (seven checks in
  the original entry). The SNTP mechanism problem and the memory round's second
  step remain deferred, both waiting on the user.
- Deliberately not implemented, at the user's choosing: on-screen QR for the
  hotspot, and self-signed HTTPS on 443.

## np-0.2.0 — build(release): remove every pre-provisioned Wi-Fi credential

- The `NP_DEV_WIFI` switch, the two-entry `DEV_WIFI` table and the
  `app_cfg_dev_wifi*()` accessors were deleted along with their call sites
  (never commit credentials): one connect attempt per retry; the ladder's
  `FORGET` rung only clears link state — there is no table left to rotate. The
  `--firmware` warning about the switch is gone from `tools/validate.sh`.
- With nothing stored in NVS, a boot lands on the bootstrap hotspot directly
  (`enter_boot()` finds no Wi-Fi and calls `start_ap_prov()`); a device that
  provisioned through the form keeps its NVS credentials — releasing them means
  the on-device credential wipe or an NVS erase at flash time. The version went
  to `np-0.2.0` by hand (`tools/bump_version.sh` only increments the patch
  digit).
- Glyph inventory 973 → 972 (one character lived only in the deleted comments);
  both font subsets untouched. Image sha256
  `ae13251673317f69ec0c3d4269ed11170ce682524f88a5fe12723b581bfaad95`, 1922144
  bytes (−704); searching the image for the removed credential strings finds
  nothing; map `.dram0.bss` `0x1b340`, `work_mem_int` `0x10000`. `--static` and
  the full gate green (`GATE EXIT=0`).

## np-0.2.1 — feat(prov): two-stage entry — hotspot is Wi-Fi only, credentials via the LAN address

- The bootstrap hotspot page no longer renders the credentials/polling/timezone
  section at all (the collapsed variant is deleted; the actions note reads
  "Tailscale credentials are filled in later via LAN mode"), and the
  pairing-code page states its purpose in plain words: "Wi-Fi joined: <ssid>" /
  "enter the Tailscale credentials from another device" / "open http://<ip>" /
  "pairing code <code>, long-press to abort". `netpanel.local` left the device
  screen, kept by the web page header and mDNS.
- The auto-prompt skeleton was already in place (`enter_boot()` starts the LAN
  session on its own when Wi-Fi is up without credentials), so this round
  changed what the two pages render and say, not the state machine.
- Trade-off to know: on routers with client (AP) isolation the LAN address is
  unreachable, and such a network then has no way to enter credentials at all —
  the collapsed hotspot section used to be the last resort.
- Glyph inventory 972 → 971; both font subsets untouched. Image sha256
  `417e63eaba07d8b8757563df48e74f14252a20910ca568fa32dbf41640474130`, 1922080
  bytes (−64); map `.dram0.bss` unchanged at `0x1b340`, `work_mem_int` still
  `0x10000`. `--static` and the full gate green (`GATE EXIT=0`). **Awaiting a
  device flash**: erased first boot shows a Wi-Fi-only hotspot page; saving
  Wi-Fi lands on the pairing-code page with the instruction line, IP and code;
  entering credentials from another device returns the device to the list and
  starts syncing; long-press abort shows the cyan "credentials pending" note
  and re-prompts on the next boot; *Clear credentials* still lands on the
  pairing-code page.

## np-0.2.2 — fix(prov): open hotspot with a pairing gate; no page-load scans; DNS fix

- The hotspot is open (`WIFI_AUTH_OPEN`): the per-boot random 8-digit password
  and its cache are gone — and with them the failure class where a phone
  auto-retries a stale saved profile for a same-named hotspot and the WPA2
  handshake fails even though the user typed the current password correctly.
  The credential cache stays but holds the SSID only, so a reopen within one
  boot keeps the name.
- Every write on the page — both modes — now requires the 6-digit pairing code
  shown on the device screen: `pairing_ok()` no longer exempts SoftAP, the code
  section renders on the hotspot page too, and the code is generated for both
  modes. Joining stays free; changing the configuration requires being able to
  read the screen.
- No mode auto-scans on page load any more. The on-load scan ran blocking in
  the single httpd task (~2 s all-channel sweep), queued the first screenful
  behind it, and borrowed the radio exactly while the phone was associating —
  the code-level root of both field symptoms ("the correct password will not
  connect", "connected but the page never opens"). *Rescan* is a button in both
  modes, and the placeholder text is now shared.
- The captive DNS answered AAAA (and every non-A) query with a header claiming
  answers and no answer body — a malformed datagram that stalls portal
  detection on some phones. `an_count` now matches the answers actually
  written; AAAA gets a legal empty answer (NODATA).
- Trade-offs recorded: the AP channel stays fixed at 6 (auto-selection
  deferred); scans stay blocking (the async engine stays deferred; manual
  triggering lowers the urgency); the session-close grace drift is untouched.
  `np_dns_server.c` is not part of the glyph contract; the glyph inventory is
  unchanged at 971.
- `--static` and the full gate are green (`GATE EXIT=0`). Image
  `build/FoloToy-AI-Passport-full-np-0.2.2.bin`, sha256
  `e8f81b61453605041ab74b178619a29a5d22f7982e75473b6c2c9df27b45df50`, 1921856
  bytes (−224); map `.dram0.bss` `0x1b330` (−16, the dropped password cache),
  `work_mem_int` `0x10000`. **Awaiting a device flash** (six checks: password-free join including a phone that cached
  an old Passport profile; first screen no longer hangs; scan list empty until
  *Rescan*; wrong pairing code rejected and correct one saves; portal
  auto-open; LAN-mode regression pass).

## np-0.2.3 — fix(prov): pairing gate retreats to the LAN stage

- The user's call after field-testing `np-0.2.2`: the pairing code must not
  block provisioning. The gate that `np-0.2.2` added to the hotspot page is
  gone — joining the open hotspot and filling Wi-Fi on it need nothing, and
  the pairing code again guards only the LAN credential stage (the `np-0.2.1`
  scope). `np-0.2.2`'s three fixes all stay: open hotspot, button-only scans,
  DNS AAAA.
- Mechanics: `pairing_ok()` exempts SoftAP again and `HTML_CODE_SECTION`
  renders only on the LAN page — which required the shared JS `post()` to
  tolerate a missing `#code` element (`var c=q('code')…`), because the old
  unconditional `q('code').value` would have thrown on a page without the
  field and deadened every button (the residual-risk class recorded in
  `np-0.1.9`, caught on read this time).
- The pairing code is generated only when a LAN session starts (`s_pairing`
  cleared for SoftAP); the hotspot screen row is back to plain "no password
  needed" and the now-unused `s_prov_c` buffer is gone.
- Latent bug fixed en route: `app_prov_start()` cleared the out-params *after*
  copying the pairing code into them — an `np-0.2.2` rearrangement slip — so
  the LAN screen would have shown an empty code; the clears now run first.
- Trade-off recorded: the hotspot stage's page writes are unauthenticated —
  accepted, since that flow is guided and happens with the user next to the
  device. Glyph inventory unchanged at 971 (one comment reworded to avoid a
  font-subset regen). `--static` and the full gate are green (`GATE EXIT=0`).
  Image `build/FoloToy-AI-Passport-full-np-0.2.3.bin`, sha256
  `ebd1df0cc7f7738f0ba9c96c8654f9f96042757815b6177605e88ea702d59ae7`, 1921872
  bytes (+16); map `.dram0.bss` `0x1b308` (−40, the dropped pairing-code
  display path and `s_prov_c`), `work_mem_int` `0x10000`. **Awaiting a
  device flash** (checks: hotspot page without a code field; *Save and
  connect* works code-free; the LAN page still rejects a wrong code and
  accepts the right one; the `np-0.2.2` regressions re-run).

## np-0.2.4 — fix(prov): hotspot-page save stops dying on LAN-only fields

- Device report from `np-0.2.3`: on the hotspot page, entering the password and
  pressing *Save and connect* did nothing — not even the "submitting" note.
  Cause: the shared `go()` read `q('token')/q('base')/q('cap')/q('tz')`
  unconditionally, but those sections render only on the LAN page — the hotspot
  page has carried no Tailscale/polling fields since `np-0.2.1`. The first
  `.value` on a missing element threw inside the onclick handler and the
  button died silently. The same throw fired in `refresh()`'s first-poll branch,
  so the hotspot page's status line also showed the misleading "device
  disconnected" and the SSID prefill never ran.
- Latent since `np-0.2.1`, masked in `np-0.2.2` (the pairing gate stopped the
  flow one step earlier), exposed by `np-0.2.3` removing that gate — this is
  the third strike of the residual risk recorded in `np-0.1.9`: no host test
  parses the page script, so a dead button passes every gate.
- Fix: guard the optional reads — the credentials block behind `q('token')&&`,
  base/cap/tz behind their own element checks, and `refresh()`'s fill behind
  `if(q('base'))`. The hotspot page now submits, its status line works, and the
  SSID prefill runs there too; the LAN page is unchanged.
- Glyph inventory unchanged at 971 (the fix and its comments are all
  reused-character text). `--static` and the full gate are green (`GATE
  EXIT=0`). Image `build/FoloToy-AI-Passport-full-np-0.2.4.bin`, 1921920
  bytes, sha256 `8e9deff45abcf9391a3cf8b8a4414030900c69ab3c1f9840f4c4a59077910c6c`;
  map `.dram0.bss` `0x1b308` and `work_mem_int` `0x10000`, both unchanged from
  `np-0.2.3`. **Awaiting a
  device flash** (checks: hotspot page *Save and connect* completes the
  session; status line shows real state; SSID prefill after *Rescan*; the
  `np-0.2.3` checks re-run, LAN page unchanged).

## np-0.2.5 — feat(ui): the last-seen detail row reads "last online"

- Device feedback: the detail row that prints a machine's offline moment was
  labelled with in-group slang ("check-in" style) that other readers could not
  parse. The label now reads "last online". Label only — the value
  logic is untouched: offline and time-synced still prints `MM-DD HH:MM`,
  otherwise the control plane's own words, and the `lastSeen` parsing, grace
  folding and row data source are all unchanged.
- Zero new glyphs: all four characters of the new label were already in the
  inventory — the detail state line has been saying "last online" for a stale
  snapshot all along (`netpanel_ui.c` state branch), so the label reuses live
  strings; the retired label's characters stay in the inventory through
  comments elsewhere. Inventory unchanged at 971, no font regeneration.
- Bilingual spec updated in three spots (screens table, lastSeen semantics
  paragraph, row-source table). The `np-0.1.x` fresh-indicator narrative that
  names the old label stays as written — history entries describe then-behavior.
- Glyph audit and `--static` green. Image `build/FoloToy-AI-Passport-full-np-0.2.5.bin`,
  1921920 bytes, sha256
  `d9ff0f70167dca0b8d63500c3ca7337b34044255ee782981858bc5d8abd20738`;
  map `.dram0.bss` `0x1b308` and `work_mem_int` `0x10000`, both unchanged from
  `np-0.2.4`. **Awaiting a device flash** (checks: the detail row shows the new
  "last online" label with no missing glyphs; everything else unchanged).

## np-0.2.6 — feat(wifi): multi-network memory, polling back to 120 s, footer shows the sync time; fix(ui): frozen battery glyph

- Two user reports: after configuring the device once per Wi-Fi environment,
  it only remembers the last network and overwrites all earlier ones; and "a
  machine I just powered off keeps showing online, and I cannot tell when the
  status will refresh". The second breaks into three layers of delay: the
  control plane omits `lastSeen` entirely for nodes that are still connected
  (its keepalive timeout of a minute or two is out of the device's hands), the
  panel itself polls from 300 s with exponential backoff up to an hour, and the
  snapshot carries a grace window. What the device side can do is tighten the
  default poll to 120 s and print "this screen was synced at HH:MM" in the
  footer, so waiting and staleness are visible instead of guessed.
- Multi-network memory: a new pure module `np_wifi_store.c` keeps up to six
  networks, most-recently-used first (upsert updates and promotes by SSID and
  evicts the oldest slot when full; select promotes on use; pick takes the
  strongest intersection from the already-RSSI-sorted scan list). `app_cfg`
  persists `wn` plus `w0sid`/`w0psk` … `w5sid`/`w5psk`, while the active pair
  stays mirrored in the `ssid`/`psk` keys (readable by older firmware and by a
  downgraded image); devices upgraded from the single-pair firmware migrate on
  first load and land as `w*` keys on the next save.
  `tests/test_np_wifi_store.c` pins insert/promote/evict/pick/overlong-input.
- Connect path: at boot and on an OK retry, if more than one network is
  remembered the panel scans first and joins the strongest match; with one
  memory or a failed scan it does not block (scanning is an optimisation, not a
  new failure point); if the scan finds none of them it says "no remembered
  Wi-Fi nearby, long-press to reconfigure" right away instead of burning the
  full connect timeout.
- The normal footer now reads "long-press menu, sync HH:MM" (dropping "OK
  detail" to fit the 216 px row; the time is the wall clock at the moment the
  snapshot was swapped in, recorded by the poll task under the same lock as the
  monotonic stamp — rendering never guesses; with no successful sync yet or an
  unsynced clock it falls back to the old "OK detail, long-press menu" text).
  The overflow tier is unchanged.
- Battery-freeze fix: the user reported that in error states (no network, no
  credentials) the gauge ignored its charge buckets. Diagnosis: the sample is
  taken about every 10 s but posts no render event, and the list only reads the
  value when it repaints; the 1 s repaint tick is gated to `ST_RUN`, and the
  offline-countdown note is only issued once the link has been seen this boot —
  so on a boot that never connects nobody repaints, and the icon stays frozen
  on the first frame's -1 dark shell (which reads as an empty battery, not as
  the `0 %` bucket). Fix: take the first sample before the boot flow, and post
  one render event whenever the sampled value changes, so every state tracks
  the real charge within ~10 s. Nothing here is host-testable pure logic;
  validation is the gate plus the device flash.
- Inventory 971 → 983, twelve new glyphs (U+5065 U+5458 U+5883 U+5899 U+58EE
  U+5FC6 U+6C70 U+6DD8 U+6E38 U+76F2 U+4EA7 U+8D76 — all from
  new comments and the new test; the new on-screen text "no remembered Wi-Fi
  nearby" and "sync" use characters already in the list). Font subsets
  regenerated. `--static` and the full gate are green (`GATE EXIT=0`). Image
  `build/FoloToy-AI-Passport-full-np-0.2.6.bin`, 1926448 bytes (+4528), sha256
  `9ae48f73aeb3d9c66e3ee02670652e5313c38ac1e43de686401b8480e91f16dc`; map
  `.dram0.bss` `0x1b560` (+600, the six-slot memory and the timestamp),
  `work_mem_int` `0x10000` unchanged. **Awaiting a device flash** (checks:
  after provisioning once per environment, a power cycle on the other network
  auto-connects; booting where none of the remembered networks is in range
  shows the "no remembered Wi-Fi nearby" message and the list's battery glyph
  is tinted by the real charge; the footer shows
  "sync HH:MM" and advances on every sync; default poll is 120 s; the
  `np-0.2.5` regression items).

## np-0.2.7 — feat(wifi): roam to a remembered network that is actually in range after a link drop

- The other half of the np-0.2.6 report: multi-network memory only acted at
  boot and on a manual retry; once the link dropped, the ladder kept re-firing
  the one active network — a device carried to another environment while
  powered (or whose AP was moved away) waited forever.
- The roaming layer: `app_wifi.c` gains `roam_if_due()`, hung off the ladder's
  kick points — from the 2nd try on, at most once per 30 s, at the moment the
  ladder is about to kick (the only moment the radio is known to be idle), it
  scans once, promotes the strongest remembered network into the active pair,
  and kicks that. It runs no schedule of its own, so a scan never swallows an
  in-flight recovery; the swap lives in memory only and persistence waits for
  the next save (the boot path re-scans and re-picks anyway); a single memory
  is not worth a scan, and if none of the found networks is remembered the
  configuration stays untouched; the `REBUILD` tier is unaffected. A boot that
  lands on the retry page is taken over by the ladder, so it inherits the same
  roaming.
- Inventory 983 → 984, one new glyph (U+6F2B, from the roaming comment). Font
  subsets regenerated. `--static` and the full gate are green (`GATE EXIT=0`).
  Image `build/FoloToy-AI-Passport-full-np-0.2.7.bin`, 1926992 bytes (+5072),
  sha256 `5a5db859647e05fb16925d3b01faa7c12058a4e2833ae2a1d1ccf4ec6694fc16`;
  map `.dram0.bss` `0x1b568` (+8, the roaming tick),
  `work_mem_int` `0x10000` unchanged. **Awaiting a device flash** (checks:
  with the device powered through a link drop, carrying it into another
  remembered network's range re-connects to it within about a minute; the
  `np-0.2.6` regression items).

## np-0.2.8 — feat(settings): system settings (screen brightness / auto sleep / AOD)

- A new settings menu: long-press OK on the list page to enter (the old
  long-press-to-status binding moves to settings; the status page stays reachable
  via OK on the detail page). The settings home has three rows — screen
  brightness, auto sleep timeout, AOD toggle — OK enters a sub-page, long-press
  OK returns to the list and persists in one go.
- Live brightness: inside the sub-page, UP/DOWN steps by 5%, `bsp_display_backlight()`
  takes effect immediately; only RAM is touched and NVS is written on leaving
  settings (avoids frequent flash wear).
- Auto sleep timeout: never / 1 / 5 / 10 / 30 minutes / 1 / 2 hours. When the
  timeout fires and AOD is off, the backlight is turned off (blank); when AOD is
  on, it switches to the AOD page (time + online device count) at 20%
  brightness. The sleep check only runs in the normal RUN state; provisioning
  and hotspot pages never sleep (they must stay readable for the pairing code /
  SSID). Any key wakes: backlight restored, blank mode returns to the previous
  view, AOD returns to the list.
- Pure logic moves into `np_display_cfg` (brightness validation, timeout-tier
  validation/labels, sleep check) with host test `test_np_display_cfg.c`.
  Config keys `bri`/`offm`/`aod` join `app_cfg`.
- Inventory 984 → 988, three new glyphs (U+4E58, U+62E8, U+706D, from the
  `np_display_cfg` comments). Font subsets regenerated. `--static` is green
  (`Repository checks` 434 files / `Host tests` PASS). Image
  `build/FoloToy-AI-Passport-full-np-0.2.8.bin`, 1932080 bytes (+5088),
  sha256
  `d68d68a47466a82ac234768631fc85151c3f7410ee5018b419be05ca5c1aacdf`; map
  `.dram0.bss` `0x1b598` (+0x30), `work_mem_int` `0x10000` unchanged.
  **Awaiting a device flash** (checks:
  settings menu navigation; brightness steps live and persists across reboot;
  each sleep tier including never; with AOD on the AOD page shows time + online
  count, with AOD off the screen blanks; a key wakes back to the list; the
  provisioning page never sleeps; the `np-0.2.7` roaming regression items).

## np-0.2.9 — feat(settings): fold the provisioning menu into system settings; fix(aod): centred AOD with a large clock and x/y count

- Following np-0.2.8's entry-point change, the status page's provisioning
  actions (sync now / LAN configuration / guide hotspot / clear credentials)
  had moved one level deeper. This release adds a *Network configuration*
  sub-page to the settings home and folds the whole status menu into it,
  behaving identically (clearing still asks for a second press). The status
  page, reached via OK on a detail page, keeps its menu as well.
- The AOD page is re-centred: the current time uses a large Montserrat 48
  glyph, and the device count becomes `x/y` in a medium font (x = online,
  y = total), refreshed every second. `CONFIG_LV_FONT_MONTSERRAT_48` is
  enabled for this.
- No new glyphs. `--static` is green (`Repository checks` 434 files /
  `Host tests` PASS). Image
  `build/FoloToy-AI-Passport-full-np-0.2.9.bin`, 2029376 bytes
  (+97296, the Montserrat 48 glyph now lives in the image), sha256
  `b57db6f26657e465c674ef66117961aa7da641c38da2d7a49256a587e7e5bbfa`; map
  `.dram0.bss` `0x1b598` (flat), `work_mem_int` `0x10000` unchanged.
  **Awaiting a device flash** (checks:
  the settings menu has a *Network configuration* sub-page and all four actions
  work; the AOD clock is large and centred with a correct `x/y` count; the
  `np-0.2.8` regression items).

## np-0.2.10 — fix(aod): truly centred AOD, x/y count two tiers larger

- Device feedback: np-0.2.9's AOD page used `LV_ALIGN_CENTER`, but the time and
  count labels were fixed-width 240 px with left alignment, pushing the text
  to the right half of the screen so it did not look centred. This release drops
  the fixed width — labels auto-size around their text and are then centred with
  `LV_ALIGN_CENTER`, so they are truly centred horizontally.
- The `x/y` device count moves from Montserrat 20 (np-0.2.9) up two tiers to
  Montserrat 28 (`CONFIG_LV_FONT_MONTSERRAT_28=y` is now enabled).
- No new glyphs. `--static` is green (`Repository checks` 434 files /
  `Host tests` PASS). Image
  `build/FoloToy-AI-Passport-full-np-0.2.10.bin`, 2066656 bytes
  (+37280, the Montserrat 28 glyph now lives in the image), sha256
  `9f8d4d3e005559cb26bd1eb3ef49d0606a0ba0f1e312132d498e038cd21c55e2`; map
  `.dram0.bss` `0x1b598` (flat), `work_mem_int` `0x10000` unchanged.
  **Awaiting a device flash** (checks:
  the AOD time and `x/y` count are both truly centred horizontally and the
  `x/y` size is comfortable; the `np-0.2.9` regression items).

## np-0.2.11 — fix(aod): hide x/y until the host list has been read

- Feedback: while the device had just joined the network but had not yet
  successfully read the Tailscale host list, the AOD page briefly showed
  "0/0", an inaccurate online count. This release adds a `has_count` gate to
  the AOD page (driven by `s_last_sync > 0`): before the first successful sync
  only the centred time is shown and the device count is hidden; after a
  successful sync the time plus `x/y` appear.
- `np_ui_aod_show/update` gain a `has_count` argument; `netpanel_app.c` passes
  `s_last_sync > 0` at both call sites. No new glyphs.
- `--static` is green (`Repository checks` 434 files / `Host tests` PASS).
  Image `build/FoloToy-AI-Passport-full-np-0.2.11.bin`, 2066784 bytes (+128),
  sha256 `315a1a2b822d805e5adb80618c07e3cc01efcb0df20f69a54cfce83a38e74eb3`;
  map `.dram0.bss` `0x1b598` (flat), `work_mem_int` `0x10000` unchanged.
  **Awaiting a device flash** (checks:
  before the first successful sync the AOD shows only the time with no `x/y`;
  after a successful sync it shows time + `x/y`; the `np-0.2.10` regression
  items).

## np-0.2.12 — fix(wifi): roam across remembered networks without re-provisioning

- Feedback: moving between several provisioned Wi-Fi networks did not reconnect
  automatically to the network at the new location. The link-down ladder only
  re-kicked `esp_wifi_connect()`, which reselected the SSID that had just
  dropped — and a non-OS driver stuck in a connect loop cannot even be
  interrupted by a scan; a full stack rebuild is the only way back to a
  scan-capable idle.
- The selection phase now runs on the ladder itself. When more than one network
  is remembered, the first down pass (3 s) and every 30 s probe run a scan
  selection first (`scan_pick`): the strongest remembered SSID on air wins; if
  no remembered network is visible, each remembered SSID is probed by a
  directed scan (`wifi_scan_ssid`) and the best responder wins, so the device
  moves on its own. A plain re-kick to the dropped SSID is no longer the
  fallback for a non-auth disconnect, and the REBUILD rungs of the ladder
  restart the stack, returning the radio to scan-capable idle so the
  selection phase can run at all.
- The AOD screen gains the battery icon (top right, same geometry as the list
  header) and a Tailscale mark above the `x/y` count. The mark stays hidden
  until the host list has been read (same `has_count` gate). New glyphs: none
  (the battery icon redraws the geometry already drawn in the list header).
- `--static` is green (`Repository checks` 434 files / `Host tests` PASS).
  Image `build/FoloToy-AI-Passport-full-np-0.2.12.bin`, 2067696 bytes
  (+912), sha256
  `168563c04f93c0f7e560d3cf3c2b2969a57839ad14a05428a75c0c6da1028aea`; map
  `.dram0.bss` `0x1b5c8` (+0x30), `work_mem_int` `0x10000` unchanged.
  Embedded version check: PASS (np-0.2.12).
  **Awaiting a device flash** (checks:
  moving between covered networks switches automatically without
  re-provisioning and without wiping the SoftAP credentials; the AOD battery
  icon matches the header battery; the `np-0.2.11` regression items).

## np-0.2.13 — fix(prov): the credentials page opens even when the link lands late

- Feedback on the np-0.2.12 flash: after successful Wi-Fi provisioning (the
  SoftAP first-time entry) the device neither jumped to the Tailscale-credentials
  LAN page nor back to the list — it stayed on the list, whose host area is empty
  without credentials.
- Root cause: the roaming work made `wifi_connect_and_run()` (the boot connect
  and the manual retry) treat the selection scan as a gate. `wifi_pick_known()`
  returning false aborted the connect with "no remembered Wi-Fi nearby". But
  the selection scan is only an optimisation, and the first scan right after the
  AP→STA switch is routinely rejected by the driver while the radio settles —
  which hits exactly the multi-network device the roaming feature creates. The
  boot connect then failed into `ST_WIFI_RETRY`, and neither the old nor the new
  `ST_WIFI_RETRY` self-heal ever reopened the credentials page, so the user was
  parked on the empty list.
- `wifi_connect_and_run()` no longer aborts on a failed selection: the scan
  still runs (so it can swap in the stronger remembered network), but a blocked
  scan falls through and connects to the active pairing anyway. The scan may
  "pick a better one"; it is not a gate on connecting.
- A new `maybe_prompt_credentials()` is the single rule for "open the
  pairing-code page when no Tailscale credential is stored": `enter_boot` uses
  it, and the `ST_WIFI_RETRY` → `ST_RUN` self-heal now calls it too, so a link
  that lands after the boot connect (late DHCP, a retry ladder that finally
  recovers) still opens the credentials LAN page instead of leaving an empty
  list on screen.
- `--static` is green (`Repository checks` 434 files / `Host tests` PASS;
  inventory regenerated to 999 codepoints from the new comment glyphs). Image
  `build/FoloToy-AI-Passport-full-np-0.2.13.bin`, 2067664 bytes
  (-32 vs np-0.2.12), sha256
  `f842eef5b4abb2786f0d9149be0f0407c7a1a7e3b96fa2fae9d04fce7b35d326`; map
  `.dram0.bss` `0x1b5c8` (unchanged), `work_mem_int` `0x10000` unchanged.
  Embedded version check: PASS (np-0.2.13). **Flashed, issue remained:** the
  credentials page still did not open on device; the one-shot boot/self-heal
  attempts missed the real failure mode — fixed in np-0.2.14 below.

## np-0.2.14 — fix(prov): the credentials page retries until it actually opens

- Feedback on the np-0.2.13 flash: the device still did not jump to the
  Tailscale-credentials LAN page after Wi-Fi came up; it ran time sync and sat
  on an empty host list. The previous fix removed the scan-failure gate, but
  the auto-open was still a single shot — only `enter_boot` and the
  `ST_WIFI_RETRY` self-heal called it, the manual OK retry did not, and any
  transient failure of the LAN-service start left the user on the empty list
  for the whole boot.
- The credentials page now opens through four equivalent paths sharing one rule
  (`maybe_prompt_credentials()`): `enter_boot`, the link-recovery self-heal,
  the manual OK retry, and a new throttled `ensure_credentials_prompt()` in the
  `ST_RUN` tick that retries every 3 s while there are no stored Tailscale
  credentials, the prompt was not dismissed, and the link is up. A failed
  LAN-service start now also frees mDNS in `app_prov_start()`, so the retry can
  succeed instead of hitting an already-initialised mDNS forever.
- `--static` is green (`Repository checks` 434 files / `Host tests` PASS;
  inventory regenerated to 1000 codepoints from the new comment glyphs). Image
  `build/FoloToy-AI-Passport-full-np-0.2.14.bin`, 2067808 bytes (+144 vs
  np-0.2.13), sha256
  `7af1b3328f8670898b90456db689120f3d003c75a106dc99046c60eb8dd7ab72`; map
  `.dram0.bss` `0x1b5c8` (unchanged), `work_mem_int` `0x10000` unchanged.
  Embedded version check: PASS (np-0.2.14). **Awaiting a device flash**
  (check: with no Tailscale credentials stored, the LAN page opens promptly
  after Wi-Fi is up, whether the device boots clean, auto-recovers from the
  retry page, or the user retries manually).

## np-0.2.15 — feat(aod): remember the pre-AOD brightness and restore it on wake

- The always-on display already fixed its backlight at 20%. This version makes
  the transition explicit: `enter_screen_off()` first captures the configured
  brightness (`s_brightness_saved = app_cfg_brightness()`), then enters the
  AOD at 20% (or full off when AOD is disabled). Waking restores
  `s_brightness_saved`, so the 20% AOD level never leaks into the normal
  display and the pre-AOD brightness is always what comes back, regardless of
  what the device booted at.
- `--static` is green (`Repository checks` 434 files / `Host tests` PASS).
  Image `build/FoloToy-AI-Passport-full-np-0.2.15.bin`, 2067824 bytes (+16 vs
  np-0.2.14), sha256
  `52c4dc6e9bf6b1b772248f1762d627ba9c4c645f676324208b2cb95622c14679`; map
  `.dram0.bss` `0x1b5c8` (unchanged), `work_mem_int` `0x10000` unchanged.
  Embedded version check: PASS (np-0.2.15). **Awaiting a device flash**
  (checks: AOD stays at 20% and waking returns to the configured brightness;
  the np-0.2.14 credentials-jump re-test).

## np-0.2.16 — feat(settings): the brightness sub-page is a live adjustment bar

- The brightness sub-page stopped being "one large percentage label" and became
  a real-time-preview **adjustment bar**: a rounded track whose filled portion
  grows left-to-right with the level, drawn exactly like the battery gauge
  (track `UI_BAR_OFF`, fill `UI_ACCENT`, the fill inset one track radius on the
  left so its square leading edge never pokes out of the rounded rail). The
  big `NN%` label stays above it. UP/DOWN still step by 5%, and the bar fill
  and the backlight update together so the preview matches what the screen
  actually shows.
- Only the on-screen control moved for this page. The RAM-only /
  write-NVS-on-exit rule is unchanged, and the np-0.2.15 AOD brightness
  restore is untouched.
- `main/np_inventory.c` regenerated to **1002** codepoints (was 1000); because
  the source-file list passed to `extract_chars.py` had dropped
  `tests/test_np_errfmt.c` and `tests/test_np_account.c`, the checked set is now
  taken straight from `--static` so no glyph can be missed again. No new
  UI-rendered glyph is needed — the comment characters already exist in the
  subset, so the font only grew.
- `--static` PASS (`Repository checks` 434 files / `Host tests` PASS;
  inventory consistent at 1002). Image
  `build/FoloToy-AI-Passport-full-np-0.2.16.bin`, 2072288 bytes (+4464 vs
  np-0.2.15), sha256
  `f049fd9e06cb55f17f477e5a9f3a3e8ae40dec9221eed0869fdff469882bedea`; app ELF
  `8d7fe23749f715552f07dc3baab885f915839e19a6045e00a9eec7b065dee9b4`; map
  `.dram0.bss` `0x1b5d8` (was 0x1b5c8 — only the two new static bar pointers),
  `work_mem_int` `0x10000` unchanged. Firmware version gate: PASS (np-0.2.16).
  **Awaiting a device flash** (check: set brightness to a level such as 60%,
  open the brightness sub-page, press UP/DOWN — the fill grows/shrinks in 5%
  steps and the backlight responds; leave and wake — the restored level is the
  remembered one, not the 20% AOD level).

## Standing deferrals (as of np-0.2.16)

- SNTP mechanism rework: the three fire-and-forget `esp_sntp_*` calls, the lwIP
  assertion behind `sntp_setoperatingmode`, and `CONFIG_LWIP_SNTP_MAX_SERVERS=2`
  silently voiding the third pool name.
- Memory round step two: pool sized from `lv_max + ~30%`, Wi-Fi radio buffers,
  `BSP_LVGL_DRAW_BUFFER_LINES`, BLE and demo sources, `s_html` — waits on the
  np-0.1.26 measurements.
- `CONFIG_FREERTOS_TIMER_TASK_PRIORITY` (moves the LVGL tick with it) — needs
  device data, not a guess.
- On-screen QR for the hotspot; self-signed HTTPS on 443 — declined by the user.
- AP channel auto-selection and an async (non-blocking) scan engine — noted in
  np-0.2.2.
- The np-0.2.0 / np-0.2.1 / np-0.2.2 / np-0.2.3 / np-0.2.4 / np-0.2.5 /
  np-0.2.6 / np-0.2.7 / np-0.2.8 / np-0.2.9 / np-0.2.10 / np-0.2.11 device-test
  items are all pending the user's flash authorisation. np-0.2.12 and np-0.2.13
  were flashed: the roaming and AOD checks ran on device, and the
  credentials-jump regression was fixed in np-0.2.14, which was then flashed
  and confirmed to open the credentials LAN page correctly. np-0.2.15 (AOD
  brightness handling) and np-0.2.16 (the brightness adjustment bar) await a
  device flash.

## Repository rename (2026-11-09)

- This feature branch lives in the fork repository
  `billyoungs/ai-passport-tailscale-panel`, renamed to
  `billyoungs/folotoy-ai-passport-tailscale-panel`. The upstream baseline
  stays `FoloToy/ai-passport`; no product, firmware or image name changes
  with the rename.

## Related

- [tailscale-panel.md](tailscale-panel.md) — current specification.
