#ifndef WATCHFOLDERCONTROLLER_H
#define WATCHFOLDERCONTROLLER_H

#include <QObject>
#include <QString>

class rpc_client;
class TorrentAddController;
class WatchFolderManager;

class WatchFolderController : public QObject
{
    Q_OBJECT

public:
    explicit WatchFolderController(WatchFolderManager *manager,
                                   TorrentAddController *torrentAddController,
                                   rpc_client *client,
                                   QObject *parent = nullptr);

    void setup();
    void loadSettings();

signals:
    void statusMessageRequested(const QString &message, int timeoutMs);
    void torrentListRefreshRequested();

private:
    WatchFolderManager *m_manager = nullptr;
    TorrentAddController *m_torrentAddController = nullptr;
    rpc_client *m_client = nullptr;

    void handleTorrentFileAddFailed(const QString &filePath,
                                    const QString &message);
    bool shouldTreatFailureAsProcessed(const QString &message) const;
};

#endif // WATCHFOLDERCONTROLLER_H
