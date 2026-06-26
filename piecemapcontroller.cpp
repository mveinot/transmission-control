#include "piecemapcontroller.h"

#include "piecemapwidget.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>
#include <QLocale>

PieceMapController::PieceMapController(QWidget *generalTab,
                                       QVBoxLayout *generalLayout,
                                       QGroupBox *generalInfoGroup,
                                       QObject *parent)
    : QObject(parent)
{
    m_group = new QGroupBox(tr("Pieces"), generalTab);
    m_group->setMinimumSize(220, 120);
    m_group->setMaximumSize(320, 170);
    m_group->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    auto *pieceMapLayout = new QVBoxLayout(m_group);
    pieceMapLayout->setContentsMargins(6, 6, 6, 6);

    m_pieceMap = new PieceMapWidget(m_group);
    pieceMapLayout->addWidget(m_pieceMap);

    auto *topGeneralRow = new QWidget(generalTab);
    auto *topGeneralLayout = new QHBoxLayout(topGeneralRow);
    topGeneralLayout->setContentsMargins(0, 0, 0, 0);
    topGeneralLayout->setSpacing(6);

    generalLayout->removeWidget(generalInfoGroup);
    topGeneralLayout->addWidget(generalInfoGroup, 1);
    topGeneralLayout->addWidget(m_group, 0, Qt::AlignTop);

    generalLayout->insertWidget(0, topGeneralRow, 0);
}

QWidget *PieceMapController::widget() const
{
    return m_group;
}

void PieceMapController::clear()
{
    if (m_pieceMap)
        m_pieceMap->clear();

    if (m_group)
        m_group->setTitle(tr("Pieces"));
}

void PieceMapController::update(const QJsonObject &details)
{
    if (!m_pieceMap || !m_group)
        return;

    const int pieceCount = details.value(QStringLiteral("pieceCount")).toInt(0);
    const QByteArray pieces =
        QByteArray::fromBase64(
            details.value(QStringLiteral("pieces")).toString().toLatin1()
        );

    m_pieceMap->setPieces(pieceCount, pieces);

    if (pieceCount <= 0) {
        m_group->setTitle(tr("Pieces"));
        return;
    }

    const int completed = m_pieceMap->completedPieceCount();
    const double percent =
        100.0 * static_cast<double>(completed) / static_cast<double>(pieceCount);

    m_group->setTitle(
        tr("Pieces (%1 / %2, %3%)")
            .arg(completed)
            .arg(pieceCount)
            .arg(QLocale().toString(percent, 'f', 1))
    );
}
