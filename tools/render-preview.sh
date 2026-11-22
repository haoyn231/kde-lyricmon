#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 haoyn231
# SPDX-License-Identifier: MIT
#
# 出图脚本：起一个假播放器，把两种弹窗布局和两种面板形态离屏渲染成 PNG，
# 写进 docs/images/。所有图片都能用这一条命令复现。
#
#   ./tools/render-preview.sh          2 倍图（默认，README 用的就是这套）
#   SCALE=1 ./tools/render-preview.sh  1 倍图（快速看布局）
#
# 2 倍图靠 QT_SCALE_FACTOR，是真正的高 DPI 渲染；先渲染再放大只会得到糊图。

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

# 离屏渲染没有平台主题：不给图标主题的话 Kirigami.Icon 会画成空白
export QT_QPA_PLATFORM=offscreen
export QT_QPA_PLATFORMTHEME=kde
export QT_SCALE_FACTOR="$SCALE"

pkill -f '[l]yrics-fake-player' 2>/dev/null || true
sleep 0.3
trap 'pkill -f "[l]yrics-fake-player" 2>/dev/null || true' EXIT

"$PLAYER" --title Yellow --artist Coldplay --album Parachutes \
          --duration 267 --position 100 --cover tools/assets/cover-demo.png \
          >/tmp/lyrics-fake.log 2>&1 &
sleep 1.2

echo "→ 弹窗（经典布局）"
"$PREVIEW" tools/preview.qml -o "$OUT/screenshot.png" \
    --width 380 --height 430 --wait 3500 >/dev/null

echo "→ 弹窗（播放器布局）"
"$PREVIEW" tools/preview-player.qml -o "$OUT/popup-player.png" \
    --width 760 --height 470 --wait 3500 >/dev/null

echo "→ 面板（单行）"
"$PREVIEW" tools/preview-compact.qml -o /tmp/panel-raw.png \
    --width 430 --height 48 --wait 2500 --dark >/dev/null
magick /tmp/panel-raw.png -fuzz 2% -trim +repage \
    -bordercolor '#1b1e20' -border 48x22 "$OUT/panel.png"

echo "→ 面板（双行）"
"$PREVIEW" tools/preview-compact.qml -o /tmp/panel-raw2.png \
    --width 620 --height 66 --wait 2500 --dark --two-lines >/dev/null
magick /tmp/panel-raw2.png -fuzz 2% -trim +repage \
    -bordercolor '#1b1e20' -border 48x22 "$OUT/panel-two-lines.png"

echo "完成：$OUT/{screenshot,popup-player,panel,panel-two-lines}.png"
