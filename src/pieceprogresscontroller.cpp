#include "pieceprogresscontroller.h"
#include "pieceprogressbarwidget.h"
#include "torrentdomain.h"

#include <QVBoxLayout>
#include <QWidget>

PieceProgressController::PieceProgressController(QWidget *generalTab,
                                                 QVBoxLayout *generalLayout,
                                                 QObject *parent)
    : QObject(parent)
{
    m_pieceProgressBar = new PieceProgressBarWidget(generalTab);
    generalLayout->insertWidget(0, m_pieceProgressBar, 0);
}

QWidget *PieceProgressController::widget() const
{
    return m_pieceProgressBar;
}

void PieceProgressController::clear()
{
    if (m_pieceProgressBar)
        m_pieceProgressBar->clear();
}

void PieceProgressController::update(const TorrentPieces &pieces)
{
    if (!m_pieceProgressBar)
        return;

    m_pieceProgressBar->setProgress(
        pieces.pieceCount,
        pieces.completedPieces,
        pieces.percentDone);
}
