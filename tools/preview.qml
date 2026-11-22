// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 开发工具：离屏渲染经典弹窗（由 lyricmon-preview 加载）。
//
// plasmashell 之外没有「弹窗」这一层，Theme 也拿不到 Plasma 配色（全是黑），
// 所以这里自己铺一张模拟弹窗底 + 一套配色，纯粹为了出图。生产 QML 里完全没有
// 这些覆盖。--light 换成浅色一套（README 主图用），--square 去掉圆角供合成用。

import QtQuick
import org.kde.kirigami as Kirigami
import "../package/contents/lib/Hao/LyricMon"
import "../package/contents/ui"

Rectangle {
    id: card

    readonly property bool light: typeof previewLight !== "undefined" ? previewLight : false
    readonly property bool square: typeof previewSquare !== "undefined" ? previewSquare : false

    radius: card.square ? 0 : 10
    color: card.light ? "#fcfcfc" : "#202325"

    Kirigami.Theme.inherit: false
    Kirigami.Theme.backgroundColor: card.light ? "#fcfcfc" : "#202325"
    Kirigami.Theme.textColor: card.light ? "#232627" : "#eff0f1"
    Kirigami.Theme.highlightColor: "#3daee9"
    Kirigami.Theme.highlightedTextColor: "#ffffff"
    Kirigami.Theme.negativeTextColor: "#da4453"
    Kirigami.Theme.neutralTextColor: "#f67400"
    Kirigami.Theme.disabledTextColor: card.light ? "#9aa0a4" : "#7f8c8d"

    // 预览环境里没有 main.qml，接线自己来
    Binding {
        target: Lyrics
        property: "player"
        value: Mpris.active
    }

    // --player 指定真实播放器时锁定它（README 主图用正在播放的音乐）；
    // 留空则按插件规则自动选主，私有会话里只有假播放器时出图依旧可复现
    Binding {
        target: Mpris
        property: "preferredService"
        value: typeof previewPlayer !== "undefined" ? previewPlayer : ""
    }

    FullRepresentation {
        anchors.fill: parent
        showControls: true
        layoutMode: "stacked"
    }
}
