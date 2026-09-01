#include "statusbarcontroller.h"

#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>
#include <QtTest>

class TestStatusBarController : public QObject
{
    Q_OBJECT

private slots:
    void freeSpaceSectionRemainsVisible();
};

void TestStatusBarController::freeSpaceSectionRemainsVisible()
{
    QMainWindow window;
    QStatusBar statusBar(&window);
    window.setStatusBar(&statusBar);

    StatusBarController controller(&statusBar, nullptr);
    controller.setup();

    auto *label = statusBar.findChild<QLabel *>(
        QStringLiteral("freeSpaceStatusLabel"));
    QVERIFY(label);
    QVERIFY(!label->isHidden());
    QVERIFY(label->text().contains(QStringLiteral("Updating")));

    controller.setFreeSpace(1024 * 1024);
    const QString measuredText = label->text();
    QVERIFY(measuredText.contains(QStringLiteral("1.0 MiB")));

    controller.setFreeSpaceAvailable(false);
    QVERIFY(!label->isHidden());
    QCOMPARE(label->text(), measuredText);

    controller.clearFreeSpace();
    QVERIFY(!label->isHidden());
    QVERIFY(label->text().contains(QStringLiteral("Updating")));
}

QTEST_MAIN(TestStatusBarController)
#include "test_statusbarcontroller.moc"
