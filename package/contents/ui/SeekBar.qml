// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 进度条：左边已播时间、右边总时长，中间一条可拖动的细条。
//
// 拖动期间只更新显示，松手才真正 SetPosition —— 每秒几十次 D-Bus 调用
// 对播放器不友好，进度条自己也会抖。

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../lib/Hao/Lyrics"

RowLayout {
    id: seek

    property var player: Mpris.active
    property real scrub: -1              // 拖动中的比例（0..1），-1 = 没在拖

    readonly property int duration: seek.player ? seek.player.lengthMs : 0
    readonly property int positionMs: seek.player ? seek.player.positionMs : 0
    readonly property bool interactive: seek.player !== null && seek.player.canControl
                                        && seek.duration > 0
    readonly property real fraction: seek.scrub >= 0
                                     ? seek.scrub
                                     : (seek.duration > 0
                                        ? Math.max(0, Math.min(1, seek.positionMs / seek.duration)) : 0)
    readonly property int shownMs: seek.scrub >= 0 ? Math.round(seek.scrub * seek.duration)
                                                   : seek.positionMs

    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color accent: Kirigami.Theme.highlightColor

    spacing: Kirigami.Units.smallSpacing

    function formatTime(ms) {
        const total = Math.max(0, Math.floor(ms / 1000));
        const minutes = Math.floor(total / 60);
        const seconds = total % 60;
        return minutes + ":" + (seconds < 10 ? "0" + seconds : seconds);
    }

    Text {
        text: seek.formatTime(seek.shownMs)
        color: Qt.rgba(seek.textColor.r, seek.textColor.g, seek.textColor.b, 0.55)
        font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.56)
        font.features: ({ "tnum": 1 })
        Layout.alignment: Qt.AlignVCenter
    }

    Item {
        id: groove
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        implicitHeight: Math.round(Kirigami.Units.gridUnit * 1.1)

        Rectangle {
            id: track
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: Math.max(3, Math.round(Kirigami.Units.gridUnit * 0.2))
            radius: height / 2
            color: Qt.rgba(seek.textColor.r, seek.textColor.g, seek.textColor.b, 0.16)
        }

        Rectangle {
            id: fill
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, track.width * seek.fraction)
            height: track.height
            radius: height / 2
            color: seek.accent

            Behavior on width {
                NumberAnimation { duration: seek.scrub >= 0 ? 0 : 240; easing.type: Easing.Linear }
            }
        }

        Rectangle {
            id: handle
            width: Math.round(Kirigami.Units.gridUnit * 0.5)
            height: width
            radius: width / 2
            x: Math.min(parent.width - width, Math.max(0, fill.width - width / 2))
            anchors.verticalCenter: parent.verticalCenter
            color: seek.accent
            visible: seek.interactive && (area.containsMouse || area.pressed)
            scale: area.pressed ? 1.25 : 1.0

            Behavior on scale {
                NumberAnimation { duration: Kirigami.Units.shortDuration }
            }
        }

        MouseArea {
            id: area
            anchors.fill: parent
            anchors.topMargin: -Math.round(Kirigami.Units.smallSpacing * 0.5)
            anchors.bottomMargin: -Math.round(Kirigami.Units.smallSpacing * 0.5)
            hoverEnabled: true
            enabled: seek.interactive
            cursorShape: seek.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor

            function ratioAt(x) {
                return Math.max(0, Math.min(1, x / width));
            }

            onPressed: (mouse) => seek.scrub = ratioAt(mouse.x)
            onPositionChanged: (mouse) => {
                if (pressed) {
                    seek.scrub = ratioAt(mouse.x);
                }
            }
            onReleased: {
                if (seek.scrub >= 0 && seek.player) {
                    seek.player.setPositionMs(Math.round(seek.scrub * seek.duration));
                }
                seek.scrub = -1;
            }
            onCanceled: seek.scrub = -1
        }
    }

    Text {
        text: seek.formatTime(seek.duration)
        color: Qt.rgba(seek.textColor.r, seek.textColor.g, seek.textColor.b, 0.4)
        font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.56)
        font.features: ({ "tnum": 1 })
        Layout.alignment: Qt.AlignVCenter
    }
}
