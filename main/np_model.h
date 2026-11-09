// main/np_model.h —— 设备快照数据模型与轮询控制器(纯逻辑)。
// 设备端由 ts_api 填充;UI 只读。上限固定,避免运行期动态扩容挤占无 PSRAM 的内部 RAM。
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define NP_MAX_DEVICES 24
#define NP_NAME_LEN    33
#define NP_ADDR_LEN    16
#define NP_OS_LEN      16
#define NP_VER_LEN     24
#define NP_OWNER_LEN   40
#define NP_ID_LEN      24   // 详情接口按它寻址:十进制设备号约 19~20 位,16 进制 nodeId 也放得下
#define NP_TAGS_LEN    20   // 标签合并成一行展示,空格分隔

typedef struct {
    char name[NP_NAME_LEN];       // 已按字体清表过滤的展示名(hostname 优先)
    char addr[NP_ADDR_LEN];       // 首选 IPv4(100.x)
    char os[NP_OS_LEN];
    char ver[NP_VER_LEN];         // clientVersion
    char owner[NP_OWNER_LEN];
    char id[NP_ID_LEN];           // 拉详情用;不是机密,但不外传
    char tags[NP_TAGS_LEN];       // 节点标签(受限节点常有,普通机器为空)
    int64_t last_seen;            // 断线时刻(0 = 在线/未知),见 np_seen_disconnect_at
    int64_t expires;              // 节点密钥到期时间
    int64_t created;              // 加入 tailnet 的时刻
    int8_t ctrl;                  // connectedToControl: 1 连着 / 0 断了 / -1 响应没给
    bool online;
    bool authorized;
    bool external;                // 他网分享进来的节点
    bool update_available;
    bool key_expiry_off;          // keyExpiryDisabled:密钥不会到期
    bool blocks_incoming;         // 拒绝入站连接(客户端侧开关)
    bool ephemeral;               // 临时节点,断开即消失
} np_device_t;

typedef struct {
    np_device_t items[NP_MAX_DEVICES];
    size_t count;             // 这次真放进列表的台数(不超过 NP_MAX_DEVICES)
    uint32_t total;           // 服务端一共有几台;大于 count 时底部要说实话
    uint64_t version;         // 每次成功同步 +1,UI 据此跳过重绘
} np_list_t;

// 清空列表(不释放内存;结构体本身是静态/调用方持有)。
void np_list_clear(np_list_t *list);

// ---------------- 单台设备的扩展信息(按需另拉一次 fields=all) ----------------
// 列表接口默认字段集里没有路由/DERP/发行版,这些只有单台详情才给。一次一台,
// 用静态缓冲承接,不进双缓冲列表 —— 24 台都带路由表会把内部堆吃光。
#define NP_ROUTE_LEN 20
#define NP_ROUTE_MAX 3
typedef struct {
    char distro[NP_OS_LEN + 16];               // "debian 12 bookworm"
    char derp[8];                              // 首选 DERP 区域号
    char routes_en[NP_ROUTE_MAX][NP_ROUTE_LEN];   // 已生效的子网路由
    char routes_adv[NP_ROUTE_MAX][NP_ROUTE_LEN];  // 已宣告但可能未获批
    uint8_t n_en, n_adv;
    int16_t derp_ms;                           // 首选 DERP 时延,-1 = 没测到
    uint16_t endpoints;                        // 候选直连端点数(0 = 只能中继)
    bool have_conn;                            // 响应里带 clientConnectivity
    bool mapping_varies;                       // NAT 映射随目的地址变化(难直连)
    bool ssh;                                  // 开了 Tailscale SSH
} np_detail_t;

void np_detail_clear(np_detail_t *d);

// 排序:在线优先,其后按展示名字节序。sort 不改变 version。
void np_list_sort(np_list_t *list);

// 统计在线数。
size_t np_list_online_count(const np_list_t *list);

// ---------------- lastSeen 语义 ----------------
// 控制面对"还连着"的节点并不给断线时刻:字段可能没有,可能是 Go 的零值时间
// (0001-01-01T00:00:00Z),也可能仍在按连接心跳刷新。只看"字段有没有"会把长
// 连接的机器判成离线。折算规则:早于 NP_TIME_FLOOR_EPOCH 的时刻、时钟没同步
// (now<=0)、以及时刻落在宽限期内的,都算"没有断线",返回 0 = 在线。
#define NP_TIME_FLOOR_EPOCH 946684800LL   // 2000-01-01T00:00:00Z,早于此的时刻一律不算数
#define NP_SEEN_GRACE_S     900LL         // 15 分钟内报过到 = 还连着
int64_t np_seen_disconnect_at(int64_t now, int64_t last_seen);

// ---------------- 在线判定(优先控制面) ----------------
// 控制面对"还连着"的节点干脆不给 lastSeen(文档:connectedToControl 为真时省略),
// 所以 connectedToControl 才是直接答案,lastSeen 折算只是它缺席时的退路。
// ctrl: 1 = 控制面说连着,0 = 说不连着,-1 = 响应里没这个字段(老接口/外部节点)。
// disconnect_at 为 np_seen_disconnect_at 的结果。
bool np_device_is_online(int8_t ctrl, int64_t disconnect_at);

// ---------------- 快照新旧(渲染时用) ----------------
// online 是"抓取那一刻"的事实,不是持续状态:链路断了、或者已经超过这一轮同步
// 自己的窗口,手上这份就不再描述现在。宽限期是给节拍留的余量,窗口算作
// NP_SNAPSHOT_GRACE_MS + 2×当前退避,否则退避到上限(1 小时)的正常间隔会被误判成过期。
// fetched_ms = 换快照那一刻的单调毫秒(0 = 本轮开机还没同步成功过)。
#define NP_SNAPSHOT_GRACE_MS 60000LL
bool np_snapshot_fresh(int64_t now_ms, int64_t fetched_ms, bool link_up,
                       uint32_t retry_delay_s);

// ---------------- 操作系统字形 ----------------
// 名称前那一枚图标用私有区码点表示,字形本体在 assets/icons 生成的点阵字体里
// (np_icon_font)。子集字体没有这些码点,所以只允许在带图标字体的标签上使用。
// 返回 0 = 认不出来,不画图标。os 为控制面给的字符串(linux/windows/darwin/…)。
uint32_t np_os_glyph(const char *os);

#define NP_GLYPH_LINUX   0xE000u  // 企鹅
#define NP_GLYPH_WINDOWS 0xE001u  // 视窗
#define NP_GLYPH_APPLE   0xE002u  // 苹果
#define NP_GLYPH_ANDROID 0xE003u  // 安卓
#define NP_GLYPH_OTHER   0xE004u  // 认得出是机器但说不清系统(NAS/路由之类)

// ---------------- Wi-Fi 信号分档(界面点亮几段) ----------------
// 状态栏不显示 dBm 数字,只按强度点亮 0..3 段:0 = 未连接(app_wifi_rssi 约定)。
int np_signal_bars(int8_t rssi);

// ---------------- 双缓冲交接(poll 任务写 → UI 任务读) ----------------
// 全量拷贝会压爆任务栈,交接用指针互换;读侧在锁内取 active 指针。
void np_lists_init(void);
np_list_t *np_lists_active(void); // UI 只读(渲染锁内)
np_list_t *np_lists_back(void);   // 数据任务写
void np_lists_swap(void);         // 数据任务在锁内调用

// ---------------- 轮询控制器 ----------------
// 成功后回到基准间隔;失败指数退避(基准×2^n,封顶上限)。时间以"秒"注入,便于主机测试。
typedef struct {
    uint32_t base_s;
    uint32_t cap_s;
    uint32_t current_delay_s;
    int64_t  next_run_s;          // <=0 表示立即
    bool     net_up;              // 网络事件驱动置位
} np_poll_t;

void np_poll_init(np_poll_t *p, uint32_t base_s, uint32_t cap_s);
// 网络就绪前恒不就绪;到点返回 true 并自动顺延一个 current_delay。
bool np_poll_due(np_poll_t *p, int64_t now_s);
void np_poll_on_success(np_poll_t *p, int64_t now_s);
void np_poll_on_fail(np_poll_t *p, int64_t now_s);
void np_poll_set_net(np_poll_t *p, bool up);
