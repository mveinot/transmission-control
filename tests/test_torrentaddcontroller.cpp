#include <QtTest/QtTest>

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "settingskeys.h"
#include "torrentaddcontroller.h"
#include "torrentbackend.h"

class RecordingTorrentBackend final : public TorrentBackend
{
public:
    QString backendName() const override { return QStringLiteral("Transmission"); }
    QString serverDisplayName() const override { return QStringLiteral("Test"); }
    QString endpointUrl() const override { return QStringLiteral("http://localhost/rpc"); }
    QString protocolDescription() const override { return {}; }
    TorrentBackendCapabilities capabilities() const override { return {}; }

    bool setServerProfile(const ServerProfile &) override { return true; }
    void init() override {}
    void getTorrentList() override {}
    void getTorrentTrackerMetadata() override {}
    void getTorrentDetails(const TorrentKey &) override {}
    void getTorrentFiles(const TorrentKey &) override {}
    void getTorrentPeers(const TorrentKey &) override {}
    void getTorrentTrackers(const TorrentKey &) override {}
    void getTorrentPieces(const TorrentKey &) override {}
    void getTorrentProperties(const TorrentKey &) override {}
    void cancelTorrentDetailRequests() override {}
    void addTorrentFromFile(const QString &, bool) override {}
    void addTorrentFromMagnet(const QString &) override {}

    void addTorrentFile(const QString &filePath,
                        const QString &downloadDir,
                        bool paused,
                        const QList<int> &,
                        const QList<int> &,
                        const QList<int> &,
                        bool deleteFileOnSuccess) override
    {
        ++fileAddCount;
        lastSource = filePath;
        lastDownloadDir = downloadDir;
        lastPaused = paused;
        lastDeleteFileOnSuccess = deleteFileOnSuccess;
    }

    void addMagnetLink(const QString &magnetLink,
                       const QString &downloadDir,
                       bool paused) override
    {
        ++magnetAddCount;
        lastSource = magnetLink;
        lastDownloadDir = downloadDir;
        lastPaused = paused;
    }

    void startTorrents(const QList<TorrentKey> &) override {}
    void startAllTorrents() override {}
    void startTorrentsNow(const QList<TorrentKey> &) override {}
    void stopTorrents(const QList<TorrentKey> &) override {}
    void stopAllTorrents() override {}
    void removeTorrents(const QList<TorrentKey> &, bool) override {}
    void verifyTorrents(const QList<TorrentKey> &) override {}
    void reannounceTorrents(const QList<TorrentKey> &) override {}
    void setTorrentLocation(const QList<TorrentKey> &, const QString &, bool) override {}
    void setTorrentFilesWanted(const TorrentKey &, const QList<int> &, bool) override {}
    void setTorrentFilesPriority(const TorrentKey &, const QList<int> &, int) override {}
    void setTorrentFilesWantedAndPriority(const TorrentKey &,
                                          const QList<int> &,
                                          bool,
                                          int) override {}
    void addTorrentTracker(const TorrentKey &, const QString &) override {}
    void editTorrentTracker(const TorrentKey &, int, const QString &) override {}
    void removeTorrentTracker(const TorrentKey &, int) override {}
    void renameTorrentPath(const TorrentKey &, const QString &, const QString &) override {}
    void setTorrentProperties(const TorrentKey &,
                              const TorrentPropertyChanges &) override {}
    void setTorrentsSequentialDownload(const QList<TorrentKey> &, bool) override {}
    void setTorrentsBandwidthPriority(const QList<TorrentKey> &, int) override {}
    void queueMoveTop(const QList<TorrentKey> &) override {}
    void queueMoveUp(const QList<TorrentKey> &) override {}
    void queueMoveDown(const QList<TorrentKey> &) override {}
    void queueMoveBottom(const QList<TorrentKey> &) override {}
    void getSessionSettings() override {}
    void getSessionStatistics() override {}
    void setSessionSettings(const QJsonObject &) override {}
    void getFreeSpace(const QString &) override {}
    void testPortForwarding() override {}
    void updateBlocklist(const QJsonObject &) override {}

    int fileAddCount = 0;
    int magnetAddCount = 0;
    QString lastSource;
    QString lastDownloadDir;
    bool lastPaused = false;
    bool lastDeleteFileOnSuccess = false;
};

class TestTorrentAddController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void skipsMagnetOptionsDialog();
    void skipsTorrentFileOptionsDialog();

private:
    QTemporaryDir m_settingsDirectory;
};

void TestTorrentAddController::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("PlanetaryTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TorrentAddController"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,
                       QSettings::UserScope,
                       m_settingsDirectory.path());
}

void TestTorrentAddController::init()
{
    QSettings().clear();
}

void TestTorrentAddController::skipsMagnetOptionsDialog()
{
    QSettings settings;
    settings.setValue(SettingsKeys::ShowMagnetLinkOptionsDialog, false);
    settings.setValue(SettingsKeys::TorrentAddDownloadDir,
                      QStringLiteral("/downloads/magnets"));
    settings.setValue(SettingsKeys::StartTorrentPaused, true);

    RecordingTorrentBackend backend;
    TorrentAddController controller(&backend, nullptr);
    QSignalSpy startedSpy(&controller, &TorrentAddController::addStarted);

    const QString magnet = QStringLiteral("magnet:?xt=urn:btih:abcdef");
    controller.addMagnetLink(magnet);

    QCOMPARE(backend.magnetAddCount, 1);
    QCOMPARE(backend.fileAddCount, 0);
    QCOMPARE(backend.lastSource, magnet);
    QCOMPARE(backend.lastDownloadDir, QStringLiteral("/downloads/magnets"));
    QVERIFY(backend.lastPaused);
    QCOMPARE(startedSpy.count(), 1);
}

void TestTorrentAddController::skipsTorrentFileOptionsDialog()
{
    QTemporaryFile torrentFile;
    QVERIFY(torrentFile.open());

    QSettings settings;
    settings.setValue(SettingsKeys::ShowTorrentFileOptionsDialog, false);
    settings.setValue(SettingsKeys::TorrentAddDownloadDir,
                      QStringLiteral("/downloads/files"));
    settings.setValue(SettingsKeys::StartTorrentPaused, true);
    settings.setValue(SettingsKeys::DeleteTorrentOnAdd, true);

    RecordingTorrentBackend backend;
    TorrentAddController controller(&backend, nullptr);
    QSignalSpy startedSpy(&controller, &TorrentAddController::addStarted);

    controller.addTorrentFile(torrentFile.fileName());

    QCOMPARE(backend.fileAddCount, 1);
    QCOMPARE(backend.magnetAddCount, 0);
    QCOMPARE(backend.lastSource, QFileInfo(torrentFile.fileName()).absoluteFilePath());
    QCOMPARE(backend.lastDownloadDir, QStringLiteral("/downloads/files"));
    QVERIFY(backend.lastPaused);
    QVERIFY(backend.lastDeleteFileOnSuccess);
    QCOMPARE(startedSpy.count(), 1);
}

QTEST_MAIN(TestTorrentAddController)
#include "test_torrentaddcontroller.moc"
