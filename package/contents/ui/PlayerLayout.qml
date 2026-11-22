// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 播放器式布局（配置里选「封面在左」时使用）：
//   左：大封面，歌名 / 艺术家 / 专辑压在封面底部，下面是进度条与控制按钮
//   右：滚动歌词
//
// 压在封面上的文字固定用白色 + 底部黑色渐隐。这里的颜色不是主题色而是
// 「图片上的文字」，主题色在任意封面上都没法保证可读，只有压一层暗底才成立。

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../lib/Hao/LyricMon"

RowLayout {
    id: player

    property bool showControls: true

    readonly property var activePlayer: Mpris.active
    readonly property bool hasTrack: player.activePlayer !== null
                                     && player.activePlayer.title.length > 0
    readonly property int coverSize: Math.round(Kirigami.Units.gridUnit * 13)
    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color faintColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.38)
    readonly property color hairline: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.12)

    spacing: Math.round(Kirigami.Units.gridUnit * 0.9)

    // ------------------------------------------------ 左：封面 + 控制
    ColumnLayout {
        // 三个宽度都钉死：只给 preferredWidth 时，布局会把剩余空间塞给这一列，
        // 右侧歌词被挤成一条缝
        Layout.preferredWidth: player.coverSize
        Layout.minimumWidth: player.coverSize
        Layout.maximumWidth: player.coverSize
        Layout.fillHeight: true
        visible: player.hasTrack
        spacing: Kirigami.Units.smallSpacing

        Item {
            id: coverBox
            Layout.preferredWidth: player.coverSize
            Layout.preferredHeight: player.coverSize

            CoverArt {
                anchors.fill: parent
                source: player.hasTrack ? player.activePlayer.artUrl : ""
            }

            // 底部渐隐：封面可能是任何颜色，只有压暗之后白字才稳
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Math.round(parent.height * 0.62)
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.82) }
                }
            }

            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Math.round(Kirigami.Units.gridUnit * 0.6)
                spacing: 1

                Text {
                    Layout.fillWidth: true
                    text: player.hasTrack ? player.activePlayer.title : ""
                    color: "#ffffff"
                    font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.86)
                    font.bold: true
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: player.hasTrack ? player.activePlayer.artist : ""
                    color: Qt.rgba(1, 1, 1, 0.82)
                    font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.64)
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: player.hasTrack ? player.activePlayer.album : ""
                    color: Qt.rgba(1, 1, 1, 0.58)
                    font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.58)
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }

            SourceBadge {
                onImage: true
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Math.round(Kirigami.Units.gridUnit * 0.45)
            }
        }

        // 把进度条和控制按钮压到列的底部，中间留白
        Item { Layout.fillHeight: true }

        SeekBar {
            Layout.fillWidth: true
            visible: player.hasTrack
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Math.round(Kirigami.Units.smallSpacing * 0.5)
            visible: player.hasTrack && player.showControls
            spacing: Math.round(Kirigami.Units.smallSpacing * 1.2)

            ControlButton {
                icon: "media-skip-backward-symbolic"
                actionEnabled: player.activePlayer !== null && player.activePlayer.canControl
                               && player.activePlayer.canGoPrevious
                onClicked: player.activePlayer.previous()
            }

            ControlButton {
                primary: true
                icon: player.activePlayer !== null && player.activePlayer.playing
                      ? "media-playback-pause-symbolic" : "media-playback-start-symbolic"
                actionEnabled: player.activePlayer !== null && player.activePlayer.canControl
                onClicked: player.activePlayer.playPause()
            }

            ControlButton {
                icon: "media-skip-forward-symbolic"
                actionEnabled: player.activePlayer !== null && player.activePlayer.canControl
                               && player.activePlayer.canGoNext
                onClicked: player.activePlayer.next()
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            visible: Mpris.count > 1 && player.hasTrack && text.length > 0
            text: player.hasTrack && player.activePlayer.identity.length > 0
                  ? player.activePlayer.identity : ""
            color: player.faintColor
            font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.52)
            elide: Text.ElideRight
        }
    }

    Rectangle {
        Layout.fillHeight: true
        Layout.preferredWidth: 1
        visible: player.hasTrack
        color: player.hairline
    }

    // ------------------------------------------------ 右：歌词
    LyricsView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: Math.round(Kirigami.Units.gridUnit * 12)
    }
}
