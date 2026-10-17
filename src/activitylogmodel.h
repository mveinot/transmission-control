#ifndef ACTIVITYLOGMODEL_H
#define ACTIVITYLOGMODEL_H

#include <QAbstractTableModel>
#include <QDateTime>
#include <QSortFilterProxyModel>
#include <QVector>

#include "activityevent.h"

// Bounded, append-only event model used by the activity dock. Entries are
// presentation snapshots and deliberately independent of the RPC log stream.
class ActivityLogModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { TimeColumn, LevelColumn, EventColumn, DetailsColumn, ServerColumn, ColumnCount };
    enum Role { LevelRole = Qt::UserRole + 1, StreamRole };

    explicit ActivityLogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void addEvent(const QString &event, const QString &details, const QString &server,
                  ActivityLevel level = ActivityLevel::Info);
    void clear();

private:
    struct Entry {
        QDateTime timestamp;
        QString event;
        QString details;
        QString server;
        ActivityLevel level = ActivityLevel::Info;
    };

    static constexpr qsizetype MaximumEntries = 500;
    QVector<Entry> m_entries;
};

class ActivityLogFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum class Filter { All, Activity, WarningsAndErrors };

    explicit ActivityLogFilterProxyModel(QObject *parent = nullptr);
    void setFilter(Filter filter);

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    Filter m_filter = Filter::All;
};

#endif // ACTIVITYLOGMODEL_H
