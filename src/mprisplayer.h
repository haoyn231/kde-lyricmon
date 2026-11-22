// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 一个 MPRIS 播放器的代理对象。
//
// 只做三件事：
//   1. 把 org.mpris.MediaPlayer2.Player 的属性摊平成 QML 好用的形式；
//   2. 把 µs 的 Position 推算成毫秒，并在播放中自己往前走（每 4 秒和播放器对一次表）；
//   3. 转发控制方法（播放/暂停、上一首/下一首、SetPosition）。
//
// 不针对任何具体播放器做适配：属性缺了就留空，能力位为 false 就禁用按钮。

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

class QTimer;

class MprisPlayer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString service READ service CONSTANT)
    Q_PROPERTY(QString identity READ identity NOTIFY identityChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    // 该实例是否是浏览器（含网页播放器），默认不参与选主
    Q_PROPERTY(bool browser READ isBrowser NOTIFY browserChanged)

    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QStringList artists READ artists NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY trackChanged)
    Q_PROPERTY(QString trackUrl READ trackUrl NOTIFY trackChanged)
    Q_PROPERTY(QString trackId READ trackId NOTIFY trackChanged)
    Q_PROPERTY(QString asText READ asText NOTIFY trackChanged)
    Q_PROPERTY(qint64 lengthMs READ lengthMs NOTIFY trackChanged)

    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(QString playbackStatus READ playbackStatus NOTIFY playbackStatusChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playbackStatusChanged)
    Q_PROPERTY(bool stopped READ stopped NOTIFY playbackStatusChanged)

    Q_PROPERTY(bool canControl READ canControl NOTIFY trackChanged)
    Q_PROPERTY(bool canPlay READ canPlay NOTIFY trackChanged)
    Q_PROPERTY(bool canPause READ canPause NOTIFY trackChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY trackChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY trackChanged)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY trackChanged)

public:
    explicit MprisPlayer(const QString &service, QObject *parent = nullptr);

    QString service() const { return m_service; }
    QString identity() const { return m_identity; }
    bool available() const { return m_available; }
    bool isBrowser() const { return m_browser; }

    QString title() const { return m_metadata.value(QStringLiteral("xesam:title")).toString(); }
    QStringList artists() const { return m_artists; }
    QString artist() const { return m_artists.join(QStringLiteral(" / ")); }
    QString album() const { return m_metadata.value(QStringLiteral("xesam:album")).toString(); }
    QString artUrl() const { return m_metadata.value(QStringLiteral("mpris:artUrl")).toString(); }
    QString trackUrl() const { return m_metadata.value(QStringLiteral("xesam:url")).toString(); }
    QString trackId() const { return m_trackId; }
    QString asText() const { return m_metadata.value(QStringLiteral("xesam:asText")).toString(); }
    qint64 lengthMs() const { return m_lengthUs / 1000; }

    qint64 positionMs() const { return m_positionMs; }
    QString playbackStatus() const { return m_playbackStatus; }
    bool playing() const { return m_playbackStatus == QLatin1String("Playing"); }
    bool stopped() const { return m_playbackStatus == QLatin1String("Stopped"); }

    bool canControl() const { return m_canControl; }
    bool canPlay() const { return m_canPlay; }
    bool canPause() const { return m_canPause; }
    bool canGoNext() const { return m_canGoNext; }
    bool canGoPrevious() const { return m_canGoPrevious; }
    bool canSeek() const { return m_canSeek; }

    // 有歌、有名字，才算能显示（Chromium 那种空 Metadata 的就别占着位置）
    bool hasTrack() const { return !title().isEmpty(); }

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void setPositionMs(qint64 positionMs);
    Q_INVOKABLE void seekByMs(qint64 deltaMs);

signals:
    void identityChanged();
    void availableChanged();
    // 服务名先给出一个判断，Identity 到手后可能修正（Chromium 系应用）
    void browserChanged();
    void trackChanged();
    void positionChanged();
    void playbackStatusChanged();

private Q_SLOTS:
    // QDBusConnection::connect 走的是老式字符串签名，这两个必须是 slot
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed,
                             const QStringList &invalidated);
    void onSeeked(qint64 positionUs);

private:
    void fetchIdentity();
    void fetchAll();
    void connectSignals();
    void applyIdentity(const QString &identity);
    void updateBrowser();
    void applyPlayerProperties(const QVariantMap &props);
    void requestPosition();
    void syncPosition(qint64 reportedMs);
    void tick();
    void setPositionInternal(qint64 ms);
    void callMethod(const QString &method);

    QString m_service;
    QString m_identity;
    QString m_trackId;
    QString m_playbackStatus = QStringLiteral("Stopped");
    QStringList m_artists;
    QVariantMap m_metadata;
    qint64 m_lengthUs = 0;
    qint64 m_positionMs = 0;
    qint64 m_baseMs = 0;          // 上一次对表时的位置
    bool m_available = true;
    bool m_browser = false;
    bool m_canControl = false;
    bool m_canPlay = true;
    bool m_canPause = true;
    bool m_canGoNext = false;
    bool m_canGoPrevious = false;
    bool m_canSeek = false;
    double m_rate = 1.0;
    int m_ticksSinceSync = 0;

    QElapsedTimer m_elapsed;
    QTimer *m_ticker = nullptr;
};
