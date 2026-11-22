// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT

#include "lrcparser.h"

#include <QRegularExpression>
#include <algorithm>

namespace {

const QRegularExpression &stampPattern()
{
    // [mm:ss]、[mm:ss.xx]、[mm:ss:xxx] 都有人写，三种都要吃
    static const QRegularExpression re(QStringLiteral("^\\[(\\d{1,3}):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]"));
    return re;
}

const QRegularExpression &metaPattern()
{
    static const QRegularExpression re(QStringLiteral("^\\[([A-Za-z#]+)[:：](.*)\\]$"));
    return re;
}

const QRegularExpression &wordTagPattern()
{
    static const QRegularExpression re(QStringLiteral("<(\\d{1,3}):(\\d{1,2})(?:[.:](\\d{1,3}))?>"));
    return re;
}

// ".5" 是 500ms，".50" 是 500ms，".500" 还是 500ms —— 按位数补零/截断
qint64 fractionToMs(const QString &fraction)
{
    if (fraction.isEmpty()) {
        return 0;
    }
    int value = fraction.toInt();
    switch (fraction.size()) {
    case 1:
        return value * 100;
    case 2:
        return value * 10;
    case 3:
        return value;
    default:
        return value / 10;
    }
}

qint64 stampToMs(const QRegularExpressionMatch &m)
{
    return m.captured(1).toLongLong() * 60000
        + m.captured(2).toLongLong() * 1000
        + fractionToMs(m.captured(3));
}

} // namespace

namespace Lrc {

QString stripWordTags(const QString &text)
{
    QString out = text;
    out.remove(wordTagPattern());
    return out;
}

Lyrics parse(const QString &raw)
{
    Lyrics result;
    if (raw.trimmed().isEmpty()) {
        return result;
    }

    struct Entry {
        qint64 timeMs;
        QString text;
        int order;
    };
    QVector<Entry> entries;
    QStringList plainRows;
    qint64 offsetMs = 0;
    bool sawTimestamp = false;

    const QStringList rows = raw.split(QRegularExpression(QStringLiteral("\r?\n")));
    for (const QString &row : rows) {
        QString rest = row;
        QVector<qint64> stamps;

        // 一行可以挂多个时间戳：[00:12.00][01:24.00]同一句
        for (;;) {
            const QRegularExpressionMatch m = stampPattern().match(rest);
            if (!m.hasMatch()) {
                break;
            }
            stamps.append(stampToMs(m));
            rest = rest.mid(m.capturedLength());
        }

        if (stamps.isEmpty()) {
            const QString trimmed = rest.trimmed();
            if (trimmed.isEmpty()) {
                continue;
            }
            const QRegularExpressionMatch meta = metaPattern().match(trimmed);
            if (meta.hasMatch()) {
                const QString key = meta.captured(1).toLower();
                if (key == QLatin1String("offset")) {
                    offsetMs = meta.captured(2).trimmed().toLongLong();
                }
                continue;                 // ti/ar/al/by/re/ve… 都不进正文
            }
            if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
                continue;                 // 认不出来的标签行，别污染歌词
            }
            plainRows.append(trimmed);
            continue;
        }

        sawTimestamp = true;
        const QString text = stripWordTags(rest).trimmed();
        if (text.isEmpty()) {
            continue;                     // 只有时间戳的空行代表间奏，跳过更干净
        }
        for (qint64 stamp : stamps) {
            entries.append({qMax<qint64>(0, stamp - offsetMs), text, int(entries.size())});
        }
    }

    if (!entries.isEmpty()) {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
            if (a.timeMs != b.timeMs) {
                return a.timeMs < b.timeMs;
            }
            return a.order < b.order;
        });

        result.synced.reserve(entries.size());
        for (const Entry &entry : entries) {
            if (!result.synced.isEmpty()) {
                const LyricLine &last = result.synced.last();
                if (last.timeMs == entry.timeMs && last.text == entry.text) {
                    continue;             // 重复行，保留一条
                }
            }
            result.synced.append({entry.timeMs, entry.text});
        }
        return result;
    }

    // 一个时间戳都没有：当作纯文本歌词，metadata 行已经在上面滤掉了
    if (!sawTimestamp) {
        result.plain = plainRows.join(QLatin1Char('\n')).trimmed();
    }
    return result;
}

QString normalize(const QString &text)
{
    QString s = text.normalized(QString::NormalizationForm_KC).toLower();

    // 括号里的补充说明对匹配没帮助，反而经常是 (Live) / (Remastered 2011) /
    // （电影版）这类会让标题对不上的东西
    static const QRegularExpression bracket(
        QStringLiteral("\\([^)]*\\)|（[^）]*）|\\[[^\\]]*\\]|【[^】]*】|「[^」]*」"));
    s.remove(bracket);

    // 「feat. 某某」之后的部分对匹配没有价值，整段砍掉；
    // 这样「Song feat. X」和「Song」会归一化成同一个标题
    static const QRegularExpression feat(
        QStringLiteral("\\s*\\b(feat|ft|featuring)\\b.*$"));
    s.remove(feat);

    // 标点、符号统一成空格，再压空白
    static const QRegularExpression punct(QStringLiteral("[\\p{P}\\p{S}]+"));
    s.replace(punct, QStringLiteral(" "));

    return s.simplified();
}

int score(const QString &title, const QStringList &artists, const QString &album,
          qint64 durationMs, const QJsonObject &candidate)
{
    const QString wantedTitle = normalize(title);
    const QString trackName = normalize(candidate.value(QStringLiteral("trackName")).toString());
    const QString name = normalize(candidate.value(QStringLiteral("name")).toString());
    if (wantedTitle.isEmpty() || (trackName.isEmpty() && name.isEmpty())) {
        return 0;
    }

    int total = 0;
    if (trackName == wantedTitle || name == wantedTitle) {
        total += 70;
    } else if (trackName.contains(wantedTitle) || wantedTitle.contains(trackName)) {
        total += 45;
    } else {
        return 0;                         // 标题都对不上，直接淘汰
    }

    const QString candidateArtist = normalize(candidate.value(QStringLiteral("artistName")).toString());
    if (!candidateArtist.isEmpty()) {
        bool hit = false;
        for (const QString &artist : artists) {
            const QString wanted = normalize(artist);
            if (wanted.isEmpty()) {
                continue;
            }
            if (wanted == candidateArtist) {
                hit = true;
                break;
            }
            if (wanted.contains(candidateArtist) || candidateArtist.contains(wanted)) {
                hit = true;               // 「RADWIMPS」对「RADWIMPS (ラッドウィンプス)」
            }
        }
        total += hit ? 25 : 0;
    }

    const QString candidateAlbum = normalize(candidate.value(QStringLiteral("albumName")).toString());
    if (!album.isEmpty() && !candidateAlbum.isEmpty() && candidateAlbum == normalize(album)) {
        total += 5;
    }

    // 时长是最可靠的一票否决：同名不同版本（live / remix / 伴奏）靠它区分
    const qint64 candidateDuration = qint64(candidate.value(QStringLiteral("duration")).toDouble() * 1000.0);
    if (durationMs > 0 && candidateDuration > 0) {
        const qint64 delta = qAbs(candidateDuration - durationMs);
        if (delta <= 2000) {
            total += 15;
        } else if (delta <= 5000) {
            total += 8;
        } else if (delta > 15000) {
            total -= 15;
        }
    }

    if (!candidate.value(QStringLiteral("syncedLyrics")).toString().trimmed().isEmpty()) {
        total += 10;                      // 同样的匹配度下优先要能滚动的那个
    }

    return total;
}

} // namespace Lrc
