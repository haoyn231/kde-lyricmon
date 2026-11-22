// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT

#include "mprisplayer.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QTimer>

namespace {

constexpr auto kPath = "/org/mpris/MediaPlayer2";
constexpr auto kPlayerIface = "org.mpris.MediaPlayer2.Player";
constexpr auto kRootIface = "org.mpris.MediaPlayer2";
constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";

// MPRIS 的 a{sv} 到了 QVariant 里往往是 QDBusArgument，直接 toString() 拿不到东西，
// 这三个 helper 负责把常见几种写法都摊平。
QString toText(const QVariant &value)
{
    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        return qdbus_cast<QString>(value);
    }
    return value.toString();
}

QStringList toTextList(const QVariant &value)
{
    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        return qdbus_cast<QStringList>(value);
    }
    if (value.canConvert<QStringList>()) {
        return value.toStringList();
    }
    const QString single = value.toString();
    return single.isEmpty() ? QStringList() : QStringList{single};
}

qint64 toNumber(const QVariant &value)
{
    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        return qdbus_cast<qlonglong>(value);
    }
    return value.toLongLong();
}

QString toPath(const QVariant &value)
{
    if (value.userType() == qMetaTypeId<QDBusObjectPath>()) {
        return value.value<QDBusObjectPath>().path();
    }
    return value.toString();
}

QVariantMap toMap(const QVariant &value)
{
    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        return qdbus_cast<QVariantMap>(value);
    }
    return value.toMap();
}

} // namespace

MprisPlayer::MprisPlayer(const QString &service, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_ticker(new QTimer(this))
{
    // 身份先拿一次：Identity 一般不会变，取不到就退化成服务名
    fetchIdentity();
    connectSignals();
    fetchAll();

    m_elapsed.start();

    // 250ms 一跳：歌词高亮需要的时间分辨率，比每帧查一次 D-Bus 便宜得多
    m_ticker->setInterval(250);
    connect(m_ticker, &QTimer::timeout, this, &MprisPlayer::tick);
}

void MprisPlayer::fetchIdentity()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, kPath, kPropsIface,
                                                      QStringLiteral("Get"));
    msg << QString::fromLatin1(kRootIface) << QStringLiteral("Identity");
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *call) {
                call->deleteLater();
                const QDBusPendingReply<QDBusVariant> reply = *call;
                if (reply.isError()) {
                    return;
                }
                const QString identity = toText(reply.value().variant());
                if (!identity.isEmpty() && identity != m_identity) {
                    m_identity = identity;
                    Q_EMIT identityChanged();
                }
            });
}

void MprisPlayer::fetchAll()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, kPath, kPropsIface,
                                                      QStringLiteral("GetAll"));
    msg << QString::fromLatin1(kPlayerIface);
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg, 4000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *call) {
                call->deleteLater();
                const QDBusPendingReply<QVariantMap> reply = *call;
                if (reply.isError()) {
                    if (m_available) {
                        m_available = false;
                        Q_EMIT availableChanged();
                    }
                    return;
                }
                applyPlayerProperties(reply.value());
            });
}

void MprisPlayer::connectSignals()
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    bus.connect(m_service, QString::fromLatin1(kPath), QString::fromLatin1(kPropsIface),
                QStringLiteral("PropertiesChanged"), this,
                SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

    // Seeked 是唯一能马上告诉我们「用户拖了进度」的信号
    bus.connect(m_service, QString::fromLatin1(kPath), QString::fromLatin1(kPlayerIface),
                QStringLiteral("Seeked"), this, SLOT(onSeeked(qint64)));
}

void MprisPlayer::onSeeked(qint64 positionUs)
{
    m_baseMs = positionUs / 1000;
    m_elapsed.restart();
    setPositionInternal(m_baseMs);
}

void MprisPlayer::onPropertiesChanged(const QString &interface, const QVariantMap &changed,
                                      const QStringList &invalidated)
{
    if (interface == QLatin1String(kRootIface)) {
        if (changed.contains(QStringLiteral("Identity"))) {
            const QString identity = toText(changed.value(QStringLiteral("Identity")));
            if (!identity.isEmpty() && identity != m_identity) {
                m_identity = identity;
                Q_EMIT identityChanged();
            }
        }
        return;
    }

    if (interface != QLatin1String(kPlayerIface) && !interface.isEmpty()) {
        return;
    }

    QVariantMap props = changed;
    // 播放位置不能被 invalidate 掉，真拿不到就下一次 tick 去问
    if (invalidated.contains(QStringLiteral("Position"))) {
        props.remove(QStringLiteral("Position"));
    }
    applyPlayerProperties(props);
}

void MprisPlayer::applyPlayerProperties(const QVariantMap &props)
{
    bool trackDirty = false;
    bool statusDirty = false;

    if (props.contains(QStringLiteral("Metadata"))) {
        const QVariantMap metadata = toMap(props.value(QStringLiteral("Metadata")));
        if (metadata != m_metadata) {
            m_metadata = metadata;
            m_artists = toTextList(metadata.value(QStringLiteral("xesam:artist")));
            m_lengthUs = toNumber(metadata.value(QStringLiteral("mpris:length")));
            const QString trackId = toPath(metadata.value(QStringLiteral("mpris:trackid")));
            if (trackId != m_trackId) {
                m_trackId = trackId;
                m_baseMs = 0;
                m_elapsed.restart();
                setPositionInternal(0);
                requestPosition();
            }
            trackDirty = true;
        }
    }

    if (props.contains(QStringLiteral("PlaybackStatus"))) {
        const QString status = toText(props.value(QStringLiteral("PlaybackStatus")));
        if (status != m_playbackStatus) {
            m_playbackStatus = status;
            statusDirty = true;
            if (playing()) {
                m_baseMs = m_positionMs;
                m_elapsed.restart();
                requestPosition();
            } else {
                requestPosition();
            }
        }
    }

    if (props.contains(QStringLiteral("Rate"))) {
        const double rate = props.value(QStringLiteral("Rate")).toDouble();
        m_rate = rate > 0 ? rate : 1.0;
    }

    const auto applyBool = [&props](const char *key, bool &target, bool &dirty) {
        const QString name = QString::fromLatin1(key);
        if (props.contains(name)) {
            const bool value = props.value(name).toBool();
            if (value != target) {
                target = value;
                dirty = true;
            }
        }
    };

    bool capsDirty = false;
    applyBool("CanControl", m_canControl, capsDirty);
    applyBool("CanPlay", m_canPlay, capsDirty);
    applyBool("CanPause", m_canPause, capsDirty);
    applyBool("CanGoNext", m_canGoNext, capsDirty);
    applyBool("CanGoPrevious", m_canGoPrevious, capsDirty);
    applyBool("CanSeek", m_canSeek, capsDirty);
    if (capsDirty) {
        trackDirty = true;                // 能力位和曲目一起通知，QML 少一次重算
    }

    if (!m_available) {
        m_available = true;
        Q_EMIT availableChanged();
    }

    if (trackDirty) {
        Q_EMIT trackChanged();
    }
    if (statusDirty) {
        if (playing()) {
            m_ticker->start();
        } else {
            // 暂停/停止时没有必要每 250ms 醒一次
            m_ticker->stop();
        }
        Q_EMIT playbackStatusChanged();
    }
    if (playing() && !m_ticker->isActive()) {
        m_ticker->start();
    }
}

void MprisPlayer::requestPosition()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, kPath, kPropsIface,
                                                      QStringLiteral("Get"));
    msg << QString::fromLatin1(kPlayerIface) << QStringLiteral("Position");
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg, 2000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *call) {
                call->deleteLater();
                const QDBusPendingReply<QDBusVariant> reply = *call;
                if (reply.isError()) {
                    return;
                }
                syncPosition(reply.value().variant().toLongLong() / 1000);
            });
}

void MprisPlayer::syncPosition(qint64 reportedMs)
{
    if (reportedMs < 0) {
        return;
    }

    if (!playing()) {
        m_baseMs = reportedMs;
        m_elapsed.restart();
        setPositionInternal(reportedMs);
        return;
    }

    // 播放器报的位置和本地推算差得不多就采信本地推算（更平滑），
    // 差得多说明发生了跳转，以播放器为准。
    const qint64 estimated = m_baseMs + qint64(m_elapsed.elapsed() * m_rate);
    if (qAbs(reportedMs - estimated) > 600) {
        m_baseMs = reportedMs;
        m_elapsed.restart();
        setPositionInternal(reportedMs);
    }
}

void MprisPlayer::tick()
{
    if (!playing()) {
        return;
    }
    qint64 estimate = m_baseMs + qint64(m_elapsed.elapsed() * m_rate);
    if (m_lengthUs > 0) {
        estimate = qMin<qint64>(estimate, m_lengthUs / 1000);
    }
    setPositionInternal(estimate);

    // 每 4 秒和播放器对一次表，长时间播放不会漂
    if (++m_ticksSinceSync >= 16) {
        m_ticksSinceSync = 0;
        requestPosition();
    }
}

void MprisPlayer::setPositionInternal(qint64 ms)
{
    const qint64 clamped = qMax<qint64>(0, ms);
    if (clamped == m_positionMs) {
        return;
    }
    m_positionMs = clamped;
    Q_EMIT positionChanged();
}

void MprisPlayer::callMethod(const QString &method)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, kPath, kPlayerIface, method);
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

void MprisPlayer::playPause()
{
    if (!m_canControl) {
        return;
    }
    callMethod(QStringLiteral("PlayPause"));
}

void MprisPlayer::next()
{
    if (!m_canControl) {
        return;
    }
    callMethod(QStringLiteral("Next"));
}

void MprisPlayer::previous()
{
    if (!m_canControl) {
        return;
    }
    callMethod(QStringLiteral("Previous"));
}

void MprisPlayer::setPositionMs(qint64 positionMs)
{
    if (!m_canControl) {
        return;
    }
    const qint64 target = qMax<qint64>(0, positionMs);

    // 有 trackid 就走标准 SetPosition；没有的话只能拿 Seek 凑（Chromium 这类）
    if (!m_trackId.isEmpty()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(m_service, kPath,
                                                          QString::fromLatin1(kPlayerIface),
                                                          QStringLiteral("SetPosition"));
        msg << QVariant::fromValue(QDBusObjectPath(m_trackId)) << qint64(target * 1000);
        QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
    } else {
        seekByMs(target - m_positionMs);
    }

    // 乐观更新：不等播放器回声，界面立刻跟上
    m_baseMs = target;
    m_elapsed.restart();
    setPositionInternal(target);
}

void MprisPlayer::seekByMs(qint64 deltaMs)
{
    if (!m_canControl || !m_canSeek) {
        return;
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, kPath,
                                                      QString::fromLatin1(kPlayerIface),
                                                      QStringLiteral("Seek"));
    msg << qint64(deltaMs * 1000);
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}
