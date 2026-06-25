#include "piecemapwidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QtMath>

PieceMapWidget::PieceMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PieceMapWidget::clear()
{
    m_pieceCount = 0;
    m_pieceBitfield.clear();
    update();
}

void PieceMapWidget::setPieces(int pieceCount, const QByteArray &pieceBitfield)
{
    m_pieceCount = qMax(0, pieceCount);
    m_pieceBitfield = pieceBitfield;
    update();
}

int PieceMapWidget::pieceCount() const
{
    return m_pieceCount;
}

int PieceMapWidget::completedPieceCount() const
{
    int count = 0;

    for (int index = 0; index < m_pieceCount; ++index) {
        if (hasPiece(index))
            ++count;
    }

    return count;
}

QSize PieceMapWidget::minimumSizeHint() const
{
    return QSize(220, 120);
}

QSize PieceMapWidget::sizeHint() const
{
    return QSize(420, 150);
}

bool PieceMapWidget::hasPiece(int index) const
{
    if (index < 0 || index >= m_pieceCount)
        return false;

    const int byteIndex = index / 8;
    const int bitIndex = 7 - (index % 8);

    if (byteIndex < 0 || byteIndex >= m_pieceBitfield.size())
        return false;

    const uchar byte = static_cast<uchar>(m_pieceBitfield.at(byteIndex));
    return (byte & (1u << bitIndex)) != 0;
}

void PieceMapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect frameRect = rect().adjusted(0, 0, -1, -1);

    painter.fillRect(frameRect, palette().base());
    painter.setPen(palette().mid().color());
    painter.drawRect(frameRect);

    if (m_pieceCount <= 0) {
        painter.setPen(palette().text().color());
        painter.drawText(frameRect.adjusted(8, 8, -8, -8),
                         Qt::AlignCenter,
                         tr("No piece data available"));
        return;
    }

    const QRect area = frameRect.adjusted(8, 8, -8, -8);

    if (area.width() <= 8 || area.height() <= 8)
        return;

    constexpr int gap = 1;

    int columns = qMax(
        1,
        static_cast<int>(qCeil(qSqrt(
            static_cast<double>(m_pieceCount)
            * static_cast<double>(area.width())
            / static_cast<double>(qMax(1, area.height()))
        )))
    );

    int rows = (m_pieceCount + columns - 1) / columns;
    int cellWidth = (area.width() - (columns - 1) * gap) / columns;
    int cellHeight = (area.height() - (rows - 1) * gap) / rows;
    int cellSize = qMin(cellWidth, cellHeight);

    while (columns > 1 && cellSize < 2) {
        --columns;
        rows = (m_pieceCount + columns - 1) / columns;
        cellWidth = (area.width() - (columns - 1) * gap) / columns;
        cellHeight = (area.height() - (rows - 1) * gap) / rows;
        cellSize = qMin(cellWidth, cellHeight);
    }

    cellSize = qMax(2, cellSize);
    rows = (m_pieceCount + columns - 1) / columns;

    const int gridWidth = columns * cellSize + (columns - 1) * gap;
    const int gridHeight = rows * cellSize + (rows - 1) * gap;

    const int startX = area.x() + (area.width() - gridWidth) / 2;
    const int startY = area.y() + (area.height() - gridHeight) / 2;

    const QColor haveColor = palette().highlight().color();
    QColor missingColor = palette().mid().color();
    missingColor.setAlpha(90);

    for (int index = 0; index < m_pieceCount; ++index) {
        const int row = index / columns;
        const int column = index % columns;

        const QRect cellRect(
            startX + column * (cellSize + gap),
            startY + row * (cellSize + gap),
            cellSize,
            cellSize
        );

        painter.fillRect(cellRect, hasPiece(index) ? haveColor : missingColor);
    }
}
