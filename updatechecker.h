#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);

    void checkForUpdates(bool userInitiated = false);

    void setCurrentVersion(const QString &version);
    void setRepository(const QString &owner, const QString &repo);

    static bool isVersionNewer(const QString &latestVersion,
                               const QString &currentVersion);

signals:
signals:
    void updateAvailable(const QString &currentVersion,
                         const QString &latestVersion,
                         const QUrl &releaseUrl,
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
    QString m_owner = QStringLiteral("mveinot");
    QString m_repo = QStringLiteral("transmission-control");

    bool m_userInitiated = false;

    QUrl latestReleaseUrl() const;
    static QList<int> parseVersionParts(const QString &version);
};

#endif // UPDATECHECKER_H