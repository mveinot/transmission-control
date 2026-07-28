#include <QtTest/QtTest>

#include "sessionoverviewwidget.h"

#include <QImage>

class TestSessionOverviewWidget : public QObject
{
    Q_OBJECT

private slots:
    void keepsFiveMinutesOfHistory();
    void clearsHistory();
    void rendersEmptyAndPopulatedStates();
};

void TestSessionOverviewWidget::keepsFiveMinutesOfHistory()
{
    SessionOverviewWidget widget;
    const qint64 now = 1'000'000;

    widget.addSample(now - 6 * 60 * 1000, 100.0, 50.0, 1, 2, 3);
    widget.addSample(now, 200.0, 75.0, 2, 3, 4);

    QCOMPARE(widget.sampleCount(), 1);
}

void TestSessionOverviewWidget::clearsHistory()
{
    SessionOverviewWidget widget;
    widget.addSample(1'000, 100.0, 50.0, 1, 2, 3);
    widget.markDisconnected();
    widget.clearHistory();

    QCOMPARE(widget.sampleCount(), 0);
}

void TestSessionOverviewWidget::rendersEmptyAndPopulatedStates()
{
    SessionOverviewWidget widget;
    widget.resize(640, 240);

    QImage emptyImage(widget.size(), QImage::Format_ARGB32_Premultiplied);
    emptyImage.fill(Qt::transparent);
    widget.render(&emptyImage);

    widget.addSample(1'000, 1024.0, 512.0, 1, 2, 3);
    widget.addSample(2'000, 2048.0, 768.0, 2, 2, 1);

    QImage populatedImage(widget.size(), QImage::Format_ARGB32_Premultiplied);
    populatedImage.fill(Qt::transparent);
    widget.render(&populatedImage);

    QVERIFY(emptyImage != populatedImage);
}

QTEST_MAIN(TestSessionOverviewWidget)
#include "test_sessionoverviewwidget.moc"
