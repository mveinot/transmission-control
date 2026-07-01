#include "torrentmodel.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QSet>
#include <QStringList>
#include <QStyle>
#include <QVariant>
#include <Qt>


namespace {

QIcon themedIcon(const QStringList &names, QStyle::StandardPixmap fallback)
{
    for (const QString &name : names) {
        const QIcon icon = QIcon::fromTheme(name);

        if (!icon.isNull())
            return icon;
    }

    if (QApplication *app = qobject_cast<QApplication *>(QApplication::instance()))
        return app->style()->standardIcon(fallback);

    return {};
}

QIcon statusIcon(int statusValue, bool hasError)
{
    if (hasError) {
        return themedIcon({
                              QStringLiteral("dialog-error"),
                              QStringLiteral("emblem-important"),
                              QStringLiteral("process-stop")
                          },
                          QStyle::SP_MessageBoxCritical);
    }

    switch (statusValue) {
    case 0: // Paused
        return themedIcon({
                              QStringLiteral("media-playback-pause"),
                              QStringLiteral("player_pause")
                          },
                          QStyle::SP_MediaPause);

    case 1: // Waiting to verify
    case 3: // Queued
    case 5: // Waiting to seed
        return themedIcon({
                              QStringLiteral("appointment-soon"),
                              QStringLiteral("chronometer"),
                              QStringLiteral("view-calendar-upcoming")
                          },
                          QStyle::SP_BrowserReload);

    case 2: // Verifying
        return themedIcon({
                              QStringLiteral("view-refresh"),
                              QStringLiteral("emblem-synchronizing")
                          },
                          QStyle::SP_BrowserReload);

    case 4: // Downloading
        return themedIcon({
                              QStringLiteral("go-down"),
                              QStringLiteral("download"),
                              QStringLiteral("folder-download")
                          },
                          QStyle::SP_ArrowDown);

    case 6: // Seeding
        return themedIcon({
                              QStringLiteral("go-up"),
                              QStringLiteral("upload"),
                              QStringLiteral("folder-upload")
                          },
                          QStyle::SP_ArrowUp);

    default:
        return themedIcon({
                              QStringLiteral("dialog-question"),
                              QStringLiteral("help-about")
                          },
                          QStyle::SP_MessageBoxQuestion);
    }
}

QBrush errorTextBrush()
{
    const QPalette palette = QApplication::palette();
    const QColor base = palette.color(QPalette::Base);

    if (base.lightness() < 128)
        return QBrush(QColor(255, 120, 120));

    return QBrush(QColor(170, 0, 0));
}

QBrush disabledTextBrush()
{
    return QBrush(QApplication::palette().color(QPalette::Disabled, QPalette::Text));
}

bool isStatusCueColumn(int column)
{
    return column == TorrentModel::NameColumn || column == TorrentModel::StatusColumn;
}

} // namespace

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

    if (role == TrackerHostsRole)
        return t.getTrackerHosts();

    if (role == PrimaryTrackerHostRole)
        return t.getPrimaryTrackerHost();

    if (role == DownloadDirRole)
        return t.getDownloadDir();

    if (role == StatusValueRole)
        return t.getStatusValue();

    if (role == HasErrorRole)
        return t.hasError();

    if (role == ErrorStringRole)
        return t.getErrorString();

    if (role == Qt::ToolTipRole) {
        if (t.hasError()) {
            const QString message = t.getErrorString().isEmpty()
                ? tr("Transmission reports an error for this torrent.")
                : t.getErrorString();

            if (index.column() == NameColumn)
                return tr("Error: %1").arg(message);

            if (index.column() == StatusColumn)
                return tr("Error: %1").arg(message);
        }

        if (index.column() == StatusColumn)
            return tr("Status: %1").arg(t.getStatus());
    }

    if (role == Qt::DecorationRole) {
        if (index.column() == StatusColumn)
            return statusIcon(t.getStatusValue(), t.hasError());

        if (index.column() == NameColumn && t.hasError())
            return statusIcon(t.getStatusValue(), true);
    }

    if (role == Qt::ForegroundRole) {
        if (t.hasError() && isStatusCueColumn(index.column()))
            return errorTextBrush();

        if (index.column() == StatusColumn && t.getStatusValue() == 0)
            return disabledTextBrush();
    }

    if (role == Qt::FontRole && t.hasError() && isStatusCueColumn(index.column())) {
        QFont font;
        font.setBold(true);
        return font;
    }

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

        case TrackerColumn:
            return t.getPrimaryTrackerHost();

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

        case AddedColumn:
            return t.getAddedDate();

        case DownloadedEverColumn:
            return t.getDownloadedEver();

        case UploadedEverColumn:
            return t.getUploadedEver();

        case DownloadDirColumn:
            return t.getDownloadDir();

        case SeedsColumn:
            return t.getSeedsSummary();

        case PeersConnectedColumn:
            return t.getPeersSummary();

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

        case TrackerColumn:
            return t.getPrimaryTrackerHost();

        case RateDownloadColumn:
            return t.getRateDownloadBytesPerSecond();

        case RateUploadColumn:
            return t.getRateUploadBytesPerSecond();

        case UploadRatioColumn:
            return t.getUploadRatioValue();

        case EtaColumn:
            return t.getEtaSeconds();

        case QueueColumn:
            return t.getQueuePosition();

        case AddedColumn:
            return t.getAddedDateSecs();

        case DownloadedEverColumn:
            return t.getDownloadedEverBytes();

        case UploadedEverColumn:
            return t.getUploadedEverBytes();

        case DownloadDirColumn:
            return t.getDownloadDir();

        case SeedsColumn:
            return t.getSeedsSortValue();

        case PeersConnectedColumn:
            return t.getPeersSortValue();

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
        case QueueColumn:
        case AddedColumn:
        case DownloadedEverColumn:
        case UploadedEverColumn:
        case SeedsColumn:
        case PeersConnectedColumn:
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

        case TrackerColumn:
            return "Tracker";

        case RateDownloadColumn:
            return "Down";

        case RateUploadColumn:
            return "Up";

        case UploadRatioColumn:
            return "Ratio";

        case EtaColumn:
            return "ETA";

        case QueueColumn:
            return "Queue";

        case AddedColumn:
            return "Added";

        case DownloadedEverColumn:
            return "Downloaded";

        case UploadedEverColumn:
            return "Uploaded";

        case DownloadDirColumn:
            return "Download Folder";

        case SeedsColumn:
            return "Seeds";

        case PeersConnectedColumn:
            return "Peers";

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
}