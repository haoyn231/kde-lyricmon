// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// LRC 解析与匹配评分的单元测试。这两块是纯函数，最容易回归，也最值得被钉住。

#include "lrcparser.h"

#include <QTest>

class LyricsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesPlainTimestamps();
    void parsesMultipleStampsPerLine();
    void handlesFractionDigits();
    void appliesOffsetTag();
    void skipsMetadataTags();
    void stripsWordLevelTags();
    void sortsAndDedupes();
    void fallsBackToPlainText();
    void treatsEmptyInputAsEmpty();
    void normalizeDropsBracketsAndFeaturing();
    void scorePrefersExactMatch();
    void scoreRejectsDifferentTitle();
    void scorePunishesDurationMismatch();
    void scoreRewardsSyncedLyrics();
};

void LyricsTest::parsesPlainTimestamps()
{
    const Lyrics lyrics = Lrc::parse(QStringLiteral(
        "[00:01.00]第一句\n"
        "[00:12.50]第二句\n"
        "[01:02.25]第三句\n"));

    QCOMPARE(lyrics.synced.size(), 3);
    QCOMPARE(lyrics.synced.at(0).timeMs, qint64(1000));
    QCOMPARE(lyrics.synced.at(1).timeMs, qint64(12500));
    QCOMPARE(lyrics.synced.at(2).timeMs, qint64(62250));
    QCOMPARE(lyrics.synced.at(1).text, QStringLiteral("第二句"));
    QVERIFY(lyrics.isSynced());
    QVERIFY(lyrics.plain.isEmpty());
}

void LyricsTest::parsesMultipleStampsPerLine()
{
    const Lyrics lyrics = Lrc::parse(QStringLiteral(
        "[00:05.00][00:35.00]副歌\n"
        "[00:20.00]主歌\n"));

    QCOMPARE(lyrics.synced.size(), 3);
    QCOMPARE(lyrics.synced.at(0).timeMs, qint64(5000));
    QCOMPARE(lyrics.synced.at(1).timeMs, qint64(20000));
    QCOMPARE(lyrics.synced.at(2).timeMs, qint64(35000));
    QCOMPARE(lyrics.synced.at(2).text, QStringLiteral("副歌"));
}

void LyricsTest::handlesFractionDigits()
{
    // .5 / .50 / .500 都代表 500ms；.050 代表 50ms
    const Lyrics lyrics = Lrc::parse(QStringLiteral(
        "[00:01.5]a\n"
        "[00:02.50]b\n"
        "[00:03.500]c\n"
        "[00:04.050]d\n"));

    QCOMPARE(lyrics.synced.size(), 4);
    QCOMPARE(lyrics.synced.at(0).timeMs, qint64(1500));
    QCOMPARE(lyrics.synced.at(1).timeMs, qint64(2500));
    QCOMPARE(lyrics.synced.at(2).timeMs, qint64(3500));
    QCOMPARE(lyrics.synced.at(3).timeMs, qint64(4050));
}

void LyricsTest::appliesOffsetTag()
{
    // offset 为正表示歌词整体提前
    const Lyrics lyrics = Lrc::parse(QStringLiteral(
        "[offset:500]\n"
        "[00:10.00]一句\n"));

    QCOMPARE(lyrics.synced.size(), 1);
    QCOMPARE(lyrics.synced.at(0).timeMs, qint64(9500));
}

void LyricsTest::skipsMetadataTags()
{
    const Lyrics lyrics = Lrc::parse(QStringLiteral(
        "[ti:标题]\n"
        "[ar:歌手]\n"
        "[al:专辑]\n"
        "[by:某人]\n"
        "[00:03.00]歌词\n"));

    QCOMPARE(lyrics.synced.size(), 1);
    QCOMPARE(lyrics.synced.at(0).text, QStringLiteral("歌词"));
}

void LyricsTest::stripsWordLevelTags()
{
    const Lyrics lyrics = Lrc::parse(QStringLiteral(
        "[00:03.00]<00:03.00>逐<00:03.40>字<00:03.80>标签\n"));

    QCOMPARE(lyrics.synced.size(), 1);
    QCOMPARE(lyrics.synced.at(0).text, QStringLiteral("逐字标签"));
}

void LyricsTest::sortsAndDedupes()
{
    const Lyrics lyrics = Lrc::parse(QStringLiteral(
        "[00:20.00]后\n"
        "[00:10.00]前\n"
        "[00:10.00]前\n"));

    QCOMPARE(lyrics.synced.size(), 2);
    QCOMPARE(lyrics.synced.at(0).text, QStringLiteral("前"));
    QCOMPARE(lyrics.synced.at(1).text, QStringLiteral("后"));
}

void LyricsTest::fallsBackToPlainText()
{
    const Lyrics lyrics = Lrc::parse(QStringLiteral(
        "第一行\n"
        "\n"
        "第二行\n"));

    QVERIFY(lyrics.synced.isEmpty());
    QCOMPARE(lyrics.plain, QStringLiteral("第一行\n第二行"));
    QVERIFY(!lyrics.isEmpty());
}

void LyricsTest::treatsEmptyInputAsEmpty()
{
    QVERIFY(Lrc::parse(QString()).isEmpty());
    QVERIFY(Lrc::parse(QStringLiteral("   \n\n")).isEmpty());
    // 只有时间戳没有文字，等于没有歌词
    QVERIFY(Lrc::parse(QStringLiteral("[00:10.00]\n[00:20.00]")).isEmpty());
}

void LyricsTest::normalizeDropsBracketsAndFeaturing()
{
    QCOMPARE(Lrc::normalize(QStringLiteral("Yellow (Live)")), QStringLiteral("yellow"));
    QCOMPARE(Lrc::normalize(QStringLiteral("夜に駆ける（电影版）")), QStringLiteral("夜に駆ける"));
    QCOMPARE(Lrc::normalize(QStringLiteral("Song feat. Someone")), QStringLiteral("song"));
    QCOMPARE(Lrc::normalize(QStringLiteral("Hello, World!")), QStringLiteral("hello world"));
}

void LyricsTest::scorePrefersExactMatch()
{
    const QJsonObject candidate{
        {QStringLiteral("trackName"), QStringLiteral("Yellow")},
        {QStringLiteral("artistName"), QStringLiteral("Coldplay")},
        {QStringLiteral("albumName"), QStringLiteral("Parachutes")},
        {QStringLiteral("duration"), 267.0},
        {QStringLiteral("syncedLyrics"), QStringLiteral("[00:01.00]x")},
    };

    const int exact = Lrc::score(QStringLiteral("Yellow"), {QStringLiteral("Coldplay")},
                                 QStringLiteral("Parachutes"), 267000, candidate);
    const int wrongAlbum = Lrc::score(QStringLiteral("Yellow"), {QStringLiteral("Coldplay")},
                                      QStringLiteral("别的专辑"), 267000, candidate);
    const int wrongArtist = Lrc::score(QStringLiteral("Yellow"), {QStringLiteral("Someone")},
                                       QString(), 267000, candidate);

    QVERIFY2(exact > wrongAlbum, "专辑对上应该加分");
    QVERIFY2(exact > wrongArtist, "艺术家对上应该加分");
    QVERIFY(exact >= 100);
}

void LyricsTest::scoreRejectsDifferentTitle()
{
    const QJsonObject candidate{
        {QStringLiteral("trackName"), QStringLiteral("Fix You")},
        {QStringLiteral("artistName"), QStringLiteral("Coldplay")},
        {QStringLiteral("duration"), 295.0},
    };
    QCOMPARE(Lrc::score(QStringLiteral("Yellow"), {QStringLiteral("Coldplay")}, QString(),
                        267000, candidate), 0);
}

void LyricsTest::scorePunishesDurationMismatch()
{
    const QJsonObject base{
        {QStringLiteral("trackName"), QStringLiteral("Yellow")},
        {QStringLiteral("artistName"), QStringLiteral("Coldplay")},
        {QStringLiteral("duration"), 267.0},
    };
    const int same = Lrc::score(QStringLiteral("Yellow"), {QStringLiteral("Coldplay")},
                                QString(), 267000, base);
    const int offByMinute = Lrc::score(QStringLiteral("Yellow"), {QStringLiteral("Coldplay")},
                                       QString(), 400000, base);
    QVERIFY2(same > offByMinute + 20, "同曲不同版本应该被时长拉开差距");
}

void LyricsTest::scoreRewardsSyncedLyrics()
{
    QJsonObject plain{
        {QStringLiteral("trackName"), QStringLiteral("Yellow")},
        {QStringLiteral("artistName"), QStringLiteral("Coldplay")},
        {QStringLiteral("duration"), 267.0},
    };
    QJsonObject synced = plain;
    synced.insert(QStringLiteral("syncedLyrics"), QStringLiteral("[00:01.00]x"));

    const int plainScore = Lrc::score(QStringLiteral("Yellow"), {QStringLiteral("Coldplay")},
                                      QString(), 267000, plain);
    const int syncedScore = Lrc::score(QStringLiteral("Yellow"), {QStringLiteral("Coldplay")},
                                       QString(), 267000, synced);
    QVERIFY(syncedScore > plainScore);
}

QTEST_MAIN(LyricsTest)
#include "tst_lyrics.moc"
