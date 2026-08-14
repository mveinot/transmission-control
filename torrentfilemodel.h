#ifndef TORRENTFILEMODEL_H
#define TORRENTFILEMODEL_H

#include "torrentdomain.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QList>
#include <QVector>
#include <memory>
#include <vector>

// Owns a stable, backend-neutral projection of one torrent's files. Routine
// progress snapshots update existing nodes so view expansion and indexes live on.
class TorrentFileModel final : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column {
        NameColumn = 0,
        PriorityColumn,
        SizeColumn,
        DoneColumn,
        RemainingColumn,
        PercentColumn,
        ColumnCount
    };

    enum Role {
        SortRole = Qt::UserRole,
        KindRole,
        FileIndexRole,
        WantedRole,
        PriorityRole,
        PathRole
    };

    explicit TorrentFileModel(QObject *parent = nullptr);

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void clear();
    void reconcile(const QVector<TorrentFile> &files);
    void setFlat(bool flat);
    bool isFlat() const;

    QList<int> fileIndices(const QModelIndex &index) const;
    QString torrentPath(const QModelIndex &index) const;

private:
    enum class TransferState { Complete, Transferring, Skipped, Mixed, Unknown };

    struct Node {
        Node *parent = nullptr;
        QString name;
        QString path;
        int fileIndex = -1;
        qint64 length = 0;
        qint64 bytesCompleted = 0;
        bool wanted = true;
        int priority = 0;
        TransferState state = TransferState::Unknown;
        QString effectivePriority;
        std::vector<std::unique_ptr<Node>> children;

        bool isFile() const { return fileIndex >= 0; }
        int row() const;
    };

    std::unique_ptr<Node> m_root;
    QVector<TorrentFile> m_files;
    QHash<int, Node *> m_filesByIndex;
    bool m_flat = false;

    Node *nodeForIndex(const QModelIndex &index) const;
    void rebuild();
    void appendFile(const TorrentFile &file);
    Node *findFolder(Node *parent, const QString &name) const;
    void updateAggregates(Node *node);
    void collectFileIndices(const Node *node, QList<int> *indices) const;
    bool sameStructure(const QVector<TorrentFile> &files) const;
    static QString priorityText(int priority);
};

#endif // TORRENTFILEMODEL_H
