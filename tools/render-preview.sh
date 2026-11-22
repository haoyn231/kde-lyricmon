#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 haoyn231
# SPDX-License-Identifier: MIT
#
# 出图脚本：用「正在播放的真实 MPRIS 播放器」把两种弹窗布局离屏渲染成浅色 2 倍图，
# 再合成成 README 顶部并排的两张主图。
#
#   ./tools/render-preview.sh
#   PLAYER=org.mpris.MediaPlayer2.vlc ./tools/render-preview.sh
#   SCALE=1 ./tools/render-preview.sh       # 1 倍图，快速看布局
#
# 图里的曲目、封面、歌词、进度全部来自真实播放器（不是假播放器），所以每次
# 跑出来的内容都不一样。主图的「弹窗 + 模糊壁纸 + 圆角投影」由
# tools/compose_hero.py 合成 —— 它是 readme-hero-shot skill 里那份脚本的副本，
# 放进项目是为了不依赖 ~/.workbuddy 下的路径。
#
# 两种布局必须按各自的比例渲染：经典是竖版 486x545，播放器布局是宽版
# 760x470。后者压成竖版会把右侧的长歌词挤掉（实测长句会被 ListView 裁掉）。
# 两张合成图高度接近，README 里按 37% / 59% 分配宽度就能并排对齐。

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

PREVIEW="$ROOT/build/lyricmon-preview"
COMPOSE="$ROOT/tools/compose_hero.py"
OUT="$ROOT/docs/images"
SCALE="${SCALE:-2}"

if [[ ! -x "$PREVIEW" || ! -f "$COMPOSE" ]]; then
    echo "先编译：cmake --build build -j\$(nproc)" >&2
    exit 1
fi

# 合成需要 Pillow，系统 python 通常没装，优先找带 PIL 的 venv
PY="${PY:-}"
if [[ -z "$PY" ]]; then
    for cand in "$HOME/.workbuddy/binaries/python/envs/default/bin/python" python3; do
        if "$cand" -c 'import PIL' >/dev/null 2>&1; then PY="$cand"; break; fi
    done
fi
if [[ -z "$PY" ]]; then
    echo "找不到带 Pillow 的 python：用 PY=/path/to/python 指定一个" >&2
    exit 1
fi

# 选播放器：PLAYER 优先；否则挑正在播放、且不是浏览器插件的那个。
# 服务名去掉 .instance* 后缀，插件注册表会按 baseName 匹配，播放器重启后仍能认出来。
PLAYER="${PLAYER:-}"
if [[ -z "$PLAYER" ]]; then
    if ! command -v playerctl >/dev/null; then
        echo "没装 playerctl，无法自动挑播放器；用 PLAYER=org.mpris.MediaPlayer2.xxx 指定" >&2
        exit 1
    fi
    while read -r name; do
        [[ -n "$name" ]] || continue
        case "$name" in
            *chromium*|*plasma-browser-integration*|*playerctld*) continue ;;
        esac
        [[ "$(playerctl -p "$name" status 2>/dev/null || true)" == "Playing" ]] || continue
        PLAYER="org.mpris.MediaPlayer2.${name%%.instance*}"
        break
    done < <(playerctl -l 2>/dev/null || true)
fi
if [[ -z "$PLAYER" ]]; then
    echo "没有找到正在播放的音乐播放器；用 PLAYER=org.mpris.MediaPlayer2.xxx 指定" >&2
    exit 1
fi
echo "→ 播放器：$PLAYER"

mkdir -p "$OUT"

# 不启动假播放器、不建私有 D-Bus：直接连当前会话总线，数据才是真实播放器给的。
# 清空平台主题，避免跟随系统深色配色；浅色由 --light 与 preview*.qml 的覆盖决定。
render() {
    local out="$1" qml="$2" width="$3" height="$4"
    env -u QT_QPA_PLATFORMTHEME \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_SCALE_FACTOR="$SCALE" \
        timeout 90 "$PREVIEW" "$qml" -o "$out" \
        --width "$width" --height "$height" --wait 4000 \
        --light --square --player "$PLAYER" 2>&1 | grep -E '^saved' || true
    if [[ ! -s "$out" ]]; then
        echo "渲染失败，没有产出 $out" >&2
        exit 1
    fi
}

compose() {
    "$PY" "$COMPOSE" --popup "$1" --out "$2" --theme light
}

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "→ 经典布局（竖版 486x545）"
render "$TMP/stacked.png" tools/preview.qml 486 545
compose "$TMP/stacked.png" "$OUT/screenshot.png"

echo "→ 播放器布局（宽版 760x470）"
render "$TMP/player.png" tools/preview-player.qml 760 470
compose "$TMP/player.png" "$OUT/screenshot-player.png"

echo "完成：$OUT/screenshot.png、$OUT/screenshot-player.png"
