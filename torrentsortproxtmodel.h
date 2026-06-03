#ifndef TORRENTSORTPROXTMODEL_H
#define TORRENTSORTPROXTMODEL_H

#include <QObject>
#include <QSortFilterProxyModel>

class TorrentSortProxtModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TorrentSortProxtModel(QObject *parent = nullptr);
protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
signals:
};

#endif // TORRENTSORTPROXTMODEL_H
