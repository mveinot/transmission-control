#include "torrentsortproxymodel.h"
//#include "rpc_client.h"
#include "torrentmodel.h"
#include <QtWidgets/qtableview.h>

TorrentSortProxyModel::TorrentSortProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void TorrentSortProxyModel::setStateFilter(StateFilter filter)
{
    if (m_stateFilter == filter)
        return;

    beginFilterChange();
    m_stateFilter = filter;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
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

static QList<int> selectedTorrentIds(const QTableView *view)
{
    QList<int> ids;
    const auto rows = view->selectionModel()->selectedRows();
    ids.reserve(rows.size());

    for (const QModelIndex &proxyIndex : rows) {
        ids.append(proxyIndex.data(Qt::UserRole).toInt());
    }

    return ids;
}

static int currentTorrentId(const QTableView *view)
{
    const QModelIndex current = view->currentIndex();
    if (!current.isValid()) {
        return -1;
    }
    return current.data(Qt::UserRole).toInt();
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

static void restoreSelectionByIds(
    QTableView *view,
    TorrentModel *sourceModel,
    QSortFilterProxyModel *proxyModel,
    const QList<int> &selectedIds,
    int currentId)
{
    QItemSelectionModel *selectionModel = view->selectionModel();
    selectionModel->clearSelection();

    for (int id : selectedIds) {
        const int sourceRow = sourceModel->rowForId(id);
        if (sourceRow < 0) {
            continue;
        }

        const QModelIndex sourceIndex = sourceModel->index(sourceRow, 0);
        const QModelIndex proxyIndex = proxyModel->mapFromSource(sourceIndex);
        if (!proxyIndex.isValid()) {
            continue;
        }

        selectionModel->select(
            proxyIndex,
            QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }

    if (currentId >= 0) {
        const int sourceRow = sourceModel->rowForId(currentId);
        if (sourceRow >= 0) {
            const QModelIndex sourceIndex = sourceModel->index(sourceRow, 0);
            const QModelIndex proxyIndex = proxyModel->mapFromSource(sourceIndex);
            if (proxyIndex.isValid()) {
                selectionModel->setCurrentIndex(
                    proxyIndex,
                    QItemSelectionModel::NoUpdate);
                view->scrollTo(proxyIndex);
            }
        }
    }
}
