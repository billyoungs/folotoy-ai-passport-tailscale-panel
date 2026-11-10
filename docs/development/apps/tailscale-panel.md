<p align="right">
  <a href="tailscale-panel.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Network Panel (Tailscale monitor app)

An application built on the AI Passport BSP: it polls the Tailscale Admin API
and shows tailnet device status on the 240 × 320 screen. Credentials are
entered through a temporary web page served by the device — no serial tooling
and no cloud relay.

## Screens

| View | Keys |
| --- | --- |
| Device list (OS glyph + online dot + name; date on the left of the header, time in the middle, Wi-Fi and battery glyphs on the right) | Up/Down select, OK opens detail; OK long-press opens system settings; with an empty list OK opens the status view |
| Device detail (state line plus ten rows: address, OS, client, owner, tags, key, last online, joined, relay, routes; the rows scroll vertically when they do not fit on one screen) | Up/Down scroll and stop at the top or bottom; OK opens status; OK long-press returns to the list |
| Tailscale monitor panel menu (head: the official mark, the product name and the version; items: sync now, LAN configuration, start the guide hotspot, clear credentials; the body carries the account and login state and the poll cadence) | Up/Down pick an item, OK runs it; OK long-press back |
| System settings (title plus four rows: screen brightness / auto-sleep timeout / AOD toggle / network configuration, current value on the right; sub-pages are brightness stepping, the sleep-tier list, the AOD switch, and the four provisioning actions) | Up/Down select, OK enters a sub-page, toggles or runs an action; inside a sub-page Up/Down adjusts or selects and OK confirms; OK long-press returns to the list and saves |

OK long-press on any sub-page returns to the list; on the list itself that same
press opens system settings. The left cell of
the header is the date, written `M/D` with no zero padding; while the clock is
not synced the same cell says *waiting for time sync* instead, and while Wi-Fi
is down it says *no network* — with no link SNTP would have no result either,
and the screen has to lead with the thing the user can act on, so *no network*
holds the higher priority. The time is a cell of its own: the label box is
placed around the screen midline at 120 px and the text is centred inside the
box, so `HH:MM` really is aligned to the centre of the screen rather than
"about middle by eye"; while the clock is unsynced the cell shows `--:--` and
does not invent a time. Date and time are therefore two functions
(`np_time_format_md()` / `np_time_format_hm()`); the detail view, which wants
the day and the hour in one glance, keeps the merged `np_time_format_mdhm()`
(`MM-DD HH:MM`).

The two glyphs on the right share one visual band: clamped vertically to
14..26, midline 20, exactly on the ink midline of the header text. The Wi-Fi mark is
three concentric arcs (225..315 degrees, passing straight over the top) plus a
dot, drawn as only the upper half of the fan, so its visible extent runs from
10.5 px above the arc centre to 2 px below it — a band 12.5 px wide whose
midline sits 4.25 px above the centre, which solves backwards to an arc centre
at y=24. The battery is a solid rectangle 12 px tall whose shell top edge sits
directly on the band's upper edge. Through `np-0.1.21` the battery sat
on the text baseline at y=19, so the whole glyph was half a band lower than
the Wi-Fi. Icons have to move as one band; shifting only one puts them out of
line again. The arcs light 1-3 segments by signal strength (the buckets live
in `np_signal_bars`); the dBm number left the screen in `np-0.1.16` — it never
was a quantity the panel could decide on. The gauge replaced the percentage
number in `np-0.1.18`: a 24 × 12 shell plus a tip, filled in proportion to the
charge — green above 45 %, blue down to 21 %, red at or below 20 %. What you
had to read a digit to learn, the shape reports at a glance, and it takes less
width. When the state of charge cannot be read the shell is drawn dark and
empty, which is not the same as claiming `0 %`. The app task samples the gauge
roughly every 10 s, and the list only picks the value up when it repaints, so
since `np-0.2.6` a changed reading posts one render event — error states (the
retry page among them) have no periodic repaint, and without that nudge the
icon would stay frozen on the boot frame. The first sample moved ahead of the
boot flow, so the very first render shows the real reading. The fill and the
tip are
siblings of the shell, not children of it: the shell's rounded corners clip
whatever is drawn inside them, so a child fill would lose its own corners and
its bottom row of pixels.

The status view is named after the product, not after the word "status". Its
head is three things: the Tailscale mark in a 24 px box at x=12, the product
name — three Chinese characters meaning "monitor panel", preceded by the word
"Tailscale" — at 20 px starting at x=42, and the version at 14 px below it.
Name and version are two lines because one line does
not exist at that size — 240 px is the whole screen — and the five body rows
moved down to 48/66/84/102/120 to make room, an 18 px pitch against the 14 px
font's 17 px line height. The displayed version drops the `np-` prefix; the
embedded string keeps it, because `tools/bump_version.sh` and the gate's
`check_embedded_version` both read that one.

The mark itself is nine discs, which is exactly what the official file is: the
geometry is transcribed into the pure table `main/np_logo.c` from
`assets/icons/tailscale-favicon.svg`, a verbatim copy of what
`https://tailscale.com/favicon.svg` serves (the file header records the source
and the date; Tailscale and its mark are a trademark of Tailscale Inc. and are
used here only to name the service being monitored). `tests/test_logo_contract.py`
re-derives the table from the SVG — centres, radius, which group is 40 %
opaque — so the two cannot drift apart, and `tests/test_np_logo.c` pins the
table's own invariants. It is drawn with `lv_obj` discs and a circle radius, the
way the Wi-Fi dot is drawn, rather than as a pixel-art glyph in the icon font:
that font works at 12 px, and its own generator warns that compositing circles
at that size turns them into mud.

Rows are prefixed with an operating-system glyph and lead with a status dot:
10 px with a constant 2 px border, filled when the machine is reachable and
hollow when it is not, so online and offline differ by shape as well as colour
(colour alone is the first thing a dim or angled screen loses).

Since `np-0.1.27` the dot answers for one more thing than the server said. The
`online` flag is decided while the JSON is parsed — `np_device_is_online()` and
the 15-minute `NP_SEEN_GRACE_S` are the tailnet's own facts — so while the
device had no address and polling was gated off, the old snapshot stayed on
screen claiming its machines were online while the footer admitted the link was
down. The dot was contradicting the sentence in the same 240 px. Both list and
detail now take a `fresh` flag next to the snapshot, computed by the pure
`np_snapshot_fresh(now_ms, fetched_ms, link_up, retry_delay_s)`: no snapshot
ever taken is stale, a down link is stale *immediately*, and an up link is
fresh for `60 s + 2 × the current backoff`, so a legitimate 3600 s wait is not
mistaken for a stall. The app stamps `s_snapshot_ms` at the moment the double
buffer swaps, inside the LVGL lock, rather than guessing at render time. What
changes on screen is only how that confidence is expressed: a stale snapshot
draws hollow dots, mutes the row names, and the detail's state line reads "last
seen online" instead of "online". The server's own numbers are untouched —
`np_seen_disconnect_at()`, `last_seen` and the *seen* row stay exactly as the
control plane reported them, because freshness is a local confidence and not a
new fact about the machine.

The row cards and their children are all aligned to the 12-228 band, the same
width as the two dividers and the footer. The theme wraps every plain container
in card padding, and unless that is cleared the whole row card is pushed right
until its edge hangs off the screen — the `np-0.1.18` report of the hostname
box extending past the right edge was exactly this — so the offsets are measured
from the card's content area, inside the 2 px border, and live in the `ROW_*`
constants in `netpanel_ui.c`. The name is pinned to one line of height with an
ellipsis closing any overflow (`ROW_NAME_W`): the card height is fixed and the
text must not push it open. Only six rows fit on the screen, so the selected
row is scrolled into view once the cursor moves past them — otherwise it would
jump on a row nobody can see. The detail view's scrolling and wrapping are
covered in *Per-machine extra fields*.

The footer has two channels and they must not be confused. A real fault prints
an `error:` prefix plus the cause, in red; a wait-state — clock not synced,
address not obtained, credentials not configured — prints its own text in the
accent colour with no error
prefix. Before `np-0.1.17` both went through the same buffer, so a panel
that was merely still starting up read as an error and sent the user looking
for a fault that did not exist. The waiting channel later became a registered
`note_kind_t` (link down / waiting for the clock / clock unresponsive / not
waiting), and `np_list_t.total` added a further case: "K more not shown". See
*Polling behavior*. Both remaining tiers come out of one pure function,
`np_menu_footer()`: the overflow text wins over the key hints because the row
cannot hold both, and the normal text names the menu entry — `np-0.1.24` moved
the way in from "OK opens detail" alone to "OK detail, long-press menu", and
`np-0.2.6` added the data moment: "long-press menu, sync HH:MM" (the wall-clock
time the snapshot was swapped in; with no successful sync yet or an unsynced
clock it falls back to "OK detail, long-press menu") — online is a fact about
the fetch moment, and without a timestamp the screen cannot say how fresh it is.

While Wi-Fi is down the list screen
doubles as the retry view: OK retries the connection, OK long-press goes back to
the bootstrap hotspot so a mistyped password can be corrected without power
cycling. Remembered networks are no longer just the last one: since `np-0.2.6`
`app_cfg` keeps up to six of them, most-recently-used first (the arithmetic
lives in pure, host-testable `np_wifi_store.c`). Before connecting — at boot or
on a deliberate retry — the panel scans: with several remembered networks in
range it joins the strongest; with none it says so right away instead of
burning the full connect timeout. A cold first boot occasionally drops the
first association on a 4-way
handshake timeout; `app_wifi.c` now retries an auth-class disconnect up to three
times inside one connect attempt before it is allowed to say the password is
wrong, because "password error" on the first boot after power-on was usually a
radio settling problem, not a wrong credential. When the last sync failed, the
status view now leads with the layer that broke, immediately followed by that
layer's raw numbers, and adds a line for the clock and the backoff; the lines
that normally show the polling interval make way, but the last two rows
(account, login state) are present in all three cases.

The last two rows of the status view answer "whose account and which network
did this machine log into". The account is the tailnet name from the config:
a Tailscale tailnet name is the account name you log in with (an e-mail for a
personal account, `company.github` / `company` style for an organisation), and
the panel has no — and adds no — extra endpoint to ask for an "account display
name"; the control-plane top-level fields other than `devices` are dropped
across the board now, and credential ownership is not something the device
should guess. With no tailnet configured the request goes through that `-`
(the key's own default tailnet), and the cell says "unspecified (key default
tailnet)". The login state is classified from the results that actually
happened this boot, and there are only four: no credentials submitted = not
configured; a credential but no success yet = waiting for the first sync;
`/devices` returned 200 = signed in; a 401/403 rejection = rejected,
reconfiguration needed. The order of the checks matters: a rejection must
outrank "has succeeded" — after a key swap or a remote un-approval, an old
success must not still be reported as "signed in"; resubmitting credentials
and clearing credentials both reset this record. The classification itself is
the pure function `np_login_classify()` and the wording is `np_login_text()`
(the account line is built by `np_account_text()`), and
`tests/test_np_account.c` pins those three priority levels and the length
caps of the four texts.

## Link recovery

Opening *Network config* used to cost the connection: the page ran
`/api/scan` as soon as it loaded, an all-channel active scan borrows the only
radio, and the driver's one-shot reconnect fires while that scan is still
running, so the request gets swallowed. Nothing then re-arms the STA interface
— the driver does not emit a second disconnect event, and the panel had no
reason of its own to try again — so the only way back was to re-provision
through the bootstrap hotspot. Three things changed:

- The page no longer scans on load in any mode; *Rescan* is a button press.
  LAN mode had already dropped the on-load scan; `np-0.2.2` removed it from
  the bootstrap hotspot as well, after field reports of "the correct password
  will not connect" and "connected but the page never opens": an on-load scan
  borrows the radio exactly while the phone is associating or waiting for the
  first screenful, and every request queued behind it times out. Both modes
  now share one placeholder — "press *Rescan* to list nearby networks" —
  because "scanning…" on a page that is not scanning reads as a stalled
  request.
- `app_wifi_scan()` records whether the link was up before it swept, and
  `heal_after_scan()` re-kicks the connection immediately afterwards if it was
  not. That catches the common case without waiting for the ladder.
- Everything else is covered by `app_wifi_reconcile()`, called from the poll
  task, which walks the backoff ladder in the pure module `main/np_link.c`:
  first try 3 s after the disconnect (the driver has had its own chance), then
  3, 6, 12, 24, capped at 30 s. At the 4th try it stops calling `esp_wifi_connect()`
  into a stuck state and issues a disconnect first — while the driver believes
  it is connecting, the connect request is dropped on the floor. At the 8th try
  it rebuilds the stack (`app_wifi_stop()` + `app_wifi_init()`), because after
  that many failed rounds the event-handle-to-event-group mapping is no longer
  something to trust; that round is what ends the "must re-provision" dead end.
  A link that associates but never gets an address is handled separately:
  DHCP does not retry on its own, so after twice the IP timeout the panel
  disconnects to make the whole connect path run again.

The ladder always re-fires the one active network from the configuration — and
once the device has been carried to another environment, that network is no
longer on the air, so kicking it can never succeed. A roaming layer therefore
hangs off the ladder's kick points: at most once every 30 s, right before the
kick, a selection scan runs and promotes the strongest remembered network to
the active pair. That is the `np-0.2.7` design; the culprit is the scan's
radio window. Once the STA is inside the driver's connection loop for a network
that is no longer on the air, the loop never ends and every
`esp_wifi_scan_start` during it is refused with `ESP_ERR_WIFI_STATE` — so the
old selection scans all came back empty and the panel never switched while the
user walked between two known WiFis (fixed in `np-0.2.12`):

*  A refused selection scan is answered by rebuilding the stack idle
  (`app_wifi_restart()`): the app cannot interrupt the driver's loop, and the
  only reliable exit is tearing it down — a fresh stack has never requested a
  connection, so the loop does not exist and scanning succeeds. Rebuilt, the
  scan runs again in the same step; the ladder's rebuild tier likewise
  rebuilds, re-arms the staircase, and forces a selection before kicking.
  Before, the rebuild tier kicked the stale network straight away, closing the
  window again before the next attempt — the loop repeated forever.
*  The disconnect handler no longer calls `esp_wifi_connect()` for the
  non-auth reasons: the driver gives itself one internal retry when the link
  drops, and that retry cannot finish for a network that is no longer in
  range — our extra kick restarted the loop and stole the radio window the
  very next selection scan needed. The ladder's 3 s step covers the same-network case.
*  A full scan returns only the strongest `NP_SCAN_KEEP` (12) entries; dense
  surrounding APs push a weak remembered network off the list, and the old
  code read "not in the list" as "not nearby". A remembered network missing
  from the list now gets a directed scan of its own (SSID-filtered, tens of
  milliseconds), and the strongest promoted.
*  Manual retry goes through the same selection path: OK on the retry page
  rebuilds the stack idle first, then scans, selects and connects.

The swap lives in memory only; persistence still waits for the next save, and
the boot path re-scans and re-picks anyway. With a single memory there is
nothing worth scanning: the ladder kicks the remembered one directly.

The ladder resets on a successful association; repeat disconnect events do not
pull it back to the first step, which would otherwise spin at 3 s forever
during a long scan. `tests/test_np_link.c` pins the timings, both escalation
points, and that reset. When the ladder brings the link back, DHCP usually
hands out a new lease along the way, so the address shown on the pairing-code
page cannot be a snapshot of the moment the page was opened; the fix for that
side effect is in *Provisioning (two modes, one page)*.

`np-0.1.27` closed four holes in this section, all of them reported as "the
buttons freeze for a while after I open *LAN configuration*":

- **The ladder is the only periodic reconnecter.** The app task used to call a
  blocking `wifi_connect_and_run()` every 30 s while it sat in
  `ST_WIFI_RETRY`, and that task is the sole consumer of the key queue — so
  every 30 s the buttons were frozen for up to ~28 s, on a timer, for as long
  as the link stayed down. The rung now only checks `app_wifi_has_ip()` and,
  when the address comes back, re-inits polling and the clock, restarts SNTP,
  and returns to `ST_RUN`. A *deliberate* retry — OK on the retry page — still
  runs the blocking path, because there the wait is the answer to a request.
- **A healthy link is no longer torn down to prove it is healthy.**
  `connect_blocking()` used to call `esp_wifi_disconnect()` unconditionally.
  It now short-circuits when the interface is associated, holds an IPv4 and the
  current SSID equals the configured one (`link_matches_config()`); the
  disconnect stays for the case it was written for, a driver stuck in
  "connecting" that swallows a new connect request.
- **Scan and reconnect do not fight over the one radio.** ESP-IDF documents
  that `esp_wifi_connect()` during a scan aborts the scan and returns
  `ESP_ERR_WIFI_STATE` (and 5.5.3 has no `esp_wifi_scan_abort()`, so the
  reverse is not available either). `app_wifi_scan()` now publishes
  `app_wifi_scan_busy()`, `app_wifi_reconcile()` returns while that is set, and
  the existing `heal_after_scan()` makes the missed rung immediately after —
  the ladder is at most one 500 ms tick late instead of killing the scan the
  user just pressed *Rescan* for.
- **A rebuild step yields to a live LAN session.** `ACT_REBUILD` runs
  `app_wifi_stop()` + `app_wifi_init()`, which tears down the netif the config
  page's httpd and mDNS are bound to. `app_wifi_reconcile()` takes
  `allow_rebuild`, and the app passes false while a LAN session is open, so the
  top rung degrades to a forget-and-kick. Bootstrap-hotspot mode needs no
  protection: `app_wifi_stop()` already cleared `s_started`.

What the screen says while all this is happening is now part of the recovery,
not an afterthought: the footer reads "Wi-Fi is down, reconnecting, next try in
N s" from `app_wifi_retry_countdown_ms()`, so a wait that is being managed does
not look like a hang.

## Provisioning (two modes, one page)

- **First boot / no Wi-Fi:** the device starts an ephemeral hotspot
  `Passport-xxxx`; the screen prints the SSID, a "no password needed" note,
  and `http://192.168.4.1`. Since `np-0.2.2` the
  hotspot is open — nothing to type — which removes the failure mode where a
  phone auto-retries a stale saved profile for a same-named hotspot and the
  WPA2 handshake fails even though the user typed the current password
  correctly. Joining is free and so are the page writes: the hotspot stage
  carries no pairing code at all (`np-0.2.3` took the gate back out of the
  hotspot page; the code belongs to the LAN credential stage only). The
  captive portal opens the page
  by itself — the gateway serves it at `/`, so no path has to be typed. Press
  *Rescan* to fill the *nearby networks* dropdown (nothing scans on page load
  any more; see *Link recovery*), or type a name, enter the network password,
  and press *Save and connect*: one button, one request, and the session
  closes. Until Wi-Fi is saved successfully, a hotspot restart reuses the same
  SSID — before `np-0.1.6` every restart drew a new random pair, which
  silently pulled the network the phone was connected to out from under it.
  Since `np-0.2.1` the page carries no Tailscale fields at all — not even
  collapsed: the hotspot step is Wi-Fi only, and the credentials are entered
  afterwards at the LAN address the device itself puts on screen once it is
  online (see the missing-credentials bullet below).
- **Day-to-day edits:** long-press OK on the list opens system settings, then
  Network configuration → *LAN configuration* (the status menu, reached via OK
  on a detail page or on an empty list, also works). The page
  is then reachable at `http://netpanel.local` (mDNS) while the device is on
  its LAN, and the screen also prints the gateway IP as a fallback for hosts
  without mDNS. *Start the guide hotspot* works at any moment: a session that
  is still open gets closed first, which before `np-0.1.24` was exactly the
  case that answered "hotspot failed to start". Every write requires the
  6-digit pairing code shown on screen,
  the session window is 10 minutes of idleness since the last request with a
  30-minute absolute cap from service start, and stored secrets are never
  echoed back — only a masked tail (last 4 characters). The credential *input*
  boxes are plain text, not dots: whoever types them is the owner, and masking
  a field they cannot read back only turns "did I typo this" into a guess. The
  Wi-Fi password field keeps its own show/hide checkbox. Here the Tailscale
  section starts open. *Save* keeps the session alive for Wi-Fi, polling and
  timezone edits, but a save that lands Tailscale credentials ends it and
  triggers an immediate sync, so the panel itself reports whether the
  credentials work.
- **Missing credentials:** once Wi-Fi is up with no Tailscale credentials
  stored, the device starts one LAN config session on its own and the screen
  shows the pairing-code page. That page states its purpose in plain words —
  "enter the Tailscale credentials from another device" — and shows the direct
  IP; `netpanel.local` left the device screen in `np-0.2.1` (too many hosts
  cannot resolve it) and stays on the web page header and mDNS only. Aborting
  with the OK long press, or letting that
  page idle out with nothing saved, suppresses the auto-start for the rest of
  the boot; *Network config* on the status page stays available.
  *Clear credentials* now ends on that same page rather than on a sentence: the
  old second line, "you can fill them in again on the config page", was a dead
  link, because without the address and the 6-digit pairing code on screen there
  is no way to reach that page. Clearing also un-suppresses the auto-start above
  — the device is back in the no-credentials state, so the "the user closed it"
  flag from the previous round no longer applies. With no IP, or when the
  service refuses to start, the status view says which of the two happened.
- **The address on the pairing-code page follows the link:** that line used to
  be a DHCP snapshot of the moment the page was opened, while the
  link-recovery ladder changes the lease underneath it — the old address
  became a dead link nobody owned, no click could open the page, and only
  re-entering the provisioning view restored it (the `np-0.1.18` device
  symptom). The app task now compares `app_wifi_ip_str()` against the four
  displayed lines once per second: the pure function `np_prompt_lan_update()`
  recomputes them and they are compared line by line, and the page repaints —
  with one log line — only when a line actually changed (an AP switch that
  kept the same address still counts as a change). The pairing code, the
  session and the hotspot are untouched; the bootstrap hotspot mode is
  excluded from this re-check, because its screen always shows the gateway
  address.
- **The checkbox should not take a whole line of its own:** the page CSS gives
  every `input` `width: 100%` (the text fields need to fill the row), so the
  show-password checkbox stretched across the whole line too and pushed its
  label onto the next row, where the two read as unrelated things (the
  `np-0.1.18` device report). `input[type=checkbox]` now has its own exception:
  it is not stretched and keeps a 6 px right margin, and its label takes
  `class=chk` with `display: flex; align-items: center`, so the box sits to the
  left of its text on one line. Both the SoftAP and LAN templates changed.
- **The screen speaks after the credentials are submitted:** in the branch where
  the credentials actually land, `handle_prov_done()` writes a "credentials
  saved, signing in" note on the cyan footer channel instead of leaving the
  picture frozen on the last provisioning frame saying nothing. The note stays
  until the first sync resolves: success withdraws it through `clear_status()`
  and a real fault overrides it in red, while a device that is genuinely stuck
  somewhere gets the more specific *waiting for time sync* or *the link
  dropped* line in its place. It deliberately does not register
  `s_link_note_shown` —
  that flag exists so the ready tick can retire a waiting line, and registering
  would clear this note while the request is still in flight, turning it into a
  flicker.
- **One task owns the session (since `np-0.1.27`).** A separate `prov_wd` task
  used to decide when the config server closes, and `app_prov_stop()` had to
  wait for it — two `vTaskDelay` loops, up to 1 s and 2.5 s, both running on the
  task that consumes key events. The page then felt frozen right after it
  opened, most visibly on a first boot with no credentials, where the automatic
  session made that wait happen nearly every time. The task is gone: the httpd
  handlers now only set a `volatile` request counter and a `volatile` close
  flag, and the app task polls `app_prov_take_close(now_us)` in the same 1 s
  block that already polls `/api/sync` requests. It stamps activity, applies the
  1.5 s browser grace after a *Save* (`np_session_request_close()` /
  `np_session_close_due()`, pure and host-tested: the grace does not cancel a
  newer request and the 30-minute cap beats the grace), and reports "close now"
  — the caller does the teardown. Because exactly one task starts and stops the
  session, the class of bug where a retiring watchdog tears down a session
  started after it no longer exists, and no generation counter was needed for
  it. Side effect: 3072 bytes of task stack plus its TCB come back to the heap.
- **The provisioning page says what its buttons do.** The pairing-code page
  accepts one gesture by design, the OK long press; UP/DOWN have nothing to move
  there. That was invisible, so an ordinary lag and "this page has no up/down"
  arrived as the same complaint. The fifth body row now states the gesture:
  "long-press OK to leave this page" for LAN mode, and "long-press OK restarts
  the hotspot" for the bootstrap hotspot, where leaving the page is not
  meaningful because there are no credentials to leave for. The wording is one
  shared constant per mode, used by both the open path and the address-refresh
  repaint.
- **No compiled-in Wi-Fi (since `np-0.2.0`):** the bench-only `NP_DEV_WIFI`
  fallback table was removed — `main/app_cfg.c` carries no hard-coded SSID or
  passphrase, and the `app_cfg_dev_wifi*()` accessors are gone with it. A device
  with no stored credentials boots straight into the bootstrap hotspot
  (`enter_boot()` calls `start_ap_prov()` when `app_cfg_has_wifi()` is false),
  and credentials provisioned through the form persist in NVS across reboots.
  A stored network that fails is retried as itself — the ladder reconnects to
  the same SSID, and that is the name the retry screen reports.

Both modes submit through one endpoint, `POST /api/apply`. Empty fields are
left untouched, unchanged values are not sent back at all, and credentials are
written only when one of the credential fields actually carries a value — so
pressing Save on a Wi-Fi-only form cannot wipe a stored token.

The scan endpoint (`GET /api/scan`) is the reason the bootstrap hotspot runs
`WIFI_MODE_APSTA` with an otherwise idle STA interface: the Wi-Fi driver
rejects `esp_wifi_scan_start()` in pure-AP mode. Results are deduplicated by
SSID (strongest signal wins), capped at 12 entries, and exclude the device's
own hotspot. The scan runs blocking inside the HTTP task, and one radio cannot
stay on the hotspot channel while it sweeps — expect the page (and, in LAN
mode, the connection itself) to stall for a second or two while it lists
networks. That stall is also why no mode scans on page load any more; a scan
runs only when *Rescan* is pressed (see *Link recovery*).

mDNS resolution comes from the managed `espressif/mdns` dependency declared in
`main/idf_component.yml`. ESP-IDF 5.5.3 ships no captive-portal DNS helper, so
the hotspot's address hijack is implemented in-tree by `main/np_dns_server.c`
(adapted from the Espressif `dns_server` example, CC0); if it fails to start,
the log says so and the page is still reachable at the hotspot gateway address.
Any other path a portal probe asks for (`/generate_204`, `/hotspot-detect.html`,
…) is answered with a redirect to `/`, which now serves the page directly.

The page still speaks plain HTTP on port 80: a phone that upgrades the bare
gateway IP to `https://` will not connect, so the screen text points at the
address as typed and the portal redirect is what usually saves the day. Serving
HTTPS would mean a self-signed certificate the browser then warns about — not
implemented.

Credentials live in NVS namespace `np`. Since `np-0.2.6` the Wi-Fi credentials
are multiple: `wn` counts the remembered networks and `w0sid`/`w0psk` through
`w5sid`/`w5psk` store them one per slot, most-recently-used first; the active
pair is still mirrored in the `ssid`/`psk` keys so an older firmware (or a
downgraded image) can still find it. Devices upgraded from the single-pair
firmware carry their old pair into the multi-network memory on first load and
persist it under the `w*` keys on the next save. Clearing them is a two-press
confirm on the status menu — the first press only rewrites the footer hint, so
the cursor stays on that item — or `/api/clear` on the page.

## Tailscale credential options

1. **API access token** (`tskey-…`): personal key, simplest; validity up to
   90 days.
2. **OAuth client** (`client_id` + `client_secret`, read-only scope): the
   device exchanges a short-lived JWT (~1 h) automatically.

The device requests `?fields=default`, which returns hostname/name/addresses/
os/clientVersion/lastSeen/expires/authorized/isExternal/updateAvailable/user.
The sensitive `machineKey`/`nodeKey` fields are never parsed or stored. TLS
trusts the pinned ISRG Root X1 CA. What `default` leaves out — distro, subnet
routes, relay, SSH — comes from a second per-device call, see *Per-machine
extra fields*.

Online state is not the presence of `lastSeen`. A node that is currently
connected carries Go's zero time in that field — `0001-01-01T00:00:00Z` — which
is a syntactically valid RFC 3339 stamp, so accepting it as a disconnect moment
made a machine that had been online for weeks read as offline, with the state
line computing an ago-string of hundreds of thousands of days. One rule now
lives in `np_seen_disconnect_at()` (`main/np_model.c`) and the parser only
supplies it a raw value: no field, an unparseable value, anything before
2000-01-01, a board clock that has not been set, or a stamp no more than a
quarter of an hour older than `now` all mean connected; only a genuinely older
stamp is an offline moment, and
that is what the detail view prints as `MM-DD HH:MM` instead of pointing at
another label. `expires` gets the same floor test for the same reason — a node
with key expiry disabled also reports the zero time. `tests/test_np_model.c`
pins each of those cases and `tests/test_np_time.c` pins that the zero time
parses, so the trap stays visible where it is set.

## Per-machine extra fields

`?fields=` takes `default` or `all`, nothing else — an explicit field list is
not supported, so there is no cheap way to ask for one extra value. Asking for
`all` on the whole list would multiply the body (a longer download, and a single
device slice would break through the 3 KiB per-device buffer), and most of the
extra fields are things only one screen ever looks at. So the list keeps polling
`default`, and a detail view that is actually open asks for `fields=all` about
that one device.

The request is a second `GET` for the device id, made by the poll task (the
task with the TLS-sized stack and the only one allowed to do HTTPS), answered
into a `np_detail_t` that is static, with the response buffer capped at 6 KiB,
and published to the screen under the LVGL mutex.
The UI task never blocks on the network: the detail page paints immediately
with `-` in the four extension rows and an accent-coloured
`fetching extra info` line, then repaints in place. Stepping to another machine
bumps a generation counter, and a response whose generation no longer matches
is discarded whole — including its request flag, so the machine now on screen
still gets its own fetch. The request is peeked, not taken, and only cleared
when the result is published: taking it first meant a lock timeout between the
two silently dropped the fetch, and clearing it on a generation mismatch would
have eaten the *new* request.

What the extra round buys, and the ten rows the detail view is now made of:

| Row | Source | Reading |
| --- | --- | --- |
| address | list | preferred IPv4 (`100.x`) |
| OS | list + extra | `os`, plus `distro` (name, version, code name) when the extra fetch lands — `linux` alone does not say it is Debian 12 |
| client | list | `clientVersion` |
| owner | list | user the node belongs to |
| tags | list | node tags; empty means `none` |
| key | list | `expires`, or `never` when `keyExpiryDisabled` |
| last online | list | offline: the `MM-DD HH:MM` disconnect moment; otherwise what the control plane itself said (`connected` / `disconnected`), because a connected node carries no `lastSeen` at all |
| joined | list | `created` — how long the machine has been on the tailnet |
| relay | extra | `DERP <region> <ms>ms · N endpoints`: the preferred relay and its measured latency, and how many candidate direct endpoints the client reported (`0` means it can only reach the tailnet through the relay) |
| routes | extra | up to two `enabledRoutes` CIDRs, `advertised …` when the node announces routes that are not enabled (usually an ACL that never approved them), `none` |

The state line under the title carries the flags that change what you would do
next: unauthorised, external, update available, ephemeral, and from the extra
fetch `SSH` (`sshEnabled`) and `NAT varies`
(`mappingVariesByDestIP`, which is why a direct connection keeps failing).

Ten rows plus the title, state line and footer hint do not fit one 320 px
screen, and the values often wrap, so the detail view is two layers: title,
state line, divider and footer hint stay fixed, and the ten rows live in a
container that scrolls only vertically. Up/Down scroll one row pitch (22 px) at
a time and simply stop at the top or bottom — leaving the page is a long-press
of OK and nothing else. Pressing the same direction again at the edge used to
exit, and that was withdrawn before anyone flashed it: mis-timing one key while
reading a middle row threw away the whole page, a price no single press
deserves. When the content does fit, Up/Down are plain no-ops and the only exit
is still the long press. The value labels no longer use `LONG_DOT`: under `LONG_DOT` LVGL wraps a long value
by itself but does not grow the object along with it, so a value folded into
two 34 px lines printed over the next row — that was the "sections overlap"
reported from the device. Values now wrap explicitly (`LONG_WRAP`) and the row
advance is measured with `lv_text_get_size()`, using the label's own font,
letter space and line space, so the next row's y steps by the measured height.
The late extra-info repaint redraws the whole screen, and the scroll offset is
saved and restored around it; the restore must call `lv_obj_update_layout()`
first, because while the child coordinates have not been computed the scroll
range reads as 0 and the restore silently drops back to the top.

Also available and deliberately left out: IPv6 addresses (about 1.9 KB of `.bss`
for an address this panel cannot do anything with), `posture*`,
`multipleConnections`, and `clientSupports` (the control plane does not fill it
in). Secrets are a different matter — `machineKey`, `nodeKey` and
`tailnetLockKey` are never parsed or stored, in either call.

One field is a loader rather than a display: `id` addresses the second
request. Tailscale device ids are decimal and run to 19-20 digits, so the
buffer that used to hold them (16 + terminator) truncated every one, and a
truncated id still produces a well-formed request that is guaranteed to 404.
`NP_ID_LEN` is 24 now, and `parse_device()` refuses an id that does not fit
instead of storing half of it.

## Polling behavior

Base interval (default 120 s) resumes after any success; failures double the
delay up to a configured cap (default 3600 s). Wi-Fi loss pauses polling and
reconnects resume it immediately. Authentication errors halt polling until
credentials are refreshed.

A failure is reported as two lines: which layer broke, and the raw numbers
behind it. `main/ts_api.c` records what the stack actually said — the HTTP
status when there was one, the esp-tls classification (`0x8001` name
resolution, `0x8002`/`0x8003`/`0x8004` socket and connect, `0x8006` connect
timeout, `0x8009` handshake timeout, `0x801A` mbedTLS returned an error), the
mbedTLS code, the certificate verification flags, the socket `errno`, the host
that failed, the free heap and largest free block at the moment of failure, and
the server's own `message` text — and `main/np_errfmt.c` reduces that to a
one-line conclusion plus a one-line detail string. It never blames the
credential unless the request reached HTTP and the server answered 401/403, and
it never claims the internet is unreachable once the handshake was reached,
because getting that far proves TCP to port 443 worked. Certificate flags are
decoded per bit (not yet valid, expired, name mismatch, untrusted root, no peer
certificate), and the mbedTLS code picks the handshake sub-cause: allocation
failure, timeout, connection reset or unexpected EOF, a malformed record, a
version mismatch, or a bad local configuration. The detail line prints the
mbedTLS code as `-0xXXXX`, the same shape esp-tls uses in its own
`mbedtls_ssl_handshake returned -0xXXXX` log line, so the screen and the serial
log are directly comparable. The board has no RTC, so a certificate check made
before SNTP answers always fails; requests therefore wait for the clock, and
while they wait the screen says so plainly (`np-0.1.21` ~ `np-0.1.23` below
records how that waiting was corrected several times against real-device
feedback). When the address itself is gone the line says "Wi-Fi is down, no IP
obtained" instead of pretending an HTTPS failure. `tests/test_np_errfmt.c`
pins every branch, including the rule that a truncated line is cut on a
codepoint boundary.

Those 20 s used to be measured only from the moment the first IP arrived at
boot, so by the time the user submits credentials the device has been online
for minutes and the grace window has long since expired — the clock gate is
then permanently open: the first login goes out with a 1970 clock and hits a
certificate whose validity has not started yet. Worse, the cause line is
assembled after the fact: if SNTP lands between the failed handshake and the
formatting, `clock_synced` reads as synced and the screen is left saying the
"certificate validity is wrong (was the clock synced?)" message — which on this
device means "beyond help, reboot it". That is exactly what the device kept
reporting on `np-0.1.18`: frequent, and cleared by a single reboot.
`np-0.1.21` changed three things together: the clock state used for attribution
became a snapshot taken at the moment the request is issued, `clock_rearm()`
started a fresh SNTP round and pushed the grace anchor to now, and the
"unsynced-clock certificate-date failure" test became the pure function
`np_errfmt_clock_wait()`, treated as a temporary state (no red fault line, no
backoff penalty, a cyan footer note, retry as soon as the window opened).

`np-0.1.23` removed that whole path in favour of **no grace period at all**: if
the clock is not synced, the request is never issued. The device feedback on
`np-0.1.22` was that the certificate message came back — because "wait 20 s,
then send anyway" is an escape hatch built to fail: on the first credential
submission the device has always been online longer than 20 s, so the gate was
open regardless, and retrying three times in place was simply doing the same
doomed thing three times while the red line kept saying "reboot it". There is
now exactly one rule, shared by the list and the detail HTTPS paths:

- The gate is "has IPv4" **and** "`time()` really moved past 1970". Both hold or
  nothing is sent, and the backoff ladder is frozen while waiting (a request
  that was never issued is not a failure — do not turn "still waiting" into the
  doubling sequence of "sync failed").
- The waiting cadence is the pure module `main/np_clock.c`
  (`tests/test_np_clock.c`): every `NP_CLOCK_RETRY_MS` = 30 s it re-arms SNTP.
  lwIP's default sync interval is ten minutes, so a first exchange lost to the
  configuration page's all-channel scan stealing the radio, or to a router that
  blocks UDP 123, would otherwise mean ten minutes of nothing. The server list
  also grew to three (`0/1/2.pool.ntp.org`) so one unreachable host does not
  sink the round.
- After `NP_CLOCK_PATIENCE_MS` = 120 s with no answer, the cyan footer changes
  from "waiting for time sync, SNTP has not answered" to "no time sync reply:
  the router may block UDP 123", said once per waiting round; re-arming keeps
  going underneath, so a user who unblocks the firewall mid-wait recovers
  without help. Which wait is being shown is registered as a `note_kind_t`
  (link down / waiting for clock / clock unresponsive / nothing waiting) and
  only repaints when that reason actually changes — the poll tick is 500 ms and
  repainting every tick turns one sentence into a flicker. Time spent with the
  link down does not count; patience restarts when the link comes back.
- The detail request is queued ahead of the full list but obeys the same gate,
  and its placeholder now says what it is waiting for ("extra information is
  waiting for time sync" / "... for the network") instead of leaving the user
  staring at a row of dashes.
- `np_errfmt_clock_wait()` was deleted with it: once the gate is strict, the
  premise "a request left with an unsynced clock" no longer exists, and keeping
  a branch for it is writing code for a case that cannot happen.

Memory is a cause worth naming precisely, and after `np-0.1.23` it should no
longer be triggered by how many hosts are in the tailnet. The board has no
PSRAM and `esp_http_client_open()` runs the TLS handshake on the same internal
heap the response buffer comes from: mbedTLS asks for one contiguous block the
size of its record buffer, so holding a fixed 24 KiB response buffer across the
handshake can strangle it — `np-0.1.13` on device reported `tls -0x7F00`
(`MBEDTLS_ERR_SSL_ALLOC_FAILED`) with `heap 30544 blk 17408`.
`http_exchange()` already allocates only after the handshake succeeded, starting
at 4 KiB and doubling towards a 24 KiB ceiling as far as the body actually
needs. That fixed the handshake but not the second step: the whole body was
still handed to cJSON, which builds a tree 2.5-4x the size of the text. Once the
tailnet grew, both steps overflowed at once and the screen read "not enough
memory for the response".

The device list now goes through `http_stream()` in `ts_api.c`: it reads 1 KiB
at a time and feeds `main/np_jstream.c`, a character-level slicing state machine
that cuts one top-level object at a time out of the `"devices"` array. Each
slice gets its own small `cJSON_Parse` tree, is filled into the snapshot by
`parse_device()`, and is freed immediately, so peak memory is decided by "the
largest single device" (a 3 KiB buffer) rather than by the whole tailnet — the
server can report 500 machines and the board still needs the same few kilobytes.
Past `NP_MAX_DEVICES` = 24 the slicer keeps counting but stops emitting. The
slicer only slices: field semantics stay in `parse_device()`, and what it must
get right on its own is braces and escaped quotes inside strings, plus a nested
`"devices"` key that is not the array. `tests/test_np_jstream.c` pins those,
together with "feeding byte-by-byte gives the same result as feeding the whole
body" (a real body arrives in pieces, so the state machine has to survive across
chunks), a value that happens to be the string `devices`, and the truncated case
where the incomplete device is dropped but still counted. A device too large for
its 3 KiB slice is skipped and only counted; the count goes into
`np_list_t.total` as it is.

The screen only has 24 rows, so what did not fit has to be said out loud:
`np_list_t.total` records how many the server actually returned, and when it is
larger than the number of rows shown the footer reads "N/M online, K more not
shown" (that line has no room left for the key hints, and the hidden count
matters more) rather than letting "24" be read as "there are only 24".
`http_exchange()` still serves the OAuth token exchange and the single-device
detail request, where the body has a bounded size; the allocate-after-handshake
and double-on-demand rules are unchanged there. The mbedTLS *protocol* knobs are
still untouched, and `np-0.1.26` is where that decision got its real reason:
`esp-tls` never calls `mbedtls_ssl_conf_max_frag_len()`, so the client sends no
`max_fragment_length` extension (RFC 6066) at all — mbedTLS 3.6 only writes that
extension when `conf->mfl_code` is not `NONE`. A server that was never asked to
fragment may legitimately send a 16 KiB record, so lowering
`MBEDTLS_SSL_IN_CONTENT_LEN` below 16384 buys RAM until the first such record
fails the handshake with `MBEDTLS_ERR_SSL_INVALID_RECORD` (`-0x7200`). What
changed instead is *when* those bytes are held: `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y`
makes mbedTLS read the record header, allocate exactly that length, and shrink
back to a 16-byte cache once the upper layer has consumed it, rather than owning
16 KiB RX + 4 KiB TX from `mbedtls_ssl_setup` until close. With no keep-alive,
every sync and every detail fetch is a fresh handshake, so that 20 KiB used to be
reserved for the whole connection including all the time it was merely waiting
for the peer. `MBEDTLS_SSL_KEEP_PEER_CERTIFICATE` stays on — it is what makes the
certificate flags on the detail line exist at all.

## Memory telemetry, and the Kconfig key that lied

`sdkconfig.defaults` carried `CONFIG_LV_MEM_SIZE_KILOBYTES=24` with a comment
claiming the LVGL pool was "an independent static area that does not share the
system heap". Both halves were wrong. LVGL 9.6 keeps that KB spelling as a
deprecated option (`default 0`, no conversion feeding anything); the key that is
actually consumed is `CONFIG_LV_MEM_SIZE`, whose component default is 65536 — so
the generated `sdkconfig` held both lines, the map showed `work_mem_int` at
65,536 bytes, and the image had been reserving 64 KiB while the configuration
claimed 24 KiB. And the pool is not separate memory: it is DRAM carved out of
what would otherwise be the internal heap — in the `np-0.1.25` map `.dram0.bss`
is 111,384 bytes and `work_mem_int` accounts for 65,536 of them (59%), and
`np-0.1.26` adds 40 bytes for the snapshot struct. On this no-PSRAM C3 that DRAM
is the same physical SRAM a TLS record buffer wants; the on-device failure record
said `heap=30544 blk=17408`, which is enough memory in total and not enough in
one piece. Same failure class as
`CONFIG_APP_PROJECT_VER` without `..._FROM_CONFIG`: an entry that parses, lands in
`sdkconfig`, and then does nothing.

Two guards, because the comment itself was part of the problem:

- `ESP_STATIC_ASSERT(CONFIG_LV_MEM_SIZE == NP_LV_POOL_BYTES, ...)` in
  `main/netpanel_app.c` — a future IDF or LVGL bump that changes the effective
  pool fails the build instead of quietly changing the memory budget.
- `check_memory_knobs()` in `tools/check_repo.py` — rejects the inert
  `CONFIG_LV_MEM_SIZE_KILOBYTES`, and requires `CONFIG_LV_MEM_SIZE` and
  `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y` to be written down rather than inherited.
  `tests/test_check_repo.py` covers both directions, including "commented out
  counts as absent".

A configuration line is only a claim; the delivered artefact is the proof, and the
tree you grep is not always the tree that was built. `tools/validate.sh` builds in
a fresh `mktemp -d` directory with `-D SDKCONFIG=` pointed inside it, so a stale
local `sdkconfig` (git-ignored) can still carry the old two-key pair while the
gate's tree carries the new one. For this round the checks are:

```sh
grep -A1 work_mem_int build/firmware/<sha>/*.map          # must stay 0x10000
grep -oE "esp_ssl_tls|esp_mbedtls_dynamic_impl" build/firmware/<sha>/*.map | sort -u
grep -o "__wrap_mbedtls_ssl_read" build/firmware/<sha>/*.map | head -1
```

The `np-0.1.26` archive satisfies all three: the pool is still 65,536 bytes (this
version deliberately did not shrink it), and the dynamic-buffer implementation plus
its `--wrap` entry points are linked in — the knob took effect instead of joining
`CONFIG_LV_MEM_SIZE_KILOBYTES` in silence.

`np-0.1.26` deliberately leaves the pool at 65536. A starved LVGL pool does not
crash: `lv_malloc()` fails, widgets stop painting, and the screen freezes — so a
guessed reduction trades a visible, diagnosable handshake failure for a silent
display failure. Measure first.

The numbers now reach three places, all existing channels:

- `main/np_mem.c` + `np_mem.h` is a pure formatter (`np_mem_format()`, covered by
  `tests/test_np_mem.c`) that writes one ASCII line — `heap= blk= dma=/=` plus
  `lv_free= lv_blk= lv_max= lv_pct= lv_frag=`. ASCII on purpose: telemetry must
  not push new glyphs into the font subset.
- the poll loop samples every 40 ticks (20 s, and the first tick at boot) and
  logs `mem ...` at INFO; collection lives in the app layer because
  `lv_mem_monitor()` walks the pool's control structures and must run under the
  LVGL lock, with a 200 ms budget — if the lock is not available the `lv_*` half
  is omitted rather than reported as zero.
- `fill_fail()` in `ts_api.c` ends its warning line with the same formatter plus
  the DMA figures, which is what distinguishes "not enough memory" from "not
  enough *contiguous* memory" on the exact attempt that failed. It does not
  report the pool: the API layer has no business taking the LVGL lock, and the
  app's own `mem ...` line carries the timestamp-correlated pool numbers.
- `GET /api/mem` in `main/app_prov.c` returns the last cached snapshot as JSON.
  Read-only, no credentials in it, and deliberately *not* `session_touch()` — an
  engineer tailing memory for hours must not hold a provisioning session (and
  with it `/api/apply`) open by reading.

How to read it: `lv_max` is a high-water mark since boot, not headroom, and you
cannot add `lv_free` to it to derive the pool size; `lv_blk` is the pool's
biggest single free block, which is the gate for the next widget; `dma_free`
covers the display path. `docs/reference/starsms007/lvgl-pool-budget-without-psram.md`
has the full reading discipline. What the measurements unlock is the second
step, in rough order of bytes:

| Lever | Current | Candidate | Internal DRAM | Cost |
| --- | --- | --- | --- | --- |
| `CONFIG_LV_MEM_SIZE` | 65536 | measured `lv_max` + ~30% (32768 / 24576) | up to ~40 KiB | too small freezes the display |
| Wi-Fi buffers | `STATIC_RX 10`, `DYNAMIC_RX 32`, `DYNAMIC_TX 32` | 6 / 16 / 16 | ~8-15 KiB | peak throughput |
| `BSP_LVGL_DRAW_BUFFER_LINES` | 40 | 24 | 9.6 KiB DMA | more flush slices, visible refresh |
| `CONFIG_BT_ENABLED` + `demo_*.c` | on / compiled | off / dropped from `SRCS`, files kept | coex tables; mostly flash | loses the template reference build |
| `s_html` page buffer | 8 KiB static | allocated for the session only | 8 KiB when idle | +8 KiB heap while a session runs |

## Chinese text pipeline

UI copy, comments included, is the single source of truth for the glyph
inventory: `assets/fonts/extract_chars.py` regenerates `main/np_inventory.c`
and `assets/fonts/netpanel_chars.txt`; `generate_netpanel_fonts.sh` builds the
Noto Sans SC subsets (lv_font_conv 1.5.3, 14/20 px). `tools/validate.sh
--static` fails if tracked sources and the inventory drift apart, dynamic
network text is filtered to the inventory (`np_text_sanitize`, unknown
codepoints render as `?`), and the UI logs any missing glyph at boot. See
[fonts README](../../../assets/fonts/README.md).

## Row icons: a pixel font of our own

The glyph in front of each name is not a font character and not an image
widget. The subset font comes from Noto Sans CJK, which has no penguin, and a
monochrome LVGL font could not draw a four-colour window flag anyway. An image
or canvas per row was the other option and it lost: six visible rows times an
extra object, plus LVGL's per-widget draw buffers, is internal heap the TLS
handshake and the response buffer need (see the memory notes above).

So the five marks are one callback `lv_font_t`:
`assets/icons/generate_os_icons.py` holds each glyph as an explicit 12-character
ASCII grid (`#` 255, `+` 150, `~` 90, `.` 0) and emits
`assets/icons/np_os_icons.c` as two const tables — codepoints and one byte per
pixel. The glyph bodies are the data; the script is only the check and the
serializer. The first version of that script *drew* the glyphs out of ellipse
and rectangle primitives, and at 12 px every one of them came out as an
unrecognisable blob — an apple whose bite landed outside the fruit, a window
whose bottom row was empty and asymmetric. Hand-placed pixels with a length and
alphabet check on emit is the honest form of this asset, and
`--preview` renders the grids in a terminal so the shapes can be reviewed
without flashing anything.

The penguin was re-laid in `np-0.1.22`: the front-facing upright version came
out as a blob at 12 px, its beak and wings eaten up by the body. It is now in
profile — the head sits at the upper left of the body, the beak sticks out
two rows to the right and the next row steps back in (the space under a
protrusion has to stay blank, or it is just a bump on the head), and the wing
is a 2 px dark diagonal across the light belly, slanting all the way from the
shoulder down under it. A 1 px transparent "eye" punched into the head was tried and
split the head in half, so there is no eye at all: at this size what survives
is the silhouette, not the detail.

`main/np_icons.c` attaches them to LVGL 9.6's software renderer:
`static_bitmap = 1` and `LV_FONT_GLYPH_FORMAT_A8` are what make
`lv_draw_sw_letter.c` take its raw-mask fast path, which allocates nothing and
blends the mask directly; the glyph index travels to the bitmap callback in
`dsc_out->gid.index`, because that descriptor is the only channel back. Box
12 × 12, stride 12, advance 14 (a 2 px gap the label does not have to know
about), `line_height` 16 with `base_line` 3 so `ofs_y = 0` puts the bottom of
the box exactly on the baseline of the text it precedes.

The fallback chain is `np_font_14 → icon font → montserrat_14` (and the 20 px
equivalent), installed on runtime copies of the subset descriptors so nothing
mutates a const global. Codepoints are `U+E000`-`U+E004`, private use area,
three bytes in UTF-8 (`np_icon_utf8()`), and the `os`-string-to-codepoint
mapping is pure logic in `np_os_glyph()` so `tests/test_np_model.c` can pin it.
`tests/test_icon_contract.py` pins the count, the private-use range (the
3-byte encoding depends on it), the pixel count per glyph, the four-value alpha
alphabet, and that the `NP_GLYPH_*` constants in `np_model.h` agree with the
generated table in order.

These codepoints stay out of the glyph inventory on purpose — they are not
text, and the generator's own docstring contains a 🐧 and a █ that must never
become glyphs. The consequence is a rule for callers: an icon byte has to be
appended *after* `np_text_sanitize()`, because the inventory filter would
otherwise replace it with `?`. Names are sanitized when the JSON is parsed and
the prefix is added when the row is drawn, which is the only order that works.

## Boot and keys

`main/main.c` brings up the BSP and `np_app_start()` creates the application
tasks. The power key can still be held when the board starts, so a GPIO-wake
boot arms `key_guard` and swallows every key action until that press comes up —
otherwise powering on with a finger on the key could immediately trigger a
long-press action such as clearing credentials. After the button driver
initializes, the guard is re-synced from the sampled voltage, because a press
that ended before the driver started never produces a release event. The app
never enters deep sleep. The backlight starts at 100% from `main/main.c` and is
then user-controlled from system settings, which also supports auto-sleep (see
"System settings", since `np-0.2.8`). Auto-dimming was removed on request in
`np-0.1.12`; the sleep feature added in `np-0.2.8` only runs in the normal RUN
state — the provisioning screens are read off the display (hotspot name on the
hotspot page, pairing code on the LAN page), so those pages never sleep, and a
dark screen is not confused with a device that has lost the network.

A key press that finds the LVGL lock busy is re-queued rather than dropped
(`np-0.1.27`). The lock is held by the display task for a whole
`lv_timer_handler()` round, including the flush, so a 500 ms wait can expire
while the screen is merely busy; the old code returned at that point and the
press simply never existed. The queue event now carries the moment it was
posted and a retry count, and a failed lock re-posts the same event up to twice
before dropping it — with a warning either way, because silence is what made
"it lagged" indistinguishable from "it is broken". Handling also measures the
age of every key it takes off the queue and logs it, with the retry count, past
300 ms ("key latency"). That line is the instrument for the one thing this
round deliberately
did not touch: the button driver samples at `ESP_TIMER_TASK` priority (1),
below `lwIP` (18) and hard-coded in the BSP's button component, so a remaining
lag of that kind belongs to the RTOS timer task and would mean moving
`CONFIG_FREERTOS_TIMER_TASK_PRIORITY` — a global change that also moves the LVGL
tick, and one to decide with measurements rather than guesses.

## System settings

Since `np-0.2.8` a long-press of OK on the list page opens system settings (the
old long-press-to-status binding gives way to settings; the status page stays
reachable via OK on the detail page). The settings home has four rows with the
current value on the right; OK enters a sub-page, and a long-press of OK returns
to the list and persists in one go.

- **Screen brightness:** UP/DOWN steps by 5% in the sub-page and
  `bsp_display_backlight()` takes effect immediately; only RAM is touched and
  NVS (`bri`, 1..100) is written when leaving settings, avoiding frequent flash
  wear.
- **Auto-sleep timeout:** tiers = never / 1 / 5 / 10 / 30 minutes / 1 / 2 hours
  (`offm`, minutes, 0 = never). When the timeout fires with AOD off,
  `bsp_display_backlight(0)` blanks the screen; with AOD on it switches to the
  AOD page at 20% brightness.
- **Always-on display:** when on (`aod` = 1), the AOD page is truly centred
  horizontally (the labels auto-size rather than sitting left-aligned inside a
  fixed 240 px box): the current time `HH:MM` in a large font (Montserrat 48);
  below it the Tailscale nine-dot mark with the device count `x/y` (x =
  online, y = total) in Montserrat 28 — two tiers larger since `np-0.2.10`;
  the battery icon top right keeps the list header's geometry and level
  rule. The mark, the battery icon, and the mark hiding with `x/y` before the
  host list arrives are from `np-0.2.12` / `np-0.2.11` respectively. The page
  refreshes every second. Until the first successful read of the Tailscale
  host list, the mark and `x/y` count are hidden and only the time is shown
  centred (since `np-0.2.11`), so an inaccurate online count is never shown
  before the data actually arrives. When off, sleep simply blanks the screen.
- **Network configuration:** since `np-0.2.9` the status page's action menu is
  folded into settings — the sub-page lists sync now / LAN configuration /
  start the guide hotspot / clear credentials, behaving exactly like the status
  menu (clearing still asks for a second press). The status page, reached via OK
  on a detail page, keeps its menu as well.
- The sleep check runs only in the normal RUN state, driven by the 1-second
  tick of `app_task`; provisioning and hotspot pages never sleep. Any key wakes:
  the backlight is restored, blank mode returns to the previous view, and AOD
  returns to the list.
- Validation, tier labels and the sleep check live in the pure-logic
  `np_display_cfg` (host-testable); the `bri` / `offm` / `aod` keys are read and
  written through `app_cfg`.

## Version and delivery

`CONFIG_APP_PROJECT_VER` in `sdkconfig.defaults` pins the app version. Left
empty, ESP-IDF substitutes a `git describe` string, and two builds of the same
working tree then share one version — which cannot tell delivered images
apart. Both switches are required: `APP_PROJECT_VER` `depends on
APP_PROJECT_VER_FROM_CONFIG` in Kconfig, so writing the string alone is dropped
silently and the image keeps the git description. Run `./tools/bump_version.sh`
before every firmware build: it increments the patch component (`np-0.1.1` →
`np-0.1.2`) and refuses to run on anything that is not
`np-MAJOR.MINOR.PATCH`. The value appears in the boot log, on the status
screen's version line (with the `np-` build prefix stripped for display, since
it carries no information for whoever is reading the screen), and in
`build/firmware/<sha256>/manifest.json`; `tools/validate.sh
--firmware` compares the version embedded in the image with `sdkconfig.defaults`
and fails on a mismatch. The delivered merged image carries that version in its
file name — `build/FoloToy-AI-Passport-full-<version>.bin` — and the gate deletes
older `FoloToy-AI-Passport-full*.bin`, so only the current build is left to
flash. History entries before `np-0.1.8` quote the old fixed path; those files
are gone from `build/` and survive only in the content-addressed
`build/firmware/<sha256>/` bundles.

## Security notes

- The config HTTP server exists only during a session window: 10 minutes of
  idleness since the last request, capped at 30 minutes from service start. The
  cap is not optional — the page polls `/api/status` every 5 seconds, so an
  idle-only timer would let a phone keep the server alive indefinitely. The
  hotspot itself is open since `np-0.2.2` (no password), so association is
  unauthenticated; the hotspot stage's page writes are unauthenticated too —
  an accepted trade-off, since that flow is guided and happens with the user
  next to the device. The LAN credential stage still gates every write with
  the 6-digit pairing code shown on the device screen, so changing the
  configuration there requires being able to read the screen.
- With no Tailscale credentials stored, the device starts one LAN config
  session by itself once the link has an IPv4 address (see `np-0.1.9`, address
  requirement added in `np-0.1.11`), so the server is no longer reachable only
  after a button press. At most once per boot: aborting
  with the OK long press, or letting the prompt page idle out with nothing
  saved, suppresses further auto-starts. The window and the pairing-code
  requirement are unchanged.
- The pairing code is derived from `esp_random()` and shown only on screen;
  never logged, never offered as a QR code, so it is typed by hand — the screen
  is the only place it appears. It exists and is checked only in LAN mode
  (`np-0.2.3`); the hotspot page has no code field. The hotspot name stays the
  same for one provisioning task and rotates only after a successful Wi-Fi
  save; there is no hotspot password since `np-0.2.2`.
- *Clear credentials* on the menu clears the Tailscale triple only — the access
  token plus the OAuth client id and secret (`app_cfg_clear_tailscale()`), then
  saves, so the NVS copy goes with it. The stored Wi-Fi SSID and passphrase are
  a different function (`app_cfg_clear_wifi()`) and no menu entry calls it, so
  the device stays on its network.
- Provisioned credentials persist in NVS (namespace `np`) in plaintext: the
  Wi-Fi SSID/passphrase, the access token, or the OAuth id/secret. Physical
  access to the flash can read them back (`esptool read_flash`); reconnection
  on boot needs those stored values, and the full mitigation (flash encryption
  plus NVS encryption) burns irreversible eFuses, so plaintext NVS is an
  accepted trade-off, not an oversight. Nothing credential-shaped is ever
  logged: the `np-0.2.4` audit went through every `ESP_LOG`/`printf` call in
  `main/` — log lines carry SSID, status codes and sizes only.
- The device clock comes from SNTP after each connect; there is no RTC.
- **No development credentials in the image (since `np-0.2.0`):** the
  `NP_DEV_WIFI` switch and its two bench passphrases were removed entirely, so
  neither the repository nor the merged image carries hard-coded Wi-Fi
  credentials anymore. The `--firmware` warning that used to remind about the
  switch is gone with it.


## Delivery history

Per-version delivery records — one Conventional Commits-style entry per
version, with the decisions, trade-offs, gate results and image identity —
live in [tailscale-panel-history.md](tailscale-panel-history.md). This document
keeps the current specification only.
