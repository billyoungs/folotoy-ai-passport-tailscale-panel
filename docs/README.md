**English** · [简体中文](/docs/README.zh_CN.md)

<h1 align="center">FoloToy AI Passport — Network Monitor Panel</h1>

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="../assets/images/logo-wordmark-dark.png">
    <img src="../assets/images/logo-wordmark.png" alt="FoloToy wordmark" width="128">
  </picture>
</p>

<p align="center">
  <strong>Wear your tailnet status.</strong><br>
  A battery-powered Tailscale dashboard you can hold in one hand.
</p>

<p align="center">
  <a href="development/apps/tailscale-panel.md"><img src="https://img.shields.io/badge/API-Tailscale%20Admin%20API-2563eb?style=flat-square" alt="Tailscale Admin API"></a>
  <a href="development/apps/tailscale-panel.md"><img src="https://img.shields.io/badge/Provisioning-Wi-Fi%20hotspot%20%E2%80%94%20no%20cloud-14b8a6?style=flat-square" alt="Local Wi-Fi provisioning"></a>
  <a href="hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md"><img src="https://img.shields.io/badge/Board-ESP32-C3%20%C2%B7%20240%C3%97320-f97316?style=flat-square" alt="ESP32-C3 board"></a>
  <a href="/LICENSE"><img src="https://img.shields.io/badge/License-MIT-64748b?style=flat-square" alt="MIT License"></a>
</p>

<p align="center">
  <a href="development/apps/tailscale-panel.md">Panel guide</a> ·
  <a href="#quick-start">Quick start</a> ·
  <a href="#three-key-navigation">Three-key navigation</a> ·
  <a href="#3-enter-tailscale-credentials">Tailscale credentials</a> ·
  <a href="#documentation-index">Documentation</a>
</p>

---

## What this build is

This firmware turns a FoloToy AI Passport board — ESP32-C3, 8 MB Flash, no
PSRAM, a 240 × 320 RGB565 display, three physical buttons (`UP` / `DOWN` /
`OK`) and a CW2017-monitored battery-powered case (see the
[hardware guide](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md)) — into a
small Tailscale management dashboard. It polls the Tailscale Admin API over
HTTPS (TLS trusts the pinned ISRG Root X1 CA), and shows your tailnet's
device list on the screen: online state, address, OS, client version, key
expiry, last connection and join time, plus relay latency and advertised
routes for the device you inspect. Wi-Fi and credentials are provisioned
through a temporary web page served by the device itself — no serial tooling,
and no third-party relay in between.

> [!IMPORTANT]
> The layer underneath is **FoloToy AI Passport**, an open MIT-licensed
> wearable AI platform: AI coding agents build custom apps for it following
> `AGENTS.md` and its guides. This repository hosts one such **derivative
> application** — the network-engineer monitor panel. `main` stays the clean
> upstream baseline, and this panel lives on the `feature/network-engineer-panel`
> branch. Upstream introduction and development workflow:
> [development guide](development/README.md) ·
> [AI development guide](development/ai-guide.md) ·
> [community projects](reference/README.md).

## Quick start

### 1. Flash

Flash the merged image to `0x0`:

```bash
esptool.py --chip esp32c3 write_flash 0x0 build/FoloToy-AI-Passport-full-<version>.bin
```

No backup of a previous firmware is needed, but a merged flash resets the
NVS namespace where Wi-Fi passwords and Tailscale credentials are stored —
after flashing you provision them again (steps 2–3 below).

### 2. First boot: join a Wi-Fi network (bootstrap hotspot)

With no stored credentials the device boots straight into an open hotspot
`Passport-<xxxx>` and shows on screen:

- the SSID (`Passport-<xxxx>`) — **no password**, since `np-0.2.2`
- the address to open: `http://192.168.4.1`

Join the hotspot with your phone or laptop; the configuration page opens by
itself (captive portal, no path to type). Then:

- press **Rescan** to list nearby networks — the form does not scan on load;
  the radio sweep costs a second or two.
- pick a network, enter its password, press **Save and connect**: one
  request, the session closes, the device joins and moves on.

You can also type an SSID by hand if the list is wrong. The hotspot keeps the
same SSID across reboots until the Wi-Fi save succeeds, so your paired phone
does not lose track of it. While the link is down, the device-list screen
doubles as the retry screen: **OK** retries the connection, **OK long-press**
returns to the hotspot so a mistyped password can be corrected.

### 3. Enter Tailscale credentials

When Wi-Fi is up and no Tailscale credentials are stored, the device starts
one LAN configuration session by itself and the screen shows:

- what to do: *enter the Tailscale credentials from another device*
- a **6-digit pairing code**
- the direct address to open, e.g. `http://192.168.1.42`

Open that address in a browser on another device, enter the code, and pick
your credential type:

| Option | When to use | Validity |
| --- | --- | --- |
| **API access token** `tskey-…` | personal key — simplest | up to 90 days |
| **OAuth client** `client_id` + `client_secret` | read-only scope; the device exchanges a short-lived JWT (~1 h) automatically | automatic |

Create the API key in the Tailscale admin console (ACL page → add a key);
it needs the power to read your tailnet's device list. Press **Save** — when
the saved fields carry Tailscale credentials, the session closes and the
panel syncs immediately, so the screen itself tells you whether the
credentials work.

Security highlights: stored secrets are never echoed back (the web page shows
only a masked 4-character tail), password inputs on the device screen are
plain text on purpose, every LAN write requires the displayed pairing code,
and the config server closes after 10 minutes idle (30-minute absolute cap).
Credentials are persisted in NVS **in plaintext** — physical flash reads can
recover them; flash encryption would burn irreversible eFuses, so this is an
accepted trade-off, not an oversight.

### 4. The device list appears

After a successful sync the home screen shows your tailnet: each row carries
an OS glyph, an online dot and the hostname, with the date on the left of the
header, `HH:MM` centred, and Wi-Fi / battery marks on the right. Done —
everything else below is navigation and background.

## Three-key navigation

`UP` / `DOWN` move, `OK` confirms, **OK long-press** goes back or opens the
menu. The full screen-by-screen table lives in the
[panel guide](development/apps/tailscale-panel.md); the useful bits:

| View | Reached by | Keys |
| --- | --- | --- |
| Device list | at startup | Up/Down select, OK opens detail; OK long-press opens system settings; with no sync yet OK opens the status view |
| Device detail (address, OS, client, owner, tags, key expiry, last seen, joined, relay, routes — ten rows, scrolls vertically) | OK on a list row | Up/Down scroll, stop at the edges; OK opens status; OK long-press returns to the list |
| Status view (menu: sync now / LAN configuration / start the guide hotspot / clear credentials; body shows account, login state and the poll cadence) | OK on a detail page, or on an empty list | Up/Down pick an item, OK runs it; OK long-press back |
| System settings (brightness, auto-sleep timeout, AOD toggle, network configuration) | OK long-press on the list | OK enters a sub-page / saves; OK long-press returns to the list and persists |

- **While Wi-Fi is down**, OK on the list retries the connection and its
  long-press returns to the bootstrap hotspot.
- **Sleep and AOD**: the backlight starts at 100 %. System settings offer
  always-on display — a centred `HH:MM` with the Tailscale mark and the live
  `x/y` device count beneath it, and the battery icon top right, at 20 %
  brightness — and auto-sleep tiers from off to 2 hours; any key wakes the
  panel to where it was.
- **Clock**: the board has no RTC. Time comes from SNTP; until the clock
  actually moves past 1970 the header reads *waiting for time sync* and no
  request is sent — certificate validation needs a valid clock, so the
  panel waits instead of failing. If the router blocks UDP 123, the screen
  says so.
- **Freshness**: the online dot also answers for stale data — if the link
  was down, dots go hollow and the detail line reads *last seen online*
  instead.

## How monitoring works

- **Polling**: 120 s base interval after any success, doubled after failures
  up to a 3600 s cap; a healthy reconnect resumes immediately.
- **Online state**: as the control plane reports (`lastSeen` within a
  15-minute grace window counts as connected), with a local freshness check
  layered on top for the link that dropped.
- **Detail rows** get their relay and route data from a second
  per-device request that only runs while the detail page is open.
- **Scale**: the list shows 24 rows; the footer carries the real total
  (`N/M online, K more not shown`). Large tailnets still cost the same RAM —
  the body is streamed and sliced, not parsed whole.
- **Non-polling paths** (missed `lastSeen` nuance, the `-%` battery gauge,
  IPv6 exclusion, the 15-minute grace, secret hygiene) are engineering notes
  in the [panel guide](development/apps/tailscale-panel.md).

## Upstream AI Passport, briefly

AI Passport is an open wearable AI platform from FoloToy: a small board, a
240 × 320 screen, three buttons, microphone/speaker and battery, with
open firmware and community examples; AI coding agents build applications
for it from a one-line requirement. This repository takes that baseline and
builds one derivative application — the Tailscale monitor panel, branched as
`feature/network-engineer-panel`. Board facts, interfaces and acceptance
rules remain shared with the upstream BSP contract; only the application
lives here.

## Documentation index

| Document | What you will find |
| --- | --- |
| [Panel guide](development/apps/tailscale-panel.md) | Full usage and UI specification of this build |
| [Delivery history](development/apps/tailscale-panel-history.md) | Per-version records: decisions, gates, image identity |
| [Hardware guide](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) | Board facts, pin map, interface boundaries, acceptance checklists |
| [Firmware layout](development/engineering/firmware-layout.md) | Partitions, flashing, and stored-data reset behaviour |
| [Development guide](development/README.md) | Upstream AI Passport workflow, conventions and CI |
| [Community projects](reference/README.md) | Example demo branches and reusable experience |
| [Fork guide](fork-guide.md) · [Changelog](CHANGELOG.md) | Downstream workflow and release history |
| [Contributing](contribution/README.md) | Documentation, commits and pull-request conventions |

---

[Contribute](../.github/CONTRIBUTING.md) · [Get help](../.github/SUPPORT.md) · [Code of conduct](../.github/CODE_OF_CONDUCT.md) · [Security](../.github/SECURITY.md) · [MIT License](../LICENSE)
