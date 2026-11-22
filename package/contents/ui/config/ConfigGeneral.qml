// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import "../../lib/Hao/Lyrics"

Kirigami.FormLayout {
    id: page

    property alias cfg_twoLines: twoLinesCheck.checked
    property alias cfg_showIcon: iconCheck.checked
    property alias cfg_hideWhenIdle: hideIdleCheck.checked
    property alias cfg_maxWidthUnits: widthSpin.value
    property alias cfg_compactFontSize: fontSizeSpin.value
    property alias cfg_usePlayerLyrics: playerLyricsCheck.checked
    property alias cfg_useLocalLyrics: localLyricsCheck.checked
    property alias cfg_useOnlineLyrics: onlineLyricsCheck.checked
    property alias cfg_extraLyricsDir: dirField.text
    property alias cfg_showControls: controlsCheck.checked
    property alias cfg_popupLayout: layoutBox.currentValue
    property string cfg_preferredPlayer

    Kirigami.Separator {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18n("面板")
    }

    PlasmaComponents3.CheckBox {
        id: twoLinesCheck
        text: i18n("两行歌词（第二行显示下一句）")
    }

    PlasmaComponents3.CheckBox {
        id: iconCheck
        text: i18n("显示音符图标")
    }

    PlasmaComponents3.CheckBox {
        id: hideIdleCheck
        text: i18n("没有播放音乐时完全收起")
    }

    PlasmaComponents3.SpinBox {
        id: widthSpin
        Kirigami.FormData.label: i18n("最大宽度：")
        from: 6
        to: 60
        stepSize: 2
        editable: true
    }

    PlasmaComponents3.SpinBox {
        id: fontSizeSpin
        Kirigami.FormData.label: i18n("面板字号：")
        from: 0
        to: 28
        stepSize: 1
        editable: true
    }

    PlasmaComponents3.Label {
        text: i18n("宽度单位是 gridUnit（约 %1 px），超出会被跑马灯滚动；字号填 0 表示跟随系统。", Kirigami.Units.gridUnit)
        font: Kirigami.Theme.smallFont
        opacity: 0.65
        Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        wrapMode: Text.WordWrap
    }

    Kirigami.Separator {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18n("歌词来源")
    }

    PlasmaComponents3.CheckBox {
        id: playerLyricsCheck
        text: i18n("使用播放器自带的歌词（xesam:asText）")
    }

    PlasmaComponents3.CheckBox {
        id: localLyricsCheck
        text: i18n("使用本地 .lrc 文件")
    }

    PlasmaComponents3.CheckBox {
        id: onlineLyricsCheck
        text: i18n("使用 LRCLIB 在线歌词")
    }

    PlasmaComponents3.Label {
        text: i18n("查找顺序：播放器 → 本地 → 在线。播放器只给了纯文本时，仍会继续找带时间轴的版本。")
        font: Kirigami.Theme.smallFont
        opacity: 0.65
        Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        wrapMode: Text.WordWrap
    }

    PlasmaComponents3.TextField {
        id: dirField
        Kirigami.FormData.label: i18n("额外歌词目录：")
        placeholderText: i18n("留空则只用 ~/.lyrics 与 ~/.local/share/lyrics")
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20
    }

    PlasmaComponents3.Label {
        text: i18n("支持这些命名：〈艺术家〉 - 〈歌名〉.lrc、〈歌名〉 - 〈艺术家〉.lrc、〈歌名〉.lrc，以及 〈艺术家〉/〈歌名〉.lrc。音频文件同目录下的同名 .lrc 也会自动使用。")
        font: Kirigami.Theme.smallFont
        opacity: 0.65
        Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        wrapMode: Text.WordWrap
    }

    Kirigami.Separator {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18n("弹窗与播放器")
    }

    PlasmaComponents3.ComboBox {
        id: layoutBox
        Kirigami.FormData.label: i18n("弹窗布局：")
        textRole: "text"
        valueRole: "value"
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        model: [
            { text: i18n("经典：信息在上，歌词在下"), value: "stacked" },
            { text: i18n("播放器：封面在左，歌词在右"), value: "player" }
        ]
    }

    PlasmaComponents3.Label {
        text: i18n("「播放器」布局把封面放大到左侧，歌名、艺术家、专辑压在封面底部，右侧是滚动歌词；弹窗也会更宽一些。")
        font: Kirigami.Theme.smallFont
        opacity: 0.65
        Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        wrapMode: Text.WordWrap
    }

    PlasmaComponents3.CheckBox {
        id: controlsCheck
        text: i18n("显示播放控制按钮")
    }

    PlasmaComponents3.ComboBox {
        id: playerBox
        Kirigami.FormData.label: i18n("跟随播放器：")
        textRole: "text"
        valueRole: "value"
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        model: {
            const list = [{ text: i18n("自动（正在播放的那个）"), value: "" }];
            const choices = Mpris.choices;
            for (let i = 0; i < choices.length; ++i) {
                list.push({ text: choices[i].label, value: choices[i].service });
            }
            return list;
        }

        Component.onCompleted: {
            const index = indexOfValue(page.cfg_preferredPlayer);
            currentIndex = index >= 0 ? index : 0;
        }
        onActivated: page.cfg_preferredPlayer = currentValue
    }

    PlasmaComponents3.Label {
        text: i18n("同时开着音乐播放器和浏览器时，面板默认跟着「正在播放且带专辑信息」的那一个；固定一个播放器可以彻底避免被抢。")
        font: Kirigami.Theme.smallFont
        opacity: 0.65
        Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        wrapMode: Text.WordWrap
    }
}
