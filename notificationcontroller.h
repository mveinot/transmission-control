#ifndef NOTIFICATIONCONTROLLER_H
#define NOTIFICATIONCONTROLLER_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
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
    void setServerName(const QString &serverName);
    void showNotification(const QString &title,
                          const QString &message,
                          int millisecondsTimeoutHint = 5000);
    void showTestNotification();
    void showTestExternalCommand(const QString &executable,
                                 const QString &argumentTemplate);

    static QStringList parseExternalArguments(const QString &argumentTemplate);

signals:
    void activityEventObserved(const QString &event, const QString &details,
                               const QString &server);
    void statusMessageRequested(const QString &message, int timeoutMs = 3000);

private:
    struct TorrentState
    {
        bool complete = false;
        bool error = false;
        bool stalled = false;
        QString errorString;
    };

    struct EventContext
    {
        QString event;
        QString name;
        int id = -1;
        QString hash;
        qint64 sizeBytes = 0;
        QString size;
        QString status;
        double progress = 0.0;
        QString downloadDir;
        QString server;
        QString error;
        QString timestamp;
    };

    bool notificationsEnabled() const;
    static bool eventEnabled(const char *settingsKey, bool defaultValue = true);
    static bool deliveryEnabled(const char *settingsKey, bool defaultValue);
    void dispatchEvent(const QString &title,
                       const QString &message,
                       const EventContext &context,
                       int millisecondsTimeoutHint);
    bool runExternalCommand(const QString &executable,
                            const QString &argumentTemplate,
                            const EventContext &context) const;
    static QString expandArgument(QString argument, const EventContext &context);
    static EventContext contextForTorrent(const QString &event,
                                          const torrent &torrentItem,
                                          const QString &serverName);
    bool showPlatformNotification(const QString &title,
                                  const QString &message,
                                  int millisecondsTimeoutHint) const;
    static bool isTorrentCompleteForNotification(const torrent &torrentItem);
    static TorrentState stateForTorrent(const torrent &torrentItem);

    DeliveryFunction m_deliveryFunction;
    QString m_serverName;
    bool m_baselineLoaded = false;
    QHash<int, TorrentState> m_knownTorrentStates;
    QSet<int> m_directlyNotifiedAddedTorrentIds;
};

#endif // NOTIFICATIONCONTROLLER_H
