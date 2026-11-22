// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 面板上的紧凑显示：图标 + 当前歌词（可选第二行显示下一句）。
//
// 两个坑和 kde-perfmon / kde-amdgpu-top 里一样：
//   1. 面板按 Layout 附加属性分格子，只写 implicitWidth 会被退回「一个图标」的
//      槽位，内容溢出压到时钟上；
//   2. 不能用铺满整块的 MouseArea 接管点击，那会吞掉按下事件，编辑模式拖不动。
//      用 TapHandler（默认 DragThreshold 策略）。

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../lib/Hao/LyricMon"

Item {
    id: compact

    signal activated()

    property bool hasTrack: false
    property bool twoLines: false
    property bool showIcon: true
    property int maxWidthUnits: 18
    property bool hideWhenIdle: true
    property int fontSizePt: 0
    property bool vertical: false

    readonly property var player: Mpris.active
    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color dimColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.55)

    // 当前播放位置落在第几行；前奏时是 -1
    readonly property int index: compact.hasTrack && compact.player
                                 ? Lyrics.indexAt(compact.player.positionMs) : -1

    readonly property string primaryText: {
        if (!compact.hasTrack || !compact.player) {
            return i18n("未在播放");
        }
        if (Lyrics.synced && compact.index >= 0 && compact.index < Lyrics.lines.length) {
            return Lyrics.lines[compact.index].text;
        }
        return compact.player.title;      // 前奏 / 纯文本歌词时退回歌名
    }

    readonly property string secondaryText: {
        if (!compact.hasTrack || !compact.player) {
            return "";
        }
        if (Lyrics.synced && compact.index >= 0 && compact.index + 1 < Lyrics.lines.length) {
            return Lyrics.lines[compact.index + 1].text;
        }
        return compact.player.artist;
    }

    readonly property int lineHeight: Math.round(primaryTextItem.implicitHeight)
    readonly property int iconSize: Kirigami.Units.iconSizes.small
    readonly property int maxTextWidth: Math.max(Kirigami.Units.gridUnit * 3,
                                                 compact.maxWidthUnits * Kirigami.Units.gridUnit
                                                 - (compact.showIcon ? compact.iconSize + Kirigami.Units.smallSpacing : 0))
    readonly property int textWidth: Math.min(compact.naturalTextWidth, compact.maxTextWidth)
    readonly property int naturalTextWidth: Math.max(primaryTextItem.naturalWidth,
                                                     secondaryTextItem.naturalWidth)

    readonly property bool collapsed: compact.hideWhenIdle && !compact.hasTrack

    visible: !collapsed
    implicitWidth: collapsed ? 0 : body.implicitWidth + Kirigami.Units.smallSpacing * 2
    implicitHeight: collapsed ? 0
                              : Math.max(Kirigami.Units.iconSizes.small,
                                         body.implicitHeight + Math.round(Kirigami.Units.smallSpacing * 0.2))

    Layout.preferredWidth: implicitWidth
    Layout.minimumWidth: implicitWidth
    Layout.maximumWidth: implicitWidth
    Layout.preferredHeight: implicitHeight
    Layout.minimumHeight: implicitHeight
    Layout.maximumHeight: implicitHeight

    Accessible.role: Accessible.Button
    Accessible.name: compact.hasTrack && compact.player
                     ? i18n("%1 — %2", compact.player.title, compact.primaryText)
                     : i18n("桌面歌词")

    RowLayout {
        id: body
        anchors.centerIn: parent
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            visible: compact.showIcon
            source: "audio-headphones-symbolic"
            color: compact.textColor
            isMask: true
            implicitWidth: compact.iconSize
            implicitHeight: compact.iconSize
            Layout.alignment: Qt.AlignVCenter
            opacity: compact.hasTrack ? 1 : 0.55
        }

        ColumnLayout {
            // 竖向面板放不下两行字，只显示图标（信息看提示气泡）
            visible: !compact.vertical
            spacing: 0
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: compact.textWidth

            MarqueeText {
                id: primaryTextItem
                Layout.fillWidth: true
                text: compact.primaryText
                color: compact.hasTrack ? compact.textColor : compact.dimColor
                font.pixelSize: compact.fontSizePt > 0 ? compact.fontSizePt
                                                       : Kirigami.Theme.defaultFont.pixelSize
            }

            MarqueeText {
                id: secondaryTextItem
                Layout.fillWidth: true
                visible: compact.twoLines && compact.secondaryText.length > 0
                text: compact.secondaryText
                color: compact.dimColor
                font.pixelSize: Math.max(8, Math.round(primaryTextItem.font.pixelSize * 0.8))
            }
        }
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    // TapHandler 而不是 MouseArea：点击展开，拖动仍然交给面板
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: compact.activated()
    }
}
