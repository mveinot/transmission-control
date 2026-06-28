#ifndef TORRENTFILTERCONTROLLER_H
#define TORRENTFILTERCONTROLLER_H

#include <QObject>
#include <QIcon>
#include <QStringList>

#include "torrent.h"
#include "torrentsortproxymodel.h"

class QAction;
class QLineEdit;
class QListWidget;
class QListWidgetItem;


class TorrentFilterController : public QObject
{
    Q_OBJECT

public:
    struct Actions {
        QAction *all = nullptr;
        QAction *downloading = nullptr;
        QAction *completed = nullptr;
        QAction *active = nullptr;
        QAction *inactive = nullptr;
        QAction *stopped = nullptr;
        QAction *error = nullptr;
    };

    explicit TorrentFilterController(QListWidget *filterList,
                                     QLineEdit *searchEdit,
                                     TorrentSortProxyModel *proxyModel,
                                     const Actions &actions,
                                     QObject *parent = nullptr);

    void setup();
    void rebuild(const QVector<torrent> &torrents);
    void setStateFilter(TorrentSortProxyModel::StateFilter filter);

private:
    enum class ItemType {
        Header,
        Status,
        Tracker,
        Folder
    };

    QListWidget *m_filterList = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    TorrentSortProxyModel *m_proxy = nullptr;
    Actions m_actions;
    QStringList m_lastTrackerHosts;
    QStringList m_lastDownloadDirs;

    void rebuildWithFilters(const QStringList &trackerHosts,
                            const QStringList &downloadDirs);
    void addStatusFilterItems();
    void addTrackerFilterItems(const QStringList &trackerHosts);
    void addFolderFilterItems(const QStringList &downloadDirs);
    QListWidgetItem *createHeaderItem(const QString &label) const;
    QListWidgetItem *createStatusItem(const QString &label,
                                      TorrentSortProxyModel::StateFilter filter) const;
    QListWidgetItem *createTrackerItem(const QString &label,
                                       const QString &trackerHost) const;
    QListWidgetItem *createFolderItem(const QString &label,
                                      const QString &downloadDir) const;
    void applyCurrentListSelection(QListWidgetItem *current);
    void selectStatusFilter(TorrentSortProxyModel::StateFilter filter);
    bool selectItem(ItemType type, const QString &value, int fallbackRow);
    void updateCheckedAction(TorrentSortProxyModel::StateFilter filter);
    static QStringList trackerHostsFromTorrents(const QVector<torrent> &torrents);
    static QStringList downloadDirsFromTorrents(const QVector<torrent> &torrents);
    static QIcon iconFromTheme(const QStringList &themeNames);
    static int typeToInt(ItemType type);
    static ItemType intToType(int value);
};

#endif // TORRENTFILTERCONTROLLER_H
