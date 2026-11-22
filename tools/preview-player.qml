// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 开发工具：离屏渲染「播放器」式弹窗（封面在左、歌词在右）。
//
// plasmashell 之外没有「弹窗」这一层，Theme 也拿不到 Plasma 配色（全是黑），
// 所以这里自己铺一张模拟弹窗底 + 一套 Breeze Dark 配色，纯粹为了出图。
// 生产 QML 里完全没有这些覆盖。

import QtQuick
import org.kde.kirigami as Kirigami
import "../package/contents/lib/Hao/LyricMon"
import "../package/contents/ui"

Rectangle {
    id: card

    radius: 10
    color: "#202325"

    Kirigami.Theme.inherit: false
    Kirigami.Theme.backgroundColor: "#202325"
    Kirigami.Theme.textColor: "#eff0f1"
    Kirigami.Theme.highlightColor: "#3daee9"
    Kirigami.Theme.highlightedTextColor: "#ffffff"
    Kirigami.Theme.negativeTextColor: "#da4453"
    Kirigami.Theme.neutralTextColor: "#f67400"
    Kirigami.Theme.disabledTextColor: "#7f8c8d"

    // 预览环境里没有 main.qml，接线自己来
    Binding {
        target: Lyrics
        property: "player"
        value: Mpris.active
    }

    // 出图要可重复：如果假播放器在总线上就固定用它，没有则自动回退
    Binding {
        target: Mpris
        property: "preferredService"
        value: "org.mpris.MediaPlayer2.fakeplayer"
    }

    FullRepresentation {
        anchors.fill: parent
        showControls: true
        layoutMode: "player"
    }
}
