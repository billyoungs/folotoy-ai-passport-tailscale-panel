#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware]" >&2
}

run_static_checks() {
    local actionlint_bin
    local test_dir
    local dead_strip_flag

    # Apple ld does not accept --gc-sections; -dead_strip is the equivalent.
    dead_strip_flag="-Wl,--gc-sections"
    if [[ "$(uname -s)" == "Darwin" ]]; then
        dead_strip_flag="-Wl,-dead_strip"
    fi

    python3 tools/check_repo.py

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_demo_navigation.c main/demo_navigation.c \
        -o "${test_dir}/test_demo_navigation"
    "${test_dir}/test_demo_navigation"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_key_guard.c -o "${test_dir}/test_key_guard"
    "${test_dir}/test_key_guard"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_display_rounding.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_display_rounding"
    "${test_dir}/test_bsp_display_rounding"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_es8311_sleep_check.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_es8311_sleep_check"
    "${test_dir}/test_bsp_es8311_sleep_check"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/battery_stubs -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_battery.c -o "${test_dir}/test_bsp_battery"
    "${test_dir}/test_bsp_battery"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_button.c -o "${test_dir}/test_bsp_button"
    "${test_dir}/test_bsp_button"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_lvgl_init.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_lvgl_init"
    "${test_dir}/test_bsp_lvgl_init"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/audio_stubs -Icomponents/bsp/include -Icomponents/bsp/src \
        tests/test_bsp_audio_recovery.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_audio_recovery"
    "${test_dir}/test_bsp_audio_recovery"
    for demo in audio low_power ble wifi; do
        "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
            -ffunction-sections -fdata-sections -Itests/demo_stubs -Imain \
            "tests/test_demo_${demo}_runtime.c" "${dead_strip_flag}" \
            -o "${test_dir}/test_demo_${demo}_runtime"
        "${test_dir}/test_demo_${demo}_runtime"
    done
    # np_json.c 是 scanlist 的依赖;测它自己时不能再重复列一遍。
    for np in text time model form scanlist wifi_store; do
        "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
            "tests/test_np_${np}.c" "main/np_${np}.c" main/np_json.c \
            main/np_inventory.c \
            -o "${test_dir}/test_np_${np}"
        "${test_dir}/test_np_${np}"
    done
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_json.c main/np_json.c main/np_inventory.c \
        -o "${test_dir}/test_np_json"
    "${test_dir}/test_np_json"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_session.c main/np_session.c \
        -o "${test_dir}/test_np_session"
    "${test_dir}/test_np_session"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_link.c main/np_link.c \
        -o "${test_dir}/test_np_link"
    "${test_dir}/test_np_link"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_clock.c main/np_clock.c \
        -o "${test_dir}/test_np_clock"
    "${test_dir}/test_np_clock"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_jstream.c main/np_jstream.c \
        -o "${test_dir}/test_np_jstream"
    "${test_dir}/test_np_jstream"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_prompt.c main/np_prompt.c \
        -o "${test_dir}/test_np_prompt"
    "${test_dir}/test_np_prompt"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_errfmt.c main/np_errfmt.c \
        -o "${test_dir}/test_np_errfmt"
    "${test_dir}/test_np_errfmt"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_account.c main/np_account.c \
        -o "${test_dir}/test_np_account"
    "${test_dir}/test_np_account"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_menu.c main/np_menu.c \
        -o "${test_dir}/test_np_menu"
    "${test_dir}/test_np_menu"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_display_cfg.c main/np_display_cfg.c \
        -o "${test_dir}/test_np_display_cfg"
    "${test_dir}/test_np_display_cfg"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_logo.c main/np_logo.c \
        -o "${test_dir}/test_np_logo"
    "${test_dir}/test_np_logo"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_np_mem.c main/np_mem.c \
        -o "${test_dir}/test_np_mem"
    "${test_dir}/test_np_mem"
    PYTHONDONTWRITEBYTECODE=1 python3 assets/fonts/extract_chars.py --check \
        main/np_text.c main/np_time.c main/np_model.c main/np_link.c main/np_form.c \
        main/np_clock.c main/np_clock.h \
        main/np_jstream.c main/np_jstream.h \
        main/np_json.c main/np_json.h main/np_scanlist.c main/np_scanlist.h \
        main/np_wifi_store.c main/np_wifi_store.h \
        main/np_session.c main/np_session.h \
        main/np_prompt.c main/np_prompt.h \
        main/np_menu.c main/np_menu.h \
        main/np_logo.c main/np_logo.h \
        main/np_mem.c main/np_mem.h \
        main/np_errfmt.c main/np_errfmt.h \
        main/np_account.c main/np_account.h \
        main/np_display_cfg.c main/np_display_cfg.h \
        main/np_icons.c main/np_icons.h \
        main/app_cfg.c main/app_wifi.c main/app_prov.c main/ts_api.c \
        main/netpanel_ui.c main/netpanel_app.c main/main.c \
        main/np_text.h main/np_time.h main/np_model.h main/np_link.h main/np_form.h \
        main/app_cfg.h main/app_wifi.h main/app_prov.h main/ts_api.h \
        main/netpanel_ui.h main/netpanel_app.h \
        tests/test_np_text.c tests/test_np_time.c tests/test_np_model.c \
        tests/test_np_form.c tests/test_np_json.c tests/test_np_scanlist.c \
        tests/test_np_wifi_store.c \
        tests/test_np_session.c tests/test_np_prompt.c \
        tests/test_np_menu.c tests/test_np_logo.c tests/test_np_mem.c \
        tests/test_np_display_cfg.c \
        tests/test_np_link.c tests/test_np_clock.c tests/test_np_jstream.c \
        tests/test_np_errfmt.c tests/test_np_account.c
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_battery_profile.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_deep_sleep_contract.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_key_wake_contract.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_icon_contract.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_logo_contract.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_check_repo.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_verify_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_archive_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_install_passport_skills.py
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

# 镜像里的版本号必须等于 sdkconfig.defaults 钉住的那个。只写
# CONFIG_APP_PROJECT_VER 而不开 CONFIG_APP_PROJECT_VER_FROM_CONFIG 时,kconfig
# 会静默丢弃字符串,镜像仍然带 git 描述 —— 构建照样 PASS,交付却分不出版本。
pinned_version() {
    sed -n 's/^CONFIG_APP_PROJECT_VER="\(.*\)"$/\1/p' "${repo_root}/sdkconfig.defaults"
}

check_embedded_version() {
    local merged_bin="$1" expected actual manifest

    if ! grep -qx "CONFIG_APP_PROJECT_VER_FROM_CONFIG=y" "${repo_root}/sdkconfig.defaults"; then
        echo "ERROR: sdkconfig.defaults 未打开 CONFIG_APP_PROJECT_VER_FROM_CONFIG,应用版本会退化成 git 描述。" >&2
        return 1
    fi
    expected="$(pinned_version)"
    if [ -z "${expected}" ]; then
        echo "ERROR: sdkconfig.defaults 未设置 CONFIG_APP_PROJECT_VER。" >&2
        return 1
    fi

    manifest="${repo_root}/build/firmware/$(sha256sum "${merged_bin}" | cut -d' ' -f1)/manifest.json"
    if [ ! -f "${manifest}" ]; then
        echo "ERROR: 找不到归档清单 ${manifest}。" >&2
        return 1
    fi
    actual="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["app_descriptor"]["version"])' "${manifest}")"
    if [ "${actual}" != "${expected}" ]; then
        echo "ERROR: 镜像内版本 '${actual}' 与期望 '${expected}' 不一致。" >&2
        return 1
    fi
    echo "Firmware version: PASS (${actual})"
}

run_firmware_checks() (
    local validation_build_dir delivered stale

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    SDKCONFIG_DEFAULTS="${repo_root}/sdkconfig.defaults" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    PYTHONDONTWRITEBYTECODE=1 python3 tools/archive_firmware.py create \
        "${validation_build_dir}" --archive-root "${repo_root}/build/firmware"
    mkdir -p "${repo_root}/build"
    # 先验镜像内版本再落地:名字带版本号,刷错的成本比留着旧版本低。
    check_embedded_version "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    delivered="${repo_root}/build/FoloToy-AI-Passport-full-$(pinned_version).bin"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" "${delivered}"
    for stale in "${repo_root}"/build/FoloToy-AI-Passport-full*.bin; do
        [ -f "${stale}" ] || continue
        [ "${stale}" = "${delivered}" ] && continue
        echo "Firmware prune: 删除上一版 ${stale#"${repo_root}/"}"
        rm -f "${stale}"
    done
    echo "Delivered: ${delivered#"${repo_root}/"}"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
