// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 弹窗：封面 + 曲目信息 → 滚动歌词 → 进度 → 播放控制。
// 颜色全部来自 Kirigami.Theme，明暗主题切换即时生效，没有写死任何前景/背景色。

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.extras as PlasmaExtras
import "../lib/Hao/Lyrics"

PlasmaExtras.Representation {
    id: page

    property bool showControls: true

    readonly property var player: Mpris.active
    readonly property bool hasTrack: page.player !== null && page.player.title.length > 0
    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color dimColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.62)
    readonly property color faintColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.4)
    readonly property color hairline: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.12)

    Layout.minimumWidth: Kirigami.Units.gridUnit * 22
    Layout.preferredWidth: Kirigami.Units.gridUnit * 25
    Layout.maximumWidth: Kirigami.Units.gridUnit * 34
    Layout.minimumHeight: Kirigami.Units.gridUnit * 20
    Layout.preferredHeight: Kirigami.Units.gridUnit * 27

    function sourceText() {
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

    contentItem: Item {
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Math.round(Kirigami.Units.gridUnit * 0.8)
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

                    // 歌词来源徽标：能一眼看出这句是播放器给的、本地文件，
                    // 还是刚从 LRCLIB 拉下来的
                    Rectangle {
                        Layout.alignment: Qt.AlignRight
                        visible: Lyrics.state === "ready" || Lyrics.state === "instrumental"
                        implicitWidth: badgeLabel.implicitWidth + Kirigami.Units.smallSpacing * 2.4
                        implicitHeight: Math.round(Kirigami.Units.gridUnit * 1.35)
                        radius: height / 2
                        color: Qt.rgba(page.textColor.r, page.textColor.g, page.textColor.b, 0.08)
                        border.width: 1
                        border.color: page.hairline

                        Text {
                            id: badgeLabel
                            anchors.centerIn: parent
                            text: Lyrics.state === "instrumental" ? i18n("纯音乐") : page.sourceText()
                            color: page.dimColor
                            font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.54)
                            font.weight: Font.DemiBold
                        }
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
                    icon: page.player !== null && page.player.playing
                          ? "media-playback-pause-symbolic" : "media-playback-start-symbolic"
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
    }
}
