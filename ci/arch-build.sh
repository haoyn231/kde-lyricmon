#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 haoyn231
# SPDX-License-Identifier: MIT
#
# 在 Arch Linux 容器里编译、跑单元测试、lint QML，并做一次离屏渲染冒烟。
# 供 .github/workflows/ci.yml 与 release.yml 调用。
set -euo pipefail

pacman -Syu --noconfirm --needed
pacman -S --noconfirm --needed \
    base-devel cmake \
    qt6-base qt6-declarative qt6-svg \
    kirigami libplasma ttf-dejavu

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

./build/lyrics-tests

# QML 静态检查：能抓出拼错的属性、漏掉的 import、不可达的分支
/usr/lib/qt6/bin/qmllint -I package/contents/lib \
    package/contents/ui/*.qml package/contents/ui/config/*.qml tools/preview*.qml

python3 -c "import json,sys; json.load(open('package/metadata.json')); print('metadata.json ok')"

# 冒烟渲染：容器里没有会话总线，所以走"没有播放器"的待机态 —— 只要布局能加载、
# 能出图、且没有 QML 运行时错误，就说明这套 QML 不是坏的。
export QT_QPA_PLATFORM=offscreen
./build/lyrics-preview tools/preview.qml -o /tmp/lyrics-popup.png \
    --width 380 --height 430 --wait 1200 > /tmp/lyrics-preview.log 2>&1 || {
    cat /tmp/lyrics-preview.log
    exit 1
}
test -s /tmp/lyrics-popup.png

if grep -qE "\.qml:[0-9]+:[0-9]+: (TypeError|ReferenceError|Cannot)" /tmp/lyrics-preview.log; then
    echo "QML runtime errors while rendering:" >&2
    grep -E "\.qml:[0-9]+:[0-9]+:" /tmp/lyrics-preview.log >&2
    exit 1
fi

echo "构建、单元测试、QML lint 与冒烟渲染均通过。"
