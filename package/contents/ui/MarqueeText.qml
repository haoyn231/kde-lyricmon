// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 一行「放不下就滚」的文字。
//
// 面板上的空间是按像素算的，歌词又常常比格子长，所以这里不用 elide 砍掉，
// 而是复制一份接在后面无缝滚动（和新闻跑马灯一个道理）。文字放得下时
// 完全静止，不浪费一点 CPU。

import QtQuick
import org.kde.kirigami as Kirigami

Item {
    id: marquee

    property string text: ""
    property font font: Kirigami.Theme.defaultFont
    property color color: Kirigami.Theme.textColor
    property int gap: Math.round(Kirigami.Units.gridUnit * 1.5)
    property real speed: 26          // 每秒滚多少像素，越大越快

    readonly property real naturalWidth: label.implicitWidth
    readonly property bool overflowing: naturalWidth > width + 1

    implicitWidth: naturalWidth
    implicitHeight: label.implicitHeight
    clip: true

    Row {
        id: ticker
        spacing: marquee.gap

        Text {
            id: label
            text: marquee.text
            color: marquee.color
            font: marquee.font
        }

        // 第二份只在需要滚动时出现，接在队尾，滚到尽头时正好补上
        Text {
            visible: marquee.overflowing
            text: marquee.text
            color: marquee.color
            font: marquee.font
        }
    }

    NumberAnimation {
        id: scroll
        target: ticker
        property: "x"
        from: 0
        to: -(marquee.naturalWidth + marquee.gap)
        duration: Math.max(3000, Math.round((marquee.naturalWidth + marquee.gap) / marquee.speed * 1000))
        loops: Animation.Infinite
        easing.type: Easing.Linear
        running: marquee.overflowing && marquee.text.length > 0
    }

    onTextChanged: {
        ticker.x = 0;
        if (scroll.running) {
            scroll.restart();
        }
    }

    onOverflowingChanged: {
        if (!overflowing) {
            ticker.x = 0;
        }
    }
}
