#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 haoyn231
# SPDX-License-Identifier: MIT
#
# 编译 C++ 插件并装到当前用户的 Plasma 目录（不需要 root）。
#   ./install.sh            编译 + 安装 / 升级
#   ./install.sh --restart  安装后重启 plasmashell
#   ./install.sh --symlink  软链接安装，改 QML 即时生效（C++ 仍需重新编译）
#   ./install.sh --no-build 只安装，不编译（已经编译过）

set -euo pipefail

PLUGIN_ID="org.hao.lyricmon"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_DIR="$ROOT/package"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
TARGET_ROOT="${XDG_DATA_HOME:-$HOME/.local/share}/plasma/plasmoids"
TARGET_DIR="$TARGET_ROOT/$PLUGIN_ID"

RESTART=0
SYMLINK=0
BUILD=1

for arg in "$@"; do
    case "$arg" in
        --restart) RESTART=1 ;;
        --symlink) SYMLINK=1 ;;
        --no-build) BUILD=0 ;;
        -h|--help)
            sed -n '3,10p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "未知参数：$arg" >&2
            exit 2
            ;;
    esac
done

if [[ ! -f "$PKG_DIR/metadata.json" ]]; then
    echo "找不到 $PKG_DIR/metadata.json，请在仓库根目录运行。" >&2
    exit 1
fi

if ! command -v plasmashell >/dev/null 2>&1; then
    echo "警告：没找到 plasmashell，确认这是 KDE Plasma 6 环境。" >&2
fi

if [[ $BUILD -eq 1 ]]; then
    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

PLUGIN_SO="$PKG_DIR/contents/lib/Hao/LyricMon/liblyricmonplugin.so"
if [[ ! -f "$PLUGIN_SO" ]]; then
    echo "缺少 $PLUGIN_SO：先不带 --no-build 跑一次。" >&2
    exit 1
fi

mkdir -p "$TARGET_ROOT"

if [[ $SYMLINK -eq 1 ]]; then
    rm -rf "$TARGET_DIR"
    ln -s "$PKG_DIR" "$TARGET_DIR"
    echo "已用软链接安装：$TARGET_DIR -> $PKG_DIR"
else
    if command -v kpackagetool6 >/dev/null 2>&1; then
        if kpackagetool6 --type Plasma/Applet --upgrade "$PKG_DIR" >/dev/null 2>&1; then
            echo "已通过 kpackagetool6 升级：$PLUGIN_ID"
        elif kpackagetool6 --type Plasma/Applet --install "$PKG_DIR" >/dev/null 2>&1; then
            echo "已通过 kpackagetool6 安装：$PLUGIN_ID"
        else
            echo "kpackagetool6 失败，退回直接复制。" >&2
            rm -rf "$TARGET_DIR"
            cp -r "$PKG_DIR" "$TARGET_DIR"
            echo "已复制到：$TARGET_DIR"
        fi
    else
        rm -rf "$TARGET_DIR"
        cp -r "$PKG_DIR" "$TARGET_DIR"
        echo "已复制到：$TARGET_DIR"
    fi
fi

cat <<EOF

安装完成。接下来：
  1. 右键面板 → 进入编辑模式 → 添加小组件
  2. 搜索「桌面歌词」把它拖到面板上

调试：plasmawindowed $PLUGIN_ID
歌词缓存：${XDG_CACHE_HOME:-$HOME/.cache}/kde-lyricmon/
EOF

if [[ $RESTART -eq 1 ]]; then
    echo
    echo "正在重启 plasmashell…"
    systemctl --user restart plasma-plasmashell.service
fi
