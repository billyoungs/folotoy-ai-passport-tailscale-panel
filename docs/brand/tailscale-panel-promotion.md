<p align="right">
  <strong>English</strong> · <a href="tailscale-panel-promotion.zh_CN.md">简体中文</a>
</p>

# Tailscale Monitor Card · listing copy and image prompts

This is the complete outward-facing copy for the network panel play, the
"Tailscale Monitor Card" (firmware `np-0.2.5`): text introduction, usage guide,
and the cover-image and UI-mockup prompts to hand to an AI image tool. The
visual baseline and the reference-based redraw workflow live in this directory's
[README](README.md); the publishing flow is described in
[publish-to-community](../development/release/publish-to-community.md).

## 1. Text introduction

### 1.1 Title (for the community market)

| Language | Recommended | Alternatives |
| --- | --- | --- |
| English | Tailscale Monitor Card | Tailscale Network Panel · Tailscale Status Badge |
| Chinese | the Chinese title in [the Chinese version](tailscale-panel-promotion.zh_CN.md) | see the Chinese version |

### 1.2 One-liner

- English: Your whole tailnet on a wearable card — status, addresses and
  details at a glance.
- Chinese: see the one-liner in [the Chinese version](tailscale-panel-promotion.zh_CN.md).

### 1.3 Short introduction (market listing blurb)

> Tailscale Monitor Card is a play (firmware) for the FoloToy AI Passport: it
> turns the transparent-shell wearable card into a pocket status board for your
> mesh. The screen lists every device in your tailnet — hostname, Tailscale
> address and online state — and a detail view adds client version, key expiry,
> DERP relay and subnet routes. Adaptive polling saves battery and traffic, and
> provisioning runs entirely from a local web page: join the device's own
> hotspot and configure everything from the browser, no app needed.

### 1.4 Feature highlights

- Live device list: green online dots, hostnames and Tailscale addresses
  (100.x.y.z) — your whole tailnet in one glance.
- Ten-row device detail: address / OS / client version / owner / tags / key
  expiry / last online / joined / DERP relay / subnet routes.
- Adaptive polling: speeds up on failures, backs off exponentially, 30 s to 1 h;
  "refresh now" from the menu.
- Two credential types: a Tailscale API access token (`tskey-`, up to 90 days)
  or an OAuth client (read-only; the device rotates a ~1 h JWT by itself).
- Local web provisioning: open password-free hotspot with captive-portal
  redirect; the LAN configuration stage is gated by the 6-digit pairing code
  shown on the device screen.
- Battery and signal always visible; automatic link recovery; clock via SNTP.

### 1.5 Requirements and compatibility

- Hardware: FoloToy AI Passport (240×320 color screen, Up / Down / OK buttons).
- Network: 2.4 GHz Wi-Fi able to reach `api.tailscale.com`.
- Account: a Tailscale account plus a read-only credential (API token or OAuth
  client).

### 1.6 Privacy and security facts (safe to quote publicly)

- Only `?fields=default` public fields are requested; sensitive fields such as
  `machineKey` / `nodeKey` are never parsed or stored.
- Credentials are stored only on the device; "clear credentials" is a
  double-confirm menu action.
- The pairing code is shown only on the device screen and never logged; TLS
  pins the ISRG Root X1 certificate.

## 2. Usage guide (end-user version)

> Written for whoever just flashed the device; drop it into the market listing.

1. **Flash the firmware**: open the official flasher (the
   [Web Flasher](https://ai-passport.folotoy.cn/tools/web-flasher/) built into
   the product site, or the general-purpose [tool.folotoy.cn](https://tool.folotoy.cn/)),
   pick this play's merged image `FoloToy-AI-Passport-full-np-0.2.5.bin` and
   write it (the merged image flashes from offset 0x0).
2. **First boot**: the screen shows the setup hotspot name (open, no password).
   Join it from your phone; the provisioning page usually pops up by itself —
   otherwise open `http://192.168.4.1` manually.
3. **Join your home Wi-Fi**: press *Rescan* on the hotspot page, pick your
   2.4 GHz network, type the password, press *Save and connect*.
4. **Enter the pairing code**: once the device is online it moves to the
   credential stage and shows a 6-digit pairing code on screen; the web page
   asks for it before any change — whoever can change the configuration must be
   able to read the screen.
5. **Add Tailscale credentials**: an OAuth client (read-only) is recommended, or
   paste an API access token (starts with `tskey-`); after saving, the device
   starts pulling the device list by itself.
6. **Start monitoring**: a green dot means online; press OK briefly to open a
   device's detail view, long-press OK to return. The status-page menu offers:
   refresh now / LAN setup / hotspot setup / clear credentials (double-confirm).
7. **Switching networks and troubleshooting**: use *hotspot setup* to re-open
   the setup hotspot for a new Wi-Fi; the clock syncs via SNTP, give it a moment
   after the first connect; a wrong Wi-Fi password simply returns you to the
   hotspot page to retry.

## 3. Cover image prompt (AI image generation)

### 3.0 Read this first

- **Always generate image-to-image, never text-to-image**: per this directory's
  README workflow, pass [`ai-passport-front.png`](ai-passport-front.png) (or the
  EVA colorway render from 3.2) as the input image and repaint only the screen
  area — a text-only description invents a look-alike device instead of
  reproducing this one.
- **Add long text in post**: AI renders of full sentences are unreliable; keep
  3–4 short strings on screen and overlay title and tagline with a layout tool
  afterwards.
- **Export**: PNG / JPEG / WebP, ≤ 10 MiB (the market's cover requirement);
  2:3 portrait.
- **Verify after generating**: shell silhouette, top lanyard hole, side keys,
  ports, and the `Folotoy` / `The Open Wearable AI Passport` prints must match
  the reference; regenerate on any drift.

### 3.1 Main prompt (standard transparent shell, paste as a whole)

```text
Use case: product-marketing hero image, reference-based screen repaint
Input image: the attached AI Passport front render is the BASE. Keep its transparent shell, rounded corners, top lanyard hole, side keys, ports, screws, speaker grille and ALL printed text (FOLOTOY, The Open Wearable AI Passport) exactly as in the reference.
Primary request: repaint ONLY the screen area with a network status dashboard. Do not reshape, move, resize or restyle the screen frame — same size, ratio, corner radius and position as the reference.
Scene/backdrop: dark desk scene, the card standing upright and slightly angled, soft bokeh of glowing network nodes connected by thin lines behind it, a blurred laptop edge far in the background.
Screen content: dark navy background (#0B1220); a small header row reading "Tailscale" with a tiny blue dot; three list rows, each with a round status dot, a hostname and an IP-style address: "nas-server 100.64.0.12" with a green dot, "phone 100.64.0.31" with a green dot, "lab-node 100.64.0.7" with a gray dot. Crisp small sans-serif, high contrast, left-aligned.
Style/medium: photorealistic product shot, sharp focus on the card, realistic transparent plastic with visible internal components.
Composition/framing: 2:3 portrait, card occupies the right two-thirds, left side kept clean for a title overlay added in post.
Lighting/mood: cool blue key light with a #3B82F6 tint, gentle rim light along the shell edge, dark tech atmosphere.
Constraints: screen shows only the described dashboard — no game art, no cartoons, no watermark, no extra logos, no text outside the screen.
```

### 3.2 Variant (EVA purple shell)

Swap the input image for
[`ai-passport-front-eva-01.png`](ai-passport-front-eva-01.png), keep everything
else from 3.1, and change two lines:

- `Scene/backdrop: near-black background with faint purple mesh lines of glowing nodes; the card standing upright, slightly angled.`
- `Lighting/mood: purple-blue dual tone, violet rim light matching the shell, dark tech atmosphere.`

### 3.3 Taglines to overlay in post

- English main: **Tailscale Monitor Card**; sub: **Your tailnet, at a glance.**
- Alternatives: "Wear it. Monitor it." · "The whole mesh, one glance away."
- Chinese taglines live in [the Chinese version](tailscale-panel-promotion.zh_CN.md).

## 4. UI mockup prompt (AI image generation)

### 4.0 Fidelity notes

UI mockups suit decorative use (social posts, collage covers); **use real device
screenshots for the market's screenshot fields** — an AI render of a dense
interface will always drift. Generate one screen per call and proof every string
afterwards. Note: the on-device interface is Simplified Chinese; the English
labels below are a stylized English rendering for EN-market art.

### 4.1 List screen (paste as a whole)

```text
Use case: flat UI mockup, no device and no photo
Primary request: a polished 2D mockup of a small status screen, portrait 3:4, vector-clean flat design, dark theme.
Layout: near-black navy background (#0B1220); a top header row with the wordmark "Tailscale" left-aligned and a small blue status dot on the right; below it a card list on slightly lighter navy (#101A2D); each row has a round status dot, a bold hostname and a muted IP address underneath, with thin dividers (#1E293B) between rows; a muted hint line at the bottom.
Rows, verbatim: "nas-server 100.64.0.12" green dot (#22C55E); "phone 100.64.0.31" green dot; "homelab 100.64.0.7" gray dot (#64748B); "grandma-tablet 100.64.0.44" green dot.
Colors: background #0B1220, cards #101A2D, selected row #1D3557, text #E2E8F0, muted text #94A3B8, accent blue #3B82F6.
Typography: small crisp sans-serif with slight pixel-game flavor, generous letter spacing, high contrast.
Constraints: flat 2D only — no bezel, no hands, no reflections, no watermark; use exactly this hex palette; nothing outside the screen canvas.
```

### 4.2 Detail screen

Same background and style as 4.1; the layout becomes a status line reading
`online` with a green dot, then key-value rows (keys muted #94A3B8, values white
#E2E8F0) with thin dividers and ellipsis truncation on long values:

- `100.64.0.12` / `ubuntu 24.04` / `client 1.82.0` / `key 02-14 (23d)` /
  `DERP Tokyo · 12 ms` / `subnet 10.0.0.0/24`

### 4.3 Status screen

Same background as 4.1; the title reads `Tailscale`; three body lines: account
name, `signed in`, `polling every 5 min`; below them a four-item menu with the
selected item on a #1D3557 highlight bar: `Refresh now` / `LAN setup` /
`Hotspot setup` / `Clear credentials`; a bottom hint reads
`Up/Down select · OK run`. (The real device renders these labels in Simplified
Chinese.)

## Related links

- Visual baseline and redraw workflow: [README.md](README.md)
- Publishing flow:
  [../development/release/publish-to-community.md](../development/release/publish-to-community.md)
- Current specification:
  [../development/apps/tailscale-panel.md](../development/apps/tailscale-panel.md)
- Per-version delivery history:
  [../development/apps/tailscale-panel-history.md](../development/apps/tailscale-panel-history.md)
