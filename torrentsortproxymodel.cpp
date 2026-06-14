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

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_stateFilter = filter;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    m_stateFilter = filter;
    invalidateFilter();
#endif
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
