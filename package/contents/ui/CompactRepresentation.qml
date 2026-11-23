// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 面板上的紧凑显示：图标 + 当前歌词（可选第二行显示下一句）。
//
// 三个坑和 kde-perfmon / kde-amdgpu-top 里一样：
//   1. 面板按 Layout 附加属性分格子，只写 implicitWidth 会被退回「一个图标」的
//      槽位，内容溢出压到时钟上；
//   2. 不能用铺满整块的 MouseArea 接管点击，那会吞掉按下事件，编辑模式拖不动。
//      用 TapHandler（默认 DragThreshold 策略）；
//   3. 宽度别从子布局上取：面板在组件构造期就会问 implicitWidth，那会儿子布局
//      还没 polish，量出来是 0，格子按 0 分，文字就把隔壁盖了（详见下面
//      contentWidth 的注释）。

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

    // 字体在这里一次性构造，不要去读子项上的 font：面板第一次问 implicitWidth
    // 的时候子项还没建完，那时读到的 font 是空的（pixelSize 0），量出来全是 0。
    // 注意也不能拿 Kirigami.Theme.defaultFont 改完再返回 —— 字体是值类型，
    // JS 里赋值只改了副本，读回来还是原字号；必须用 Qt.font 重新造一个。
    readonly property int primaryPixelSize: compact.fontSizePt > 0 ? compact.fontSizePt
                                                                  : Kirigami.Theme.defaultFont.pixelSize
    readonly property int secondaryPixelSize: Math.max(8, Math.round(compact.primaryPixelSize * 0.8))
    readonly property font primaryFont: Qt.font({
        family: Kirigami.Theme.defaultFont.family,
        pixelSize: compact.primaryPixelSize,
        weight: Kirigami.Theme.defaultFont.weight,
        italic: Kirigami.Theme.defaultFont.italic
    })
    readonly property font secondaryFont: Qt.font({
        family: Kirigami.Theme.defaultFont.family,
        pixelSize: compact.secondaryPixelSize,
        weight: Kirigami.Theme.defaultFont.weight,
        italic: Kirigami.Theme.defaultFont.italic
    })

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

    readonly property int iconSize: Kirigami.Units.iconSizes.small
    readonly property int maxTextWidth: Math.max(Kirigami.Units.gridUnit * 3,
                                                 compact.maxWidthUnits * Kirigami.Units.gridUnit
                                                 - (compact.showIcon ? compact.iconSize + Kirigami.Units.smallSpacing : 0))

    // 文字宽度必须自己量，不能等子布局。行布局/Text 在第一次 polish 之前
    // implicitWidth 还是 0，那时算出来的宽度只剩两倍 smallSpacing（8px），面板
    // 就照着这个数分格子；格子比字窄、内容又是居中摆的，文字于是往两边溢出，
    // 压到隔壁组件的图标上 —— 也就是「刚开机未在播放和别的组件重合」。
    //
    // 这里用 FontMetrics 而不是 TextMetrics：后者的 advanceWidth 要等一次 polish
    // 才算得出来，构造期读到的还是 0；FontMetrics.advanceWidth() 是普通方法调用，
    // 当场就有正确值。
    FontMetrics {
        id: primaryMetrics
        font: compact.primaryFont
    }

    FontMetrics {
        id: secondaryMetrics
        font: compact.secondaryFont
    }

    readonly property int naturalTextWidth: Math.max(Math.ceil(primaryMetrics.advanceWidth(compact.primaryText)),
                                                     Math.ceil(secondaryMetrics.advanceWidth(compact.secondaryText)))
    readonly property int textWidth: Math.min(compact.naturalTextWidth, compact.maxTextWidth)
    // 和行布局里的实际排布用同一套算式（行布局的 spacing 只在两个子项之间生效，
    // 竖向面板只留一个图标时就没有那段间距）
    readonly property int contentWidth: {
        if (compact.vertical) {
            return compact.showIcon ? compact.iconSize : 0;
        }
        return (compact.showIcon ? compact.iconSize + Kirigami.Units.smallSpacing : 0)
               + compact.textWidth;
    }

    readonly property bool collapsed: compact.hideWhenIdle && !compact.hasTrack

    visible: !collapsed
    // 兜底：万一面板给的格子还是比内容窄，宁可裁掉也不能画到隔壁去
    clip: true
    implicitWidth: collapsed ? 0 : compact.contentWidth + Kirigami.Units.smallSpacing * 2
    implicitHeight: collapsed ? 0
                              : Math.max(compact.iconSize,
                                         Math.ceil(primaryMetrics.height)
                                         + (compact.twoLines && compact.secondaryText.length > 0
                                            ? Math.ceil(secondaryMetrics.height) : 0)
                                         + Math.round(Kirigami.Units.smallSpacing * 0.2))

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
                font: compact.primaryFont
            }

            MarqueeText {
                id: secondaryTextItem
                Layout.fillWidth: true
                visible: compact.twoLines && compact.secondaryText.length > 0
                text: compact.secondaryText
                color: compact.dimColor
                font: compact.secondaryFont
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
