<p align="right">
  <a href="tailscale-panel-promotion.md">English</a> · <strong>简体中文</strong>
</p>

# Tailscale 监控牌 · 项目文案与生图提示词

本文是网络面板玩法「Tailscale 监控牌」(固件 `np-0.2.5`)的成套对外文案:文字介绍、
使用说明,以及交给 AI 生图工具的封面图与界面图提示词。视觉基线与图生图工作流遵循
本目录 [README.zh_CN.md](README.zh_CN.md);发布流程见
[发布到 AI Passport 社区](../development/release/publish-to-community.zh_CN.md)。

## 1. 文字介绍

### 1.1 标题(社区市场用)

| 语言 | 推荐 | 备选 |
| --- | --- | --- |
| 中文 | Tailscale 监控牌 | Tailscale 网络面板 · 口袋 Tailscale 状态牌 |
| 英文 | Tailscale Monitor Card | Tailscale Network Panel · Tailscale Status Badge |

### 1.2 一句话简介

- 中文:把整张 Tailscale 组网戴在身上 —— 设备在线状态、地址与详情,抬手即见。
- 英文:Your whole tailnet on a wearable card — status, addresses and details at a
  glance.

### 1.3 短介绍(社区市场简介栏)

> Tailscale 监控牌是 FoloToy AI Passport 的一款玩法固件:把这块透明外壳的穿戴卡
> 变成随身的网络监控台。屏幕上实时显示组网内每台设备的主机名、Tailscale 地址与
> 在线状态;点进详情,客户端版本、密钥有效期、DERP 中继与子网路由一目了然。
> 自适应轮询省电省流量,本地网页配网免装 App —— 手机连上设备热点就能完成全部配置。

### 1.4 功能亮点

- 实时设备列表:在线绿点、主机名、Tailscale 地址(100.x.y.z),一眼扫全 tailnet。
- 十行设备详情:地址 / 系统 / 客户端版本 / 归属 / 标签 / 密钥有效期 / 上次在线 /
  加入时间 / DERP 中继 / 子网路由。
- 自适应轮询:失败提速、成功指数退避,30 秒~1 小时自动调节;菜单可「立即刷新」。
- 两种凭据:Tailscale API 访问令牌(`tskey-`,最长 90 天)或 OAuth 客户端(只读,
  设备自动换发约 1 小时的短期 JWT)。
- 本地网页配网:开放热点免密连接、门户自动弹出;局域网配置阶段凭设备屏幕上的
  6 位配对码把门。
- 电量与信号常显,断链自动重连;时间由 SNTP 校准。

### 1.5 要求与兼容

- 硬件:FoloToy AI Passport(240×320 彩色屏幕、上/下/确定三按键)。
- 网络:2.4 GHz Wi-Fi,且可达 `api.tailscale.com`。
- 账号:Tailscale 账号 + 只读凭据(API token 或 OAuth client)。

### 1.6 隐私与安全口径(可直接对外引用)

- 只请求 `?fields=default` 公开字段;`machineKey` / `nodeKey` 等敏感字段从不解析、
  从不存储。
- 凭据仅保存在设备本地;「清除凭据」是二次确认的菜单操作。
- 配对码只在设备屏幕显示,绝不写入日志;TLS 钉住 ISRG Root X1 根证书。

## 2. 使用说明(用户版)

> 面向拿到设备的人,可原样放进社区市场的使用说明栏。

1. **刷入固件**:浏览器打开官方刷机入口(产品官网内置的
   [Web Flasher](https://ai-passport.folotoy.cn/tools/web-flasher/) 或通用刷机站
   [tool.folotoy.cn](https://tool.folotoy.cn/)),选择本玩法的合并镜像
   `FoloToy-AI-Passport-full-np-0.2.5.bin` 写入(整镜像自 0x0 起刷)。
2. **首次开机**:屏幕显示引导热点名(开放热点、无需密码)。用手机连接该热点,配网
   页面一般会自动弹出;没弹就手动访问 `http://192.168.4.1`。
3. **连家庭 Wi-Fi**:在热点页点「重新扫描」,选中家里的 2.4 GHz Wi-Fi,输入密码,
   点「保存并联网」。
4. **输入配对码**:设备联网成功后转入凭据录入阶段,屏幕显示 6 位配对码;在网页上
   输入这串配对码才能继续 —— 能改配置的人,必须看得到设备屏幕。
5. **填 Tailscale 凭据**:推荐 OAuth 客户端(只读),或粘贴 API 访问令牌(`tskey-`
   开头);保存后设备自动开始拉取设备列表。
6. **开始监控**:列表页绿点 = 在线;短按「确定」进详情,长按「确定」返回列表。
   系统状态页菜单:立即刷新 / 局域网配置 / 引导热点配网 / 清除凭据(二次确认)。
7. **换网与排障**:换 Wi-Fi 用「引导热点配网」重开热点;时钟靠 SNTP 校准,首次联网
   后稍等片刻;Wi-Fi 密码填错会退回热点页重填。

## 3. 封面图提示词(AI 生图)

### 3.0 使用须知

- **务必图生图,不要纯文生图**:按本目录 README 的工作流,把
  [`ai-passport-front.png`](ai-passport-front.png)(或 3.2 的 EVA 配色渲染图)作为
  输入图传给生图工具,只重绘屏幕区域 —— 纯文字描述会虚构出一台"长得像"的设备,
  而不是还原这一台。
- **文字尽量后期加**:AI 渲染成段文字不可靠,屏上只留 3~4 个短字符串;主标语与
  副标语建议成图后用排版工具叠加。
- **导出**:PNG / JPEG / WebP,≤ 10 MiB(社区市场封面要求);比例 2:3 竖版。
- **成图核对**:外壳轮廓、顶部挂绳孔、侧键、接口、`Folotoy` 与
  `The Open Wearable AI Passport` 印字必须与参考一致,偏差即重生成。

### 3.1 主提示词(标准透明壳,整段直接粘贴)

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

### 3.2 变体(EVA 紫壳)

以 [`ai-passport-front-eva-01.png`](ai-passport-front-eva-01.png) 替换输入图,其余
同 3.1,另改两句:

- `Scene/backdrop: near-black background with faint purple mesh lines of glowing nodes; the card standing upright, slightly angled.`
- `Lighting/mood: purple-blue dual tone, violet rim light matching the shell, dark tech atmosphere.`

### 3.3 建议后期叠加的标语

- 中文主标:**Tailscale 监控牌**;副标:**戴在身上的组网状态牌**
- 英文主标:**Tailscale Monitor Card**;副标:**Your tailnet, at a glance.**
- 备选副标:「整张网络,抬手可见」 / "Wear it. Monitor it."

## 4. 界面图提示词(AI 生图)

### 4.0 保真度须知

界面图适合装饰性展示(推文配图、帖子拼图);**社区市场的「截图」栏请用真机截图**,
AI 渲染密集中文界面必然出错。逐屏生成、每次只生成一块屏,成图后逐字核对屏上文字。

### 4.1 列表屏(整段直接粘贴)

```text
Use case: flat UI mockup, no device and no photo
Primary request: a polished 2D mockup of a small status screen, portrait 3:4, vector-clean flat design, dark theme.
Layout: near-black navy background (#0B1220); a top header row with the wordmark "Tailscale" left-aligned and a small blue status dot on the right; below it a card list on slightly lighter navy (#101A2D); each row has a round status dot, a bold hostname and a muted IP address underneath, with thin dividers (#1E293B) between rows; a muted hint line at the bottom.
Rows, verbatim: "nas-server 100.64.0.12" green dot (#22C55E); "phone 100.64.0.31" green dot; "homelab 100.64.0.7" gray dot (#64748B); "grandma-tablet 100.64.0.44" green dot.
Colors: background #0B1220, cards #101A2D, selected row #1D3557, text #E2E8F0, muted text #94A3B8, accent blue #3B82F6.
Typography: small crisp sans-serif with slight pixel-game flavor, generous letter spacing, high contrast.
Constraints: flat 2D only — no bezel, no hands, no reflections, no watermark; use exactly this hex palette; nothing outside the screen canvas.
```

### 4.2 详情屏

同 4.1 的底色与风格,布局改为:顶部状态行 `online` 配绿色圆点;下方键值行
(键灰 #94A3B8、值白 #E2E8F0),每行细分隔线,值过长行尾以省略号截断:

- `100.64.0.12` / `ubuntu 24.04` / `client 1.82.0` / `key 02-14 (23d)` /
  `DERP Tokyo · 12 ms` / `subnet 10.0.0.0/24`

### 4.3 系统状态屏

同 4.1 底色;标题 `Tailscale`,正文三行:账号名、`已登录`、`轮询 5 分钟`;下方
四项菜单,选中项以 #1D3557 高亮条标出:`立即刷新` / `局域网配置` / `引导热点配网` /
`清除凭据`;底部提示 `上下选择 · 确定执行`。(这三屏文案与真机一致;真机为简体中文
界面。)

## 相关链接

- 视觉基线与图生图工作流:[README.zh_CN.md](README.zh_CN.md)
- 发布流程:[../development/release/publish-to-community.zh_CN.md](../development/release/publish-to-community.zh_CN.md)
- 现行规格:[../development/apps/tailscale-panel.zh_CN.md](../development/apps/tailscale-panel.zh_CN.md)
- 逐版本记录:[../development/apps/tailscale-panel-history.zh_CN.md](../development/apps/tailscale-panel-history.zh_CN.md)
