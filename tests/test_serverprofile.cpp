#include <QtTest>

#include "serverprofile.h"
#include "settingskeys.h"

#include <QSettings>
#include <QTemporaryDir>

class TestServerProfile : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void roundTripsTypedProfiles();
    void selectsDefaultThenCurrentThenFirstValid();

private:
    QTemporaryDir m_settingsDirectory;
};

void TestServerProfile::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("PlanetaryTests"));
    QCoreApplication::setApplicationName(QStringLiteral("ServerProfiles"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       m_settingsDirectory.path());
}

void TestServerProfile::roundTripsTypedProfiles()
{
    QSettings().clear();

    ServerProfile transmission;
    transmission.name = QStringLiteral("Home");
    transmission.rpcUrl = QStringLiteral("http://localhost:9091/transmission/rpc");
    transmission.username = QStringLiteral("user");
    transmission.password = QStringLiteral("secret");
    transmission.folderMappings = {
        {QStringLiteral("/downloads"), QStringLiteral("/Volumes/Downloads")}
    };

    ServerProfile qBittorrent;
    qBittorrent.backendType = QStringLiteral("qbittorrent");
    qBittorrent.name = QStringLiteral("Remote");
    qBittorrent.rpcUrl = QStringLiteral("https://seedbox.example");

    ServerProfileRepository repository;
    repository.saveProfiles({transmission, qBittorrent});
    const QVector<ServerProfile> loaded = repository.loadProfiles();

    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded.at(0).settingsIndex, 0);
    QCOMPARE(loaded.at(0).name, transmission.name);
    QCOMPARE(loaded.at(0).password, transmission.password);
    QCOMPARE(loaded.at(0).folderMappings.size(), 1);
    QCOMPARE(loaded.at(0).folderMappings.first().remotePath,
             QStringLiteral("/downloads"));
    QCOMPARE(loaded.at(1).settingsIndex, 1);
    QCOMPARE(loaded.at(1).backendType, QStringLiteral("qbittorrent"));
}

void TestServerProfile::selectsDefaultThenCurrentThenFirstValid()
{
    QSettings().clear();

    ServerProfile invalid;
    invalid.name = QStringLiteral("Incomplete");

    ServerProfile firstValid;
    firstValid.rpcUrl = QStringLiteral("http://first.example");

    ServerProfile current;
    current.backendType = QStringLiteral("deluge");
    current.rpcUrl = QStringLiteral("http://current.example:8112");

    ServerProfileRepository repository;
    repository.saveProfiles({invalid, firstValid, current});
    repository.setCurrentIndex(2);

    QSettings settings;
    settings.setValue(SettingsKeys::ServersDefaultIndex, 1);
    QCOMPARE(repository.preferredIndex(), 1);

    settings.setValue(SettingsKeys::ServersDefaultIndex, 99);
    QCOMPARE(repository.preferredIndex(), 2);

    repository.setCurrentIndex(99);
    QCOMPARE(repository.preferredIndex(), 1);
}

QTEST_MAIN(TestServerProfile)
#include "test_serverprofile.moc"
