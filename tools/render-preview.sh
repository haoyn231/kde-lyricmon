#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 haoyn231
# SPDX-License-Identifier: MIT
#
# 出图脚本：把两种弹窗布局和两种面板形态离屏渲染成浅色 PNG，写进 docs/images/。
#
#   ./tools/render-preview.sh          2 倍图（默认，README 用的就是这套）
#   SCALE=1 ./tools/render-preview.sh  1 倍图（快速看布局）
#
# 每张图都在独立的 D-Bus 会话里渲染：只挂一个假播放器，所以画面里不会冒出
# 别的播放器名字，歌词和进度也完全可复现。
#
# README 顶部的主图（docs/images/screenshot.png）不在这个脚本里：它是用
# readme-hero-shot 那套流程「弹窗 + 模糊壁纸 + 圆角投影」合成出来的，源图用
#   ./build/lyricmon-preview tools/preview.qml --light --square --width 486 --height 525
# 渲染，再交给合成脚本。别用这里的普通弹窗图覆盖它。

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

PREVIEW="$ROOT/build/lyricmon-preview"
PLAYER="$ROOT/build/lyricmon-fake-player"
SCALE="${SCALE:-2}"
OUT="$ROOT/docs/images"

if [[ ! -x "$PREVIEW" || ! -x "$PLAYER" ]]; then
    echo "先编译：cmake --build build -j\$(nproc)" >&2
    exit 1
fi

mkdir -p "$OUT"

# 一张图一个私有会话：只有假播放器，所以画面里不会冒出别的播放器名字。
# 不用 dbus-run-session：它要等总线上的客户端全部退出，一旦有进程没收拾干净
# 就会一直挂着。这里自己起 dbus-daemon，渲染完按 PID 收掉，可控。
shot() {
    local out="$1" qml="$2"
    shift 2

    local addr dpid player
    mapfile -t _bus < <(timeout 5 dbus-daemon --session --fork --print-address=1 --print-pid=1)
    addr="${_bus[0]}"
    dpid="${_bus[1]}"

    DBUS_SESSION_BUS_ADDRESS="$addr" "$PLAYER" --title Yellow --artist Coldplay \
        --album Parachutes --duration 267 --position 96 \
        --cover tools/assets/cover-demo.png >/dev/null 2>&1 &
    player=$!
    sleep 1.3

    # SHOT_PT 决定要不要带平台主题：
    #   弹窗不带 —— 默认浅色调色板，符号图标才是深色，符合浅色弹窗；
    #   面板带   —— Kirigami.Icon 没有图标主题时干脆不画，耳机图标会消失。
    DBUS_SESSION_BUS_ADDRESS="$addr" QT_SCALE_FACTOR="$SCALE" QT_QPA_PLATFORM=offscreen \
        QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME="${SHOT_PT:-}" \
        timeout 40 "$PREVIEW" "$qml" -o "$out" --light "$@" 2>&1 | grep -E "^saved" || true

    kill "$player" 2>/dev/null || true
    kill "$dpid" 2>/dev/null || true
}

SHOT_PT=""
echo "→ 弹窗（播放器布局）"
shot "$OUT/popup-player.png" tools/preview-player.qml --width 760 --height 470 --wait 4000

SHOT_PT="kde"
echo "→ 面板（单行）"
shot /tmp/panel-raw.png tools/preview-compact.qml --width 430 --height 48 --wait 2500
magick /tmp/panel-raw.png -fuzz 2% -trim +repage \
    -bordercolor '#eff0f1' -border 48x22 "$OUT/panel.png"

echo "→ 面板（双行）"
shot /tmp/panel-raw2.png tools/preview-compact.qml --width 620 --height 66 --wait 2500 --two-lines
magick /tmp/panel-raw2.png -fuzz 2% -trim +repage \
    -bordercolor '#eff0f1' -border 48x22 "$OUT/panel-two-lines.png"

echo "完成：$OUT/popup-player.png、panel.png、panel-two-lines.png"
echo "主图 screenshot.png 由 readme-hero-shot 流程合成，不在这里重出。"
