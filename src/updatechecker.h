#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

// Performs one asynchronous release-manifest request at a time and reports a
// normalized semantic-version comparison to its controller.
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    struct Manifest {
        QString version;
        int build = 0;
        QString displayVersion;
        QString minimumMacOSVersion;
        QString packageType;
        QUrl downloadUrl;
        QUrl releaseNotesUrl;
        QString releaseNotesMarkdown;
        QString sha256;
        bool platformArtifactAvailable = true;
    };

    explicit UpdateChecker(QObject *parent = nullptr);

    void checkForUpdates(bool userInitiated = false, bool beta = false);

    void setCurrentVersion(const QString &version);

    static bool isVersionNewer(const QString &latestVersion,
                               const QString &currentVersion);
    static bool parseManifest(const QByteArray &data,
                              Manifest *manifest,
                              QString *errorMessage = nullptr,
                              const QString &expectedChannel = QStringLiteral("stable"),
                              const QString &expectedPlatform = QStringLiteral("macos-universal"));

signals:
    void updateAvailable(const QString &currentVersion,
                         const QString &latestVersion,
                         const QUrl &downloadUrl,
                         const QString &sha256,
                         const QUrl &releaseUrl,
                         const QString &releaseNotesMarkdown,
                         const QString &packageType,
                         bool userInitiated);

    void noUpdateAvailable(const QString &currentVersion,
                           const QString &latestVersion,
                           const QUrl &releaseUrl,
                           bool userInitiated);

    void noPlatformUpdateAvailable(const QUrl &releaseUrl,
                                   bool userInitiated);

    void updateCheckFailed(const QString &message,
                           bool userInitiated);

private slots:
    void handleReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_network = nullptr;

    QString m_currentVersion;

    static QUrl manifestUrl(bool beta);
    static QString currentPlatformKey();
    static QList<int> parseVersionParts(const QString &version);
};

#endif // UPDATECHECKER_H
