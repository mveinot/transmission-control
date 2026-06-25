#include "piecemapwidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QtMath>

PieceMapWidget::PieceMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(180, 80);
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
    return QSize(180, 80);
}

QSize PieceMapWidget::sizeHint() const
{
    return QSize(260, 100);
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

    int cellSize = 1;
    int gap = 0;
    int columns = qMax(1, area.width());
    int rows = (m_pieceCount + columns - 1) / columns;

    /*
     * Choose the largest square cell size that fits all pieces in the
     * available area. The previous version tried to keep a roughly square
     * grid, then backed off by reducing the column count when cells became
     * too small. That made large torrents collapse into a giant off-screen
     * single-column grid, which rendered as the sad little vertical line of
     * shame.
     */
    for (int candidateSize = 12; candidateSize >= 1; --candidateSize) {
        const int candidateGap = candidateSize >= 4 ? 1 : 0;
        const int candidateColumns = qMax(
            1,
            (area.width() + candidateGap) / (candidateSize + candidateGap)
        );

        const int candidateRows =
            (m_pieceCount + candidateColumns - 1) / candidateColumns;

        const int candidateGridHeight =
            candidateRows * candidateSize
            + qMax(0, candidateRows - 1) * candidateGap;

        if (candidateGridHeight <= area.height()) {
            cellSize = candidateSize;
            gap = candidateGap;
            columns = candidateColumns;
            rows = candidateRows;
            break;
        }
    }

    const int gridWidth = columns * cellSize + qMax(0, columns - 1) * gap;
    const int gridHeight = rows * cellSize + qMax(0, rows - 1) * gap;

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
