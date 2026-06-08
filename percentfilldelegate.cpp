#include "percentfilldelegate.h"
//#include "rpc_client.h"
#include "torrentmodel.h"
#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>

PercentFillDelegate::PercentFillDelegate(int percentColumn,
                                         int percentRole,
                                         QObject *parent)
    : QStyledItemDelegate(parent),
    m_percentColumn(percentColumn),
    m_percentRole(percentRole)
{
}

static QColor readableFillColor(const QPalette &palette, bool selected)
{
    QColor fill = palette.color(QPalette::Accent);

    if (!fill.isValid())
        fill = palette.color(QPalette::Highlight);

    const QColor background = selected
                                  ? palette.color(QPalette::Highlight)
                                  : palette.color(QPalette::Base);

    // If too dark against a dark background, brighten it.
    if (background.lightness() < 128 && fill.lightness() < 140) {
        fill = fill.lighter(180);
    }

    // If too light against a light background, darken it.
    if (background.lightness() >= 128 && fill.lightness() > 190) {
        fill = fill.darker(140);
    }

    fill.setAlpha(selected ? 90 : 120);
    return fill;
}

void PercentFillDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    if (index.column() != TorrentModel::PercentDoneColumn) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    if (index.column() != m_percentColumn) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    bool ok = false;
    const double percent = index.data(m_percentRole).toDouble(&ok);

    if (!ok) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const double boundedPercent = qBound(0.0, percent, 100.0);

    painter->save();

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    // Draw the normal item background first, including selection.
    opt.text.clear();

    const QStyle *style = opt.widget
                              ? opt.widget->style()
                              : QApplication::style();

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    // Draw the left-to-right fill area.
    QRect fillRect = option.rect.adjusted(2, 2, -2, -2);
    fillRect.setWidth(static_cast<int>(
        fillRect.width() * (boundedPercent / 100.0)
        ));

    const bool selected = option.state & QStyle::State_Selected;
    QColor fillColor = readableFillColor(option.palette, selected);

    if (!(option.state & QStyle::State_Selected)) {
        fillColor.setAlpha(70);
    } else {
        fillColor = option.palette.highlightedText().color();
        fillColor.setAlpha(60);
    }

    painter->fillRect(fillRect, fillColor);

    // Draw the percent text on top.
    const QString text = QString("%1%").arg(boundedPercent, 0, 'f', 1);

    QColor textColor = (option.state & QStyle::State_Selected)
                           ? option.palette.highlightedText().color()
                           : option.palette.text().color();

    painter->setPen(textColor);
    painter->drawText(option.rect, Qt::AlignCenter, text);

    painter->restore();
}