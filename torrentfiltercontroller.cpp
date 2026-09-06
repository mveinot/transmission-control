#include "torrentfiltercontroller.h"
#include "appicons.h"
#include "settingskeys.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QHash>
#include <QIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QListWidgetItem>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QSize>

#include <algorithm>
#include <array>
#include <utility>

namespace {
constexpr int FilterTypeRole = Qt::UserRole;
constexpr int FilterValueRole = Qt::UserRole + 1;
constexpr int FilterBaseLabelRole = Qt::UserRole + 2;
constexpr int FilterSectionRole = Qt::UserRole + 3;
constexpr int FilterIconRole = Qt::UserRole + 4;
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

    QSettings settings;
    m_trackersCollapsed =
        settings.value(SettingsKeys::FilterTrackersCollapsed, false).toBool();
    m_foldersCollapsed =
        settings.value(SettingsKeys::FilterFoldersCollapsed, false).toBool();
    m_labelsCollapsed =
        settings.value(SettingsKeys::FilterLabelsCollapsed, false).toBool();
    m_groupsCollapsed =
        settings.value(SettingsKeys::FilterGroupsCollapsed, false).toBool();

    m_filterList->setIconSize(QSize(20, 20));

    auto &icons = AppIcons::IconManager::instance();
    icons.bindAction(m_actions.all, AppIcons::Id::FilterAll);
    icons.bindAction(m_actions.downloading, AppIcons::Id::StatusDownloading);
    icons.bindAction(m_actions.waiting, AppIcons::Id::StatusQueued);
    icons.bindAction(m_actions.completed, AppIcons::Id::StatusComplete);
    icons.bindAction(m_actions.active, AppIcons::Id::StatusActive);
    icons.bindAction(m_actions.inactive, AppIcons::Id::StatusInactive);
    icons.bindAction(m_actions.stopped, AppIcons::Id::StatusStopped);
    icons.bindAction(m_actions.error, AppIcons::Id::StatusError);
    connect(&icons, &AppIcons::IconManager::themeChanged,
            this, &TorrentFilterController::refreshIcons);

    if (m_searchEdit) {
        m_searchEdit->setClearButtonEnabled(true);
        m_searchEdit->setPlaceholderText(tr("Search torrents..."));

        connect(m_searchEdit, &QLineEdit::textChanged,
                m_proxy, &TorrentSortProxyModel::setSearchText);

        connect(m_searchEdit, &QLineEdit::textChanged,
                this, [this]() {
                    updateFilterItemCounts();
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

    if (m_actions.waiting)
        connect(m_actions.waiting, &QAction::triggered, this, [this]() {
            setStateFilter(TorrentSortProxyModel::StateFilter::Waiting);
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
    connect(m_filterList, &QListWidget::itemClicked,
            this, [this](QListWidgetItem *item) {
                if (!item
                    || intToType(item->data(FilterTypeRole).toInt()) != ItemType::Header) {
                    return;
                }

                const ItemType sectionType =
                    intToType(item->data(FilterSectionRole).toInt());
                switch (sectionType) {
                case ItemType::Tracker:
                    setSectionCollapsed(sectionType, !m_trackersCollapsed);
                    break;
                case ItemType::Folder:
                    setSectionCollapsed(sectionType, !m_foldersCollapsed);
                    break;
                case ItemType::Label:
                    setSectionCollapsed(sectionType, !m_labelsCollapsed);
                    break;
                case ItemType::Group:
                    setSectionCollapsed(sectionType, !m_groupsCollapsed);
                    break;
                case ItemType::Header:
                case ItemType::Status:
                    break;
                }
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

    rebuildWithFilters(QStringList(), QStringList(), QStringList(), QStringList(),
                       false, false);
    setStateFilter(TorrentSortProxyModel::StateFilter::All);
    updateFilterStatusSignals();
}

void TorrentFilterController::rebuild(const QVector<torrent> &torrents)
{
    m_torrents = torrents;

    const QStringList trackerHosts = trackerHostsFromTorrents(torrents);
    const QStringList downloadDirs = downloadDirsFromTorrents(torrents);
    const QStringList labels = labelsFromTorrents(torrents);
    const QStringList groups = groupsFromTorrents(torrents);
    const bool labelsAvailable = std::any_of(
        torrents.cbegin(), torrents.cend(),
        [](const torrent &item) { return item.labelsAvailable(); });
    const bool groupsAvailable = std::any_of(
        torrents.cbegin(), torrents.cend(),
        [](const torrent &item) { return item.groupAvailable(); });

    // Rebuild dynamic sections only when their distinct domains change.
    if (trackerHosts == m_lastTrackerHosts
        && downloadDirs == m_lastDownloadDirs
        && labels == m_lastLabels
        && groups == m_lastGroups
        && labelsAvailable == m_labelsAvailable
        && groupsAvailable == m_groupsAvailable
        && m_filterList
        && m_filterList->count() > 0) {
        updateFilterItemCounts();
        return;
    }

    rebuildWithFilters(trackerHosts, downloadDirs, labels, groups,
                       labelsAvailable, groupsAvailable);
}

void TorrentFilterController::setStateFilter(TorrentSortProxyModel::StateFilter filter)
{
    if (!m_proxy)
        return;

    m_proxy->setStateFilter(filter);
    clearCategoricalFilters();
    updateCheckedAction(filter);
    selectStatusFilter(filter);
    updateFilterStatusSignals();
}

void TorrentFilterController::rebuildWithFilters(const QStringList &trackerHosts,
                                                 const QStringList &downloadDirs,
                                                 const QStringList &labels,
                                                 const QStringList &groups,
                                                 bool labelsAvailable,
                                                 bool groupsAvailable)
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
    m_lastLabels = labels;
    m_lastGroups = groups;
    m_labelsAvailable = labelsAvailable;
    m_groupsAvailable = groupsAvailable;

    QSignalBlocker blocker(m_filterList);
    m_filterList->clear();

    addStatusFilterItems();
    addTrackerFilterItems(trackerHosts);
    addFolderFilterItems(downloadDirs);
    if (labelsAvailable)
        addLabelFilterItems(labels);
    if (groupsAvailable)
        addGroupFilterItems(groups);
    applySectionCollapseState();

    const bool restoredSelection = selectItem(currentType, currentValue, 1);

    if (!restoredSelection && m_proxy) {
        m_proxy->setStateFilter(TorrentSortProxyModel::StateFilter::All);
        clearCategoricalFilters();
        updateCheckedAction(TorrentSortProxyModel::StateFilter::All);
    }

    updateFilterItemCounts();
    updateFilterStatusSignals();
}

void TorrentFilterController::addStatusFilterItems()
{
    if (!m_filterList)
        return;

    m_filterList->addItem(createHeaderItem(tr("Status")));
    m_filterList->addItem(createStatusItem(tr("All"), TorrentSortProxyModel::StateFilter::All));
    m_filterList->addItem(createStatusItem(tr("Downloading"), TorrentSortProxyModel::StateFilter::Downloading));
    m_filterList->addItem(createStatusItem(tr("Waiting"), TorrentSortProxyModel::StateFilter::Waiting));
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

    m_filterList->addItem(createHeaderItem(tr("Trackers"), ItemType::Tracker));
    m_filterList->addItem(createTrackerItem(tr("All Trackers"), QString()));

    for (const QString &trackerHost : trackerHosts)
        m_filterList->addItem(createTrackerItem(trackerHost, trackerHost));
}

void TorrentFilterController::addFolderFilterItems(const QStringList &downloadDirs)
{
    if (!m_filterList || downloadDirs.isEmpty())
        return;

    m_filterList->addItem(createHeaderItem(tr("Folders"), ItemType::Folder));
    m_filterList->addItem(createFolderItem(tr("All Folders"), QString()));

    for (const QString &downloadDir : downloadDirs)
        m_filterList->addItem(createFolderItem(downloadDir, downloadDir));
}

void TorrentFilterController::addLabelFilterItems(const QStringList &labels)
{
    if (!m_filterList)
        return;

    m_filterList->addItem(createHeaderItem(tr("Labels"), ItemType::Label));
    m_filterList->addItem(
        createMetadataItem(ItemType::Label, tr("Unlabelled"), QString(),
                           AppIcons::Id::FilterAll));

    for (const QString &label : labels)
        m_filterList->addItem(createMetadataItem(
            ItemType::Label, label, label, AppIcons::Id::FilterAll));
}

void TorrentFilterController::addGroupFilterItems(const QStringList &groups)
{
    if (!m_filterList)
        return;

    m_filterList->addItem(createHeaderItem(tr("Groups"), ItemType::Group));
    m_filterList->addItem(
        createMetadataItem(ItemType::Group, tr("No Group"), QString(),
                           AppIcons::Id::FilterFolder));

    for (const QString &group : groups)
        m_filterList->addItem(createMetadataItem(
            ItemType::Group, group, group, AppIcons::Id::FilterFolder));
}

QListWidgetItem *TorrentFilterController::createHeaderItem(
    const QString &label, ItemType sectionType) const
{
    auto *item = new QListWidgetItem(label);
    item->setFlags(sectionType == ItemType::Header ? Qt::NoItemFlags
                                                   : Qt::ItemIsEnabled);
    item->setData(FilterTypeRole, typeToInt(ItemType::Header));
    item->setData(FilterSectionRole, typeToInt(sectionType));
    item->setData(FilterValueRole, QString());
    item->setData(FilterBaseLabelRole, label);

    if (sectionType != ItemType::Header) {
        // A text disclosure glyph inherits the current foreground palette,
        // retaining contrast across live light/dark appearance changes.
        item->setText(QStringLiteral("▾ ") + label);
        item->setToolTip(tr("Click to collapse or expand this section."));
    }

    QFont headerFont = item->font();
    headerFont.setBold(true);
    item->setFont(headerFont);

    return item;
}

QListWidgetItem *TorrentFilterController::createStatusItem(
    const QString &label,
    TorrentSortProxyModel::StateFilter filter) const
{
    AppIcons::Id iconId = AppIcons::Id::FilterAll;

    switch (filter) {
    case TorrentSortProxyModel::StateFilter::All:
        iconId = AppIcons::Id::FilterAll;
        break;
    case TorrentSortProxyModel::StateFilter::Downloading:
        iconId = AppIcons::Id::StatusDownloading;
        break;
    case TorrentSortProxyModel::StateFilter::Waiting:
        iconId = AppIcons::Id::StatusQueued;
        break;
    case TorrentSortProxyModel::StateFilter::Completed:
        iconId = AppIcons::Id::StatusComplete;
        break;
    case TorrentSortProxyModel::StateFilter::Active:
        iconId = AppIcons::Id::StatusActive;
        break;
    case TorrentSortProxyModel::StateFilter::Inactive:
        iconId = AppIcons::Id::StatusInactive;
        break;
    case TorrentSortProxyModel::StateFilter::Stopped:
        iconId = AppIcons::Id::StatusStopped;
        break;
    case TorrentSortProxyModel::StateFilter::Error:
        iconId = AppIcons::Id::StatusError;
        break;
    }

    auto *item = new QListWidgetItem(
        AppIcons::IconManager::instance().icon(iconId), label);
    item->setData(FilterTypeRole, typeToInt(ItemType::Status));
    item->setData(FilterValueRole, QString::number(static_cast<int>(filter)));
    item->setData(FilterBaseLabelRole, label);
    item->setData(FilterIconRole, static_cast<int>(iconId));
    return item;
}

QListWidgetItem *TorrentFilterController::createTrackerItem(const QString &label,
                                                            const QString &trackerHost) const
{
    const AppIcons::Id iconId = AppIcons::Id::FilterTracker;
    auto *item = new QListWidgetItem(
        AppIcons::IconManager::instance().icon(iconId), label);
    item->setData(FilterTypeRole, typeToInt(ItemType::Tracker));
    item->setData(FilterValueRole, trackerHost);
    item->setData(FilterBaseLabelRole, label);
    item->setData(FilterIconRole, static_cast<int>(iconId));
    return item;
}

QListWidgetItem *TorrentFilterController::createFolderItem(const QString &label,
                                                           const QString &downloadDir) const
{
    const AppIcons::Id iconId = AppIcons::Id::FilterFolder;
    auto *item = new QListWidgetItem(
        AppIcons::IconManager::instance().icon(iconId), label);
    item->setData(FilterTypeRole, typeToInt(ItemType::Folder));
    item->setData(FilterValueRole, downloadDir);
    item->setData(FilterBaseLabelRole, label);
    item->setData(FilterIconRole, static_cast<int>(iconId));
    return item;
}

QListWidgetItem *TorrentFilterController::createMetadataItem(
    ItemType type,
    const QString &label,
    const QString &value,
    AppIcons::Id iconId) const
{
    auto *item = new QListWidgetItem(
        AppIcons::IconManager::instance().icon(iconId), label);
    item->setData(FilterTypeRole, typeToInt(type));
    item->setData(FilterValueRole, value);
    item->setData(FilterBaseLabelRole, label);
    item->setData(FilterIconRole, static_cast<int>(iconId));
    return item;
}

void TorrentFilterController::refreshIcons()
{
    if (!m_filterList)
        return;

    const auto &icons = AppIcons::IconManager::instance();
    for (int row = 0; row < m_filterList->count(); ++row) {
        QListWidgetItem *item = m_filterList->item(row);
        const QVariant iconId = item ? item->data(FilterIconRole) : QVariant();
        if (iconId.isValid())
            item->setIcon(icons.icon(static_cast<AppIcons::Id>(iconId.toInt())));
    }
}

void TorrentFilterController::clearCategoricalFilters()
{
    if (!m_proxy)
        return;

    m_proxy->setTrackerFilter(QString());
    m_proxy->setDownloadDirFilter(QString());
    m_proxy->clearLabelFilter();
    m_proxy->clearGroupFilter();
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
        clearCategoricalFilters();
        updateCheckedAction(stateFilter);
        updateFilterStatusSignals();
        return;
    }

    if (type == ItemType::Tracker) {
        m_proxy->setStateFilter(TorrentSortProxyModel::StateFilter::All);
        clearCategoricalFilters();
        m_proxy->setTrackerFilter(current->data(FilterValueRole).toString());
        updateCheckedAction(TorrentSortProxyModel::StateFilter::All);
        updateFilterStatusSignals();
        return;
    }

    if (type == ItemType::Folder) {
        m_proxy->setStateFilter(TorrentSortProxyModel::StateFilter::All);
        clearCategoricalFilters();
        m_proxy->setDownloadDirFilter(current->data(FilterValueRole).toString());
        updateCheckedAction(TorrentSortProxyModel::StateFilter::All);
        updateFilterStatusSignals();
        return;
    }

    if (type == ItemType::Label) {
        m_proxy->setStateFilter(TorrentSortProxyModel::StateFilter::All);
        clearCategoricalFilters();
        m_proxy->setLabelFilter(current->data(FilterValueRole).toString());
        updateCheckedAction(TorrentSortProxyModel::StateFilter::All);
        updateFilterStatusSignals();
        return;
    }

    if (type == ItemType::Group) {
        m_proxy->setStateFilter(TorrentSortProxyModel::StateFilter::All);
        clearCategoricalFilters();
        m_proxy->setGroupFilter(current->data(FilterValueRole).toString());
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
    case TorrentSortProxyModel::StateFilter::Waiting:
        action = m_actions.waiting;
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



void TorrentFilterController::updateFilterItemCounts()
{
    if (!m_filterList)
        return;

    using StateFilter = TorrentSortProxyModel::StateFilter;
    static constexpr std::array<StateFilter, 8> stateFilters {
        StateFilter::All,
        StateFilter::Downloading,
        StateFilter::Waiting,
        StateFilter::Completed,
        StateFilter::Active,
        StateFilter::Inactive,
        StateFilter::Stopped,
        StateFilter::Error
    };

    QHash<int, int> stateCounts;
    QHash<QString, int> trackerCounts;
    QHash<QString, int> folderCounts;
    QHash<QString, int> labelCounts;
    QHash<QString, int> groupCounts;
    int matchedTorrentCount = 0;
    int unlabelledCount = 0;
    int ungroupedCount = 0;

    // Aggregate every count domain in one torrent pass. A torrent contributes
    // once per distinct tracker/label even if a backend returns duplicates.
    for (const torrent &torrentItem : std::as_const(m_torrents)) {
        if (!torrentMatchesSearch(torrentItem))
            continue;

        ++matchedTorrentCount;

        for (StateFilter filter : stateFilters) {
            if (torrentMatchesState(torrentItem, filter))
                ++stateCounts[static_cast<int>(filter)];
        }

        QSet<QString> torrentTrackers;
        for (const QString &tracker : torrentItem.getTrackerHosts())
            torrentTrackers.insert(tracker.toCaseFolded());
        for (const QString &tracker : std::as_const(torrentTrackers))
            ++trackerCounts[tracker];

        ++folderCounts[torrentItem.getDownloadDir()];

        const QStringList labels = torrentItem.getLabels();
        if (labels.isEmpty()) {
            ++unlabelledCount;
        } else {
            QSet<QString> torrentLabels;
            for (const QString &label : labels)
                torrentLabels.insert(label.toCaseFolded());
            for (const QString &label : std::as_const(torrentLabels))
                ++labelCounts[label];
        }

        const QString group = torrentItem.getGroup();
        if (group.isEmpty())
            ++ungroupedCount;
        else
            ++groupCounts[group.toCaseFolded()];
    }

    for (int row = 0; row < m_filterList->count(); ++row) {
        QListWidgetItem *item = m_filterList->item(row);

        if (!item)
            continue;

        const ItemType type = intToType(item->data(FilterTypeRole).toInt());
        if (type == ItemType::Header)
            continue;

        const QString baseLabel = item->data(FilterBaseLabelRole).toString();
        if (baseLabel.isEmpty())
            continue;

        const QString value = item->data(FilterValueRole).toString();
        int count = 0;

        if (type == ItemType::Status)
            count = stateCounts.value(value.toInt());
        else if (type == ItemType::Tracker)
            count = value.isEmpty()
                        ? matchedTorrentCount
                        : trackerCounts.value(value.toCaseFolded());
        else if (type == ItemType::Folder)
            count = value.isEmpty()
                        ? matchedTorrentCount
                        : folderCounts.value(value);
        else if (type == ItemType::Label)
            count = value.isEmpty()
                        ? unlabelledCount
                        : labelCounts.value(value.toCaseFolded());
        else if (type == ItemType::Group)
            count = value.isEmpty()
                        ? ungroupedCount
                        : groupCounts.value(value.toCaseFolded());

        item->setText(displayLabelWithCount(baseLabel, count));
    }
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

    if (m_proxy->labelFilterActive()) {
        const QString label = m_proxy->labelFilter();
        parts << (label.isEmpty()
                      ? tr("Filtered by label: Unlabelled")
                      : tr("Filtered by label: %1").arg(label));
    } else if (m_proxy->groupFilterActive()) {
        const QString group = m_proxy->groupFilter();
        parts << (group.isEmpty()
                      ? tr("Filtered by group: No Group")
                      : tr("Filtered by group: %1").arg(group));
    } else if (!tracker.isEmpty()) {
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
    case TorrentSortProxyModel::StateFilter::Waiting:
        return tr("Waiting");
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


QString TorrentFilterController::displayLabelWithCount(const QString &label, int count) const
{
    return tr("%1 (%2)").arg(label).arg(count);
}

bool TorrentFilterController::torrentMatchesSearch(const torrent &torrentItem) const
{
    if (!m_proxy || m_proxy->searchText().isEmpty())
        return true;

    return torrentItem.getName().contains(m_proxy->searchText(), Qt::CaseInsensitive);
}

bool TorrentFilterController::torrentMatchesState(
    const torrent &torrentItem,
    TorrentSortProxyModel::StateFilter filter)
{
    return TorrentSortProxyModel::matchesState(
        filter,
        torrentItem.getStatusValue(),
        torrentItem.getPercentDone(),
        torrentItem.hasError(),
        torrentItem.getRateDownloadBytesPerSecond(),
        torrentItem.getRateUploadBytesPerSecond());
}

void TorrentFilterController::showFilterContextMenu(const QPoint &position)
{
    if (!m_filterList)
        return;

    QListWidgetItem *item = m_filterList->itemAt(position);
    if (!item)
        return;

    const ItemType type = intToType(item->data(FilterTypeRole).toInt());
    if (type != ItemType::Tracker && type != ItemType::Folder
        && type != ItemType::Label && type != ItemType::Group
        && type != ItemType::Header) {
        return;
    }

    ItemType sectionType = type;
    if (type == ItemType::Header) {
        sectionType = intToType(item->data(FilterSectionRole).toInt());
        if (sectionType == ItemType::Header || sectionType == ItemType::Status)
            return;
    }

    QMenu menu(m_filterList);

    QAction *copyAction = nullptr;
    if (type != ItemType::Header) {
        const QString value = item->data(FilterValueRole).toString();
        if (!value.isEmpty())
            copyAction = menu.addAction(tr("Copy"));
    }

    if (copyAction)
        menu.addSeparator();

    QString sectionName;
    bool collapsed = false;
    switch (sectionType) {
    case ItemType::Tracker:
        sectionName = tr("Trackers");
        collapsed = m_trackersCollapsed;
        break;
    case ItemType::Folder:
        sectionName = tr("Folders");
        collapsed = m_foldersCollapsed;
        break;
    case ItemType::Label:
        sectionName = tr("Labels");
        collapsed = m_labelsCollapsed;
        break;
    case ItemType::Group:
        sectionName = tr("Groups");
        collapsed = m_groupsCollapsed;
        break;
    case ItemType::Header:
    case ItemType::Status:
        return;
    }

    QAction *collapseAction = menu.addAction(tr("Collapse %1").arg(sectionName));
    QAction *expandAction = menu.addAction(tr("Expand %1").arg(sectionName));
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
    const char *settingsKey = nullptr;
    if (type == ItemType::Tracker) {
        if (m_trackersCollapsed == collapsed)
            return;
        m_trackersCollapsed = collapsed;
        settingsKey = SettingsKeys::FilterTrackersCollapsed;
    } else if (type == ItemType::Folder) {
        if (m_foldersCollapsed == collapsed)
            return;
        m_foldersCollapsed = collapsed;
        settingsKey = SettingsKeys::FilterFoldersCollapsed;
    } else if (type == ItemType::Label) {
        if (m_labelsCollapsed == collapsed)
            return;
        m_labelsCollapsed = collapsed;
        settingsKey = SettingsKeys::FilterLabelsCollapsed;
    } else if (type == ItemType::Group) {
        if (m_groupsCollapsed == collapsed)
            return;
        m_groupsCollapsed = collapsed;
        settingsKey = SettingsKeys::FilterGroupsCollapsed;
    } else {
        return;
    }

    // Persist only explicit transitions; setup restores member state directly
    // to avoid redundant writes during every application launch.
    QSettings().setValue(settingsKey, collapsed);
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
        if (type == ItemType::Header) {
            const ItemType sectionType =
                intToType(item->data(FilterSectionRole).toInt());
            bool collapsed = false;
            switch (sectionType) {
            case ItemType::Tracker:
                collapsed = m_trackersCollapsed;
                break;
            case ItemType::Folder:
                collapsed = m_foldersCollapsed;
                break;
            case ItemType::Label:
                collapsed = m_labelsCollapsed;
                break;
            case ItemType::Group:
                collapsed = m_groupsCollapsed;
                break;
            case ItemType::Header:
            case ItemType::Status:
                continue;
            }
            const QString baseLabel = item->data(FilterBaseLabelRole).toString();
            item->setText((collapsed ? QStringLiteral("▸ ")
                                     : QStringLiteral("▾ "))
                          + baseLabel);
            continue;
        }

        if (type == ItemType::Tracker) {
            item->setHidden(m_trackersCollapsed);
        } else if (type == ItemType::Folder) {
            item->setHidden(m_foldersCollapsed);
        } else if (type == ItemType::Label) {
            item->setHidden(m_labelsCollapsed);
        } else if (type == ItemType::Group) {
            item->setHidden(m_groupsCollapsed);
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

QStringList TorrentFilterController::labelsFromTorrents(const QVector<torrent> &torrents)
{
    QHash<QString, QString> labelsByFoldedValue;
    for (const torrent &torrentItem : torrents) {
        for (const QString &label : torrentItem.getLabels()) {
            const QString trimmed = label.trimmed();
            const QString folded = trimmed.toCaseFolded();
            if (!trimmed.isEmpty() && !labelsByFoldedValue.contains(folded))
                labelsByFoldedValue.insert(folded, trimmed);
        }
    }

    QStringList labels = labelsByFoldedValue.values();
    std::sort(labels.begin(), labels.end(), [](const QString &lhs, const QString &rhs) {
        return QString::localeAwareCompare(lhs, rhs) < 0;
    });
    return labels;
}

QStringList TorrentFilterController::groupsFromTorrents(const QVector<torrent> &torrents)
{
    QHash<QString, QString> groupsByFoldedValue;
    for (const torrent &torrentItem : torrents) {
        const QString group = torrentItem.getGroup().trimmed();
        const QString folded = group.toCaseFolded();
        if (!group.isEmpty() && !groupsByFoldedValue.contains(folded))
            groupsByFoldedValue.insert(folded, group);
    }

    QStringList groups = groupsByFoldedValue.values();
    std::sort(groups.begin(), groups.end(), [](const QString &lhs, const QString &rhs) {
        return QString::localeAwareCompare(lhs, rhs) < 0;
    });
    return groups;
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
    case ItemType::Label:
        return ItemType::Label;
    case ItemType::Group:
        return ItemType::Group;
    case ItemType::Header:
    default:
        return ItemType::Header;
    }
}
