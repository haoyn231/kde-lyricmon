// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 开发工具：离屏渲染面板上的紧凑形态。--two-lines 可以切到双行。

import QtQuick
import org.kde.kirigami as Kirigami
import "../package/contents/lib/Hao/LyricMon"
import "../package/contents/ui"

Item {
    id: root


    readonly property bool twoLines: typeof previewTwoLines !== "undefined" ? previewTwoLines : false
    readonly property bool dark: typeof previewDark !== "undefined" ? previewDark : false

    // 面板底色不来自 Kirigami（那是窗体内的配色），所以预览里自己铺一层，
    // 顺便把配色改成和真机深色主题一致，出图才和弹窗截图是一套
    Kirigami.Theme.inherit: false
    Kirigami.Theme.backgroundColor: dark ? "#1b1e20" : "#eff0f1"
    Kirigami.Theme.textColor: dark ? "#eff0f1" : "#232627"
    Kirigami.Theme.highlightColor: "#3daee9"
    Kirigami.Theme.highlightedTextColor: "#ffffff"
    Kirigami.Theme.disabledTextColor: dark ? "#7f8c8d" : "#9aa0a4"

    Binding {
        target: Lyrics
        property: "player"
        value: Mpris.active
    }

    // --player 指定真实播放器时锁定它；留空则自动选主
    Binding {
        target: Mpris
        property: "preferredService"
        value: typeof previewPlayer !== "undefined" ? previewPlayer : ""
    }

    // 面板底色，方便看清文字在面板上的观感
    Rectangle {
        anchors.fill: parent
        color: root.dark ? "#1b1e20" : Kirigami.Theme.backgroundColor
    }

    CompactRepresentation {
        anchors.centerIn: parent
        hasTrack: Mpris.active !== null
        twoLines: root.twoLines
        showIcon: true
        maxWidthUnits: 18
        hideWhenIdle: false
    }
}
