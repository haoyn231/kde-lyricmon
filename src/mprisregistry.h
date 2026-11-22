// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 播放器注册表：盯住 org.mpris.MediaPlayer2.*，为每个实例建一个 MprisPlayer，
// 再按「正在播放 > 暂停 > 有歌 > 什么都没有」的规则挑出当前要显示的那一个。
//
// 选谁不看播放器名字，只看 MPRIS 的状态位，所以 Spotify / Strawberry / 浏览器
// 里的网页播放器走的是同一套逻辑。

#pragma once

#include <QList>
#include <QObject>
#include <QVariantList>

class MprisPlayer;
class MprisRegistry : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList players READ players NOTIFY playersChanged)
    Q_PROPERTY(QObject *active READ active NOTIFY activeChanged)
    Q_PROPERTY(int count READ count NOTIFY playersChanged)
    // 配置页用：每项 { service, label, playing }
    Q_PROPERTY(QVariantList choices READ choices NOTIFY playersChanged)
    // 空字符串 = 自动跟随；否则固定用这个 service
    Q_PROPERTY(QString preferredService READ preferredService WRITE setPreferredService
                   NOTIFY preferredServiceChanged)

public:
    explicit MprisRegistry(QObject *parent = nullptr);

    QVariantList players() const;
    QObject *active() const;
    int count() const { return int(m_players.size()); }
    QVariantList choices() const;

    QString preferredService() const { return m_preferred; }
    void setPreferredService(const QString &service);

    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void playersChanged();
    void activeChanged();
    void preferredServiceChanged();

private Q_SLOTS:
    // 直接听 NameOwnerChanged（arg0namespace 匹配），播放器一上总线就能发现。
    // QDBusServiceWatcher 只认精确名字，前缀监听在这里不生效。
    void onNameOwnerChanged(const QString &service, const QString &oldOwner,
                            const QString &newOwner);

private:
    void scan();
    void addService(const QString &service);
    void removeService(const QString &service);
    void updateActive();

    QList<MprisPlayer *> m_players;
    MprisPlayer *m_active = nullptr;
    QString m_preferred;
};
