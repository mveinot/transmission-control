#ifndef ELIDEDTEXTTOOLTIPDELEGATE_H
#define ELIDEDTEXTTOOLTIPDELEGATE_H

#include <QStyledItemDelegate>

// Adds the complete display text as a tooltip only when the platform style
// elides it. Any semantic tooltip supplied by the model remains available.
class ElidedTextTooltipDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ElidedTextTooltipDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    bool helpEvent(QHelpEvent *event,
                   QAbstractItemView *view,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;
};

#endif // ELIDEDTEXTTOOLTIPDELEGATE_H
