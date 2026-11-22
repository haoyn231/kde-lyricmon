// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 弹窗。两种布局：
//   stacked（默认）：信息在上、歌词在中、进度与控制在下
//   player：封面在左（元信息压在封面上）+ 进度与控制，歌词在右
//
// 颜色全部来自 Kirigami.Theme，明暗主题切换即时生效；唯一写死的白色是压在
// 封面图片上的文字（那里必须靠渐隐保证可读，和主题无关）。

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.extras as PlasmaExtras
import "../lib/Hao/LyricMon"

PlasmaExtras.Representation {
    id: page

    property bool showControls: true
    // stacked / player
    property string layoutMode: "stacked"

    readonly property bool playerMode: page.layoutMode === "player"
    readonly property var player: Mpris.active
    readonly property bool hasTrack: page.player !== null && page.player.title.length > 0
    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color dimColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.62)
    readonly property color faintColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.4)
    readonly property color hairline: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.12)

    // 播放器布局要横向铺开，纵向反而可以矮一点
    Layout.minimumWidth: page.playerMode ? Kirigami.Units.gridUnit * 33
                                         : Kirigami.Units.gridUnit * 22
    Layout.preferredWidth: page.playerMode ? Kirigami.Units.gridUnit * 42
                                           : Kirigami.Units.gridUnit * 25
    Layout.maximumWidth: page.playerMode ? Kirigami.Units.gridUnit * 58
                                         : Kirigami.Units.gridUnit * 34
    Layout.minimumHeight: page.playerMode ? Kirigami.Units.gridUnit * 19
                                          : Kirigami.Units.gridUnit * 20
    Layout.preferredHeight: page.playerMode ? Kirigami.Units.gridUnit * 26
                                            : Kirigami.Units.gridUnit * 27

    contentItem: Item {
        // ------------------------------------------------ 经典布局
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Math.round(Kirigami.Units.gridUnit * 0.8)
            visible: !page.playerMode
            spacing: Kirigami.Units.largeSpacing

            // ---------------- 头部：封面 + 曲目 + 来源 ----------------
            RowLayout {
                Layout.fillWidth: true
                spacing: Math.round(Kirigami.Units.gridUnit * 0.7)
                visible: page.hasTrack

                CoverArt {
                    Layout.alignment: Qt.AlignTop
                    size: Math.round(Kirigami.Units.gridUnit * 3.4)
                    source: page.hasTrack ? page.player.artUrl : ""
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: page.hasTrack ? page.player.title : ""
                        color: page.textColor
                        font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.88)
                        font.bold: true
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    Text {
                        Layout.fillWidth: true
                        text: page.hasTrack ? page.player.artist : ""
                        visible: text.length > 0
                        color: page.dimColor
                        font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.66)
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    Text {
                        Layout.fillWidth: true
                        text: page.hasTrack ? page.player.album : ""
                        visible: text.length > 0
                        color: page.faintColor
                        font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.6)
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: Math.round(Kirigami.Units.smallSpacing * 0.6)

                    SourceBadge {
                        Layout.alignment: Qt.AlignRight
                    }

                    // 多个播放器时标出当前跟的是谁（切换在设置里）
                    Text {
                        Layout.alignment: Qt.AlignRight
                        visible: Mpris.count > 1 && page.player !== null
                        text: page.player !== null && page.player.identity.length > 0
                              ? page.player.identity : ""
                        color: page.faintColor
                        font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.52)
                        elide: Text.ElideRight
                        Layout.maximumWidth: Kirigami.Units.gridUnit * 7
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                visible: page.hasTrack
                color: page.hairline
            }

            // ---------------- 歌词 ----------------
            LyricsView {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            // ---------------- 进度 ----------------
            SeekBar {
                Layout.fillWidth: true
                visible: page.hasTrack
            }

            // ---------------- 播放控制 ----------------
            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                visible: page.showControls && page.hasTrack
                spacing: Math.round(Kirigami.Units.gridUnit * 0.5)

                Item { Layout.fillWidth: true }

                ControlButton {
                    icon: "media-skip-backward-symbolic"
                    actionEnabled: page.player !== null && page.player.canControl
                                   && page.player.canGoPrevious
                    onClicked: page.player.previous()
                }

                ControlButton {
                    primary: true
                    symbol: page.player !== null && page.player.playing ? "pause" : "play"
                    actionEnabled: page.player !== null && page.player.canControl
                    onClicked: page.player.playPause()
                }

                ControlButton {
                    icon: "media-skip-forward-symbolic"
                    actionEnabled: page.player !== null && page.player.canControl
                                   && page.player.canGoNext
                    onClicked: page.player.next()
                }

                Item { Layout.fillWidth: true }
            }
        }

        // ------------------------------------------------ 播放器布局
        Loader {
            anchors.fill: parent
            anchors.margins: Math.round(Kirigami.Units.gridUnit * 0.8)
            active: page.playerMode
            visible: active

            sourceComponent: PlayerLayout {
                showControls: page.showControls
            }
        }
    }
}
