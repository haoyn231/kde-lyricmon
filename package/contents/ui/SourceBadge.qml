// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 歌词来源徽标：能一眼看出这句歌词是播放器给的、本地文件，还是刚从 LRCLIB 拉的。
// 弹窗的两种布局都要用，所以单独拆出来。

import QtQuick
import org.kde.kirigami as Kirigami
import "../lib/Hao/Lyrics"

Rectangle {
    id: badge

    // 压在封面上时用半透明黑底 + 白字：封面颜色不可控，只有这样才能保证可读
    property bool onImage: false

    readonly property color textColor: Kirigami.Theme.textColor
    readonly property bool relevant: Lyrics.state === "ready" || Lyrics.state === "instrumental"
    readonly property string label: {
        if (Lyrics.state === "instrumental") {
            return i18n("纯音乐");
        }
        switch (Lyrics.source) {
        case "player":
            return i18n("播放器自带");
        case "local":
            return i18n("本地 .lrc");
        case "lrclib":
            return "LRCLIB";
        default:
            return "";
        }
    }

    visible: relevant
    implicitWidth: badgeLabel.implicitWidth + Kirigami.Units.smallSpacing * 2.4
    implicitHeight: Math.round(Kirigami.Units.gridUnit * 1.35)
    radius: height / 2
    color: badge.onImage ? Qt.rgba(0, 0, 0, 0.45)
                         : Qt.rgba(badge.textColor.r, badge.textColor.g, badge.textColor.b, 0.08)
    border.width: 1
    border.color: badge.onImage ? Qt.rgba(1, 1, 1, 0.22)
                                : Qt.rgba(badge.textColor.r, badge.textColor.g, badge.textColor.b, 0.12)

    Text {
        id: badgeLabel
        anchors.centerIn: parent
        text: badge.label
        color: badge.onImage ? "#ffffff"
                             : Qt.rgba(badge.textColor.r, badge.textColor.g, badge.textColor.b, 0.62)
        font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.54)
        font.weight: Font.DemiBold
    }
}
