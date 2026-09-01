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
        QUrl downloadUrl;
        QUrl releaseNotesUrl;
        QString releaseNotesMarkdown;
        QString sha256;
    };

    explicit UpdateChecker(QObject *parent = nullptr);

    void checkForUpdates(bool userInitiated = false);

    void setCurrentVersion(const QString &version);

    static bool isVersionNewer(const QString &latestVersion,
                               const QString &currentVersion);
    static bool parseManifest(const QByteArray &data,
                              Manifest *manifest,
                              QString *errorMessage = nullptr);

signals:
    void updateAvailable(const QString &currentVersion,
                         const QString &latestVersion,
                         const QUrl &releaseUrl,
                         const QString &releaseNotesMarkdown,
                         bool userInitiated);

    void noUpdateAvailable(const QString &currentVersion,
                           const QString &latestVersion,
                           const QUrl &releaseUrl,
                           bool userInitiated);

    void updateCheckFailed(const QString &message,
                           bool userInitiated);

private slots:
    void handleReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_network = nullptr;

    QString m_currentVersion;

    static QUrl manifestUrl();
    static QList<int> parseVersionParts(const QString &version);
};

#endif // UPDATECHECKER_H
