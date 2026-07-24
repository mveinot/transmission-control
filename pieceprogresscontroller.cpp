#include "pieceprogresscontroller.h"
#include "pieceprogressbarwidget.h"

#include <QJsonObject>
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

void PieceProgressController::update(const QJsonObject &details)
{
    if (!m_pieceProgressBar)
        return;

    const int pieceCount = details.value(QStringLiteral("pieceCount")).toInt(0);
    const QByteArray pieces =
        QByteArray::fromBase64(
            details.value(QStringLiteral("pieces")).toString().toLatin1()
        );

    m_pieceProgressBar->setProgress(
        pieceCount,
        pieces,
        details.value(QStringLiteral("percentDone")).toDouble(0.0));
}
