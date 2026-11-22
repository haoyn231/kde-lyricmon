#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 haoyn231
# SPDX-License-Identifier: MIT
#
# 出图脚本：起一个假播放器，把面板的两种形态离屏渲染成 PNG 写进 docs/images/，
# 另外把弹窗渲染到 /tmp 方便看排版。
#
# 注意：docs/images/screenshot.png 是在真实 plasmashell 会话里截的（真实主题、
# 真实字体渲染），不能由这个脚本复现。要更新它：
#   ./install.sh --symlink && plasmawindowed org.hao.lyrics
# 然后在弹窗上截图，裁掉窗口阴影。
#
# 用法：
#   ./tools/render-preview.sh
#   SCALE=1 ./tools/render-preview.sh     1 倍图（快速看布局）

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

PREVIEW="$ROOT/build/lyrics-preview"
PLAYER="$ROOT/build/lyrics-fake-player"
SCALE="${SCALE:-2}"
OUT="$ROOT/docs/images"

if [[ ! -x "$PREVIEW" || ! -x "$PLAYER" ]]; then
    echo "先编译：cmake --build build -j\$(nproc)" >&2
    exit 1
fi

mkdir -p "$OUT"

# 预览器没有平台主题时找不到图标主题，图标会画成空白
export QT_QPA_PLATFORM=offscreen
export QT_QPA_PLATFORMTHEME=kde

pkill -f '[l]yrics-fake-player' 2>/dev/null || true
sleep 0.3
trap 'pkill -f "[l]yrics-fake-player" 2>/dev/null || true' EXIT

"$PLAYER" --title Yellow --artist Coldplay --album Parachutes \
          --duration 267 --position 100 --cover tools/assets/cover-demo.png \
          >/tmp/lyrics-fake.log 2>&1 &
sleep 1

echo "→ 面板（单行）"
"$PREVIEW" tools/preview-compact.qml -o /tmp/panel-raw.png \
    --width 430 --height 48 --wait 2500 --scale "$SCALE" --dark >/dev/null
magick /tmp/panel-raw.png -fuzz 2% -trim +repage \
    -bordercolor '#1b1e20' -border 48x22 "$OUT/panel.png"

echo "→ 面板（双行）"
"$PREVIEW" tools/preview-compact.qml -o /tmp/panel-raw2.png \
    --width 620 --height 66 --wait 2500 --scale "$SCALE" --dark --two-lines >/dev/null
magick /tmp/panel-raw2.png -fuzz 2% -trim +repage \
    -bordercolor '#1b1e20' -border 48x22 "$OUT/panel-two-lines.png"

echo "→ 弹窗（浅色离屏，仅用于看排版）"
"$PREVIEW" tools/preview.qml -o /tmp/popup-preview.png \
    --width 380 --height 430 --wait 3500 --scale "$SCALE" >/dev/null

echo "完成："
echo "  $OUT/panel.png"
echo "  $OUT/panel-two-lines.png"
echo "  /tmp/popup-preview.png（排版检查；README 的 screenshot.png 来自真实会话）"
