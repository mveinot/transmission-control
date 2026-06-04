#include "progressbardelegate.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionProgressBar>

ProgressBarDelegate::ProgressBarDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void ProgressBarDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    double percent = index.data(Qt::UserRole + 1).toDouble();

    QStyleOptionProgressBar progressOption;
    progressOption.rect = option.rect.adjusted(3, 3, -3, -3);
    progressOption.minimum = 0;
    progressOption.maximum = 100;
    progressOption.progress = qBound(0, static_cast<int>(percent), 100);
    progressOption.text = QString("%1%").arg(percent, 0, 'f', 1);
    progressOption.textVisible = true;

    QApplication::style()->drawControl(
        QStyle::CE_ProgressBar,
        &progressOption,
        painter
        );
}