#include "activitylogmodel.h"

#include <QLocale>

ActivityLogModel::ActivityLogModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ActivityLogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

int ActivityLogModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ActivityLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    if (role == Qt::ToolTipRole)
        return entry.details;
    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case TimeColumn:
        return QLocale().toString(entry.timestamp.time(), QLocale::ShortFormat);
    case EventColumn:
        return entry.event;
    case DetailsColumn:
        return entry.details;
    case ServerColumn:
        return entry.server;
    default:
        return {};
    }
}

QVariant ActivityLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case TimeColumn: return tr("Time");
    case EventColumn: return tr("Event");
    case DetailsColumn: return tr("Details");
    case ServerColumn: return tr("Server");
    default: return {};
    }
}

void ActivityLogModel::addEvent(const QString &event, const QString &details,
                                const QString &server)
{
    if (m_entries.size() >= MaximumEntries) {
        beginRemoveRows({}, 0, 0);
        m_entries.removeFirst();
        endRemoveRows();
    }

    const int row = m_entries.size();
    beginInsertRows({}, row, row);
    m_entries.append({QDateTime::currentDateTime(), event, details, server});
    endInsertRows();
}

void ActivityLogModel::clear()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
}
