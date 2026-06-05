#ifndef PERCENTFILLDELEGATE_H
#define PERCENTFILLDELEGATE_H

#include <QStyledItemDelegate>
#include <QColor>
#include <QPalette>

class PercentFillDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit PercentFillDelegate(int percentColumn,
                                 int percentRole = Qt::UserRole + 1,
                                 QObject *parent = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    int m_percentColumn;
    int m_percentRole;
};

#endif // PERCENTFILLDELEGATE_H