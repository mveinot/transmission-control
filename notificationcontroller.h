#ifndef NOTIFICATIONCONTROLLER_H
#define NOTIFICATIONCONTROLLER_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

class torrent;

class NotificationController : public QObject
{
    Q_OBJECT

public:
    explicit NotificationController(QObject *parent = nullptr);

    void processTorrentList(const QVector<torrent> &torrents);
    void showNotification(const QString &title,
                          const QString &message,
                          int millisecondsTimeoutHint = 5000);

signals:
    void statusMessageRequested(const QString &message, int timeoutMs = 3000);

private:
    bool notificationsEnabled() const;
    bool showPlatformNotification(const QString &title,
                                  const QString &message,
                                  int millisecondsTimeoutHint) const;
    static bool isTorrentCompleteForNotification(const torrent &torrentItem);

    bool m_completedTorrentNotificationBaselineLoaded = false;
    QSet<int> m_knownCompletedTorrentIds;
};

#endif // NOTIFICATIONCONTROLLER_H
