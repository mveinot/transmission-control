#ifndef TORRENTLISTCONTROLLER_H
#define TORRENTLISTCONTROLLER_H

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

#include "torrentkey.h"

class QAction;
class QModelIndex;
class QPoint;
class QTableView;
class QWidget;
class TorrentBackend;
class TorrentSortProxyModel;
class TablePlaceholderController;

// Owns torrent-table interaction state: selection mapping, action enablement,
// context menus, queue operations, and persisted columns.
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
                                   TorrentBackend *client,
                                   QWidget *dialogParent,
                                   QObject *parent = nullptr);

    void setup(const ActionSet &actions);
    void restoreViewState();
    void saveViewState() const;
    // A server switch invalidates the previous successful-load state; routine
    // polling retains it so the existing table remains usable during refresh.
    void beginTorrentListRefresh(bool serverChanged = false);
    void markTorrentListLoaded();
    void markTorrentListLoadFailed(const QString &message);

    TorrentKey currentTorrentKey() const;
    QList<TorrentKey> selectedTorrentKeys() const;
    QStringList selectedTorrentNames() const;

    void setCurrentTorrentDetails(TorrentKey torrentKey,
                                  const QString &hashString,
                                  const QString &magnetLink);
    void clearCurrentTorrentDetails();
    void setDefaultDownloadDir(const QString &downloadDir);
    void setSequentialDownloadSupported(bool supported);
    void setCurrentTorrentSequentialDownload(TorrentKey torrentKey, bool enabled, bool known);
    void setCurrentTorrentBandwidthPriority(TorrentKey torrentKey, int priority, bool known);
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
    void torrentSelected(TorrentKey torrentKey);
    void torrentSelectionCleared();
    void statusMessageRequested(const QString &message, int timeoutMs);
    void torrentListRefreshRequested();
    void torrentDetailsRefreshRequested(TorrentKey torrentKey);

private:
    QTableView *m_tableView = nullptr;
    TorrentSortProxyModel *m_proxyModel = nullptr;
    TorrentBackend *m_client = nullptr;
    QWidget *m_dialogParent = nullptr;
    ActionSet m_actions;

    TorrentKey m_currentDetailsTorrentKey;
    QString m_currentTorrentHashString;
    QString m_currentTorrentMagnetLink;
    bool m_sequentialDownloadSupported = false;
    TorrentKey m_currentSequentialDownloadTorrentKey;
    bool m_currentSequentialDownloadEnabled = false;
    bool m_currentSequentialDownloadKnown = false;
    TorrentKey m_currentBandwidthPriorityTorrentKey;
    int m_currentBandwidthPriority = 0;
    bool m_currentBandwidthPriorityKnown = false;
    QString m_defaultDownloadDir;
    std::function<QString()> m_currentDetailsDownloadDirProvider;
    TorrentKey m_lastEmittedTorrentKey;
    bool m_torrentListLoaded = false;
    bool m_connectingToServer = false;
    bool m_torrentListLoadFailed = false;
    QString m_torrentListLoadFailureMessage;
    std::unique_ptr<TablePlaceholderController> m_placeholderController;

    void invokeSelectedTorrentCommand(void (TorrentBackend::*command)(const QList<TorrentKey> &),
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
