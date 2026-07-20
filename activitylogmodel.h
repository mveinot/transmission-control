#ifndef ACTIVITYLOGMODEL_H
#define ACTIVITYLOGMODEL_H

#include <QAbstractTableModel>
#include <QDateTime>
#include <QVector>

// Bounded, append-only event model used by the activity dock. Entries are
// presentation snapshots and deliberately independent of the RPC log stream.
class ActivityLogModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { TimeColumn, EventColumn, DetailsColumn, ServerColumn, ColumnCount };

    explicit ActivityLogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void addEvent(const QString &event, const QString &details, const QString &server);
    void clear();

private:
    struct Entry {
        QDateTime timestamp;
        QString event;
        QString details;
        QString server;
    };

    static constexpr qsizetype MaximumEntries = 500;
    QVector<Entry> m_entries;
};

#endif // ACTIVITYLOGMODEL_H
