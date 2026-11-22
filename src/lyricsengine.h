// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 歌词引擎：拿到一首歌（标题/艺术家/专辑/时长 + 播放器可能自带的文本），
// 按「播放器自带 → 本地 .lrc → LRCLIB」的顺序找歌词，并给出可同步的行列表。
//
// 与 MPRIS 完全解耦：引擎不认识播放器，只认几个字符串和时长。

#pragma once

#include "lrcparser.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVector>

class MprisPlayer;
class QNetworkAccessManager;
class QJsonObject;

class LyricsEngine : public QObject
{
    Q_OBJECT

    // 由 QML 绑定 Mpris.active；为空表示没有在放的歌
    Q_PROPERTY(QObject *player READ player WRITE setPlayer NOTIFY playerChanged)

    // idle / searching / ready / instrumental / notfound / error
    Q_PROPERTY(QString state READ state NOTIFY lyricsChanged)
    // player / local / lrclib（缓存命中也算 lrclib）
    Q_PROPERTY(QString source READ source NOTIFY lyricsChanged)
    // [{ time: <ms>, text: <行> }]，纯文本歌词时 time 为 -1
    Q_PROPERTY(QVariantList lines READ lines NOTIFY lyricsChanged)
    Q_PROPERTY(bool synced READ synced NOTIFY lyricsChanged)
    Q_PROPERTY(QString plainText READ plainText NOTIFY lyricsChanged)
    Q_PROPERTY(QString message READ message NOTIFY lyricsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY lyricsChanged)

    Q_PROPERTY(bool usePlayerLyrics READ usePlayerLyrics WRITE setUsePlayerLyrics
                   NOTIFY providersChanged)
    Q_PROPERTY(bool useLocalLyrics READ useLocalLyrics WRITE setUseLocalLyrics
                   NOTIFY providersChanged)
    Q_PROPERTY(bool useOnlineLyrics READ useOnlineLyrics WRITE setUseOnlineLyrics
                   NOTIFY providersChanged)
    Q_PROPERTY(QString extraLyricsDir READ extraLyricsDir WRITE setExtraLyricsDir
                   NOTIFY providersChanged)

public:
    explicit LyricsEngine(QObject *parent = nullptr);

    QObject *player() const;
    void setPlayer(QObject *player);

    QString state() const { return m_state; }
    QString source() const { return m_source; }
    QVariantList lines() const { return m_lines; }
    bool synced() const { return m_synced; }
    QString plainText() const { return m_plain; }
    QString message() const { return m_message; }
    bool busy() const { return m_busy; }

    bool usePlayerLyrics() const { return m_usePlayerLyrics; }
    void setUsePlayerLyrics(bool enabled);
    bool useLocalLyrics() const { return m_useLocalLyrics; }
    void setUseLocalLyrics(bool enabled);
    bool useOnlineLyrics() const { return m_useOnlineLyrics; }
    void setUseOnlineLyrics(bool enabled);
    QString extraLyricsDir() const { return m_extraLyricsDir; }
    void setExtraLyricsDir(const QString &dir);

    // 当前播放位置落在第几行；-1 表示还没到第一句（前奏）
    Q_INVOKABLE int indexAt(qint64 positionMs) const;
    Q_INVOKABLE void retry();

Q_SIGNALS:
    void playerChanged();
    void lyricsChanged();
    void providersChanged();

private Q_SLOTS:
    void onTrackChanged();

private:
    void resolve();
    void consider(const Lyrics &lyrics, const QString &source);
    void remember();
    void finish();
    void setState(const QString &state, const QString &message = QString());
    void fetchOnline(int serial);
    void fetchSearch(int serial);
    void applyOnlineObject(const QJsonObject &object, int serial);
    void saveCache(const QString &text, const QString &source, bool instrumental) const;
    QString findLocalLrc() const;
    QStringList searchDirs() const;
    QString cacheFilePath() const;
    QString trackKey() const;
    static QVariantList toVariant(const QVector<LyricLine> &lines);

    // 同一首歌的歌词，进程内缓存一份，切歌来回切不必重新解析
    struct CachedLyrics {
        Lyrics lyrics;
        QString source;
    };

    QPointer<MprisPlayer> m_player;
    QNetworkAccessManager *m_network = nullptr;

    QString m_key;
    QString m_state = QStringLiteral("idle");
    QString m_source;
    QString m_message;
    QVariantList m_lines;
    QVector<qint64> m_times;
    bool m_synced = false;
    bool m_busy = false;
    QString m_plain;
    bool m_haveAny = false;

    bool m_usePlayerLyrics = true;
    bool m_useLocalLyrics = true;
    bool m_useOnlineLyrics = true;
    QString m_extraLyricsDir;

    int m_serial = 0;
    QHash<QString, CachedLyrics> m_memoryCache;
};
