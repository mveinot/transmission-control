#include "pieceprogressbarwidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QtGlobal>

namespace {

QColor blendedColor(const QColor &background,
                    const QColor &foreground,
                    double foregroundFraction)
{
    const double fraction = qBound(0.0, foregroundFraction, 1.0);
    const auto blend = [fraction](int from, int to) {
        return qRound(from + (to - from) * fraction);
    };

    return QColor(blend(background.red(), foreground.red()),
                  blend(background.green(), foreground.green()),
                  blend(background.blue(), foreground.blue()),
                  blend(background.alpha(), foreground.alpha()));
}

} // namespace

PieceProgressBarWidget::PieceProgressBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(22);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void PieceProgressBarWidget::clear()
{
    m_pieceCount = 0;
    m_pieceBitfield.clear();
    m_percentDone = 0.0;
    setToolTip(QString());
    update();
}

void PieceProgressBarWidget::setProgress(int pieceCount,
                                         const QByteArray &pieceBitfield,
                                         double percentDone)
{
    m_pieceCount = qMax(0, pieceCount);
    m_pieceBitfield = pieceBitfield;
    m_percentDone = qBound(0.0, percentDone, 1.0);
    setToolTip(tr("%1% downloaded · %2 of %3 pieces complete")
                   .arg(m_percentDone * 100.0, 0, 'f', 1)
                   .arg(completedPieceCount())
                   .arg(m_pieceCount));
    update();
}

int PieceProgressBarWidget::pieceCount() const
{
    return m_pieceCount;
}

int PieceProgressBarWidget::completedPieceCount() const
{
    int completed = 0;
    for (int index = 0; index < m_pieceCount; ++index) {
        if (hasPiece(index))
            ++completed;
    }
    return completed;
}

double PieceProgressBarWidget::percentDone() const
{
    return m_percentDone;
}

PieceProgressBarWidget::PieceRange PieceProgressBarWidget::pieceRangeForColumn(
    int column, int columnCount, int pieceCount)
{
    if (column < 0 || column >= columnCount || columnCount <= 0 || pieceCount <= 0)
        return {};

    // Integer boundaries assign every piece exactly once while allowing bars
    // to grow wider than one pixel when the widget has spare horizontal space.
    const int first = static_cast<int>(
        (static_cast<qint64>(column) * pieceCount) / columnCount);
    const int last = static_cast<int>(
        (static_cast<qint64>(column + 1) * pieceCount) / columnCount);
    return { first, last };
}

QSize PieceProgressBarWidget::minimumSizeHint() const
{
    return QSize(80, 22);
}

QSize PieceProgressBarWidget::sizeHint() const
{
    return QSize(500, 22);
}

bool PieceProgressBarWidget::hasPiece(int index) const
{
    if (index < 0 || index >= m_pieceCount)
        return false;

    const int byteIndex = index / 8;
    if (byteIndex < 0 || byteIndex >= m_pieceBitfield.size())
        return false;

    const int bitIndex = 7 - (index % 8);
    const uchar byte = static_cast<uchar>(m_pieceBitfield.at(byteIndex));
    return (byte & (1u << bitIndex)) != 0;
}

double PieceProgressBarWidget::completedFraction(const PieceRange &range) const
{
    const int count = range.lastExclusive - range.first;
    if (count <= 0)
        return 0.0;

    int completed = 0;
    for (int index = range.first; index < range.lastExclusive; ++index) {
        if (hasPiece(index))
            ++completed;
    }
    return static_cast<double>(completed) / count;
}

void PieceProgressBarWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect frame = rect().adjusted(0, 0, -1, -1);
    if (frame.width() <= 0 || frame.height() <= 0)
        return;

    const QColor background = palette().base().color();
    const QColor foreground = palette().highlight().color();
    painter.fillRect(frame, background);

    const QRect content = frame.adjusted(1, 1, -1, -1);
    const int topHeight = content.height() / 2;
    const QRect overallRect(content.x(), content.y(), content.width(), topHeight);
    const QRect piecesRect(content.x(), content.y() + topHeight,
                           content.width(), content.height() - topHeight);

    const int progressWidth =
        qRound(overallRect.width() * m_percentDone);
    if (progressWidth > 0) {
        painter.fillRect(QRect(overallRect.x(), overallRect.y(),
                               progressWidth, overallRect.height()),
                         foreground);
    }

    if (m_pieceCount > 0 && piecesRect.width() > 0) {
        const int columns = qMin(m_pieceCount, piecesRect.width());
        for (int column = 0; column < columns; ++column) {
            const int left = piecesRect.x()
                + static_cast<int>((static_cast<qint64>(column)
                                    * piecesRect.width()) / columns);
            const int right = piecesRect.x()
                + static_cast<int>((static_cast<qint64>(column + 1)
                                    * piecesRect.width()) / columns);
            const PieceRange range =
                pieceRangeForColumn(column, columns, m_pieceCount);

            painter.fillRect(QRect(left, piecesRect.y(), qMax(1, right - left),
                                   piecesRect.height()),
                             blendedColor(background, foreground,
                                          completedFraction(range)));
        }
    }

    painter.setPen(palette().mid().color());
    painter.drawLine(content.left(), piecesRect.top() - 1,
                     content.right(), piecesRect.top() - 1);
    painter.drawRect(frame);
}
