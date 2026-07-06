#ifndef TORRENTLISTCONTROLLER_H
#define TORRENTLISTCONTROLLER_H

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

class QAction;
class QModelIndex;
class QPoint;
class QTableView;
class QWidget;
class rpc_client;
class TorrentSortProxyModel;
class TablePlaceholderController;

class TorrentListController : public QObject
{
    Q_OBJECT

public:
    struct ActionSet
    {
        QAction *start = nullptr;
        QAction *stop = nullptr;
        QAction *deleteTorrent = nullptr;
        QAction *verify = nullptr;
        QAction *reannounce = nullptr;
        QAction *forceStart = nullptr;
    };

    explicit TorrentListController(QTableView *tableView,
                                   TorrentSortProxyModel *proxyModel,
                                   rpc_client *client,
                                   QWidget *dialogParent,
                                   QObject *parent = nullptr);

    void setup(const ActionSet &actions);
    void restoreViewState();
    void saveViewState() const;
    void beginTorrentListRefresh();
    void markTorrentListLoaded();
    void markTorrentListLoadFailed(const QString &message);

    int currentTorrentId() const;
    QList<int> selectedTorrentIds() const;
    QStringList selectedTorrentNames() const;

    void setCurrentTorrentDetails(int torrentId,
                                  const QString &hashString,
                                  const QString &magnetLink);
    void clearCurrentTorrentDetails();
    void setDefaultDownloadDir(const QString &downloadDir);
    void setSequentialDownloadSupported(bool supported);
    void setCurrentTorrentSequentialDownload(int torrentId, bool enabled, bool known);
    void setCurrentTorrentBandwidthPriority(int torrentId, int priority, bool known);
    void setCurrentDetailsDownloadDirProvider(const std::function<QString()> &provider);

public slots:
    void handleTableClicked(const QModelIndex &proxyIndex);
    void updateActionState();
    void showContextMenu(const QPoint &pos);
    void showHeaderContextMenu(const QPoint &pos);

    void deleteSelectedTorrents();
    void startSelectedTorrents();
    void stopSelectedTorrents();
    void reannounceSelectedTorrents();
    void verifySelectedTorrents();
    void forceStartSelectedTorrents();
    void setSelectedTorrentsLocation();
    void setSelectedTorrentsSequentialDownload(bool enabled);
    void setSelectedTorrentsBandwidthPriority(int priority);
    void showSelectedTorrentProperties();
    void copySelectedTorrentMagnetLink();
    void copySelectedTorrentHash();
    void queueMoveSelectedTop();
    void queueMoveSelectedUp();
    void queueMoveSelectedDown();
    void queueMoveSelectedBottom();

signals:
    void torrentSelected(int torrentId);
    void torrentSelectionCleared();
    void statusMessageRequested(const QString &message, int timeoutMs);
    void torrentListRefreshRequested();
    void torrentDetailsRefreshRequested(int torrentId);

private:
    QTableView *m_tableView = nullptr;
    TorrentSortProxyModel *m_proxyModel = nullptr;
    rpc_client *m_client = nullptr;
    QWidget *m_dialogParent = nullptr;
    ActionSet m_actions;

    int m_currentDetailsTorrentId = -1;
    QString m_currentTorrentHashString;
    QString m_currentTorrentMagnetLink;
    bool m_sequentialDownloadSupported = false;
    int m_currentSequentialDownloadTorrentId = -1;
    bool m_currentSequentialDownloadEnabled = false;
    bool m_currentSequentialDownloadKnown = false;
    int m_currentBandwidthPriorityTorrentId = -1;
    int m_currentBandwidthPriority = 0;
    bool m_currentBandwidthPriorityKnown = false;
    QString m_defaultDownloadDir;
    std::function<QString()> m_currentDetailsDownloadDirProvider;
    int m_lastEmittedTorrentId = -1;
    bool m_torrentListLoaded = false;
    bool m_torrentListLoadFailed = false;
    QString m_torrentListLoadFailureMessage;
    std::unique_ptr<TablePlaceholderController> m_placeholderController;

    void invokeSelectedTorrentCommand(void (rpc_client::*command)(const QList<int> &),
                                      const QString &message);
    void copyTextToClipboard(const QString &text,
                             const QString &statusMessage);
    void refreshCurrentTorrentDetails();
    void applyDefaultColumnVisibility();
    void updateCurrentTorrentSelection();
    void applySavedColumnVisibility();
    void restoreDefaultColumnOrder();
    void setColumnVisible(int column, bool visible);
    void resetColumns();
    void configureHorizontalHeader();
    void updatePlaceholder();
};

#endif // TORRENTLISTCONTROLLER_H
