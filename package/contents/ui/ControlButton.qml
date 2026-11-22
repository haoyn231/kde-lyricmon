// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 弹窗里的圆形图标按钮。主按钮（播放/暂停）用强调色填充，其余是透明的
// 悬停高亮 —— 和 kde-perfmon 的 ActionButton 一个路子，只是这里更圆。

import QtQuick
import org.kde.kirigami as Kirigami

Item {
    id: button

    property string icon: ""
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

    implicitWidth: diameter
    implicitHeight: diameter
    opacity: button.actionEnabled ? 1 : 0.5

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: {
            if (button.primary) {
                return Qt.rgba(button.accent.r, button.accent.g, button.accent.b,
                                hover.hovered ? 1 : 0.9);
            }
            return Qt.rgba(button.tone.r, button.tone.g, button.tone.b,
                            hover.hovered ? 0.10 : 0.0);
        }

        Behavior on color {
            ColorAnimation { duration: Kirigami.Units.shortDuration }
        }
    }

    Kirigami.Icon {
        anchors.centerIn: parent
        source: button.icon
        implicitWidth: Math.round(button.diameter * (button.primary ? 0.46 : 0.52))
        implicitHeight: implicitWidth
        color: button.primary ? button.onAccent : button.tone
        isMask: true
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
    Accessible.name: button.icon
    Accessible.onPressAction: if (button.actionEnabled) button.clicked()
}
