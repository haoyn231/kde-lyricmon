// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 封面。
//
// 这里刻意没有用 MultiEffect / OpacityMask 去做圆角裁剪：两者都依赖
// layer / ShaderEffectSource 生成离屏纹理，在没有 GPU 的离屏环境里会静默失效
// （描出来就是一块空白），而这种「只在真机才对」的效果最难排查。所以封面按
// 唱片封面本来的样子做成方形，只在拿不到图时显示一张圆角占位卡。

import QtQuick
import org.kde.kirigami as Kirigami

Item {
    id: art

    property string source: ""
    property int size: Kirigami.Units.gridUnit * 3.4

    readonly property color textColor: Kirigami.Theme.textColor
    readonly property bool ready: cover.status === Image.Ready

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.fill: parent
        radius: art.ready ? 0 : Math.round(art.size * 0.12)
        color: Qt.rgba(art.textColor.r, art.textColor.g, art.textColor.b, 0.08)
        border.width: 1
        border.color: Qt.rgba(art.textColor.r, art.textColor.g, art.textColor.b, 0.12)

        Behavior on radius {
            NumberAnimation { duration: Kirigami.Units.shortDuration }
        }
    }

    Image {
        id: cover
        anchors.fill: parent
        source: art.source
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
    }

    Kirigami.Icon {
        anchors.centerIn: parent
        visible: !art.ready
        source: "media-optical-audio-symbolic"
        color: art.textColor
        isMask: true
        implicitWidth: Math.round(art.size * 0.38)
        implicitHeight: implicitWidth
        opacity: 0.35
    }
}
