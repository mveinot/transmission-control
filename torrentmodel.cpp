#include "torrentmodel.h"
#include "colorthememanager.h"
#include "iconthememanager.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QSet>
#include <QStringList>
#include <QVariant>
#include <Qt>


namespace {

QIcon statusIcon(int statusValue, bool hasError)
{
    const auto &icons = AppIcons::IconThemeManager::instance();
    if (hasError)
        return icons.icon(AppIcons::Id::StatusError);

    switch (statusValue) {
    case 0: // Paused
        return icons.icon(AppIcons::Id::StatusStopped);

    case 1: // Waiting to verify
    case 3: // Queued
    case 5: // Waiting to seed
        return icons.icon(AppIcons::Id::StatusQueued);

    case 2: // Verifying
        return icons.icon(AppIcons::Id::StatusVerifying);

    case 4: // Downloading
        return icons.icon(AppIcons::Id::StatusDownloading);

    case 6: // Seeding
        return icons.icon(AppIcons::Id::StatusSeeding);

    default:
        return icons.icon(AppIcons::Id::StatusUnknown);
    }
}


QBrush errorTextBrush()
{
    return QBrush(AppColors::ColorThemeManager::instance().color(
        AppColors::Role::Error));
}

QBrush disabledTextBrush()
{
    return QBrush(AppColors::ColorThemeManager::instance().color(
        AppColors::Role::Inactive));
}

bool isStatusCueColumn(int column)
{
    return column == TorrentModel::NameColumn || column == TorrentModel::StatusColumn;
}

} // namespace

TorrentModel::TorrentModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    connect(&AppIcons::IconThemeManager::instance(),
            &AppIcons::IconThemeManager::themeChanged,
            this,
            [this]() {
                if (torrentVector.isEmpty())
                    return;

                const QVector<int> roles {Qt::DecorationRole};
                emit dataChanged(index(0, NameColumn),
                                 index(torrentVector.size() - 1, NameColumn),
                                 roles);
                emit dataChanged(index(0, StatusColumn),
                                 index(torrentVector.size() - 1, StatusColumn),
                                 roles);
            });
    connect(&AppColors::ColorThemeManager::instance(),
            &AppColors::ColorThemeManager::themeChanged,
            this,
            [this]() {
                if (torrentVector.isEmpty())
                    return;

                emit dataChanged(index(0, 0),
                                 index(torrentVector.size() - 1,
                                       ColumnCount - 1),
                                 {Qt::ForegroundRole});
            });
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
    const bool verificationVisible = t.isVerificationStatus();
    const bool visibleError = t.hasError() && !verificationVisible;

    if (role == TrackerHostsRole)
        return t.getTrackerHosts();

    if (role == PrimaryTrackerHostRole)
        return t.getPrimaryTrackerHost();

    if (role == DownloadDirRole)
        return t.getDownloadDir();

    if (role == LabelsRole)
        return t.getLabels();

    if (role == GroupRole)
        return t.getGroup();

    if (role == StatusValueRole)
        return t.getStatusValue();

    if (role == HasErrorRole)
        return t.hasError();

    if (role == ErrorStringRole)
        return t.getErrorString();

    if (role == DownloadCompletionRole)
        return t.getPercentDone();

    if (role == Qt::ToolTipRole) {
        if (verificationVisible
            && (index.column() == NameColumn
                || index.column() == StatusColumn)) {
            QString text = tr("Status: %1").arg(t.getStatus());
            if (t.hasVerificationProgress()) {
                text += tr("\nVerification progress: %1%").arg(
                    QString::number(t.getVerificationProgress(), 'f', 1));
            }
            if (t.hasError()) {
                const QString message = t.getErrorString().isEmpty()
                    ? tr("The torrent backend reports an error for this torrent.")
                    : t.getErrorString();
                text += tr("\nPrevious error: %1").arg(message);
            }
            return text;
        }

        if (visibleError) {
            const QString message = t.getErrorString().isEmpty()
                ? tr("The torrent backend reports an error for this torrent.")
                : t.getErrorString();

            if (index.column() == NameColumn)
                return tr("Error: %1").arg(message);

            if (index.column() == StatusColumn)
                return tr("Error: %1").arg(message);
        }

        if (index.column() == StatusColumn)
            return tr("Status: %1").arg(t.getStatus());

        if (index.column() == PercentDoneColumn
            && t.isVerificationStatus()) {
            if (t.hasVerificationProgress()) {
                return tr("Verification progress: %1%\nDownload completed: %2%")
                    .arg(QString::number(t.getVerificationProgress(), 'f', 1),
                         QString::number(t.getPercentDone(), 'f', 1));
            }
            return tr("Verification progress unavailable.\nDownload completed: %1%")
                .arg(QString::number(t.getPercentDone(), 'f', 1));
        }

        if (index.column() == HealthColumn)
            return t.getHealthDetails();
    }

    if (role == Qt::DecorationRole) {
        if (index.column() == StatusColumn)
            return statusIcon(t.getStatusValue(), visibleError);

        if (index.column() == NameColumn && visibleError)
            return statusIcon(t.getStatusValue(), true);
    }

    if (role == Qt::ForegroundRole) {
        if (visibleError && isStatusCueColumn(index.column()))
            return errorTextBrush();

        if (index.column() == StatusColumn && t.getStatusValue() == 0)
            return disabledTextBrush();
    }

    if (role == Qt::FontRole && visibleError && isStatusCueColumn(index.column())) {
        QFont font;
        font.setBold(true);
        return font;
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case IdColumn:
            return t.getKey();

        case NameColumn:
            return t.getName();

        case SizeColumn:
            return t.getSize();

        case PercentDoneColumn:
            return t.getDisplayedPercentDone() < 0.0
                       ? QStringLiteral("—")
                       : QString::number(t.getDisplayedPercentDone(), 'f', 1)
                             + "%";

        case StatusColumn:
            return t.getStatus();

        case HealthColumn:
            return t.getHealth();

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
        return t.getKey();
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
            return t.getKey();

        case NameColumn:
            return t.getName();

        case SizeColumn:
            return t.getSizeBytes();

        case PercentDoneColumn:
            return t.getDisplayedPercentDone();

        case StatusColumn:
            return t.getStatus();

        case HealthColumn:
            return t.getHealthScore();

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

        case HealthColumn:
            return "Health";

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

int TorrentModel::rowForKey(const TorrentKey &key) const
{
    return m_rowByKey.value(key, -1);
}

void TorrentModel::clear()
{
    beginResetModel();

    torrentVector.clear();
    m_rowByKey.clear();

    endResetModel();
}

void TorrentModel::rebuildIndex()
{
    // Row numbers can shift after removals and inserts; deriving the map from
    // the vector is less error-prone than incrementally repairing it.
    m_rowByKey.clear();
    m_rowByKey.reserve(torrentVector.size());

    for (int row = 0; row < torrentVector.size(); ++row)
        m_rowByKey.insert(torrentVector.at(row).getKey(), row);
}

void TorrentModel::applyUpdate(const QVector<torrent> &incoming)
{
    QHash<TorrentKey, torrent> incomingByKey;
    incomingByKey.reserve(incoming.size());

    QSet<TorrentKey> incomingKeys;
    incomingKeys.reserve(incoming.size());

    for (const torrent &t : incoming) {
        incomingByKey.insert(t.getKey(), t);
        incomingKeys.insert(t.getKey());
    }

    /*
     * Remove rows that disappeared.
     *
     * Remove from back to front so row numbers remain valid as rows are removed.
     * Because invalidating your own row indexes mid-loop is one of those
     * little self-inflicted wounds C++ never prevents.
     */
    for (int row = torrentVector.size() - 1; row >= 0; --row) {
        const TorrentKey key = torrentVector.at(row).getKey();

        if (!incomingKeys.contains(key)) {
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
        const TorrentKey key = torrentVector.at(row).getKey();

        auto it = incomingByKey.constFind(key);

        if (it == incomingByKey.cend()) {
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
        if (!m_rowByKey.contains(t.getKey())) {
            const int row = torrentVector.size();

            beginInsertRows(QModelIndex(), row, row);
            torrentVector.append(t);
            endInsertRows();

            m_rowByKey.insert(t.getKey(), row);
        }
    }

    rebuildIndex();
}
