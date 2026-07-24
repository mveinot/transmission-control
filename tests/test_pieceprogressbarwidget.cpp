#include <QtTest/QtTest>

#include "pieceprogressbarwidget.h"

class TestPieceProgressBarWidget : public QObject
{
    Q_OBJECT

private slots:
    void decodesTransmissionBitOrder();
    void partitionsPiecesAcrossAvailableColumns();
    void clampsOverallProgress();
};

void TestPieceProgressBarWidget::decodesTransmissionBitOrder()
{
    PieceProgressBarWidget widget;
    widget.setProgress(4, QByteArray::fromHex("a0"), 0.5);

    QCOMPARE(widget.pieceCount(), 4);
    QCOMPARE(widget.completedPieceCount(), 2);
}

void TestPieceProgressBarWidget::partitionsPiecesAcrossAvailableColumns()
{
    const PieceProgressBarWidget::PieceRange first =
        PieceProgressBarWidget::pieceRangeForColumn(0, 3, 10);
    const PieceProgressBarWidget::PieceRange middle =
        PieceProgressBarWidget::pieceRangeForColumn(1, 3, 10);
    const PieceProgressBarWidget::PieceRange last =
        PieceProgressBarWidget::pieceRangeForColumn(2, 3, 10);

    QCOMPARE(first.first, 0);
    QCOMPARE(first.lastExclusive, 3);
    QCOMPARE(middle.first, 3);
    QCOMPARE(middle.lastExclusive, 6);
    QCOMPARE(last.first, 6);
    QCOMPARE(last.lastExclusive, 10);
}

void TestPieceProgressBarWidget::clampsOverallProgress()
{
    PieceProgressBarWidget widget;
    widget.setProgress(0, QByteArray(), 1.5);
    QCOMPARE(widget.percentDone(), 1.0);

    widget.setProgress(0, QByteArray(), -0.5);
    QCOMPARE(widget.percentDone(), 0.0);
    QCOMPARE(widget.sizeHint().height(), 22);
}

QTEST_MAIN(TestPieceProgressBarWidget)
#include "test_pieceprogressbarwidget.moc"
