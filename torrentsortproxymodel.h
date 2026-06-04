#ifndef TORRENTSORTPROXYMODEL_H
#define TORRENTSORTPROXYMODEL_H

#include <QSortFilterProxyModel>

class TorrentSortProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum class StateFilter {
        All,
        Downloading,
        Completed,
        Active,
        Inactive,
        Stopped,
        Error
    };

    explicit TorrentSortProxyModel(QObject *parent = nullptr);

    void setStateFilter(StateFilter filter);
    StateFilter stateFilter() const;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    StateFilter m_stateFilter = StateFilter::All;
};

#endif // TORRENTSORTPROXYMODEL_H
