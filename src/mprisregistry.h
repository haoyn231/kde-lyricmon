// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 播放器注册表：盯住 org.mpris.MediaPlayer2.*，为每个实例建一个 MprisPlayer，
// 再按「正在播放 > 暂停 > 有歌 > 什么都没有」的规则挑出当前要显示的那一个。
//
// 浏览器也注册 MPRIS，但看视频不该抢走歌词面板：默认把浏览器实例排除在选主
// 之外（可在设置里放开，让网页里的音乐也显示歌词）。

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
    // 默认 true：浏览器（含网页播放器）不参与选主，看视频不会抢走歌词
    Q_PROPERTY(bool ignoreBrowsers READ ignoreBrowsers WRITE setIgnoreBrowsers
                   NOTIFY ignoreBrowsersChanged)

public:
    explicit MprisRegistry(QObject *parent = nullptr);

    QVariantList players() const;
    QObject *active() const;
    int count() const { return int(m_players.size()); }
    QVariantList choices() const;

    QString preferredService() const { return m_preferred; }
    void setPreferredService(const QString &service);

    bool ignoreBrowsers() const { return m_ignoreBrowsers; }
    void setIgnoreBrowsers(bool ignore);

    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void playersChanged();
    void activeChanged();
    void preferredServiceChanged();
    void ignoreBrowsersChanged();

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
    bool eligible(const MprisPlayer *player) const;

    QList<MprisPlayer *> m_players;
    MprisPlayer *m_active = nullptr;
    QString m_preferred;
    bool m_ignoreBrowsers = true;
};
