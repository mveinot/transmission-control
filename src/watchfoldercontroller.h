#ifndef WATCHFOLDERCONTROLLER_H
#define WATCHFOLDERCONTROLLER_H

#include <QObject>
#include <QString>

class TorrentBackend;
class TorrentAddController;
class WatchFolderManager;

// Bridges stable-file detection to torrent addition and closes the retry loop
// using path-correlated RPC success/failure signals.
class WatchFolderController : public QObject
{
    Q_OBJECT

public:
    explicit WatchFolderController(WatchFolderManager *manager,
                                   TorrentAddController *torrentAddController,
                                   TorrentBackend *client,
                                   QObject *parent = nullptr);

    void setup();
    void loadSettings();

signals:
    void statusMessageRequested(const QString &message, int timeoutMs);
    void torrentListRefreshRequested();
    // Activity events are completed with the profile display name by MainWindow.
    void activityEventRequested(const QString &event, const QString &details);

private:
    WatchFolderManager *m_manager = nullptr;
    TorrentAddController *m_torrentAddController = nullptr;
    TorrentBackend *m_client = nullptr;

    void handleTorrentFileAddFailed(const QString &filePath,
                                    const QString &message);
    bool shouldTreatFailureAsProcessed(const QString &message) const;
};

#endif // WATCHFOLDERCONTROLLER_H
