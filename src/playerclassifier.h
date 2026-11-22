// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 区分「音乐播放器」和「浏览器」。
//
// 浏览器（以及 Plasma 的浏览器集成）也会注册 org.mpris.MediaPlayer2.*，
// 网页里一放视频就带着 MPRIS 元数据出现。歌词面板不应该跟着视频走，所以默认
// 把这类实例排除在「当前播放器」之外。
//
// 判断只依赖服务名和 Identity，不去猜 Metadata：
//   - 服务名是 chromium / firefox / brave / … 并且还没拿到 Identity 时，先按浏览器处理；
//   - Chromium 系应用（Electron 等）也用 org.mpris.MediaPlayer2.chromium.instanceN
//     注册，Identity 才是应用名（例如 QQ），所以 Identity 一旦到手就以它为准；
//   - plasma-browser-integration 是浏览器扩展的代理，服务名本身就足够确定。

#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace PlayerClassifier {

// 去掉 org.mpris.MediaPlayer2. 前缀和 .instanceN 后缀，转小写
inline QString serviceBaseName(const QString &service)
{
    static const QString prefix = QStringLiteral("org.mpris.MediaPlayer2.");
    QString name = service.startsWith(prefix) ? service.mid(prefix.size()) : service;

    const int instance = name.indexOf(QStringLiteral(".instance"));
    if (instance > 0) {
        name.truncate(instance);
    }
    return name.toLower();
}

inline bool isBrowserService(const QString &service)
{
    const QString name = serviceBaseName(service);
    static const QStringList names = {
        QStringLiteral("chromium"),
        QStringLiteral("chrome"),
        QStringLiteral("google-chrome"),
        QStringLiteral("google-chrome-stable"),
        QStringLiteral("ungoogled-chromium"),
        QStringLiteral("thorium"),
        QStringLiteral("brave"),
        QStringLiteral("brave-browser"),
        QStringLiteral("vivaldi"),
        QStringLiteral("microsoft-edge"),
        QStringLiteral("msedge"),
        QStringLiteral("edge"),
        QStringLiteral("opera"),
        QStringLiteral("firefox"),
        QStringLiteral("firefox-esr"),
        QStringLiteral("librewolf"),
        QStringLiteral("waterfox"),
        QStringLiteral("falkon"),
        QStringLiteral("epiphany"),
        QStringLiteral("gnome-web"),
        QStringLiteral("qutebrowser"),
        QStringLiteral("midori"),
        QStringLiteral("yandex-browser"),
        QStringLiteral("yandex"),
        // Plasma 浏览器集成的 MPRIS 代理，Identity 是浏览器名字
        QStringLiteral("plasma-browser-integration"),
    };
    return names.contains(name);
}

inline bool isBrowserIdentity(const QString &identity)
{
    const QString id = identity.trimmed().toLower();
    if (id.isEmpty()) {
        return false;
    }

    // 按词比较，避免把 Chromecast 之类含 "chrome" 的应用算进来。
    static const QRegularExpression separators(QStringLiteral("[^\\p{L}\\p{N}]+"));
    const QStringList tokens = id.split(separators, Qt::SkipEmptyParts);
    static const QStringList names = {
        QStringLiteral("chromium"),
        QStringLiteral("chrome"),
        QStringLiteral("brave"),
        QStringLiteral("vivaldi"),
        QStringLiteral("opera"),
        QStringLiteral("firefox"),
        QStringLiteral("librewolf"),
        QStringLiteral("waterfox"),
        QStringLiteral("falkon"),
        QStringLiteral("epiphany"),
        QStringLiteral("qutebrowser"),
        QStringLiteral("midori"),
        QStringLiteral("yandex"),
        QStringLiteral("thorium"),
        QStringLiteral("msedge"),
        QStringLiteral("mozilla"),
    };
    for (const QString &token : tokens) {
        if (names.contains(token)) {
            return true;
        }
    }

    // "Microsoft Edge" 靠 edge 一个词；"GNOME Web" 的 Identity 有时只有 Web
    if (tokens.contains(QStringLiteral("edge"))) {
        return true;
    }
    if (tokens.contains(QStringLiteral("web"))
        && (tokens.size() == 1 || tokens.contains(QStringLiteral("gnome")))) {
        return true;
    }
    return false;
}

// Identity 为空时只能信服务名；拿到 Identity 后以它为准，这样 Chromium 系的
// 独立音乐应用不会被误判成浏览器。
inline bool isBrowser(const QString &service, const QString &identity = QString())
{
    if (isBrowserIdentity(identity)) {
        return true;
    }
    if (serviceBaseName(service) == QLatin1String("plasma-browser-integration")) {
        return true;
    }
    return identity.isEmpty() && isBrowserService(service);
}

} // namespace PlayerClassifier
