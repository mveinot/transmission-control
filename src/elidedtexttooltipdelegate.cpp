#include "elidedtexttooltipdelegate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QHelpEvent>
#include <QStyle>
#include <QToolTip>

ElidedTextTooltipDelegate::ElidedTextTooltipDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void ElidedTextTooltipDelegate::paint(
    QPainter *painter,
    const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
#ifdef Q_OS_WIN
    QStyleOptionViewItem rowOption(option);
    rowOption.state &= ~QStyle::State_HasFocus;
    QStyledItemDelegate::paint(painter, rowOption, index);
#else
    QStyledItemDelegate::paint(painter, option, index);
#endif
}

bool ElidedTextTooltipDelegate::helpEvent(
    QHelpEvent *event,
    QAbstractItemView *view,
    const QStyleOptionViewItem &option,
    const QModelIndex &index)
{
    if (!event || !view || event->type() != QEvent::ToolTip)
        return QStyledItemDelegate::helpEvent(event, view, option, index);

    QStyleOptionViewItem styledOption(option);
    initStyleOption(&styledOption, index);

    const QStyle *style = styledOption.widget
        ? styledOption.widget->style()
        : QApplication::style();
    const QRect textRect = style->subElementRect(
        QStyle::SE_ItemViewItemText, &styledOption, styledOption.widget);

    // Asking QFontMetrics to perform the same elision as the item delegate
    // accounts for icon space, platform margins, and the active column width.
    const QString elidedText = styledOption.fontMetrics.elidedText(
        styledOption.text, styledOption.textElideMode, textRect.width());

    if (elidedText == styledOption.text)
        return QStyledItemDelegate::helpEvent(event, view, option, index);

    QString tooltip = styledOption.text;
    const QString modelTooltip = index.data(Qt::ToolTipRole).toString().trimmed();

    if (!modelTooltip.isEmpty() && modelTooltip != tooltip)
        tooltip += QStringLiteral("\n\n") + modelTooltip;

    QToolTip::showText(event->globalPos(), tooltip, view, option.rect);
    return true;
}
