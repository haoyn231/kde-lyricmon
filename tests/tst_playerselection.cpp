// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// 浏览器识别的单元测试。分类只看服务名和 Identity，不碰 D-Bus，适合无显示、
// 无 D-Bus 的 CI 环境。

#include "playerclassifier.h"

#include <QTest>

class PlayerSelectionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stripsPrefixAndInstance();
    void detectsBrowserServices();
    void keepsMusicPlayerServices();
    void detectsBrowserIdentities();
    void ignoresChromeLikeWords();
    void trustsIdentityOverSharedChromiumService();
    void treatsChromiumWithoutIdentityAsBrowser();
    void detectsPlasmaBrowserIntegration();
};

void PlayerSelectionTest::stripsPrefixAndInstance()
{
    QCOMPARE(PlayerClassifier::serviceBaseName(
                 QStringLiteral("org.mpris.MediaPlayer2.chromium.instance5512")),
             QStringLiteral("chromium"));
    QCOMPARE(PlayerClassifier::serviceBaseName(QStringLiteral("org.mpris.MediaPlayer2.vlc")),
             QStringLiteral("vlc"));
    QCOMPARE(PlayerClassifier::serviceBaseName(QStringLiteral("org.mpris.MediaPlayer2.vlc.instance9")),
             QStringLiteral("vlc"));
}

void PlayerSelectionTest::detectsBrowserServices()
{
    const QStringList services = {
        QStringLiteral("org.mpris.MediaPlayer2.chromium.instance5512"),
        QStringLiteral("org.mpris.MediaPlayer2.firefox.instance42"),
        QStringLiteral("org.mpris.MediaPlayer2.google-chrome.instance7"),
        QStringLiteral("org.mpris.MediaPlayer2.brave.instance1"),
        QStringLiteral("org.mpris.MediaPlayer2.vivaldi.instance2"),
        QStringLiteral("org.mpris.MediaPlayer2.plasma-browser-integration"),
    };
    for (const QString &service : services) {
        QVERIFY2(PlayerClassifier::isBrowserService(service), qPrintable(service));
    }
}

void PlayerSelectionTest::keepsMusicPlayerServices()
{
    const QStringList services = {
        QStringLiteral("org.mpris.MediaPlayer2.spotify.instance1"),
        QStringLiteral("org.mpris.MediaPlayer2.vlc.instance7"),
        QStringLiteral("org.mpris.MediaPlayer2.genraku.instance34180"),
        QStringLiteral("org.mpris.MediaPlayer2.strawberry"),
        QStringLiteral("org.mpris.MediaPlayer2.mpv.instance3"),
        QStringLiteral("org.mpris.MediaPlayer2.playerctld"),
    };
    for (const QString &service : services) {
        QVERIFY2(!PlayerClassifier::isBrowserService(service), qPrintable(service));
    }
}

void PlayerSelectionTest::detectsBrowserIdentities()
{
    const QStringList identities = {
        QStringLiteral("Chrome"),
        QStringLiteral("Google Chrome"),
        QStringLiteral("Chromium"),
        QStringLiteral("Mozilla Firefox"),
        QStringLiteral("Brave Browser"),
        QStringLiteral("Vivaldi"),
        QStringLiteral("Opera"),
        QStringLiteral("Microsoft Edge"),
        QStringLiteral("GNOME Web"),
        QStringLiteral("qutebrowser"),
    };
    for (const QString &identity : identities) {
        QVERIFY2(PlayerClassifier::isBrowserIdentity(identity), qPrintable(identity));
    }
}

void PlayerSelectionTest::ignoresChromeLikeWords()
{
    QVERIFY(!PlayerClassifier::isBrowserIdentity(QStringLiteral("Chromecast Audio")));
    QVERIFY(!PlayerClassifier::isBrowserIdentity(QStringLiteral("Spotify")));
    QVERIFY(!PlayerClassifier::isBrowserIdentity(QStringLiteral("VLC media player")));
    QVERIFY(!PlayerClassifier::isBrowserIdentity(QString()));
}

void PlayerSelectionTest::trustsIdentityOverSharedChromiumService()
{
    // QQ 这类 Chromium 系应用也用 org.mpris.MediaPlayer2.chromium.instanceN，
    // 只要 Identity 不是浏览器，就不能把它当成浏览器忽略掉。
    const QString service = QStringLiteral("org.mpris.MediaPlayer2.chromium.instance2633");
    QVERIFY(!PlayerClassifier::isBrowser(service, QStringLiteral("QQ")));
    QVERIFY(PlayerClassifier::isBrowser(service, QStringLiteral("Chrome")));
}

void PlayerSelectionTest::treatsChromiumWithoutIdentityAsBrowser()
{
    const QString service = QStringLiteral("org.mpris.MediaPlayer2.chromium.instance5512");
    QVERIFY(PlayerClassifier::isBrowser(service));
    QVERIFY(PlayerClassifier::isBrowser(service, QString()));
}

void PlayerSelectionTest::detectsPlasmaBrowserIntegration()
{
    const QString service = QStringLiteral("org.mpris.MediaPlayer2.plasma-browser-integration");
    QVERIFY(PlayerClassifier::isBrowser(service));
    QVERIFY(PlayerClassifier::isBrowser(service, QStringLiteral("Google Chrome")));
}

QTEST_GUILESS_MAIN(PlayerSelectionTest)

#include "tst_playerselection.moc"
