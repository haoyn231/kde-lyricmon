// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 桌面歌词 —— 主入口。
//
// 数据通路：
//   Mpris（C++ 单例，QtDBus）→ Mpris.active 是当前播放器
//   Lyrics（C++ 单例）→ 绑定 Lyrics.player = Mpris.active，自己负责找歌词
//   QML 只做两件事：把当前行画在面板上，把细节画在弹窗里
//
// 这里没有任何 D-Bus 调用，也没有网络请求：QML 里拿不到 QtDBus，而把 HTTP
// 塞进 QML 又会让缓存、超时、并发变得很难写对。

import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import "../lib/Hao/Lyrics"

PlasmoidItem {
    id: root

    Plasmoid.title: i18n("桌面歌词")
    preferredRepresentation: compactRepresentation

    // ---------------------------------------------------------------- 数据接线
    // 播放器换了、歌词来源改了，都由这两个单例自己处理，QML 不参与流程控制。
    // 注意这里必须用 Binding：Lyrics / Mpris 是单例，不是 attached property，
    // 写 `Lyrics.player: ...` 会被当成附加属性而报 Non-existent attached object。
    Binding {
        target: Lyrics
        property: "player"
        value: Mpris.active
    }
    Binding {
        target: Lyrics
        property: "usePlayerLyrics"
        value: plasmoid.configuration.usePlayerLyrics
    }
    Binding {
        target: Lyrics
        property: "useLocalLyrics"
        value: plasmoid.configuration.useLocalLyrics
    }
    Binding {
        target: Lyrics
        property: "useOnlineLyrics"
        value: plasmoid.configuration.useOnlineLyrics
    }
    Binding {
        target: Lyrics
        property: "extraLyricsDir"
        value: plasmoid.configuration.extraLyricsDir
    }
    Binding {
        target: Mpris
        property: "preferredService"
        value: plasmoid.configuration.preferredPlayer
    }

    readonly property var activePlayer: Mpris.active

    // ---------------------------------------------------------------- 提示气泡
    toolTipMainText: {
        if (!root.activePlayer) {
            return i18n("桌面歌词");
        }
        return root.activePlayer.title;
    }
    toolTipSubText: {
        if (!root.activePlayer) {
            return i18n("没有正在播放的音乐");
        }
        const parts = [];
        if (root.activePlayer.artist.length > 0) {
            parts.push(root.activePlayer.artist);
        }
        if (root.activePlayer.album.length > 0) {
            parts.push(root.activePlayer.album);
        }
        const head = parts.join("  ·  ");
        const tail = root.lyricsHint();
        return tail.length > 0 ? head + "\n" + tail : head;
    }

    function lyricsHint() {
        switch (Lyrics.state) {
        case "searching":
            return i18n("正在查找歌词…");
        case "notfound":
            return i18n("没有找到歌词");
        case "instrumental":
            return i18n("纯音乐");
        case "error":
            return i18n("歌词获取失败");
        case "ready":
            return Lyrics.synced ? i18n("同步歌词 · %1", root.sourceText())
                                 : i18n("歌词 · %1", root.sourceText());
        default:
            return "";
        }
    }

    function sourceText() {
        switch (Lyrics.source) {
        case "player":
            return i18n("播放器自带");
        case "local":
            return i18n("本地 .lrc");
        case "lrclib":
            return "LRCLIB";
        default:
            return i18n("未知");
        }
    }

    // ---------------------------------------------------------------- 表示层
    compactRepresentation: CompactRepresentation {
        hasTrack: root.activePlayer !== null && root.activePlayer.title.length > 0
        twoLines: plasmoid.configuration.twoLines
        showIcon: plasmoid.configuration.showIcon
        maxWidthUnits: plasmoid.configuration.maxWidthUnits
        hideWhenIdle: plasmoid.configuration.hideWhenIdle
        fontSizePt: plasmoid.configuration.compactFontSize
        vertical: plasmoid.formFactor === PlasmaCore.Types.Vertical

        onActivated: {
            root.expanded = !root.expanded;
        }
    }

    fullRepresentation: FullRepresentation {
        showControls: plasmoid.configuration.showControls
    }
}
