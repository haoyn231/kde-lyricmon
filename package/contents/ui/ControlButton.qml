// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 弹窗里的圆形按钮。主按钮（播放/暂停）用强调色填充，其余是透明的悬停高亮
// —— 和 kde-perfmon 的 ActionButton 一个路子，只是这里更圆。
//
// 播放/暂停的图形是自己画的（三角形 / 两根竖条），不依赖图标着色：
// Kirigami.Icon 的 color 在部分环境里会被忽略、退回主题文字色，那样深色主题
// 就是浅色图形压在亮蓝上，对比只有 2:1。画出来的图形永远取 onAccent，稳。

import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

Item {
    id: button

    property string icon: ""
    // "play" / "pause"：画出来的图形，优先于 icon
    property string symbol: ""
    property bool actionEnabled: true
    property bool primary: false

    signal clicked

    readonly property int diameter: primary ? Math.round(Kirigami.Units.gridUnit * 2.6)
                                            : Math.round(Kirigami.Units.gridUnit * 2.1)
    readonly property color tone: button.actionEnabled ? Kirigami.Theme.textColor
                                                       : Kirigami.Theme.disabledTextColor
    readonly property color accent: Kirigami.Theme.highlightColor
    // 强调色深浅不定（有人用亮黄），按亮度挑一个对比色比直接用 highlightedTextColor 稳
    readonly property color onAccent: (0.299 * accent.r + 0.587 * accent.g + 0.114 * accent.b) > 0.62
                                      ? "#1b1e20" : "#ffffff"

    // 主按钮的底色：深色主题下把强调色压暗一点，浅色字形压上去更清楚
    readonly property bool lightText: (0.299 * tone.r + 0.587 * tone.g + 0.114 * tone.b) > 0.5
    readonly property color primaryFill: button.lightText ? Qt.darker(button.accent, 1.3)
                                                          : button.accent
    readonly property color glyphColor: button.primary ? button.onAccent : button.tone

    implicitWidth: diameter
    implicitHeight: diameter
    opacity: button.actionEnabled ? 1 : 0.45

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: {
            if (button.primary) {
                const fill = button.primaryFill;
                return Qt.rgba(fill.r, fill.g, fill.b, hover.hovered ? 1 : 0.92);
            }
            return Qt.rgba(button.tone.r, button.tone.g, button.tone.b,
                            hover.hovered ? 0.10 : 0.0);
        }

        Behavior on color {
            ColorAnimation { duration: Kirigami.Units.shortDuration }
        }
    }

    // ---------------- 图标（上一首 / 下一首）
    Kirigami.Icon {
        anchors.centerIn: parent
        visible: button.symbol === ""
        source: button.icon
        implicitWidth: Math.round(button.diameter * 0.52)
        implicitHeight: implicitWidth
        color: button.tone
        isMask: true
    }

    // ---------------- 画出来的播放/暂停
    Item {
        id: glyph

        anchors.centerIn: parent
        visible: button.symbol !== ""
        width: Math.round(button.diameter * 0.40)
        height: width

        // 暂停：两根圆角竖条
        Row {
            anchors.centerIn: parent
            spacing: Math.round(glyph.width * 0.22)
            visible: button.symbol === "pause"

            Repeater {
                model: 2
                delegate: Rectangle {
                    width: Math.round(glyph.width * 0.30)
                    height: Math.round(glyph.height * 0.86)
                    radius: Math.round(width * 0.3)
                    color: button.glyphColor
                }
            }
        }

        // 播放：一个三角形
        Shape {
            anchors.fill: parent
            visible: button.symbol === "play"
            // 三角形的视觉重心偏左，往右挪一点点才像居中
            x: Math.round(parent.width * 0.04)

            ShapePath {
                fillColor: button.glyphColor
                strokeWidth: 0
                startX: glyph.width * 0.16
                startY: glyph.height * 0.10
                PathLine { x: glyph.width * 0.94; y: glyph.height * 0.50 }
                PathLine { x: glyph.width * 0.16; y: glyph.height * 0.90 }
            }
        }
    }

    HoverHandler {
        id: hover
        cursorShape: button.actionEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    TapHandler {
        enabled: button.actionEnabled
        acceptedButtons: Qt.LeftButton
        onTapped: button.clicked()
    }

    Accessible.role: Accessible.Button
    Accessible.name: button.symbol !== "" ? button.symbol : button.icon
    Accessible.onPressAction: if (button.actionEnabled) button.clicked()
}
