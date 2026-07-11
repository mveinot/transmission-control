#ifndef NOTIFICATIONCONTROLLER_H
#define NOTIFICATIONCONTROLLER_H

#include <QHash>
#include <QSet>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

class torrent;

class NotificationController : public QObject
{
    Q_OBJECT

public:
    using DeliveryFunction = std::function<bool(const QString &, const QString &, int)>;

    explicit NotificationController(QObject *parent = nullptr,
                                    DeliveryFunction deliveryFunction = {});

    void processTorrentList(const QVector<torrent> &torrents);
    void handleTorrentAdded(int torrentId, const QString &name);
    void resetBaseline();
    void showNotification(const QString &title,
                          const QString &message,
                          int millisecondsTimeoutHint = 5000);
    void showTestNotification();

signals:
    void statusMessageRequested(const QString &message, int timeoutMs = 3000);

private:
    struct TorrentState
    {
        bool complete = false;
        bool error = false;
        bool stalled = false;
        QString errorString;
    };

    bool notificationsEnabled() const;
    static bool eventEnabled(const char *settingsKey, bool defaultValue = true);
    bool showPlatformNotification(const QString &title,
                                  const QString &message,
                                  int millisecondsTimeoutHint) const;
    static bool isTorrentCompleteForNotification(const torrent &torrentItem);
    static TorrentState stateForTorrent(const torrent &torrentItem);

    DeliveryFunction m_deliveryFunction;
    bool m_baselineLoaded = false;
    QHash<int, TorrentState> m_knownTorrentStates;
    QSet<int> m_directlyNotifiedAddedTorrentIds;
};

#endif // NOTIFICATIONCONTROLLER_H
