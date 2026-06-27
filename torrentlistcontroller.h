#ifndef TORRENTLISTCONTROLLER_H
#define TORRENTLISTCONTROLLER_H

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class QAction;
class QModelIndex;
class QPoint;
class QTableView;
class QWidget;
class rpc_client;
class TorrentSortProxyModel;

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

    int currentTorrentId() const;
    QList<int> selectedTorrentIds() const;
    QStringList selectedTorrentNames() const;

    void setCurrentTorrentDetails(int torrentId,
                                  const QString &hashString,
                                  const QString &magnetLink);
    void clearCurrentTorrentDetails();
    void setDefaultDownloadDir(const QString &downloadDir);
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
    void showSelectedTorrentProperties();
    void copySelectedTorrentMagnetLink();
    void copySelectedTorrentHash();
    void queueMoveSelectedTop();
    void queueMoveSelectedUp();
    void queueMoveSelectedDown();
    void queueMoveSelectedBottom();

signals:
    void torrentSelected(int torrentId);
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
    QString m_defaultDownloadDir;
    std::function<QString()> m_currentDetailsDownloadDirProvider;

    void invokeSelectedTorrentCommand(void (rpc_client::*command)(const QList<int> &),
                                      const QString &message);
    void copyTextToClipboard(const QString &text,
                             const QString &statusMessage);
    void refreshCurrentTorrentDetails();
    void applyDefaultColumnVisibility();
    void applySavedColumnVisibility();
    void restoreDefaultColumnOrder();
    void setColumnVisible(int column, bool visible);
    void resetColumns();
    void configureHorizontalHeader();
};

#endif // TORRENTLISTCONTROLLER_H
