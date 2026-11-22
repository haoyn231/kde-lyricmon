// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 开发工具：离屏渲染弹窗（由 lyrics-preview 加载）。数据来自真播放器或
// tools/fakeplayer，所以出图就是真实排版。

import QtQuick
import org.kde.kirigami as Kirigami
import "../package/contents/lib/Hao/Lyrics"
import "../package/contents/ui"

Item {
    id: root


    // 预览环境里没有 main.qml，接线自己来
    Binding {
        target: Lyrics
        property: "player"
        value: Mpris.active
    }

    // 出图要可重复：如果假播放器在总线上就固定用它，没有则自动回退
    Binding {
        target: Mpris
        property: "preferredService"
        value: "org.mpris.MediaPlayer2.fakeplayer"
    }

    FullRepresentation {
        anchors.fill: parent
        showControls: true
    }
}
