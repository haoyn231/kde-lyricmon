// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// QML 侧只看到两个单例：
//   Mpris  —— 播放器注册表，Mpris.active 就是当前要显示的那个播放器
//   Lyrics —— 歌词引擎，绑定 Lyrics.player = Mpris.active
//
// 之所以有一个 C++ 插件：Qt 没有把 D-Bus 暴露给 QML，而 QML 也没有官方绑定。
// 与其在 QML 里轮询 `dbus-send`，不如把这层做薄、做在 C++ 里。

#include "lyricsengine.h"
#include "mprisregistry.h"

#include <QQmlEngine>
#include <QQmlExtensionPlugin>

class LyricMonPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override
    {
        qmlRegisterSingletonType<MprisRegistry>(uri, 1, 0, "Mpris",
                                                [](QQmlEngine *, QJSEngine *) -> QObject * {
                                                    return new MprisRegistry;
                                                });
        qmlRegisterSingletonType<LyricsEngine>(uri, 1, 0, "Lyrics",
                                               [](QQmlEngine *, QJSEngine *) -> QObject * {
                                                   return new LyricsEngine;
                                               });
    }
};

#include "lyricmonplugin.moc"
