#include <QtTest/QtTest>

#include "colorthememanager.h"
#include "pieceprogressbarwidget.h"

#include <QImage>

class TestPieceProgressBarWidget : public QObject
{
    Q_OBJECT

private slots:
    void decodesTransmissionBitOrder();
    void partitionsPiecesAcrossAvailableColumns();
    void clampsOverallProgress();
    void usesSemanticThemeColors();
    void exposesProgressToAssistiveClients();
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

void TestPieceProgressBarWidget::usesSemanticThemeColors()
{
    PieceProgressBarWidget widget;
    widget.resize(100, 22);
    widget.setProgress(8, QByteArray::fromHex("f0"), 0.5);

    QImage image(widget.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    widget.render(&image);

    const auto &colors = AppColors::ColorThemeManager::instance();
    const QColor foreground = colors.color(AppColors::Role::PieceComplete);
    const QColor background = colors.color(AppColors::Role::PieceRemaining);
    QCOMPARE(image.pixelColor(25, 4).rgba(), foreground.rgba());
    QCOMPARE(image.pixelColor(75, 4).rgba(), background.rgba());
    QCOMPARE(image.pixelColor(25, 16).rgba(), foreground.rgba());
    QCOMPARE(image.pixelColor(75, 16).rgba(), background.rgba());
}

void TestPieceProgressBarWidget::exposesProgressToAssistiveClients()
{
    PieceProgressBarWidget widget;
    QCOMPARE(widget.focusPolicy(), Qt::NoFocus);
    QVERIFY(!widget.accessibleName().isEmpty());
    QVERIFY(!widget.accessibleDescription().isEmpty());

    widget.setProgress(4, QByteArray::fromHex("a0"), 0.5);
    QVERIFY(widget.accessibleDescription().contains(QStringLiteral("50.0%")));
    QVERIFY(widget.accessibleDescription().contains(QStringLiteral("2 of 4")));
    QCOMPARE(widget.accessibleDescription(), widget.toolTip());

    widget.clear();
    QVERIFY(!widget.accessibleDescription().isEmpty());
}

QTEST_MAIN(TestPieceProgressBarWidget)
#include "test_pieceprogressbarwidget.moc"
