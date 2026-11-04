#include <QtTest/QtTest>

#include <QNetworkReply>

#include <cstring>
#include <utility>

#include "updatechecker.h"

class ManifestReply final : public QNetworkReply
{
public:
    explicit ManifestReply(QByteArray data, QObject *parent = nullptr)
        : QNetworkReply(parent), m_data(std::move(data))
    {
        setOpenMode(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    void abort() override {}
    qint64 bytesAvailable() const override
    {
        return m_data.size() - m_offset + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        if (m_offset >= m_data.size())
            return -1;
        const qint64 amount = qMin(maxSize, m_data.size() - m_offset);
        std::memcpy(data, m_data.constData() + m_offset,
                    static_cast<size_t>(amount));
        m_offset += amount;
        return amount;
    }

private:
    QByteArray m_data;
    qsizetype m_offset = 0;
};

class TestUpdateChecker : public QObject
{
    Q_OBJECT

private slots:
    void versionComparison_data();
    void versionComparison();
    void parsesStableManifest();
    void updateResultsKeepTheChannelOfTheirReply();
    void rejectsInvalidManifest_data();
    void rejectsInvalidManifest();
};

void TestUpdateChecker::versionComparison_data()
{
    QTest::addColumn<QString>("latestVersion");
    QTest::addColumn<QString>("currentVersion");
    QTest::addColumn<bool>("isNewer");

    QTest::newRow("patch newer") << QStringLiteral("1.2.1") << QStringLiteral("1.2.0") << true;
    QTest::newRow("leading v") << QStringLiteral("v1.2.1") << QStringLiteral("1.2.0") << true;
    QTest::newRow("two digit minor") << QStringLiteral("1.10.0") << QStringLiteral("1.9.9") << true;
    QTest::newRow("missing patch equivalent") << QStringLiteral("1.2") << QStringLiteral("1.2.0") << false;
    QTest::newRow("equal full version") << QStringLiteral("1.2.0") << QStringLiteral("1.2.0") << false;
    QTest::newRow("older latest") << QStringLiteral("1.2.0") << QStringLiteral("1.2.1") << false;
    QTest::newRow("prerelease suffix treated as non-numeric tail") << QStringLiteral("v1.2.0-beta") << QStringLiteral("1.2.0") << false;
    QTest::newRow("build component newer") << QStringLiteral("1.2.0.42") << QStringLiteral("1.2.0.41") << true;
}

void TestUpdateChecker::versionComparison()
{
    QFETCH(QString, latestVersion);
    QFETCH(QString, currentVersion);
    QFETCH(bool, isNewer);

    QCOMPARE(UpdateChecker::isVersionNewer(latestVersion, currentVersion), isNewer);
}

void TestUpdateChecker::parsesStableManifest()
{
    const QByteArray data = R"json({
        "schemaVersion": 1,
        "channel": "stable",
        "version": "2.0.0",
        "build": 376,
        "displayVersion": "2.0.0.376",
        "minimumMacOSVersion": "13.0",
        "downloadUrl": "https://github.com/mveinot/transmission-control/releases/download/v2.0.0.376/Planetary-2.0.0.376-macOS-universal.dmg",
        "releaseNotesUrl": "https://github.com/mveinot/transmission-control/releases/tag/v2.0.0.376",
        "releaseNotesMarkdown": "## What's new\n\n- Universal macOS binary",
        "sha256": "fd0dd6fa10d9a3098f6ac554cf6419ee96b01a54e16e3b59b504fb6c5f4e0cf8"
    })json";

    UpdateChecker::Manifest manifest;
    QString errorMessage;
    QVERIFY2(UpdateChecker::parseManifest(data, &manifest, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(manifest.version, QStringLiteral("2.0.0"));
    QCOMPARE(manifest.build, 376);
    QCOMPARE(manifest.displayVersion, QStringLiteral("2.0.0.376"));
    QCOMPARE(manifest.minimumMacOSVersion, QStringLiteral("13.0"));
    QCOMPARE(manifest.downloadUrl,
             QUrl(QStringLiteral("https://github.com/mveinot/transmission-control/releases/download/v2.0.0.376/Planetary-2.0.0.376-macOS-universal.dmg")));
    QCOMPARE(manifest.releaseNotesUrl,
             QUrl(QStringLiteral("https://github.com/mveinot/transmission-control/releases/tag/v2.0.0.376")));
    QCOMPARE(manifest.releaseNotesMarkdown,
             QStringLiteral("## What's new\n\n- Universal macOS binary"));
    QCOMPARE(manifest.sha256,
             QStringLiteral("fd0dd6fa10d9a3098f6ac554cf6419ee96b01a54e16e3b59b504fb6c5f4e0cf8"));

    QByteArray dataWithoutReleaseNotes = data;
    dataWithoutReleaseNotes.replace(
        "        \"releaseNotesMarkdown\": \"## What's new\\n\\n- Universal macOS binary\",\n",
        "");
    UpdateChecker::Manifest manifestWithoutReleaseNotes;
    QVERIFY2(UpdateChecker::parseManifest(dataWithoutReleaseNotes,
                                          &manifestWithoutReleaseNotes,
                                          &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(manifestWithoutReleaseNotes.releaseNotesMarkdown.isEmpty());
}

void TestUpdateChecker::updateResultsKeepTheChannelOfTheirReply()
{
    const QByteArray stableManifest = R"json({
        "schemaVersion": 1,
        "channel": "stable",
        "version": "2.0.0",
        "build": 376,
        "displayVersion": "2.0.0.376",
        "minimumMacOSVersion": "13.0",
        "downloadUrl": "https://example.com/Planetary.dmg",
        "releaseNotesUrl": "https://example.com/releases/2.0.0.376",
        "sha256": "fd0dd6fa10d9a3098f6ac554cf6419ee96b01a54e16e3b59b504fb6c5f4e0cf8"
    })json";

    UpdateChecker checker;
    checker.setCurrentVersion(QStringLiteral("1.0.0"));
    QSignalSpy available(&checker, &UpdateChecker::updateAvailable);
    QVERIFY(available.isValid());

    QByteArray betaManifest = stableManifest;
    betaManifest.replace("\"stable\"", "\"beta\"");

    // Deliver replies in the reverse order from which the corresponding
    // channel checks would commonly be started.
    auto *betaReply = new ManifestReply(betaManifest, &checker);
    betaReply->setProperty("userInitiated", true);
    betaReply->setProperty("beta", true);
    QVERIFY(QMetaObject::invokeMethod(
        &checker, "handleReplyFinished", Qt::DirectConnection,
        Q_ARG(QNetworkReply *, betaReply)));

    auto *stableReply = new ManifestReply(stableManifest, &checker);
    stableReply->setProperty("userInitiated", true);
    stableReply->setProperty("beta", false);
    QVERIFY(QMetaObject::invokeMethod(
        &checker, "handleReplyFinished", Qt::DirectConnection,
        Q_ARG(QNetworkReply *, stableReply)));

    QCOMPARE(available.size(), 2);
    QCOMPARE(available.at(0).at(8).toBool(), true);
    QCOMPARE(available.at(1).at(8).toBool(), false);
}

void TestUpdateChecker::rejectsInvalidManifest_data()
{
    QTest::addColumn<QByteArray>("data");

    const QByteArray valid = R"json({
        "schemaVersion": 1,
        "channel": "stable",
        "version": "2.0.0",
        "build": 376,
        "displayVersion": "2.0.0.376",
        "minimumMacOSVersion": "13.0",
        "downloadUrl": "https://example.com/Planetary.dmg",
        "releaseNotesUrl": "https://example.com/releases/2.0.0.376",
        "sha256": "fd0dd6fa10d9a3098f6ac554cf6419ee96b01a54e16e3b59b504fb6c5f4e0cf8"
    })json";

    QTest::newRow("not json") << QByteArray("not json");
    QTest::newRow("unsupported schema")
        << QByteArray(valid).replace("\"schemaVersion\": 1", "\"schemaVersion\": 2");
    QTest::newRow("wrong channel")
        << QByteArray(valid).replace("\"stable\"", "\"beta\"");
    QTest::newRow("mismatched display version")
        << QByteArray(valid).replace("\"2.0.0.376\"", "\"2.0.0.377\"");
    QTest::newRow("insecure download URL")
        << QByteArray(valid).replace("https://example.com/Planetary.dmg",
                                     "http://example.com/Planetary.dmg");
    QTest::newRow("missing digest")
        << QByteArray(valid).replace(
               "fd0dd6fa10d9a3098f6ac554cf6419ee96b01a54e16e3b59b504fb6c5f4e0cf8",
               "");
}

void TestUpdateChecker::rejectsInvalidManifest()
{
    QFETCH(QByteArray, data);

    UpdateChecker::Manifest manifest;
    QString errorMessage;
    QVERIFY(!UpdateChecker::parseManifest(data, &manifest, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

QTEST_MAIN(TestUpdateChecker)
#include "test_updatechecker.moc"
