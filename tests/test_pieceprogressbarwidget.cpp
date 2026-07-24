#include <QtTest/QtTest>

#include "pieceprogressbarwidget.h"

#include <QImage>

class TestPieceProgressBarWidget : public QObject
{
    Q_OBJECT

private slots:
    void decodesTransmissionBitOrder();
    void partitionsPiecesAcrossAvailableColumns();
    void clampsOverallProgress();
    void usesFixedHighContrastColors();
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

void TestPieceProgressBarWidget::usesFixedHighContrastColors()
{
    PieceProgressBarWidget widget;
    widget.resize(100, 22);
    widget.setProgress(8, QByteArray::fromHex("f0"), 0.5);

    QImage image(widget.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    widget.render(&image);

    const QColor foreground(QStringLiteral("#403878"));
    const QColor background(QStringLiteral("#ffffff"));
    QCOMPARE(image.pixelColor(25, 4), foreground);
    QCOMPARE(image.pixelColor(75, 4), background);
    QCOMPARE(image.pixelColor(25, 16), foreground);
    QCOMPARE(image.pixelColor(75, 16), background);
}

QTEST_MAIN(TestPieceProgressBarWidget)
#include "test_pieceprogressbarwidget.moc"
