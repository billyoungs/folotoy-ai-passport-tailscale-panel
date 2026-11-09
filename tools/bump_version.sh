#!/usr/bin/env bash
# tools/bump_version.sh —— 递增 sdkconfig.defaults 里的应用版本号。
#
# 版本号是固件身份的一部分:它进 esp_app_desc(设备端状态页标题、串口开机日志),
# 也进 build/firmware/<sha>/manifest.json。留空 CONFIG_APP_PROJECT_VER 时 ESP-IDF
# 会退化成 "488beda-dirty" 这种 git 描述,同一份改动重复构建得到同一个字符串,
# 分不清"刷进去的是哪一版"。所以每次改动后要出镜像,先跑这个脚本再构建。
#
# 用法(仓库根目录):
#   ./tools/bump_version.sh            # np-0.1.1 -> np-0.1.2
#   ./tools/bump_version.sh --check    # 只打印当前版本,不改文件
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
conf="${repo_root}/sdkconfig.defaults"

line="$(grep -m1 '^CONFIG_APP_PROJECT_VER=' "$conf" || true)"
if [ -z "${line}" ]; then
    echo "ERROR: ${conf} 里没有 CONFIG_APP_PROJECT_VER" >&2
    exit 1
fi

ver="${line#CONFIG_APP_PROJECT_VER=\"}"
ver="${ver%\"}"
if ! printf '%s' "${ver}" | grep -Eq '^np-[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "ERROR: 版本号格式应为 np-MAJOR.MINOR.PATCH,当前为 '${ver}'" >&2
    exit 1
fi

if [ "${1:-}" = "--check" ]; then
    echo "${ver}"
    exit 0
fi

prefix="${ver%.*}"
patch="${ver##*.}"
next="${prefix}.$((patch + 1))"

# 只替换这一行,不重排配置;临时文件与原文件同目录,避免跨设备 mv 失败。
sed "s|^CONFIG_APP_PROJECT_VER=\"${ver}\"|CONFIG_APP_PROJECT_VER=\"${next}\"|" \
    "${conf}" > "${conf}.tmp"
mv "${conf}.tmp" "${conf}"
echo "${ver} -> ${next}"
