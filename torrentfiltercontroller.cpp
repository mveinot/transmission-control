#include "torrentfiltercontroller.h"

#include <QAction>
#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSet>
#include <QSignalBlocker>
#include <QStyle>

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

    if (m_searchEdit) {
        connect(m_searchEdit, &QLineEdit::textChanged,
                m_proxy, &TorrentSortProxyModel::setSearchText);
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

    // The .ui file used to pre-populate this list with icons, then the
    // runtime rebuild replaced it with plain text rows. Build the initial
    // contents here instead so the sidebar looks the same before and after
    // the first torrent refresh. Humanity survives another scrollbar.
    rebuildWithTrackers(QStringList());
    setStateFilter(TorrentSortProxyModel::StateFilter::All);
}

void TorrentFilterController::rebuild(const QVector<torrent> &torrents)
{
    const QStringList trackerHosts = trackerHostsFromTorrents(torrents);

    if (trackerHosts == m_lastTrackerHosts && m_filterList && m_filterList->count() > 0)
        return;

    rebuildWithTrackers(trackerHosts);
}

void TorrentFilterController::setStateFilter(TorrentSortProxyModel::StateFilter filter)
{
    if (!m_proxy)
        return;

    m_proxy->setStateFilter(filter);
    m_proxy->setTrackerFilter(QString());
    updateCheckedAction(filter);
    selectStatusFilter(filter);
}

void TorrentFilterController::rebuildWithTrackers(const QStringList &trackerHosts)
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

    QSignalBlocker blocker(m_filterList);
    m_filterList->clear();

    addStatusFilterItems();
    addTrackerFilterItems(trackerHosts);

    const bool restoredSelection = selectItem(currentType, currentValue, 1);

    if (!restoredSelection && m_proxy) {
        m_proxy->setStateFilter(TorrentSortProxyModel::StateFilter::All);
        m_proxy->setTrackerFilter(QString());
        updateCheckedAction(TorrentSortProxyModel::StateFilter::All);
    }
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
    static const QIcon allIcon = iconFromTheme({
        QStringLiteral("view-list-details"),
        QStringLiteral("mail-mark-important")
    });
    static const QIcon downloadingIcon = iconFromTheme({
        QStringLiteral("go-down"),
        QStringLiteral("go-next")
    });
    static const QIcon completeIcon = iconFromTheme({
        QStringLiteral("emblem-default"),
        QStringLiteral("go-up")
    });
    static const QIcon activeIcon = iconFromTheme({
        QStringLiteral("media-playback-start")
    });
    static const QIcon inactiveIcon = iconFromTheme({
        QStringLiteral("media-playback-stop")
    });
    static const QIcon stoppedIcon = iconFromTheme({
        QStringLiteral("media-playback-pause")
    });
    static const QIcon errorIcon = iconFromTheme({
        QStringLiteral("dialog-error"),
        QStringLiteral("edit-clear")
    });

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
    static const QIcon trackerIcon = iconFromTheme({
        QStringLiteral("network-server"),
        QStringLiteral("network-workgroup"),
        QStringLiteral("folder-remote")
    });

    auto *item = new QListWidgetItem(trackerIcon, label);
    item->setData(FilterTypeRole, typeToInt(ItemType::Tracker));
    item->setData(FilterValueRole, trackerHost);
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
        updateCheckedAction(stateFilter);
        return;
    }

    if (type == ItemType::Tracker) {
        m_proxy->setStateFilter(TorrentSortProxyModel::StateFilter::All);
        m_proxy->setTrackerFilter(current->data(FilterValueRole).toString());
        updateCheckedAction(TorrentSortProxyModel::StateFilter::All);
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

QIcon TorrentFilterController::iconFromTheme(const QStringList &themeNames)
{
    for (const QString &themeName : themeNames) {
        const QIcon icon = QIcon::fromTheme(themeName);

        if (!icon.isNull())
            return icon;
    }

    return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
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
    case ItemType::Header:
    default:
        return ItemType::Header;
    }
}
