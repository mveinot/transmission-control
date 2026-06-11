#include "torrentmodel.h"

#include <QSet>
#include <QVariant>
#include <Qt>

TorrentModel::TorrentModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int TorrentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : torrentVector.size();
}

int TorrentModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant TorrentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() ||
        index.row() < 0 ||
        index.row() >= torrentVector.size()) {
        return {};
    }

    const torrent &t = torrentVector.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case IdColumn:
            return t.getId();

        case NameColumn:
            return t.getName();

        case SizeColumn:
            return t.getSize();

        case PercentDoneColumn:
            return QString::number(t.getPercentDone(), 'f', 1) + "%";

        case StatusColumn:
            return t.getStatus();

        case RateDownloadColumn:
            return t.getRateDownload();

        case RateUploadColumn:
            return t.getRateUpload();

        case UploadRatioColumn:
            return t.getUploadRatio();

        case EtaColumn:
            return t.getEta();

        case QueueColumn:
            return t.getQueuePosition();

        default:
            return {};
        }
    }

    /*
     * Torrent ID role.
     *
     * MainWindow selection helpers can use this from any valid source index:
     *
     *     sourceIndex.data(Qt::UserRole).toInt()
     */
    if (role == Qt::UserRole) {
        return t.getId();
    }

    /*
     * Raw sortable/delegate-friendly values.
     *
     * DisplayRole is for humans.
     * Qt::UserRole + 1 is for sorting/delegates.
     *
     * Yes, separating those saves us from sorting "1.2 GiB" against "900 MiB"
     * alphabetically like a tiny spreadsheet goblin.
     */
    if (role == Qt::UserRole + 1) {
        switch (index.column()) {
        case IdColumn:
            return t.getId();

        case NameColumn:
            return t.getName();

        case SizeColumn:
            return t.getSizeBytes();

        case PercentDoneColumn:
            return t.getPercentDone();

        case StatusColumn:
            return t.getStatus();

        case RateDownloadColumn:
            return t.getRateDownload();

        case RateUploadColumn:
            return t.getRateUpload();

        case UploadRatioColumn:
            return t.getUploadRatio();

        case EtaColumn:
            return t.getEta();

        case QueueColumn:
            return t.getQueuePosition();

        default:
            return {};
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case IdColumn:
        case SizeColumn:
        case PercentDoneColumn:
        case RateDownloadColumn:
        case RateUploadColumn:
        case UploadRatioColumn:
        case EtaColumn:
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

        default:
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    return {};
}

QVariant TorrentModel::headerData(int section,
                                  Qt::Orientation orientation,
                                  int role) const
{
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case IdColumn:
            return "ID";

        case NameColumn:
            return "Name";

        case SizeColumn:
            return "Size";

        case PercentDoneColumn:
            return "Completed";

        case StatusColumn:
            return "Status";

        case RateDownloadColumn:
            return "Down";

        case RateUploadColumn:
            return "Up";

        case UploadRatioColumn:
            return "Ratio";

        case EtaColumn:
            return "ETA";

        default:
            return {};
        }
    }

    return section + 1;
}

torrent TorrentModel::getTorrent(int row) const
{
    if (row < 0 || row >= torrentVector.size()) {
        return torrent(QJsonValue());
    }

    return torrentVector.at(row);
}

int TorrentModel::rowForId(int id) const
{
    return m_rowById.value(id, -1);
}

void TorrentModel::clear()
{
    beginResetModel();

    torrentVector.clear();
    m_rowById.clear();

    endResetModel();

    emit listUpdated();
}

void TorrentModel::rebuildIndex()
{
    m_rowById.clear();
    m_rowById.reserve(torrentVector.size());

    for (int row = 0; row < torrentVector.size(); ++row) {
        m_rowById.insert(torrentVector.at(row).getId(), row);
    }
}

void TorrentModel::applyUpdate(const QVector<torrent> &incoming)
{
    QHash<int, torrent> incomingById;
    incomingById.reserve(incoming.size());

    QSet<int> incomingIds;
    incomingIds.reserve(incoming.size());

    for (const torrent &t : incoming) {
        incomingById.insert(t.getId(), t);
        incomingIds.insert(t.getId());
    }

    /*
     * Remove rows that disappeared.
     *
     * Remove from back to front so row numbers remain valid as rows are removed.
     * Because invalidating your own row indexes mid-loop is one of those
     * little self-inflicted wounds C++ never prevents.
     */
    for (int row = torrentVector.size() - 1; row >= 0; --row) {
        const int id = torrentVector.at(row).getId();

        if (!incomingIds.contains(id)) {
            beginRemoveRows(QModelIndex(), row, row);
            torrentVector.removeAt(row);
            endRemoveRows();
        }
    }

    rebuildIndex();

    /*
     * Update existing rows in place.
     *
     * Always replace the stored torrent object so files/peers stay fresh,
     * even if the visible table fields did not change.
     */
    for (int row = 0; row < torrentVector.size(); ++row) {
        const int id = torrentVector.at(row).getId();

        auto it = incomingById.constFind(id);

        if (it == incomingById.cend()) {
            continue;
        }

        const torrent &updated = it.value();

        const bool displayChanged =
            !torrentVector.at(row).sameDisplayData(updated);

        torrentVector[row] = updated;

        if (displayChanged) {
            emit dataChanged(
                index(row, 0),
                index(row, ColumnCount - 1)
                );
        }
    }

    rebuildIndex();

    /*
     * Insert new rows.
     *
     * Appending is simplest; the proxy handles sorted/filter view order.
     */
    for (const torrent &t : incoming) {
        if (!m_rowById.contains(t.getId())) {
            const int row = torrentVector.size();

            beginInsertRows(QModelIndex(), row, row);
            torrentVector.append(t);
            endInsertRows();

            m_rowById.insert(t.getId(), row);
        }
    }

    rebuildIndex();

    emit listUpdated();
}