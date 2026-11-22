// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT

#include "mprisregistry.h"

#include "mprisplayer.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>


namespace {

constexpr auto kMprisPrefix = "org.mpris.MediaPlayer2";

// 大多数播放器的服务名带实例后缀（org.mpris.MediaPlayer2.vlc.instance1234），
// 重启就变。配置里记住的名字要按「去掉实例号」再比一次，否则用户固定好的
// 播放器在对方重启后会静默失效。
QString baseName(const QString &service)
{
    const int index = service.indexOf(QLatin1String(".instance"));
    return index > 0 ? service.left(index) : service;
}

} // namespace

MprisRegistry::MprisRegistry(QObject *parent)
    : QObject(parent)
{
    // arg0namespace 匹配：org.mpris.MediaPlayer2.* 的任何增删都会回调。
    // 播放器启动、退出、崩溃重启都能被接住，不需要定时扫描。
    QDBusConnection bus = QDBusConnection::sessionBus();
    // 这里故意不加 arg0namespace 过滤。Qt 的 argumentMatch 参数会拼成 arg0='…'，
    // 传 "arg0namespace='…'" 反而变成 arg0="arg0namespace='…'"（永远匹配不上，
    // 而且 connect() 照样返回 true）。NameOwnerChanged 本身频率很低，收下来再筛更稳。
    bus.connect(QString(), QString(), QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("NameOwnerChanged"), this,
                SLOT(onNameOwnerChanged(QString, QString, QString)));

    scan();
}

QVariantList MprisRegistry::players() const
{
    QVariantList list;
    list.reserve(m_players.size());
    for (MprisPlayer *player : m_players) {
        list.append(QVariant::fromValue<QObject *>(player));
    }
    return list;
}

QObject *MprisRegistry::active() const
{
    // MprisPlayer 在头文件里只有前向声明，向上转型放在这里做
    return static_cast<QObject *>(m_active);
}

QVariantList MprisRegistry::choices() const
{
    QVariantList list;
    for (MprisPlayer *player : m_players) {
        QString label = player->identity();
        if (label.isEmpty()) {
            label = player->service().section(QLatin1Char('.'), -1);
        }
        list.append(QVariantMap{
            {QStringLiteral("service"), player->service()},
            {QStringLiteral("label"), label},
            {QStringLiteral("playing"), player->playing() && player->hasTrack()},
        });
    }
    return list;
}

void MprisRegistry::setPreferredService(const QString &service)
{
    if (m_preferred == service) {
        return;
    }
    m_preferred = service;
    Q_EMIT preferredServiceChanged();
    updateActive();
}

void MprisRegistry::refresh()
{
    scan();
}

void MprisRegistry::scan()
{
    QDBusReply<QStringList> reply =
        QDBusConnection::sessionBus().interface()->registeredServiceNames();
    if (!reply.isValid()) {
        return;
    }

    QStringList live;
    const QString prefix = QString::fromLatin1(kMprisPrefix) + QLatin1Char('.');
    for (const QString &name : reply.value()) {
        if (name.startsWith(prefix)) {
            live.append(name);
        }
    }

    for (const QString &service : live) {
        addService(service);
    }
    // 已经死掉但没收到信号（比如 plasmashell 启动得比播放器晚）的，清出去
    const QList<MprisPlayer *> snapshot = m_players;
    for (MprisPlayer *player : snapshot) {
        if (!live.contains(player->service())) {
            removeService(player->service());
        }
    }
}

void MprisRegistry::addService(const QString &service)
{
    for (MprisPlayer *player : m_players) {
        if (player->service() == service) {
            return;
        }
    }

    auto *player = new MprisPlayer(service, this);
    m_players.append(player);

    // 任何一个播放器的状态变化都可能改变「谁该显示」，统统重新算一遍
    connect(player, &MprisPlayer::trackChanged, this, &MprisRegistry::updateActive);
    connect(player, &MprisPlayer::playbackStatusChanged, this, &MprisRegistry::updateActive);
    connect(player, &MprisPlayer::identityChanged, this, &MprisRegistry::playersChanged);
    connect(player, &MprisPlayer::availableChanged, this, [this, player]() {
        if (!player->available()) {
            removeService(player->service());
        } else {
            updateActive();
        }
    });

    Q_EMIT playersChanged();
    updateActive();
}

void MprisRegistry::removeService(const QString &service)
{
    for (int i = 0; i < m_players.size(); ++i) {
        MprisPlayer *player = m_players.at(i);
        if (player->service() != service) {
            continue;
        }
        m_players.removeAt(i);
        if (m_active == player) {
            m_active = nullptr;
            Q_EMIT activeChanged();
        }
        player->deleteLater();
        Q_EMIT playersChanged();
        updateActive();
        return;
    }
}

void MprisRegistry::updateActive()
{
    MprisPlayer *best = nullptr;

    // 用户固定了播放器就用它（除非它连歌都没有）
    if (!m_preferred.isEmpty()) {
        for (MprisPlayer *player : m_players) {
            if (player->service() == m_preferred && player->hasTrack()) {
                best = player;
                break;
            }
        }
        if (!best) {
            for (MprisPlayer *player : m_players) {
                if (player->hasTrack() && baseName(player->service()) == baseName(m_preferred)) {
                    best = player;
                    break;
                }
            }
        }
    }

    // 黏性：正在显示的那个只要还在放，就别因为别的播放器（比如浏览器里的
    // 一个视频标签页）也开始播放就把面板抢走
    if (!best && m_active && m_active->hasTrack() && m_active->playing()) {
        best = m_active;
    }

    if (!best) {
        // 同时有几个在放时，带专辑信息的那一个更像"音乐"
        for (MprisPlayer *player : m_players) {
            if (player->hasTrack() && player->playing() && !player->album().isEmpty()) {
                best = player;
                break;
            }
        }
    }

    if (!best) {
        for (MprisPlayer *player : m_players) {
            if (player->hasTrack() && player->playing()) {
                best = player;
                break;
            }
        }
    }

    if (!best) {
        for (MprisPlayer *player : m_players) {
            if (player->hasTrack() && !player->stopped()) {
                best = player;
                break;
            }
        }
    }

    if (!best) {
        for (MprisPlayer *player : m_players) {
            if (player->hasTrack()) {
                best = player;
                break;
            }
        }
    }

    // 没有任何播放器报出有效的曲目信息时返回 null，界面就显示待机态；
    // 而不是挂着一个「未知曲目」的空壳。
    if (best == m_active) {
        return;
    }
    m_active = best;
    Q_EMIT activeChanged();
}

void MprisRegistry::onNameOwnerChanged(const QString &service, const QString &oldOwner,
                                       const QString &newOwner)
{
    const QString prefix = QString::fromLatin1(kMprisPrefix) + QLatin1Char('.');
    if (!service.startsWith(prefix)) {
        return;
    }
    if (newOwner.isEmpty()) {
        removeService(service);
    } else if (oldOwner.isEmpty()) {
        addService(service);
    } else {
        // 换了 owner：老对象作废，重新建一个
        removeService(service);
        addService(service);
    }
}
