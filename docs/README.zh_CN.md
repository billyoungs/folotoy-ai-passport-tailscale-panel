[English](/docs/README.md) · **简体中文**

<h1 align="center">FoloToy AI Passport —— Tailscale 监控面板</h1>

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="../assets/images/logo-wordmark-dark.png">
    <img src="../assets/images/logo-wordmark.png" alt="FoloToy 字标" width="128">
  </picture>
</p>

<p align="center">
  <strong>把 tailnet 状态戴在手上。</strong><br>
  一台有电池、能拿在掌心里的 Tailscale 监控牌。
</p>

<p align="center">
  <a href="development/apps/tailscale-panel.zh_CN.md"><img src="https://img.shields.io/badge/API-Tailscale%20Admin%20API-2563eb?style=flat-square" alt="Tailscale Admin API"></a>
  <a href="development/apps/tailscale-panel.zh_CN.md"><img src="https://img.shields.io/badge/%E9%85%8D%E7%BD%91-Wi-Fi%20%E7%83%AD%E7%82%B9%20%E2%80%94%20%E6%97%A0%E4%BA%91%E8%BD%AC%E5%8F%91-14b8a6?style=flat-square" alt="本地 Wi-Fi 配网"></a>
  <a href="hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md"><img src="https://img.shields.io/badge/%E6%9D%BF%E5%8D%A1-ESP32-C3%20%C2%B7%20240%C3%97320-f97316?style=flat-square" alt="ESP32-C3 板卡"></a>
  <a href="/LICENSE"><img src="https://img.shields.io/badge/License-MIT-64748b?style=flat-square" alt="MIT 许可证"></a>
</p>

<p align="center">
  <a href="development/apps/tailscale-panel.zh_CN.md">面板指南</a> ·
  <a href="#快速开始">快速开始</a> ·
  <a href="#三键导航">三键导航</a> ·
  <a href="#凭据入口">Tailscale 凭据</a> ·
  <a href="#文档索引">文档</a>
</p>

---

## 这是什么固件

这份固件把一块 FoloToy AI Passport 板卡——ESP32-C3、8 MB Flash、无 PSRAM、
240 × 320 RGB565 屏幕、三个实体按键(`UP` / `DOWN` / `OK`)、
CW2017 电量计的有电池外壳(见
[硬件指南](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md))——
变成一台小小的 Tailscale 管理看板。它通过 HTTPS 轮询 Tailscale Admin API
(TLS 信任固定的 ISRG Root X1 CA),把 tailnet 里的设备状态显示在屏幕上:
在线状态、地址、系统、客户端版本、密钥到期、最近上线与加入时间,以及
查看某台设备时的中继延迟和宣告路由。Wi-Fi 与凭据通过设备自己提供的
临时网页配置——不用串口工具,也不经过任何第三方中继。

> [!IMPORTANT]
> 承载这一切的底层是 **FoloToy AI Passport**:一个开放的 MIT 许可可穿戴
> AI 平台,AI 编程助手按照 `AGENTS.md` 与配套指南为其开发各种应用。本仓库
> 是其中一个**衍生应用**——网络工程师监控面板。`main` 保持干净的上游基线,
> 面板活在 `feature/network-engineer-panel` 分支上。上游介绍与开发流程:
> [开发指南](development/README.zh_CN.md) ·
> [AI 开发指南](development/ai-guide.zh_CN.md) ·
> [社区作品](reference/README.zh_CN.md)。

## 快速开始

### 1. 烧录

把合并镜像烧到 `0x0`:

```bash
esptool.py --chip esp32c3 write_flash 0x0 build/FoloToy-AI-Passport-full-<版本号>.bin
```

无需备份原有固件,但合并烧录会重置存放 Wi-Fi 密码与 Tailscale 凭据的
NVS 命名空间——烧完需要按下面第 2、3 步重新配置。

### 2. 首次开机:连接 Wi-Fi(引导热点)

没有已存凭据时,设备直接开一个无密码热点 `Passport-<xxxx>`,屏幕上显示:

- 热点名(`Passport-<xxxx>`)——**无密码**,自 `np-0.2.2` 起如此
- 要打开的地址:`http://192.168.4.1`

用手机或电脑连上热点,配置页面会自动弹出(captive portal,不用输入路径)。
然后:

- 点 **Rescan** 列出附近网络——列表页**不会**自动扫描,扫描要按按钮;
  扫描占用唯一无线电一两秒。
- 选一个网络、输入密码,点 **Save and connect**:一次请求、会话结束,
  设备连上并进入下一步。

列表不对时也可以手动输入 SSID。热点名在 Wi-Fi 保存成功之前重开不变,
配对过的手机不会找不到它。Wi-Fi 掉线时,设备列表页兼任重试页:**OK**
重试连接,**长按 OK** 回到热点页改密码。

### 3. 录入 Tailscale 凭据(配对码页)

Wi-Fi 已连接但还没有 Tailscale 凭据时,设备自动开一个 LAN 配置会话,
屏幕显示:

- 要做什么:*在另一台设备上录入 Tailscale 凭据*
- 一个 **6 位配对码**
- 要打开的直连地址,例如 `http://192.168.1.42`

在另一台设备的浏览器里打开该地址,输入配对码,选择凭据类型
(见[下文](#凭据入口)):

| 选项 | 适用场景 | 有效期 |
| --- | --- | --- |
| **API 访问令牌** `tskey-…` | 个人密钥,最简单 | 最长 90 天 |
| **OAuth 客户端** `client_id` + `client_secret` | 只读权限;设备自动换取约 1 小时的短 JWT | 自动续期 |

API 密钥在 Tailscale 管理控制台(ACL 页面→添加密钥)创建,需要能读取
本 tailnet 设备列表的权限。点 **Save**——当保存的字段携带 Tailscale
凭据时,会话结束并立刻同步,屏幕会直接告诉你凭据是否可用。

安全要点:已存密钥永不回显(网页只显示掩码后的 4 位尾码);设备屏幕上
密码输入框刻意用明文;所有 LAN 写入都需要屏幕显示的 6 位配对码;配置
服务器空闲 10 分钟关闭(最长 30 分钟硬上限)。凭据以**明文**存放在 NVS
中——物理读取 flash 可以得到它们;开启 flash 加密要烧不可逆的 eFuse,
这是有意接受的代价,不是疏漏。

### 4. 设备列表出现

同步成功后,首页就是 tailnet 的设备列表:每行一个 OS 图标、一个在线
状态点和主机名,页头左侧是日期,中间是居中对齐的 `HH:MM`,右侧是
Wi-Fi 与电池图标。到这里就完成了,下面只是导航与原理。

## 三键导航

`UP` / `DOWN` 移动,`OK` 确认,**长按 OK** 返回或开菜单。逐屏的完整表格
在[面板指南](development/apps/tailscale-panel.zh_CN.md)里,这里放好用的
部分:

| 界面 | 如何进入 | 按键 |
| --- | --- | --- |
| 设备列表 | 开机后 | 上下选择,OK 打开详情;长按 OK 进系统设置;还没同步过时 OK 进入状态页 |
| 设备详情(地址、OS、客户端、归属、标签、密钥到期、最近在线、加入时间、中继、路由共十行,纵向滚动) | 列表选中行按 OK | 上下滚动、到顶到底停下;OK 进入状态页;长按 OK 返回列表 |
| 状态页(菜单项:立即同步 / LAN 配置 / 启动引导热点 / 清除凭据;正文显示账户、登录状态与轮询节奏) | 详情页或空列表按 OK | 上下选中,OK 执行;长按 OK 返回 |
| 系统设置(亮度、自动休眠时长、AOD 开关、网络配置) | 列表页长按 OK | OK 进入子页 / 确认;长按 OK 返回列表并保存 |

- **Wi-Fi 掉线时**,列表页按 OK 重试连接,长按 OK 回到引导热点。
- **休眠与 AOD**:背光默认 100%。系统设置提供常亮显示(居中大号
  `HH:MM`,下方实时设备数 `x/y`,以 20% 亮度续航),自动休眠从关闭到
  2 小时共七档;任何按键都会唤醒并回到原界面。
- **时钟**:板卡没有 RTC,时间来自 SNTP。时钟还没真正越过 1970 年时,
  页头显示 *waiting for time sync* 并且不会发出请求——证书校验需要
  有效时钟,面板选择等待而不是反复失败。路由器封了 UDP 123 时屏幕会
  直接说明。
- **数据新鲜度**:在线圆点同时代表数据的可信度——链路断过之后圆点变
  空心,详情状态行显示 *last seen online*,不会把旧快照当实时数据。

## 凭据入口

两种凭据,任选其一:

- **API 访问令牌**(`tskey-…` 开头):在 Tailscale 管理控制台的 ACL 页
  生成个人密钥,最简单,有效期最长 90 天。
- **OAuth 客户端**(`client_id` + `client_secret`):授予只读权限,设备
  每次自动用它换取约一小时寿命的 JWT,适合不想轮换密钥的场景。

设备请求 `?fields=default`(列表接口),并按需对单台设备请求完整字段;
`machineKey` / `nodeKey` 这类敏感字段从不解析、从不存储。已保存的密钥
存入 NVS(命名空间 `np`),重启后自动重连;在配置页清除、或在状态菜单
长按两次确认的"清除凭据",会连同 NVS 副本一起删掉(只清 Tailscale
三元组,不动 Wi-Fi)。

## 监控是怎么工作的

- **轮询**:成功之后回到 120 秒基础间隔;失败翻倍,最高 3600 秒封顶;
  链路恢复后立刻继续。
- **在线判定**:以控制平面的 `lastSeen` 为准(15 分钟宽限期内算在线),
  再叠加本地对链路本身是否健康的新鲜度检查。
- **详情行**的中继延迟与路由来自第二次按设备发起的请求,只在详情页
  打开时才会请求。
- **规模**:列表一屏 24 行,底栏显示真实总数(`N/M online, K more not
  shown`)。大 tailnet 也占同样的内存——响应体流式切片处理,不整体
  解析。
- 其余工程细节(轮询门控、内存遥测、字体流水线、安全策略)都写在
  [面板指南](development/apps/tailscale-panel.zh_CN.md)里。

## 上游 AI Passport,一句话

AI Passport 是 FoloToy 的开放可穿戴 AI 平台:一小块板卡、240 × 320 屏幕、
三个按键、麦克风与扬声器、电池,固件开放,社区有大量示例,AI 编程助手
用一句需求就能为它开发应用。本仓库取这个基线,做出其中一个衍生应用——
Tailscale 监控面板,以 `feature/network-engineer-panel` 分支承载。板卡
事实、接口与验收规则与上游 BSP 契约共享,本分支只含应用本体。

## 文档索引

| 文档 | 你可以找到 |
| --- | --- |
| [面板指南](development/apps/tailscale-panel.zh_CN.md) | 本固件的完整使用与界面规范 |
| [版本交付史](development/apps/tailscale-panel-history.zh_CN.md) | 逐版本记录:决策、门禁、镜像标识 |
| [硬件指南](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md) | 板卡事实、引脚表、接口边界、验收清单 |
| [固件布局](development/engineering/firmware-layout.zh_CN.md) | 分区、烧录方式、已存数据重置行为 |
| [开发指南](development/README.zh_CN.md) | 上游 AI Passport 的工作流、规范与 CI |
| [社区作品](reference/README.zh_CN.md) | 示例 demo 分支与可复用经验 |
| [Fork 指南](fork-guide.zh_CN.md) · [更新记录](CHANGELOG.zh_CN.md) | 下游工作流与版本历史 |
| [参与贡献](contribution/README.zh_CN.md) | 文档、提交与 Pull Request 约定 |

---

[参与贡献](../.github/CONTRIBUTING.zh_CN.md) · [获取帮助](../.github/SUPPORT.zh_CN.md) · [行为准则](../.github/CODE_OF_CONDUCT.zh_CN.md) · [安全说明](../.github/SECURITY.zh_CN.md) · [MIT 许可证](../LICENSE)
