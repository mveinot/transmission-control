#ifndef TORRENTMODEL_H
#define TORRENTMODEL_H

#include <QAbstractTableModel>
#include <QHash>
#include <QVector>

#include "torrent.h"

class TorrentModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        IdColumn,
        NameColumn,
        SizeColumn,
        PercentDoneColumn,
        StatusColumn,
        TrackerColumn,
        RateDownloadColumn,
        RateUploadColumn,
        UploadRatioColumn,
        EtaColumn,
        QueueColumn,
        AddedColumn,
        DownloadedEverColumn,
        UploadedEverColumn,
        DownloadDirColumn,
        SeedsColumn,
        PeersConnectedColumn,
        ColumnCount
    };

    enum Roles {
        SortRole = Qt::UserRole + 1,
        TrackerHostsRole,
        PrimaryTrackerHostRole,
        DownloadDirRole
    };

    explicit TorrentModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    torrent getTorrent(int row) const;
    int rowForId(int id) const;

public slots:
    void applyUpdate(const QVector<torrent> &incoming);
    void clear();

private:
    QVector<torrent> torrentVector;
    QHash<int, int> m_rowById;

    void rebuildIndex();
};

#endif // TORRENTMODEL_H