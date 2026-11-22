// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT

#include "lyricsengine.h"

#include "mprisplayer.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

namespace {

constexpr auto kUserAgent = "kde-lyricmon/0.2.0 (https://github.com/haoyn231/kde-lyricmon)";
constexpr auto kLrclibGet = "https://lrclib.net/api/get";
constexpr auto kLrclibSearch = "https://lrclib.net/api/search";
constexpr int kMatchThreshold = 60;

QNetworkRequest makeRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setRawHeader("Accept", "application/json");
    // 歌词拿不到就退化成纯文本，绝不能让界面等它
    request.setTransferTimeout(8000);
    return request;
}

// 文件名里不能出现路径分隔符；艺术家名里带 "/" 的情况并不罕见
QString sanitizeFileName(QString name)
{
    name.replace(QLatin1Char('/'), QLatin1Char('_'));
    name.replace(QLatin1Char('\\'), QLatin1Char('_'));
    name.replace(QLatin1Char(':'), QLatin1Char('-'));
    return name.trimmed();
}

} // namespace

LyricsEngine::LyricsEngine(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QObject *LyricsEngine::player() const
{
    return m_player.data();
}

void LyricsEngine::setPlayer(QObject *player)
{
    auto *mpris = qobject_cast<MprisPlayer *>(player);
    if (mpris == m_player) {
        return;
    }

    if (m_player) {
        disconnect(m_player, nullptr, this, nullptr);
    }
    m_player = mpris;

    if (m_player) {
        connect(m_player, &MprisPlayer::trackChanged, this, &LyricsEngine::onTrackChanged);
    }

    Q_EMIT playerChanged();
    onTrackChanged();
}

void LyricsEngine::setUsePlayerLyrics(bool enabled)
{
    if (m_usePlayerLyrics == enabled) {
        return;
    }
    m_usePlayerLyrics = enabled;
    Q_EMIT providersChanged();
    resolve();
}

void LyricsEngine::setUseLocalLyrics(bool enabled)
{
    if (m_useLocalLyrics == enabled) {
        return;
    }
    m_useLocalLyrics = enabled;
    Q_EMIT providersChanged();
    resolve();
}

void LyricsEngine::setUseOnlineLyrics(bool enabled)
{
    if (m_useOnlineLyrics == enabled) {
        return;
    }
    m_useOnlineLyrics = enabled;
    Q_EMIT providersChanged();
    resolve();
}

void LyricsEngine::setExtraLyricsDir(const QString &dir)
{
    if (m_extraLyricsDir == dir) {
        return;
    }
    m_extraLyricsDir = dir;
    Q_EMIT providersChanged();
    resolve();
}

QString LyricsEngine::trackKey() const
{
    if (!m_player) {
        return QString();
    }
    const QString title = m_player->title().trimmed();
    if (title.isEmpty()) {
        return QString();
    }
    return title + QLatin1Char('\x1f') + m_player->artist() + QLatin1Char('\x1f')
        + m_player->album() + QLatin1Char('\x1f') + QString::number(m_player->lengthMs());
}

void LyricsEngine::onTrackChanged()
{
    const QString key = trackKey();

    if (key.isEmpty()) {
        ++m_serial;
        m_key.clear();
        m_synced = false;
        m_haveAny = false;
        m_busy = false;
        m_lines.clear();
        m_times.clear();
        m_plain.clear();
        m_source.clear();
        m_message.clear();
        setState(QStringLiteral("idle"));
        return;
    }

    // 元数据抖动（比如封面换了、播放器重复发了一遍 Metadata）不必重新找歌词，
    // 但纯文本升到同步歌词这种「补全」要走一遍。
    if (key == m_key && (m_synced || m_state == QLatin1String("instrumental"))) {
        return;
    }

    m_key = key;
    resolve();
}

void LyricsEngine::resolve()
{
    ++m_serial;

    m_haveAny = false;
    m_synced = false;
    m_busy = false;
    m_lines.clear();
    m_times.clear();
    m_plain.clear();
    m_source.clear();
    m_message.clear();

    if (m_key.isEmpty()) {
        setState(QStringLiteral("idle"));
        return;
    }

    setState(QStringLiteral("searching"));

    const auto cached = m_memoryCache.constFind(m_key);
    if (cached != m_memoryCache.constEnd()) {
        consider(cached->lyrics, cached->source);
        if (m_synced) {
            finish();
            return;
        }
    }

    // ① 播放器自带（xesam:asText）：最快，通常也是纯文本
    if (m_usePlayerLyrics && m_player) {
        const QString text = m_player->asText();
        if (!text.trimmed().isEmpty()) {
            consider(Lrc::parse(text), QStringLiteral("player"));
        }
    }
    if (m_synced) {
        finish();
        return;
    }

    // ② 本地 .lrc：离线可用，一般带时间轴
    if (m_useLocalLyrics) {
        const QString path = findLocalLrc();
        if (!path.isEmpty()) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                consider(Lrc::parse(QString::fromUtf8(file.readAll())), QStringLiteral("local"));
            }
        }
    }
    if (m_synced) {
        finish();
        return;
    }

    // ③ 在线（先看自己缓存过没有）
    if (m_useOnlineLyrics) {
        QFile cache(cacheFilePath());
        if (cache.exists() && cache.open(QIODevice::ReadOnly)) {
            const QJsonObject object = QJsonDocument::fromJson(cache.readAll()).object();
            if (object.value(QStringLiteral("instrumental")).toBool()) {
                m_busy = false;
                setState(QStringLiteral("instrumental"));
                return;
            }
            const QString text = object.value(QStringLiteral("text")).toString();
            if (!text.trimmed().isEmpty()) {
                consider(Lrc::parse(text), QStringLiteral("lrclib"));
            }
        }
        if (m_synced) {
            finish();
            return;
        }

        m_busy = true;
        // 已经有纯文本就先显示出来，同步歌词继续在后台找
        setState(m_haveAny ? QStringLiteral("ready") : QStringLiteral("searching"));
        fetchOnline(m_serial);
        return;
    }

    finish();
}

void LyricsEngine::consider(const Lyrics &lyrics, const QString &source)
{
    if (!lyrics.synced.isEmpty()) {
        if (m_synced) {
            return;                    // 先到的同步歌词说了算
        }
        m_synced = true;
        m_haveAny = true;
        m_source = source;
        m_lines = toVariant(lyrics.synced);
        m_times.clear();
        m_times.reserve(lyrics.synced.size());
        for (const LyricLine &line : lyrics.synced) {
            m_times.append(line.timeMs);
        }
        if (m_plain.isEmpty()) {
            m_plain = lyrics.plain;
        }
        remember();
        Q_EMIT lyricsChanged();
        return;
    }

    if (!lyrics.plain.trimmed().isEmpty() && !m_haveAny) {
        m_haveAny = true;
        m_synced = false;
        m_source = source;
        m_plain = lyrics.plain;
        m_lines.clear();
        m_times.clear();
        remember();
        Q_EMIT lyricsChanged();
    }
}

// 把当前结果存进进程内缓存。上限 100 首，超过就整体清掉 —— 歌词本身很小，
// 但也没必要陪着 plasmashell 一直开到天荒地老。
void LyricsEngine::remember()
{
    if (!m_haveAny || m_key.isEmpty()) {
        return;
    }
    if (m_memoryCache.size() > 100) {
        m_memoryCache.clear();
    }

    CachedLyrics entry;
    entry.source = m_source;
    entry.lyrics.plain = m_plain;
    entry.lyrics.synced.reserve(m_times.size());
    for (int i = 0; i < m_times.size(); ++i) {
        const QVariantMap line = m_lines.at(i).toMap();
        entry.lyrics.synced.append({m_times.at(i), line.value(QStringLiteral("text")).toString()});
    }
    m_memoryCache.insert(m_key, entry);
}

void LyricsEngine::finish()
{
    m_busy = false;
    if (m_haveAny) {
        setState(QStringLiteral("ready"));
    } else {
        setState(QStringLiteral("notfound"));
    }
}

void LyricsEngine::setState(const QString &state, const QString &message)
{
    const bool changed = (state != m_state) || (message != m_message);
    m_state = state;
    m_message = message;
    if (changed) {
        Q_EMIT lyricsChanged();
    }
}

QVariantList LyricsEngine::toVariant(const QVector<LyricLine> &lines)
{
    QVariantList list;
    list.reserve(lines.size());
    for (const LyricLine &line : lines) {
        list.append(QVariantMap{
            {QStringLiteral("time"), line.timeMs},
            {QStringLiteral("text"), line.text},
        });
    }
    return list;
}

int LyricsEngine::indexAt(qint64 positionMs) const
{
    int low = 0;
    int high = m_times.size() - 1;
    int found = -1;
    while (low <= high) {
        const int mid = (low + high) / 2;
        if (m_times.at(mid) <= positionMs) {
            found = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return found;
}

void LyricsEngine::retry()
{
    m_memoryCache.remove(m_key);
    resolve();
}

QStringList LyricsEngine::searchDirs() const
{
    QStringList dirs;
    QString extra = m_extraLyricsDir.trimmed();
    if (extra.startsWith(QLatin1String("~/"))) {
        extra = QDir::homePath() + extra.mid(1);
    }
    if (!extra.isEmpty()) {
        dirs.append(extra);
    }
    dirs.append(QDir::homePath() + QStringLiteral("/.lyrics"));
    dirs.append(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                + QStringLiteral("/lyrics"));
    return dirs;
}

QString LyricsEngine::findLocalLrc() const
{
    if (!m_player) {
        return QString();
    }

    const QString title = sanitizeFileName(m_player->title());
    const QString artist = sanitizeFileName(m_player->artists().value(0));
    const QString album = sanitizeFileName(m_player->album());
    if (title.isEmpty()) {
        return QString();
    }

    QStringList names;
    if (!artist.isEmpty()) {
        names << QStringLiteral("%1 - %2.lrc").arg(artist, title);
        names << QStringLiteral("%2 - %1.lrc").arg(artist, title);
    }
    names << title + QStringLiteral(".lrc");

    QStringList paths;

    // 本地播放器（Strawberry、Elisa…）会把文件路径放在 xesam:url 里，
    // 同目录的同名 .lrc 就是最靠谱的歌词
    const QString trackUrl = m_player->trackUrl();
    if (trackUrl.startsWith(QLatin1String("file://"))) {
        const QFileInfo info(QUrl(trackUrl).toLocalFile());
        paths << info.absolutePath() + QLatin1Char('/') + info.completeBaseName()
                + QStringLiteral(".lrc");
    }

    const QStringList dirs = searchDirs();
    for (const QString &dir : dirs) {
        for (const QString &name : names) {
            paths << dir + QLatin1Char('/') + name;
        }
        if (!artist.isEmpty()) {
            paths << QStringLiteral("%1/%2/%3.lrc").arg(dir, artist, title);
            if (!album.isEmpty()) {
                paths << QStringLiteral("%1/%2/%3/%4.lrc").arg(dir, artist, album, title);
            }
        }
    }

    for (const QString &path : paths) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return QString();
}

QString LyricsEngine::cacheFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/kde-lyricmon");
    const QByteArray hash =
        QCryptographicHash::hash(m_key.toUtf8(), QCryptographicHash::Sha1).toHex();
    return dir + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".json");
}

void LyricsEngine::saveCache(const QString &text, const QString &source, bool instrumental) const
{
    const QString path = cacheFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject object;
    object.insert(QStringLiteral("text"), text);
    object.insert(QStringLiteral("source"), source);
    object.insert(QStringLiteral("instrumental"), instrumental);
    object.insert(QStringLiteral("savedAt"), QDateTime::currentSecsSinceEpoch());

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }
}

void LyricsEngine::fetchOnline(int serial)
{
    if (!m_player) {
        finish();
        return;
    }

    // 没有艺术家名的话 /api/get 会直接 400（它要求 artist_name+track_name），
    // 这种情况直接走搜索接口，别先撞一次墙
    if (m_player->artists().value(0).trimmed().isEmpty()) {
        fetchSearch(serial);
        return;
    }

    QUrl url(QString::fromLatin1(kLrclibGet));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("artist_name"), m_player->artists().value(0));
    query.addQueryItem(QStringLiteral("track_name"), m_player->title());
    if (!m_player->album().isEmpty()) {
        query.addQueryItem(QStringLiteral("album_name"), m_player->album());
    }
    if (m_player->lengthMs() > 0) {
        // LRCLIB 的 duration 容差是 2 秒，四舍五入到秒刚好
        query.addQueryItem(QStringLiteral("duration"),
                           QString::number((m_player->lengthMs() + 500) / 1000));
    }
    url.setQuery(query);

    QNetworkReply *reply = m_network->get(makeRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, serial]() {
        reply->deleteLater();
        if (serial != m_serial) {
            return;                    // 已经切歌了，这条回包作废
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 404 = 曲库没有；400 = 参数不被接受（比如时长/名字不规范）。
        // 两种都还有救：换成模糊搜索再试一次。
        if (status == 404 || status == 400) {
            fetchSearch(serial);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            m_busy = false;
            if (m_haveAny) {
                setState(QStringLiteral("ready"));
            } else {
                setState(QStringLiteral("error"), reply->errorString());
            }
            return;
        }

        const QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
        if (object.isEmpty()) {
            fetchSearch(serial);
            return;
        }
        applyOnlineObject(object, serial);
    });
}

void LyricsEngine::fetchSearch(int serial)
{
    if (!m_player) {
        finish();
        return;
    }

    QUrl url(QString::fromLatin1(kLrclibSearch));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("track_name"), m_player->title());
    if (!m_player->artists().isEmpty()) {
        query.addQueryItem(QStringLiteral("artist_name"), m_player->artists().value(0));
    }
    url.setQuery(query);

    QNetworkReply *reply = m_network->get(makeRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, serial]() {
        reply->deleteLater();
        if (serial != m_serial) {
            return;
        }
        m_busy = false;

        if (reply->error() != QNetworkReply::NoError) {
            if (m_haveAny) {
                setState(QStringLiteral("ready"));
            } else {
                setState(QStringLiteral("error"), reply->errorString());
            }
            return;
        }

        const QJsonArray results = QJsonDocument::fromJson(reply->readAll()).array();
        QJsonObject best;
        int bestScore = 0;
        const QString title = m_player ? m_player->title() : QString();
        const QStringList artists = m_player ? m_player->artists() : QStringList();
        const QString album = m_player ? m_player->album() : QString();
        const qint64 durationMs = m_player ? m_player->lengthMs() : 0;

        for (const QJsonValue &value : results) {
            const QJsonObject candidate = value.toObject();
            const int candidateScore =
                Lrc::score(title, artists, album, durationMs, candidate);
            if (candidateScore > bestScore) {
                bestScore = candidateScore;
                best = candidate;
            }
        }

        if (bestScore >= kMatchThreshold && !best.isEmpty()) {
            applyOnlineObject(best, serial);
            return;
        }

        if (m_haveAny) {
            setState(QStringLiteral("ready"));
        } else {
            setState(QStringLiteral("notfound"));
        }
    });
}

void LyricsEngine::applyOnlineObject(const QJsonObject &object, int serial)
{
    if (serial != m_serial) {
        return;
    }
    m_busy = false;

    if (object.value(QStringLiteral("instrumental")).toBool()) {
        saveCache(QString(), QStringLiteral("lrclib"), true);
        setState(QStringLiteral("instrumental"));
        return;
    }

    const QString synced = object.value(QStringLiteral("syncedLyrics")).toString();
    const QString plain = object.value(QStringLiteral("plainLyrics")).toString();

    if (!synced.trimmed().isEmpty()) {
        consider(Lrc::parse(synced), QStringLiteral("lrclib"));
        saveCache(synced, QStringLiteral("lrclib"), false);
    } else if (!plain.trimmed().isEmpty()) {
        consider(Lrc::parse(plain), QStringLiteral("lrclib"));
        saveCache(plain, QStringLiteral("lrclib"), false);
    }

    if (m_haveAny) {
        setState(QStringLiteral("ready"));
    } else {
        setState(QStringLiteral("notfound"));
    }
}
