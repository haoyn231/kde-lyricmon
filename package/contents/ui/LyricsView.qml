// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 弹窗中间的歌词区：同步歌词居中滚动、当前行高亮、点某一行跳过去；
// 纯文本歌词退化成可滚动的整段；两者都没有时显示状态提示。

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import "../lib/Hao/Lyrics"

Item {
    id: root

    property var player: Mpris.active

    readonly property int currentIndex: root.player ? Lyrics.indexAt(root.player.positionMs) : -1
    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color dimColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.5)
    readonly property color faintColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.32)
    readonly property color accentColor: Kirigami.Theme.highlightColor
    readonly property bool hasSynced: Lyrics.synced && Lyrics.lines.length > 0
    readonly property bool hasPlain: !root.hasSynced && Lyrics.plainText.trim().length > 0
    readonly property bool showStatus: !root.hasSynced && !root.hasPlain

    function seekTo(index) {
        if (!root.player || !root.player.canControl) {
            return;
        }
        const line = Lyrics.lines[index];
        if (!line || line.time < 0) {
            return;
        }
        // 往前 50ms 落点，避免刚好卡在上一行的边界上
        root.player.setPositionMs(line.time + 50);
    }

    function follow() {
        if (root.currentIndex >= 0) {
            view.positionViewAtIndex(root.currentIndex, ListView.Center);
        } else {
            view.positionViewAtBeginning();
        }
    }

    onCurrentIndexChanged: root.follow()
    Component.onCompleted: Qt.callLater(root.follow)

    // 首次布局完成、或弹窗高度变化（比如换了布局）之后要重新居中，
    // 否则第一次 positionViewAtIndex 是按旧尺寸算的，当前句会偏掉
    onHeightChanged: Qt.callLater(root.follow)

    // 两种弹窗布局之间切换时，列表是被隐藏再显示的，得重新居中一次
    onVisibleChanged: {
        if (visible) {
            Qt.callLater(root.follow);
        }
    }

    // ---------------------------------------------------------------- 同步歌词
    ListView {
        id: view
        anchors.fill: parent
        // 上下留一点余量：滚动时首尾那半行会贴着分隔线/进度条被硬切，
        // 留白之后观感上只是一个"渐隐的边"，不至于像被裁掉
        anchors.topMargin: Math.round(Kirigami.Units.gridUnit * 0.45)
        anchors.bottomMargin: Math.round(Kirigami.Units.gridUnit * 0.45)
        visible: root.hasSynced
        clip: true
        model: Lyrics.lines
        spacing: Math.round(Kirigami.Units.gridUnit * 0.42)
        boundsBehavior: Flickable.StopAtBounds

        // 前后留白，让第一句和最后一句也能停在正中
        header: Item {
            width: view.width
            height: Math.max(0, Math.round(view.height / 2 - Kirigami.Units.gridUnit * 1.2))
        }
        footer: Item {
            width: view.width
            height: Math.max(0, Math.round(view.height / 2 - Kirigami.Units.gridUnit * 1.2))
        }

        delegate: Text {
            id: line

            required property var modelData
            required property int index

            readonly property bool current: index === root.currentIndex

            width: view.width
            text: modelData.text
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            color: current ? root.accentColor : root.textColor
            opacity: current ? 1 : Math.max(0.26, 0.6 - Math.abs(index - root.currentIndex) * 0.07)
            font.pixelSize: Math.round(Kirigami.Units.gridUnit * (current ? 0.98 : 0.84))
            font.weight: current ? Font.DemiBold : Font.Normal
            topPadding: Math.round(Kirigami.Units.smallSpacing * 0.5)
            bottomPadding: Math.round(Kirigami.Units.smallSpacing * 0.5)

            Behavior on opacity {
                NumberAnimation { duration: Kirigami.Units.shortDuration }
            }

            TapHandler {
                enabled: root.player !== null && root.player.canControl
                acceptedButtons: Qt.LeftButton
                onTapped: root.seekTo(line.index)
            }

            HoverHandler {
                cursorShape: root.player !== null && root.player.canControl
                             ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }

        // 程序滚动要动画，手动拖拽时不要（不然越拖越黏）
        Behavior on contentY {
            enabled: !view.dragging
            NumberAnimation {
                duration: 320
                easing.type: Easing.OutCubic
            }
        }
    }

    // ---------------------------------------------------------------- 纯文本歌词
    Flickable {
        id: plainView
        anchors.fill: parent
        visible: root.hasPlain
        clip: true
        contentWidth: width
        contentHeight: plainText.implicitHeight + Kirigami.Units.gridUnit
        boundsBehavior: Flickable.StopAtBounds

        Text {
            id: plainText
            width: plainView.width
            topPadding: Math.round(Kirigami.Units.gridUnit * 0.5)
            text: Lyrics.plainText
            color: root.textColor
            opacity: 0.85
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.82)
            lineHeight: 1.45
        }
    }

    // ---------------------------------------------------------------- 状态提示
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width, Kirigami.Units.gridUnit * 18)
        visible: root.showStatus
        spacing: Kirigami.Units.smallSpacing

        PlasmaComponents3.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: Lyrics.state === "searching"
            visible: running
            implicitWidth: Kirigami.Units.gridUnit * 1.6
            implicitHeight: implicitWidth
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: {
                switch (Lyrics.state) {
                case "searching":
                    return i18n("正在查找歌词…");
                case "notfound":
                    return i18n("没有找到这首歌的歌词");
                case "instrumental":
                    return i18n("纯音乐，没有歌词");
                case "error":
                    return i18n("歌词获取失败");
                case "idle":
                    return i18n("没有正在播放的音乐");
                default:
                    return i18n("等待歌词…");
                }
            }
            color: root.textColor
            opacity: 0.85
            font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.78)
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            visible: text.length > 0
            text: {
                if (Lyrics.state === "notfound") {
                    return i18n("把同名 .lrc 放进 ~/.lyrics 就能离线显示");
                }
                if (Lyrics.state === "error") {
                    return Lyrics.message.length > 0 ? Lyrics.message
                                                     : i18n("检查一下网络，或者换个歌词来源");
                }
                return "";
            }
            color: root.faintColor
            font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.6)
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.smallSpacing
            visible: Lyrics.state === "notfound" || Lyrics.state === "error"
            implicitWidth: retryLabel.implicitWidth + Kirigami.Units.smallSpacing * 4
            implicitHeight: Math.round(Kirigami.Units.gridUnit * 1.6)
            radius: height / 2
            color: retryHover.hovered ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.16)
                                      : Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.08)
            border.width: 1
            border.color: Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.16)

            Text {
                id: retryLabel
                anchors.centerIn: parent
                text: i18n("重新获取")
                color: root.textColor
                font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.62)
            }

            HoverHandler {
                id: retryHover
                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                acceptedButtons: Qt.LeftButton
                onTapped: Lyrics.retry()
            }
        }
    }
}
