#ifndef WINDOWLAYOUTCONTROLLER_H
#define WINDOWLAYOUTCONTROLLER_H

#include <QObject>

class QAction;
class QActionGroup;
class QMainWindow;
class QMenu;
class QMenuBar;
class QSplitter;
class QStatusBar;
class QToolBar;
class QWidget;

// Owns persistent main-window chrome and splitter policy. Application behavior
// reacts to visibility signals without being coupled to layout mechanics.
class WindowLayoutController : public QObject
{
    Q_OBJECT

public:
    struct Widgets {
        QToolBar *toolBar = nullptr;
        QStatusBar *statusBar = nullptr;
        QSplitter *contentSplitter = nullptr;
        QSplitter *mainSplitter = nullptr;
        QWidget *detailsPane = nullptr;
        QWidget *filterSidebar = nullptr;
    };

    WindowLayoutController(QMainWindow *window,
                           const Widgets &widgets,
                           QObject *parent = nullptr);

    void setupViewMenu(QMenuBar *menuBar, QAction *beforeAction);
    bool restoreWindowState();
    void saveState() const;

    QMenu *viewMenu() const;
    bool detailsPaneVisible() const;
    QMenu *createToolBarPopupMenu();

signals:
    void detailsPaneVisibilityChanged(bool visible);

private:
    QMainWindow *m_window = nullptr;
    Widgets m_widgets;
    QMenu *m_viewMenu = nullptr;
    QAction *m_showToolBarAction = nullptr;
    QAction *m_showStatusBarAction = nullptr;
    QAction *m_showDetailsPaneAction = nullptr;
    QAction *m_showFilterSidebarAction = nullptr;
    QActionGroup *m_toolBarStyleActionGroup = nullptr;
    int m_detailsPaneHeight = 300;
    int m_filterSidebarWidth = 220;

    void restoreWidgetState();
    void setToolBarVisible(bool visible);
    void setStatusBarVisible(bool visible);
    void setDetailsPaneVisible(bool visible);
    void setFilterSidebarVisible(bool visible);
    void setToolBarButtonStyle(QAction *action);
    void applyToolBarButtonStyle(Qt::ToolButtonStyle style);
};

#endif // WINDOWLAYOUTCONTROLLER_H
