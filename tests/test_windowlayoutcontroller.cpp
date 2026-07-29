#include <QtTest>

#include "settingskeys.h"
#include "windowlayoutcontroller.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolBar>

namespace {

QAction *actionWithText(QMenu *menu, const QString &text)
{
    if (!menu)
        return nullptr;
    for (QAction *action : menu->actions()) {
        if (action->text() == text)
            return action;
    }
    return nullptr;
}

} // namespace

class TestWindowLayoutController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void restoresAndPersistsVisibility();

private:
    QTemporaryDir m_settingsDirectory;
};

void TestWindowLayoutController::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("PlanetaryTests"));
    QCoreApplication::setApplicationName(QStringLiteral("WindowLayout"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       m_settingsDirectory.path());
}

void TestWindowLayoutController::restoresAndPersistsVisibility()
{
    QSettings settings;
    settings.clear();
    settings.setValue(SettingsKeys::MainWindowToolBarVisible, false);
    settings.setValue(SettingsKeys::MainWindowStatusBarVisible, true);
    settings.setValue(SettingsKeys::MainWindowDetailsPaneVisible, true);
    settings.setValue(SettingsKeys::MainWindowFilterSidebarVisible, false);

    QMainWindow window;
    QToolBar toolBar(&window);
    toolBar.setObjectName(QStringLiteral("testToolBar"));
    window.addToolBar(&toolBar);
    QStatusBar statusBar(&window);
    window.setStatusBar(&statusBar);

    QSplitter contentSplitter(Qt::Vertical, &window);
    QWidget upperPane(&contentSplitter);
    QWidget detailsPane(&contentSplitter);
    QSplitter mainSplitter(Qt::Horizontal, &upperPane);
    QWidget filterSidebar(&mainSplitter);
    QWidget torrentPane(&mainSplitter);

    WindowLayoutController::Widgets widgets;
    widgets.toolBar = &toolBar;
    widgets.statusBar = &statusBar;
    widgets.contentSplitter = &contentSplitter;
    widgets.mainSplitter = &mainSplitter;
    widgets.detailsPane = &detailsPane;
    widgets.filterSidebar = &filterSidebar;

    WindowLayoutController controller(&window, widgets);
    QMenu helpMenu(QStringLiteral("Help"), &window);
    window.menuBar()->addMenu(&helpMenu);
    controller.setupViewMenu(window.menuBar(), helpMenu.menuAction());

    QVERIFY(toolBar.isHidden());
    QVERIFY(!statusBar.isHidden());
    QVERIFY(!detailsPane.isHidden());
    QVERIFY(filterSidebar.isHidden());

    QSignalSpy detailsVisibility(
        &controller,
        &WindowLayoutController::detailsPaneVisibilityChanged);
    QAction *detailsAction =
        actionWithText(controller.viewMenu(), QStringLiteral("Details Pane"));
    QVERIFY(detailsAction);
    detailsAction->setChecked(false);
    QVERIFY(detailsPane.isHidden());
    QCOMPARE(detailsVisibility.size(), 1);
    QCOMPARE(detailsVisibility.first().first().toBool(), false);

    controller.saveState();
    QCOMPARE(settings.value(SettingsKeys::MainWindowToolBarVisible).toBool(),
             false);
    QCOMPARE(settings.value(SettingsKeys::MainWindowStatusBarVisible).toBool(),
             true);
    QCOMPARE(settings.value(
                 SettingsKeys::MainWindowDetailsPaneVisible).toBool(),
             false);
    QCOMPARE(settings.value(
                 SettingsKeys::MainWindowFilterSidebarVisible).toBool(),
             false);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication application(argc, argv);
    TestWindowLayoutController test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_windowlayoutcontroller.moc"
