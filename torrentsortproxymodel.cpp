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
    case TorrentModel::StatusColumn:
    case TorrentModel::EtaColumn:
    case TorrentModel::QueueColumn:
        return lhs.toInt() < rhs.toInt();

    case TorrentModel::PercentDoneColumn:
    case TorrentModel::RateDownloadColumn:
    case TorrentModel::RateUploadColumn:
    case TorrentModel::UploadRatioColumn:
        return lhs.toDouble() < rhs.toDouble();
    case TorrentModel::SizeColumn:
        return lhs.toLongLong() < rhs.toLongLong();

    case TorrentModel::NameColumn:
    default:
        return QString::localeAwareCompare(lhs.toString(), rhs.toString()) < 0;
    }
}

bool TorrentSortProxyModel::filterAcceptsRow(int sourceRow,
                                             const QModelIndex &sourceParent) const
{
    return matchesStateFilter(sourceRow, sourceParent)
    && matchesSearchFilter(sourceRow, sourceParent)
        && matchesTrackerFilter(sourceRow, sourceParent);
}
/*
{
    if (m_stateFilter == StateFilter::All)
        return true;

    const QModelIndex statusIndex =
        sourceModel()->index(sourceRow, TorrentModel::StatusColumn, sourceParent);

    const QModelIndex percentIndex =
        sourceModel()->index(sourceRow, TorrentModel::PercentDoneColumn, sourceParent);

    const QString status =
        sourceModel()->data(statusIndex, Qt::DisplayRole).toString();

    const double percent =
        sourceModel()->data(percentIndex, Qt::UserRole + 1).toDouble();

    switch (m_stateFilter) {
    case StateFilter::All:
        return true;

    case StateFilter::Downloading:
        return status == "Downloading";

    case StateFilter::Completed:
        return percent >= 100.0;

    case StateFilter::Active:
        return status == "Downloading" ||
               status == "Seeding" ||
               status == "Verifying";

    case StateFilter::Inactive:
        return status == "Paused" ||
               status == "Queued" ||
               status == "Waiting to Verify" ||
               status == "Waiting to Seed";

    case StateFilter::Stopped:
        return status == "Paused";

    case StateFilter::Error:
        // Not currently supported by your model/request.
        return false;
    }

    return true;
}
*/

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

    const QModelIndex statusIndex =
        model->index(sourceRow, TorrentModel::StatusColumn, sourceParent);

    const QString status =
        model->data(statusIndex, Qt::DisplayRole).toString();

    switch (m_stateFilter) {
    case StateFilter::All:
        return true;

    case StateFilter::Downloading:
        return status == QStringLiteral("Downloading")
               || status == QStringLiteral("Queued");

    case StateFilter::Completed: {
        const QModelIndex percentIndex =
            model->index(sourceRow, TorrentModel::PercentDoneColumn, sourceParent);

        const double percentDone =
            model->data(percentIndex, Qt::UserRole + 1).toDouble();

        return percentDone >= 100.0;
    }

    case StateFilter::Active: {
        if (status == QStringLiteral("Downloading")
            || status == QStringLiteral("Seeding")
            || status == QStringLiteral("Queued")) {
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
        return status == QStringLiteral("Paused")
               || status == QStringLiteral("Waiting to Verify")
               || status == QStringLiteral("Waiting to Seed");

    case StateFilter::Stopped:
        return status == QStringLiteral("Paused");

    case StateFilter::Error:
        return status.contains(QStringLiteral("Error"), Qt::CaseInsensitive);
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