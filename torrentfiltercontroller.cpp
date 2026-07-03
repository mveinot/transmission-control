#include "torrentfiltercontroller.h"
#include "appicons.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QListWidgetItem>
#include <QSet>
#include <QSignalBlocker>
#include <QSize>

#include <algorithm>

namespace {
constexpr int FilterTypeRole = Qt::UserRole;
constexpr int FilterValueRole = Qt::UserRole + 1;
}

TorrentFilterController::TorrentFilterController(QListWidget *filterList,
                                                 QLineEdit *searchEdit,
                                                 TorrentSortProxyModel *proxyModel,
                                                 const Actions &actions,
                                                 QObject *parent)
    : QObject(parent),
      m_filterList(filterList),
      m_searchEdit(searchEdit),
      m_proxy(proxyModel),
      m_actions(actions)
{
}

void TorrentFilterController::setup()
{
    if (!m_filterList || !m_proxy)
        return;

    m_filterList->setIconSize(QSize(20, 20));

    if (m_searchEdit) {
        m_searchEdit->setClearButtonEnabled(true);
        m_searchEdit->setPlaceholderText(tr("Search torrents..."));

        connect(m_searchEdit, &QLineEdit::textChanged,
                m_proxy, &TorrentSortProxyModel::setSearchText);

        connect(m_searchEdit, &QLineEdit::textChanged,
                this, [this]() {
                    updateFilterStatusSignals();
                });
    }

    if (m_actions.all)
        connect(m_actions.all, &QAction::triggered, this, [this]() {
            setStateFilter(TorrentSortProxyModel::StateFilter::All);
        });

    if (m_actions.downloading)
        connect(m_actions.downloading, &QAction::triggered, this, [this]() {
            setStateFilter(TorrentSortProxyModel::StateFilter::Downloading);
        });

    if (m_actions.completed)
        connect(m_actions.completed, &QAction::triggered, this, [this]() {
            setStateFilter(TorrentSortProxyModel::StateFilter::Completed);
        });

    if (m_actions.active)
        connect(m_actions.active, &QAction::triggered, this, [this]() {
            setStateFilter(TorrentSortProxyModel::StateFilter::Active);
        });

    if (m_actions.inactive)
        connect(m_actions.inactive, &QAction::triggered, this, [this]() {
            setStateFilter(TorrentSortProxyModel::StateFilter::Inactive);
        });

    if (m_actions.stopped)
        connect(m_actions.stopped, &QAction::triggered, this, [this]() {
            setStateFilter(TorrentSortProxyModel::StateFilter::Stopped);
        });

    if (m_actions.error)
        connect(m_actions.error, &QAction::triggered, this, [this]() {
            setStateFilter(TorrentSortProxyModel::StateFilter::Error);
        });

    connect(m_filterList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current, QListWidgetItem *) {
                applyCurrentListSelection(current);
            });

    m_filterList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_filterList, &QListWidget::customContextMenuRequested,
            this, &TorrentFilterController::showFilterContextMenu);

    // The .ui file used to pre-populate this list with icons, then the
    // runtime rebuild replaced it with plain text rows. Build the initial
    // contents here instead so the sidebar looks the same before and after
    // the first torrent refresh. Humanity survives another scrollbar.
    if (m_proxy) {
        connect(m_proxy, &QAbstractItemModel::modelReset,
                this, &TorrentFilterController::updateFilterStatusSignals);
        connect(m_proxy, &QAbstractItemModel::rowsInserted,
                this, &TorrentFilterController::updateFilterStatusSignals);
        connect(m_proxy, &QAbstractItemModel::rowsRemoved,
                this, &TorrentFilterController::updateFilterStatusSignals);
        connect(m_proxy, &QAbstractItemModel::layoutChanged,
                this, &TorrentFilterController::updateFilterStatusSignals);
    }

    rebuildWithFilters(QStringList(), QStringList());
    setStateFilter(TorrentSortProxyModel::StateFilter::All);
    updateFilterStatusSignals();
}

void TorrentFilterController::rebuild(const QVector<torrent> &torrents)
{
    const QStringList trackerHosts = trackerHostsFromTorrents(torrents);
    const QStringList downloadDirs = downloadDirsFromTorrents(torrents);

    if (trackerHosts == m_lastTrackerHosts
        && downloadDirs == m_lastDownloadDirs
        && m_filterList
        && m_filterList->count() > 0) {
        return;
    }

    rebuildWithFilters(trackerHosts, downloadDirs);
}

void TorrentFilterController::setStateFilter(TorrentSortProxyModel::StateFilter filter)
{
    if (!m_proxy)
        return;

    m_proxy->setStateFilter(filter);
    m_proxy->setTrackerFilter(QString());
    m_proxy->setDownloadDirFilter(QString());
    updateCheckedAction(filter);
    selectStatusFilter(filter);
    updateFilterStatusSignals();
}

void TorrentFilterController::rebuildWithFilters(const QStringList &trackerHosts,
                                                 const QStringList &downloadDirs)
{
    if (!m_filterList)
        return;

    const QListWidgetItem *currentItem = m_filterList->currentItem();
    const ItemType currentType = currentItem
                                     ? intToType(currentItem->data(FilterTypeRole).toInt())
                                     : ItemType::Status;
    const QString currentValue = currentItem
                                     ? currentItem->data(FilterValueRole).toString()
                                     : QString::number(static_cast<int>(TorrentSortProxyModel::StateFilter::All));

    m_lastTrackerHosts = trackerHosts;
    m_lastDownloadDirs = downloadDirs;

    QSignalBlocker blocker(m_filterList);
    m_filterList->clear();

    addStatusFilterItems();
    addTrackerFilterItems(trackerHosts);
    addFolderFilterItems(downloadDirs);
    applySectionCollapseState();

    const bool restoredSelection = selectItem(currentType, currentValue, 1);

    if (!restoredSelection && m_proxy) {
        m_proxy->setStateFilter(TorrentSortProxyModel::StateFilter::All);
        m_proxy->setTrackerFilter(QString());
        m_proxy->setDownloadDirFilter(QString());
        updateCheckedAction(TorrentSortProxyModel::StateFilter::All);
    }

    updateFilterStatusSignals();
}

void TorrentFilterController::addStatusFilterItems()
{
    if (!m_filterList)
        return;

    m_filterList->addItem(createHeaderItem(tr("Status")));
    m_filterList->addItem(createStatusItem(tr("All"), TorrentSortProxyModel::StateFilter::All));
    m_filterList->addItem(createStatusItem(tr("Downloading"), TorrentSortProxyModel::StateFilter::Downloading));
    m_filterList->addItem(createStatusItem(tr("Complete"), TorrentSortProxyModel::StateFilter::Completed));
    m_filterList->addItem(createStatusItem(tr("Active"), TorrentSortProxyModel::StateFilter::Active));
    m_filterList->addItem(createStatusItem(tr("Inactive"), TorrentSortProxyModel::StateFilter::Inactive));
    m_filterList->addItem(createStatusItem(tr("Stopped"), TorrentSortProxyModel::StateFilter::Stopped));
    m_filterList->addItem(createStatusItem(tr("Error"), TorrentSortProxyModel::StateFilter::Error));
}

void TorrentFilterController::addTrackerFilterItems(const QStringList &trackerHosts)
{
    if (!m_filterList || trackerHosts.isEmpty())
        return;

    m_filterList->addItem(createHeaderItem(tr("Trackers")));
    m_filterList->addItem(createTrackerItem(tr("All Trackers"), QString()));

    for (const QString &trackerHost : trackerHosts)
        m_filterList->addItem(createTrackerItem(trackerHost, trackerHost));
}

void TorrentFilterController::addFolderFilterItems(const QStringList &downloadDirs)
{
    if (!m_filterList || downloadDirs.isEmpty())
        return;

    m_filterList->addItem(createHeaderItem(tr("Folders")));
    m_filterList->addItem(createFolderItem(tr("All Folders"), QString()));

    for (const QString &downloadDir : downloadDirs)
        m_filterList->addItem(createFolderItem(downloadDir, downloadDir));
}

QListWidgetItem *TorrentFilterController::createHeaderItem(const QString &label) const
{
    auto *item = new QListWidgetItem(label);
    item->setFlags(Qt::NoItemFlags);
    item->setData(FilterTypeRole, typeToInt(ItemType::Header));
    item->setData(FilterValueRole, QString());

    QFont headerFont = item->font();
    headerFont.setBold(true);
    item->setFont(headerFont);

    return item;
}

QListWidgetItem *TorrentFilterController::createStatusItem(
    const QString &label,
    TorrentSortProxyModel::StateFilter filter) const
{
    static const QIcon allIcon = AppIcons::icon(AppIcons::Icon::FilterAll);
    static const QIcon downloadingIcon = AppIcons::icon(AppIcons::Icon::StatusDownloading);
    static const QIcon completeIcon = AppIcons::icon(AppIcons::Icon::StatusComplete);
    static const QIcon activeIcon = AppIcons::icon(AppIcons::Icon::StatusActive);
    static const QIcon inactiveIcon = AppIcons::icon(AppIcons::Icon::StatusInactive);
    static const QIcon stoppedIcon = AppIcons::icon(AppIcons::Icon::StatusStopped);
    static const QIcon errorIcon = AppIcons::icon(AppIcons::Icon::StatusError);

    QIcon icon;

    switch (filter) {
    case TorrentSortProxyModel::StateFilter::All:
        icon = allIcon;
        break;
    case TorrentSortProxyModel::StateFilter::Downloading:
        icon = downloadingIcon;
        break;
    case TorrentSortProxyModel::StateFilter::Completed:
        icon = completeIcon;
        break;
    case TorrentSortProxyModel::StateFilter::Active:
        icon = activeIcon;
        break;
    case TorrentSortProxyModel::StateFilter::Inactive:
        icon = inactiveIcon;
        break;
    case TorrentSortProxyModel::StateFilter::Stopped:
        icon = stoppedIcon;
        break;
    case TorrentSortProxyModel::StateFilter::Error:
        icon = errorIcon;
        break;
    }

    auto *item = new QListWidgetItem(icon, label);
    item->setData(FilterTypeRole, typeToInt(ItemType::Status));
    item->setData(FilterValueRole, QString::number(static_cast<int>(filter)));
    return item;
}

QListWidgetItem *TorrentFilterController::createTrackerItem(const QString &label,
                                                            const QString &trackerHost) const
{
    static const QIcon trackerIcon = AppIcons::icon(AppIcons::Icon::FilterTracker);

    auto *item = new QListWidgetItem(trackerIcon, label);
    item->setData(FilterTypeRole, typeToInt(ItemType::Tracker));
    item->setData(FilterValueRole, trackerHost);
    return item;
}

QListWidgetItem *TorrentFilterController::createFolderItem(const QString &label,
                                                           const QString &downloadDir) const
{
    static const QIcon folderIcon = AppIcons::icon(AppIcons::Icon::FilterFolder);

    auto *item = new QListWidgetItem(folderIcon, label);
    item->setData(FilterTypeRole, typeToInt(ItemType::Folder));
    item->setData(FilterValueRole, downloadDir);
    return item;
}

void TorrentFilterController::applyCurrentListSelection(QListWidgetItem *current)
{
    if (!current || !m_proxy)
        return;

    const ItemType type = intToType(current->data(FilterTypeRole).toInt());

    if (type == ItemType::Status) {
        const auto stateFilter = static_cast<TorrentSortProxyModel::StateFilter>(
            current->data(FilterValueRole).toString().toInt()
            );

        m_proxy->setStateFilter(stateFilter);
        m_proxy->setTrackerFilter(QString());
        m_proxy->setDownloadDirFilter(QString());
        updateCheckedAction(stateFilter);
        updateFilterStatusSignals();
        return;
    }

    if (type == ItemType::Tracker) {
        m_proxy->setStateFilter(TorrentSortProxyModel::StateFilter::All);
        m_proxy->setTrackerFilter(current->data(FilterValueRole).toString());
        m_proxy->setDownloadDirFilter(QString());
        updateCheckedAction(TorrentSortProxyModel::StateFilter::All);
        updateFilterStatusSignals();
        return;
    }

    if (type == ItemType::Folder) {
        m_proxy->setStateFilter(TorrentSortProxyModel::StateFilter::All);
        m_proxy->setTrackerFilter(QString());
        m_proxy->setDownloadDirFilter(current->data(FilterValueRole).toString());
        updateCheckedAction(TorrentSortProxyModel::StateFilter::All);
        updateFilterStatusSignals();
    }
}

void TorrentFilterController::selectStatusFilter(TorrentSortProxyModel::StateFilter filter)
{
    selectItem(ItemType::Status, QString::number(static_cast<int>(filter)), 1);
}

bool TorrentFilterController::selectItem(ItemType type,
                                         const QString &value,
                                         int fallbackRow)
{
    if (!m_filterList)
        return false;

    QSignalBlocker blocker(m_filterList);

    for (int row = 0; row < m_filterList->count(); ++row) {
        QListWidgetItem *item = m_filterList->item(row);

        if (!item)
            continue;

        const ItemType itemType = intToType(item->data(FilterTypeRole).toInt());
        const QString itemValue = item->data(FilterValueRole).toString();

        if (itemType == type && itemValue == value) {
            m_filterList->setCurrentRow(row);
            return true;
        }
    }

    if (fallbackRow >= 0 && fallbackRow < m_filterList->count())
        m_filterList->setCurrentRow(fallbackRow);

    return false;
}

void TorrentFilterController::updateCheckedAction(TorrentSortProxyModel::StateFilter filter)
{
    QAction *action = nullptr;

    switch (filter) {
    case TorrentSortProxyModel::StateFilter::All:
        action = m_actions.all;
        break;
    case TorrentSortProxyModel::StateFilter::Downloading:
        action = m_actions.downloading;
        break;
    case TorrentSortProxyModel::StateFilter::Completed:
        action = m_actions.completed;
        break;
    case TorrentSortProxyModel::StateFilter::Active:
        action = m_actions.active;
        break;
    case TorrentSortProxyModel::StateFilter::Inactive:
        action = m_actions.inactive;
        break;
    case TorrentSortProxyModel::StateFilter::Stopped:
        action = m_actions.stopped;
        break;
    case TorrentSortProxyModel::StateFilter::Error:
        action = m_actions.error;
        break;
    }

    if (action)
        action->setChecked(true);
}


void TorrentFilterController::updateFilterStatusSignals()
{
    if (!m_proxy)
        return;

    const int visibleCount = m_proxy->rowCount();
    const QAbstractItemModel *source = m_proxy->sourceModel();
    const int totalCount = source ? source->rowCount() : visibleCount;

    emit resultCountChanged(visibleCount, totalCount);
    emit filterSummaryChanged(filterSummary());
}

QString TorrentFilterController::filterSummary() const
{
    if (!m_proxy)
        return QString();

    QStringList parts;

    const QString tracker = m_proxy->trackerFilter();
    const QString folder = m_proxy->downloadDirFilter();

    if (!tracker.isEmpty()) {
        parts << tr("Filtered by tracker: %1").arg(tracker);
    } else if (!folder.isEmpty()) {
        parts << tr("Filtered by folder: %1").arg(folder);
    } else if (m_proxy->stateFilter() != TorrentSortProxyModel::StateFilter::All) {
        parts << tr("Filtered by status: %1").arg(statusFilterName(m_proxy->stateFilter()));
    }

    const QString searchText = m_proxy->searchText();
    if (!searchText.isEmpty())
        parts << tr("Search: %1").arg(searchText);

    return parts.join(QStringLiteral(" · "));
}

QString TorrentFilterController::statusFilterName(TorrentSortProxyModel::StateFilter filter) const
{
    switch (filter) {
    case TorrentSortProxyModel::StateFilter::All:
        return tr("All");
    case TorrentSortProxyModel::StateFilter::Downloading:
        return tr("Downloading");
    case TorrentSortProxyModel::StateFilter::Completed:
        return tr("Complete");
    case TorrentSortProxyModel::StateFilter::Active:
        return tr("Active");
    case TorrentSortProxyModel::StateFilter::Inactive:
        return tr("Inactive");
    case TorrentSortProxyModel::StateFilter::Stopped:
        return tr("Stopped");
    case TorrentSortProxyModel::StateFilter::Error:
        return tr("Error");
    }

    return QString();
}

void TorrentFilterController::showFilterContextMenu(const QPoint &position)
{
    if (!m_filterList)
        return;

    QListWidgetItem *item = m_filterList->itemAt(position);
    if (!item)
        return;

    const ItemType type = intToType(item->data(FilterTypeRole).toInt());
    if (type != ItemType::Tracker && type != ItemType::Folder && type != ItemType::Header)
        return;

    ItemType sectionType = type;
    if (type == ItemType::Header) {
        const QString label = item->text();
        if (label == tr("Trackers")) {
            sectionType = ItemType::Tracker;
        } else if (label == tr("Folders")) {
            sectionType = ItemType::Folder;
        } else {
            return;
        }
    }

    QMenu menu(m_filterList);

    QAction *copyAction = nullptr;
    if (type == ItemType::Tracker || type == ItemType::Folder) {
        const QString value = item->data(FilterValueRole).toString();
        if (!value.isEmpty())
            copyAction = menu.addAction(tr("Copy"));
    }

    if (copyAction)
        menu.addSeparator();

    QAction *collapseAction = menu.addAction(sectionType == ItemType::Tracker
                                                 ? tr("Collapse Trackers")
                                                 : tr("Collapse Folders"));
    QAction *expandAction = menu.addAction(sectionType == ItemType::Tracker
                                               ? tr("Expand Trackers")
                                               : tr("Expand Folders"));

    const bool collapsed = sectionType == ItemType::Tracker
                               ? m_trackersCollapsed
                               : m_foldersCollapsed;
    collapseAction->setEnabled(!collapsed);
    expandAction->setEnabled(collapsed);

    QAction *selectedAction = menu.exec(m_filterList->viewport()->mapToGlobal(position));

    if (!selectedAction)
        return;

    if (selectedAction == copyAction) {
        copyFilterValueToClipboard(item);
        return;
    }

    if (selectedAction == collapseAction) {
        setSectionCollapsed(sectionType, true);
        return;
    }

    if (selectedAction == expandAction)
        setSectionCollapsed(sectionType, false);
}

void TorrentFilterController::copyFilterValueToClipboard(QListWidgetItem *item) const
{
    if (!item)
        return;

    const QString value = item->data(FilterValueRole).toString();
    if (value.isEmpty())
        return;

    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard)
        clipboard->setText(value);
}

void TorrentFilterController::setSectionCollapsed(ItemType type, bool collapsed)
{
    if (type == ItemType::Tracker) {
        m_trackersCollapsed = collapsed;
    } else if (type == ItemType::Folder) {
        m_foldersCollapsed = collapsed;
    } else {
        return;
    }

    applySectionCollapseState();
}

void TorrentFilterController::applySectionCollapseState()
{
    if (!m_filterList)
        return;

    for (int row = 0; row < m_filterList->count(); ++row) {
        QListWidgetItem *item = m_filterList->item(row);
        if (!item)
            continue;

        const ItemType type = intToType(item->data(FilterTypeRole).toInt());
        if (type == ItemType::Tracker) {
            item->setHidden(m_trackersCollapsed);
        } else if (type == ItemType::Folder) {
            item->setHidden(m_foldersCollapsed);
        }
    }
}

QStringList TorrentFilterController::trackerHostsFromTorrents(const QVector<torrent> &torrents)
{
    QSet<QString> uniqueTrackers;

    for (const torrent &torrentItem : torrents) {
        for (const QString &trackerHost : torrentItem.getTrackerHosts()) {
            const QString normalized = trackerHost.trimmed().toLower();

            if (!normalized.isEmpty())
                uniqueTrackers.insert(normalized);
        }
    }

    QStringList trackerHosts = uniqueTrackers.values();
    std::sort(trackerHosts.begin(), trackerHosts.end());
    return trackerHosts;
}

QStringList TorrentFilterController::downloadDirsFromTorrents(const QVector<torrent> &torrents)
{
    QSet<QString> uniqueDirs;

    for (const torrent &torrentItem : torrents) {
        const QString downloadDir = torrentItem.getDownloadDir().trimmed();

        if (!downloadDir.isEmpty())
            uniqueDirs.insert(downloadDir);
    }

    QStringList downloadDirs = uniqueDirs.values();
    std::sort(downloadDirs.begin(), downloadDirs.end(), [](const QString &lhs, const QString &rhs) {
        return QString::localeAwareCompare(lhs, rhs) < 0;
    });
    return downloadDirs;
}

int TorrentFilterController::typeToInt(ItemType type)
{
    return static_cast<int>(type);
}

TorrentFilterController::ItemType TorrentFilterController::intToType(int value)
{
    switch (static_cast<ItemType>(value)) {
    case ItemType::Status:
        return ItemType::Status;
    case ItemType::Tracker:
        return ItemType::Tracker;
    case ItemType::Folder:
        return ItemType::Folder;
    case ItemType::Header:
    default:
        return ItemType::Header;
    }
}
