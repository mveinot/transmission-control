#include <QtGlobal>
#include "torrentsortproxymodel.h"
#include "torrentmodel.h"

TorrentSortProxyModel::TorrentSortProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void TorrentSortProxyModel::setStateFilter(StateFilter filter)
{
    if (m_stateFilter == filter)
        return;

    m_stateFilter = filter;
    refreshFilter();
}

TorrentSortProxyModel::StateFilter TorrentSortProxyModel::stateFilter() const
{
    return m_stateFilter;
}

bool TorrentSortProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const QVariant lhs = sourceModel()->data(left, Qt::UserRole + 1);
    const QVariant rhs = sourceModel()->data(right, Qt::UserRole + 1);

    switch (left.column()) {
    case TorrentModel::IdColumn:
    case TorrentModel::EtaColumn:
    case TorrentModel::QueueColumn:
    case TorrentModel::AddedColumn:
    case TorrentModel::DownloadedEverColumn:
    case TorrentModel::UploadedEverColumn:
    case TorrentModel::SeedsColumn:
    case TorrentModel::PeersConnectedColumn:
    case TorrentModel::HealthColumn:
        return lhs.toLongLong() < rhs.toLongLong();

    case TorrentModel::PercentDoneColumn:
    case TorrentModel::RateDownloadColumn:
    case TorrentModel::RateUploadColumn:
    case TorrentModel::UploadRatioColumn:
        return lhs.toDouble() < rhs.toDouble();
    case TorrentModel::SizeColumn:
        return lhs.toLongLong() < rhs.toLongLong();

    case TorrentModel::NameColumn:
    case TorrentModel::StatusColumn:
    case TorrentModel::TrackerColumn:
    case TorrentModel::DownloadDirColumn:
    default:
        return QString::localeAwareCompare(lhs.toString(), rhs.toString()) < 0;
    }
}

bool TorrentSortProxyModel::filterAcceptsRow(int sourceRow,
                                             const QModelIndex &sourceParent) const
{
    // Predicates remain orthogonal so choosing a tracker or folder preserves
    // the active state and free-text constraints.
    return matchesStateFilter(sourceRow, sourceParent)
        && matchesSearchFilter(sourceRow, sourceParent)
        && matchesTrackerFilter(sourceRow, sourceParent)
        && matchesDownloadDirFilter(sourceRow, sourceParent);
}

void TorrentSortProxyModel::refreshFilter()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
}

void TorrentSortProxyModel::setSearchText(const QString &searchText)
{
    const QString normalized = searchText.trimmed();

    if (m_searchText == normalized)
        return;

    m_searchText = normalized;
    refreshFilter();
}

QString TorrentSortProxyModel::searchText() const
{
    return m_searchText;
}

bool TorrentSortProxyModel::matchesSearchFilter(int sourceRow,
                                                const QModelIndex &sourceParent) const
{
    if (m_searchText.isEmpty())
        return true;

    const QAbstractItemModel *model = sourceModel();

    if (!model)
        return true;

    const QModelIndex nameIndex =
        model->index(sourceRow, TorrentModel::NameColumn, sourceParent);

    const QString name =
        model->data(nameIndex, Qt::DisplayRole).toString();

    return name.contains(m_searchText, Qt::CaseInsensitive);
}

bool TorrentSortProxyModel::matchesStateFilter(int sourceRow,
                                               const QModelIndex &sourceParent) const
{
    if (m_stateFilter == StateFilter::All)
        return true;

    const QAbstractItemModel *model = sourceModel();

    if (!model)
        return true;

    const QModelIndex nameIndex =
        model->index(sourceRow, TorrentModel::NameColumn, sourceParent);

    if (m_stateFilter == StateFilter::Error)
        return model->data(nameIndex, TorrentModel::HasErrorRole).toBool();

    const int statusValue =
        model->data(nameIndex, TorrentModel::StatusValueRole).toInt();

    switch (m_stateFilter) {
    case StateFilter::All:
        return true;

    case StateFilter::Downloading:
        return statusValue == 4 // Downloading
               || statusValue == 3; // Queued

    case StateFilter::Completed: {
        const QModelIndex percentIndex =
            model->index(sourceRow, TorrentModel::PercentDoneColumn, sourceParent);

        const double percentDone =
            model->data(percentIndex, Qt::UserRole + 1).toDouble();

        return percentDone >= 100.0;
    }

    case StateFilter::Active: {
        if (statusValue == 4 // Downloading
            || statusValue == 6 // Seeding
            || statusValue == 2) { // Verifying
            return true;
        }

        const QModelIndex downIndex =
            model->index(sourceRow, TorrentModel::RateDownloadColumn, sourceParent);

        const QModelIndex upIndex =
            model->index(sourceRow, TorrentModel::RateUploadColumn, sourceParent);

        const double downRate =
            model->data(downIndex, TorrentModel::SortRole).toDouble();

        const double upRate =
            model->data(upIndex, TorrentModel::SortRole).toDouble();

        return downRate > 0.0 || upRate > 0.0;
    }

    case StateFilter::Inactive:
        return statusValue == 0 // Paused
               || statusValue == 1 // Waiting to Verify
               || statusValue == 3 // Queued
               || statusValue == 5; // Waiting to Seed

    case StateFilter::Stopped:
        return statusValue == 0; // Paused

    case StateFilter::Error:
        return false;
    }

    return true;
}

void TorrentSortProxyModel::setTrackerFilter(const QString &trackerHost)
{
    const QString normalized = trackerHost.trimmed().toLower();

    if (m_trackerFilter == normalized)
        return;

    m_trackerFilter = normalized;
    refreshFilter();
}

QString TorrentSortProxyModel::trackerFilter() const
{
    return m_trackerFilter;
}

bool TorrentSortProxyModel::matchesTrackerFilter(int sourceRow,
                                                 const QModelIndex &sourceParent) const
{
    if (m_trackerFilter.isEmpty())
        return true;

    const QAbstractItemModel *model = sourceModel();

    if (!model)
        return true;

    const QModelIndex nameIndex =
        model->index(sourceRow, TorrentModel::NameColumn, sourceParent);

    const QStringList trackerHosts =
        model->data(nameIndex, TorrentModel::TrackerHostsRole).toStringList();

    for (const QString &trackerHost : trackerHosts) {
        if (trackerHost.compare(m_trackerFilter, Qt::CaseInsensitive) == 0)
            return true;
    }

    return false;
}
void TorrentSortProxyModel::setDownloadDirFilter(const QString &downloadDir)
{
    const QString normalized = downloadDir.trimmed();

    if (m_downloadDirFilter == normalized)
        return;

    m_downloadDirFilter = normalized;
    refreshFilter();
}

QString TorrentSortProxyModel::downloadDirFilter() const
{
    return m_downloadDirFilter;
}

bool TorrentSortProxyModel::matchesDownloadDirFilter(int sourceRow,
                                                     const QModelIndex &sourceParent) const
{
    if (m_downloadDirFilter.isEmpty())
        return true;

    const QAbstractItemModel *model = sourceModel();

    if (!model)
        return true;

    const QModelIndex nameIndex =
        model->index(sourceRow, TorrentModel::NameColumn, sourceParent);

    const QString downloadDir =
        model->data(nameIndex, TorrentModel::DownloadDirRole).toString();

    return downloadDir.compare(m_downloadDirFilter, Qt::CaseSensitive) == 0;
}
