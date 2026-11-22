// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 开发用的假播放器：在会话总线上冒充一个 MPRIS 播放器。
//
// 为什么要它：真实播放器的进度不受控，截图和联调都不可重复。这里可以
//   ./lyricmon-fake-player --title ... --artist ... --duration 210 --lrc song.lrc
// 起一个「正在播放」的播放器，位置随真实时间走，还能用 --position 直接跳。
// 它也顺便充当 MPRIS 消费端的一组活体样本：Metadata / PlaybackStatus /
// Seeked / PropertiesChanged 都按规范发出去。
//
// 用 QDBusVirtualObject 而不是一堆 slot，是为了让同一个路径上同时挂
// org.freedesktop.DBus.Properties 和 org.mpris.MediaPlayer2.Player 两个接口
// —— 真实播放器就是这么分工的。

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include "introspection.h"

#include <cstdio>

namespace {

constexpr auto kService = "org.mpris.MediaPlayer2.fakeplayer";
constexpr auto kPath = "/org/mpris/MediaPlayer2";
constexpr auto kPlayerIface = "org.mpris.MediaPlayer2.Player";
constexpr auto kRootIface = "org.mpris.MediaPlayer2";
constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";

} // namespace

class FakePlayer : public QDBusVirtualObject
{
    Q_OBJECT

public:
    FakePlayer(const QString &title, const QStringList &artists, const QString &album,
               const QString &artUrl, const QString &asText, qint64 durationUs,
               qint64 startPositionUs, bool playing, QObject *parent = nullptr)
        : QDBusVirtualObject(parent)
        , m_title(title)
        , m_artists(artists)
        , m_album(album)
        , m_artUrl(artUrl)
        , m_asText(asText)
        , m_durationUs(durationUs)
        , m_basePositionUs(startPositionUs)
        , m_playing(playing)
    {
        m_clock.start();
    }

    QString introspect(const QString &path) const override
    {
        return path == QLatin1String(kPath)
            ? QString::fromLatin1(fakeplayer::kIntrospectionXml)
            : QString();
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        const QString iface = message.interface();
        const QString member = message.member();

        if (iface == QLatin1String(kPropsIface)) {
            if (member == QLatin1String("Get")) {
                const QString property = message.arguments().value(1).toString();
                const QVariantMap props = propertiesFor(message.arguments().value(0).toString());
                connection.send(message.createReply(QVariant::fromValue(
                    QDBusVariant(props.value(property)))));
                return true;
            }
            if (member == QLatin1String("GetAll")) {
                connection.send(message.createReply(
                    propertiesFor(message.arguments().value(0).toString())));
                return true;
            }
            if (member == QLatin1String("Set")) {
                connection.send(message.createReply());
                return true;
            }
            return false;
        }

        if (iface == QLatin1String(kRootIface)) {
            if (member == QLatin1String("Raise")) {
                connection.send(message.createReply());
                return true;
            }
            if (member == QLatin1String("Quit")) {
                QCoreApplication::quit();
                connection.send(message.createReply());
                return true;
            }
            return false;
        }

        if (iface != QLatin1String(kPlayerIface)) {
            return false;
        }

        if (member == QLatin1String("PlayPause")) {
            togglePlayback(connection);
        } else if (member == QLatin1String("Play")) {
            if (!m_playing) {
                togglePlayback(connection);
            }
        } else if (member == QLatin1String("Pause")) {
            if (m_playing) {
                togglePlayback(connection);
            }
        } else if (member == QLatin1String("Next")) {
            std::fprintf(stderr, "Next requested\n");
        } else if (member == QLatin1String("Previous")) {
            std::fprintf(stderr, "Previous requested\n");
        } else if (member == QLatin1String("Seek")) {
            seekTo(positionUs() + message.arguments().value(0).toLongLong(), connection);
        } else if (member == QLatin1String("SetPosition")) {
            seekTo(message.arguments().value(1).toLongLong(), connection);
        } else {
            return false;
        }

        connection.send(message.createReply());
        return true;
    }

private:
    qint64 positionUs() const
    {
        const qint64 advance = m_playing ? m_clock.elapsed() * 1000 : 0;
        return m_basePositionUs + advance;
    }

    QVariantMap propertiesFor(const QString &interface) const
    {
        if (interface == QLatin1String(kRootIface)) {
            return {
                {QStringLiteral("Identity"), QStringLiteral("Fake Player")},
                {QStringLiteral("CanQuit"), true},
                {QStringLiteral("CanRaise"), true},
                {QStringLiteral("DesktopEntry"), QStringLiteral("fakeplayer")},
            };
        }
        if (interface != QLatin1String(kPlayerIface)) {
            return {};
        }

        QVariantMap metadata;
        metadata.insert(QStringLiteral("mpris:trackid"),
                        QVariant::fromValue(QDBusObjectPath(QStringLiteral("/fake/track/1"))));
        metadata.insert(QStringLiteral("mpris:length"), m_durationUs);
        metadata.insert(QStringLiteral("xesam:title"), m_title);
        metadata.insert(QStringLiteral("xesam:artist"), m_artists);
        if (!m_album.isEmpty()) {
            metadata.insert(QStringLiteral("xesam:album"), m_album);
        }
        if (!m_artUrl.isEmpty()) {
            metadata.insert(QStringLiteral("mpris:artUrl"), m_artUrl);
        }
        if (!m_asText.isEmpty()) {
            metadata.insert(QStringLiteral("xesam:asText"), m_asText);
        }

        return {
            {QStringLiteral("PlaybackStatus"),
             QString::fromLatin1(m_playing ? "Playing" : "Paused")},
            {QStringLiteral("Metadata"), metadata},
            {QStringLiteral("Position"), positionUs()},
            {QStringLiteral("Rate"), 1.0},
            {QStringLiteral("Volume"), 1.0},
            {QStringLiteral("CanControl"), true},
            {QStringLiteral("CanPlay"), true},
            {QStringLiteral("CanPause"), true},
            {QStringLiteral("CanGoNext"), true},
            {QStringLiteral("CanGoPrevious"), true},
            {QStringLiteral("CanSeek"), true},
        };
    }

    void emitPropertiesChanged(const QDBusConnection &connection, const QVariantMap &changed)
    {
        QDBusMessage signal = QDBusMessage::createSignal(QString::fromLatin1(kPath),
                                                         QString::fromLatin1(kPropsIface),
                                                         QStringLiteral("PropertiesChanged"));
        signal << QString::fromLatin1(kPlayerIface) << changed << QStringList();
        connection.send(signal);
    }

    void togglePlayback(const QDBusConnection &connection)
    {
        m_basePositionUs = positionUs();
        m_clock.restart();
        m_playing = !m_playing;
        emitPropertiesChanged(connection,
                              {{QStringLiteral("PlaybackStatus"),
                                QString::fromLatin1(m_playing ? "Playing" : "Paused")}});
        std::fprintf(stderr, "playback -> %s\n", m_playing ? "Playing" : "Paused");
    }

    void seekTo(qint64 positionUs, const QDBusConnection &connection)
    {
        m_basePositionUs = qMax<qint64>(0, positionUs);
        m_clock.restart();
        QDBusMessage signal = QDBusMessage::createSignal(QString::fromLatin1(kPath),
                                                         QString::fromLatin1(kPlayerIface),
                                                         QStringLiteral("Seeked"));
        signal << m_basePositionUs;
        connection.send(signal);
    }

    QString m_title;
    QStringList m_artists;
    QString m_album;
    QString m_artUrl;
    QString m_asText;
    qint64 m_durationUs;
    qint64 m_basePositionUs;
    bool m_playing;
    QElapsedTimer m_clock;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Fake MPRIS player for kde-lyricmon"));
    parser.addHelpOption();
    const QCommandLineOption titleOpt({QStringLiteral("t"), QStringLiteral("title")},
                                      QStringLiteral("Track title"), QStringLiteral("title"),
                                      QStringLiteral("風たちの声"));
    const QCommandLineOption artistOpt({QStringLiteral("a"), QStringLiteral("artist")},
                                       QStringLiteral("Artist"), QStringLiteral("artist"),
                                       QStringLiteral("RADWIMPS"));
    const QCommandLineOption albumOpt({QStringLiteral("b"), QStringLiteral("album")},
                                      QStringLiteral("Album"), QStringLiteral("album"),
                                      QStringLiteral("天気の子"));
    const QCommandLineOption durationOpt({QStringLiteral("d"), QStringLiteral("duration")},
                                         QStringLiteral("Duration in seconds"),
                                         QStringLiteral("seconds"), QStringLiteral("157"));
    const QCommandLineOption coverOpt({QStringLiteral("c"), QStringLiteral("cover")},
                                      QStringLiteral("Cover image path"), QStringLiteral("path"));
    const QCommandLineOption lrcOpt({QStringLiteral("l"), QStringLiteral("lrc")},
                                    QStringLiteral("Serve this .lrc as xesam:asText"),
                                    QStringLiteral("path"));
    const QCommandLineOption positionOpt({QStringLiteral("p"), QStringLiteral("position")},
                                         QStringLiteral("Start position in seconds"),
                                         QStringLiteral("seconds"), QStringLiteral("0"));
    const QCommandLineOption pausedOpt(QStringLiteral("paused"),
                                       QStringLiteral("Start paused"));
    parser.addOptions({titleOpt, artistOpt, albumOpt, durationOpt, coverOpt, lrcOpt,
                       positionOpt, pausedOpt});
    parser.process(app);

    QString asText;
    if (parser.isSet(lrcOpt)) {
        QFile file(parser.value(lrcOpt));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            asText = QString::fromUtf8(file.readAll());
        } else {
            std::fprintf(stderr, "cannot read %s\n", qPrintable(parser.value(lrcOpt)));
        }
    }

    QString cover;
    if (parser.isSet(coverOpt)) {
        cover = QUrl::fromLocalFile(QFileInfo(parser.value(coverOpt)).absoluteFilePath()).toString();
    }

    const qint64 durationUs = parser.value(durationOpt).toLongLong() * 1000000LL;

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QString::fromLatin1(kService))) {
        std::fprintf(stderr, "cannot register service: %s\n",
                     qPrintable(bus.lastError().message()));
        return 1;
    }

    FakePlayer player(parser.value(titleOpt), {parser.value(artistOpt)},
                      parser.value(albumOpt), cover, asText, durationUs,
                      parser.value(positionOpt).toLongLong() * 1000000LL,
                      !parser.isSet(pausedOpt));

    if (!bus.registerVirtualObject(QString::fromLatin1(kPath), &player,
                                   QDBusConnection::SubPath)) {
        std::fprintf(stderr, "cannot register object: %s\n",
                     qPrintable(bus.lastError().message()));
        return 1;
    }

    std::fprintf(stderr, "fake player up: %s  \"%s\" — %s (%lld s)\n", kService,
                 qPrintable(parser.value(titleOpt)), qPrintable(parser.value(artistOpt)),
                 static_cast<long long>(durationUs / 1000000));

    return app.exec();
}

#include "fakeplayer.moc"
