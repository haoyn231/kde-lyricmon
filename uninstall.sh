#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 haoyn231
# SPDX-License-Identifier: MIT
#
# 卸载小程序（只动当前用户的 Plasma 目录，不碰歌词缓存）。

set -euo pipefail

PLUGIN_ID="org.hao.lyricmon"
TARGET_ROOT="${XDG_DATA_HOME:-$HOME/.local/share}/plasma/plasmoids"
TARGET_DIR="$TARGET_ROOT/$PLUGIN_ID"

if [[ ! -e "$TARGET_DIR" && ! -L "$TARGET_DIR" ]]; then
    echo "没有安装：$TARGET_DIR"
    exit 0
fi

if command -v kpackagetool6 >/dev/null 2>&1; then
    kpackagetool6 --type Plasma/Applet --remove "$PLUGIN_ID" >/dev/null 2>&1 || true
fi
rm -rf "$TARGET_DIR"
echo "已删除：$TARGET_DIR"
echo "歌词缓存仍在 ${XDG_CACHE_HOME:-$HOME/.cache}/kde-lyricmon/，需要的话手动删除。"
