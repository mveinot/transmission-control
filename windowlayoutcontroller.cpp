#include "windowlayoutcontroller.h"

#include "settingskeys.h"

#include <QAction>
#include <QActionGroup>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

#include <utility>

namespace {
constexpr int DefaultDetailsPaneHeight = 300;
constexpr int DefaultFilterSidebarWidth = 220;
constexpr Qt::ToolButtonStyle DefaultToolBarButtonStyle =
    Qt::ToolButtonIconOnly;

bool isSupportedToolBarButtonStyle(Qt::ToolButtonStyle style)
{
    return style == Qt::ToolButtonIconOnly
           || style == Qt::ToolButtonTextOnly
           || style == Qt::ToolButtonTextBesideIcon;
}

Qt::ToolButtonStyle toolButtonStyleFromVariant(
    const QVariant &value, Qt::ToolButtonStyle fallback)
{
    bool ok = false;
    const auto style =
        static_cast<Qt::ToolButtonStyle>(value.toInt(&ok));
    return ok && isSupportedToolBarButtonStyle(style) ? style : fallback;
}

void syncToolBarStyleActions(QActionGroup *group, Qt::ToolButtonStyle style)
{
    if (!group)
        return;

    const QSignalBlocker blocker(group);
    const bool exclusive = group->isExclusive();
    group->setExclusive(false);
    for (QAction *action : group->actions()) {
        action->setChecked(
            toolButtonStyleFromVariant(action->data(),
                                       DefaultToolBarButtonStyle) == style);
    }
    group->setExclusive(exclusive);
}
} // namespace

WindowLayoutController::WindowLayoutController(QMainWindow *window,
                                               const Widgets &widgets,
                                               QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_widgets(widgets)
{
}

void WindowLayoutController::setupViewMenu(QMenuBar *menuBar,
                                           QAction *beforeAction)
{
    if (!m_window || !menuBar)
        return;

    if (m_widgets.toolBar)
        m_widgets.toolBar->setWindowTitle(tr("Toolbar"));

    m_viewMenu = new QMenu(tr("View"), m_window);
    menuBar->insertMenu(beforeAction, m_viewMenu);

    m_showToolBarAction = m_viewMenu->addAction(tr("Toolbar"));
    m_showToolBarAction->setCheckable(true);
    m_showToolBarAction->setToolTip(tr("Show or hide the main toolbar"));

    m_showStatusBarAction = m_viewMenu->addAction(tr("Status Bar"));
    m_showStatusBarAction->setCheckable(true);
    m_showStatusBarAction->setToolTip(tr("Show or hide the status bar"));

    m_showDetailsPaneAction = m_viewMenu->addAction(tr("Details Pane"));
    m_showDetailsPaneAction->setCheckable(true);
    m_showDetailsPaneAction->setToolTip(
        tr("Show or hide the selected torrent details pane"));

    m_showFilterSidebarAction =
        m_viewMenu->addAction(tr("Filter Sidebar"));
    m_showFilterSidebarAction->setCheckable(true);
    m_showFilterSidebarAction->setToolTip(
        tr("Show or hide torrent status, tracker, folder, label, and group filters"));

    m_viewMenu->addSeparator();
    QMenu *styleMenu = m_viewMenu->addMenu(tr("Toolbar Display"));
    m_toolBarStyleActionGroup = new QActionGroup(this);
    m_toolBarStyleActionGroup->setExclusive(true);

    const auto addStyleAction =
        [this, styleMenu](const QString &text, Qt::ToolButtonStyle style) {
            QAction *action = styleMenu->addAction(text);
            action->setCheckable(true);
            action->setData(static_cast<int>(style));
            m_toolBarStyleActionGroup->addAction(action);
        };
    addStyleAction(tr("Icons Only"), Qt::ToolButtonIconOnly);
    addStyleAction(tr("Icons and Text"), Qt::ToolButtonTextBesideIcon);
    addStyleAction(tr("Text Only"), Qt::ToolButtonTextOnly);

    connect(m_showToolBarAction, &QAction::toggled,
            this, &WindowLayoutController::setToolBarVisible);
    connect(m_showStatusBarAction, &QAction::toggled,
            this, &WindowLayoutController::setStatusBarVisible);
    connect(m_showDetailsPaneAction, &QAction::toggled,
            this, &WindowLayoutController::setDetailsPaneVisible);
    connect(m_showFilterSidebarAction, &QAction::toggled,
            this, &WindowLayoutController::setFilterSidebarVisible);
    connect(m_toolBarStyleActionGroup, &QActionGroup::triggered,
            this, &WindowLayoutController::setToolBarButtonStyle);

    if (m_widgets.contentSplitter) {
        connect(m_widgets.contentSplitter, &QSplitter::splitterMoved,
                this, [this](int, int index) {
                    if (index != 1)
                        return;
                    const int paneSize =
                        m_widgets.contentSplitter->sizes().value(1);
                    if (paneSize > 0)
                        m_detailsPaneHeight = paneSize;
                    else if (m_showDetailsPaneAction)
                        m_showDetailsPaneAction->setChecked(false);
                });
    }

    if (m_widgets.mainSplitter) {
        connect(m_widgets.mainSplitter, &QSplitter::splitterMoved,
                this, [this](int, int index) {
                    if (index != 1)
                        return;
                    const int sidebarSize =
                        m_widgets.mainSplitter->sizes().value(0);
                    if (sidebarSize > 0)
                        m_filterSidebarWidth = sidebarSize;
                    else if (m_showFilterSidebarAction)
                        m_showFilterSidebarAction->setChecked(false);
                });
    }

    if (m_widgets.toolBar) {
        connect(m_widgets.toolBar, &QToolBar::visibilityChanged,
                this, [this](bool visible) {
                    if (m_showToolBarAction
                        && m_showToolBarAction->isChecked() != visible) {
                        const QSignalBlocker blocker(m_showToolBarAction);
                        m_showToolBarAction->setChecked(visible);
                    }
                    QSettings().setValue(
                        SettingsKeys::MainWindowToolBarVisible, visible);
                });
    }

    restoreWidgetState();
}

void WindowLayoutController::restoreWidgetState()
{
    QSettings settings;
    const bool toolBarVisible =
        settings.value(SettingsKeys::MainWindowToolBarVisible, true).toBool();
    const bool statusBarVisible =
        settings.value(SettingsKeys::MainWindowStatusBarVisible, true).toBool();
    const bool detailsVisible =
        settings.value(SettingsKeys::MainWindowDetailsPaneVisible, true).toBool();
    const bool sidebarVisible =
        settings.value(SettingsKeys::MainWindowFilterSidebarVisible, true).toBool();
    m_detailsPaneHeight = qMax(
        1, settings.value(SettingsKeys::MainWindowDetailsPaneHeight,
                          DefaultDetailsPaneHeight).toInt());
    m_filterSidebarWidth = qMax(
        1, settings.value(SettingsKeys::MainWindowFilterSidebarWidth,
                          DefaultFilterSidebarWidth).toInt());
    const Qt::ToolButtonStyle style = toolButtonStyleFromVariant(
        settings.value(SettingsKeys::MainWindowToolBarStyle,
                       static_cast<int>(DefaultToolBarButtonStyle)),
        DefaultToolBarButtonStyle);

    const auto setChecked = [](QAction *action, bool checked) {
        if (!action)
            return;
        const QSignalBlocker blocker(action);
        action->setChecked(checked);
    };
    setChecked(m_showToolBarAction, toolBarVisible);
    setChecked(m_showStatusBarAction, statusBarVisible);
    setChecked(m_showDetailsPaneAction, detailsVisible);
    setChecked(m_showFilterSidebarAction, sidebarVisible);

    if (m_widgets.toolBar)
        m_widgets.toolBar->setVisible(toolBarVisible);
    if (m_widgets.statusBar)
        m_widgets.statusBar->setVisible(statusBarVisible);
    if (m_widgets.detailsPane)
        m_widgets.detailsPane->setVisible(detailsVisible);
    if (m_widgets.filterSidebar)
        m_widgets.filterSidebar->setVisible(sidebarVisible);

    if (detailsVisible && m_widgets.contentSplitter) {
        QTimer::singleShot(0, this, [this]() {
            const int total = m_widgets.contentSplitter->height();
            m_widgets.contentSplitter->setSizes(
                {qMax(1, total - m_detailsPaneHeight), m_detailsPaneHeight});
        });
    }
    if (sidebarVisible && m_widgets.mainSplitter) {
        QTimer::singleShot(0, this, [this]() {
            const int total = m_widgets.mainSplitter->width();
            m_widgets.mainSplitter->setSizes(
                {m_filterSidebarWidth,
                 qMax(1, total - m_filterSidebarWidth)});
        });
    }
    applyToolBarButtonStyle(style);
}

bool WindowLayoutController::restoreWindowState()
{
    if (!m_window)
        return false;
    const QByteArray state =
        QSettings().value(SettingsKeys::MainWindowState).toByteArray();
    return !state.isEmpty() && m_window->restoreState(state, 2);
}

void WindowLayoutController::saveState() const
{
    if (!m_window)
        return;

    QSettings settings;
    settings.setValue(SettingsKeys::MainWindowToolBarVisible,
                      m_widgets.toolBar && !m_widgets.toolBar->isHidden());
    settings.setValue(SettingsKeys::MainWindowStatusBarVisible,
                      m_widgets.statusBar && !m_widgets.statusBar->isHidden());
    settings.setValue(SettingsKeys::MainWindowDetailsPaneVisible,
                      detailsPaneVisible());
    settings.setValue(SettingsKeys::MainWindowDetailsPaneHeight,
                      m_detailsPaneHeight);
    settings.setValue(SettingsKeys::MainWindowFilterSidebarVisible,
                      m_showFilterSidebarAction
                          && m_showFilterSidebarAction->isChecked());
    settings.setValue(SettingsKeys::MainWindowFilterSidebarWidth,
                      m_filterSidebarWidth);
    if (m_widgets.toolBar) {
        settings.setValue(
            SettingsKeys::MainWindowToolBarStyle,
            static_cast<int>(m_widgets.toolBar->toolButtonStyle()));
    }
    settings.setValue(SettingsKeys::MainWindowState,
                      m_window->saveState(2));
    settings.sync();
}

QMenu *WindowLayoutController::viewMenu() const
{
    return m_viewMenu;
}

bool WindowLayoutController::detailsPaneVisible() const
{
    return m_showDetailsPaneAction
           && m_showDetailsPaneAction->isChecked()
           && m_widgets.detailsPane
           && !m_widgets.detailsPane->isHidden();
}

void WindowLayoutController::setToolBarVisible(bool visible)
{
    if (m_widgets.toolBar)
        m_widgets.toolBar->setVisible(visible);
    QSettings().setValue(SettingsKeys::MainWindowToolBarVisible, visible);
}

void WindowLayoutController::setStatusBarVisible(bool visible)
{
    if (m_widgets.statusBar)
        m_widgets.statusBar->setVisible(visible);
    QSettings().setValue(SettingsKeys::MainWindowStatusBarVisible, visible);
}

void WindowLayoutController::setDetailsPaneVisible(bool visible)
{
    if (!m_widgets.detailsPane || !m_widgets.contentSplitter)
        return;

    if (!visible) {
        const int height = m_widgets.contentSplitter->sizes().value(1);
        if (height > 0)
            m_detailsPaneHeight = height;
        m_widgets.detailsPane->hide();
    } else {
        m_widgets.detailsPane->show();
        const int total = m_widgets.contentSplitter->height();
        m_widgets.contentSplitter->setSizes(
            {qMax(1, total - m_detailsPaneHeight), m_detailsPaneHeight});
    }

    QSettings settings;
    settings.setValue(SettingsKeys::MainWindowDetailsPaneVisible, visible);
    settings.setValue(SettingsKeys::MainWindowDetailsPaneHeight,
                      m_detailsPaneHeight);
    emit detailsPaneVisibilityChanged(visible);
}

void WindowLayoutController::setFilterSidebarVisible(bool visible)
{
    if (!m_widgets.filterSidebar || !m_widgets.mainSplitter)
        return;

    if (!visible) {
        const int width = m_widgets.mainSplitter->sizes().value(0);
        if (width > 0)
            m_filterSidebarWidth = width;
        m_widgets.filterSidebar->hide();
    } else {
        m_widgets.filterSidebar->show();
        const int total = m_widgets.mainSplitter->width();
        m_widgets.mainSplitter->setSizes(
            {m_filterSidebarWidth, qMax(1, total - m_filterSidebarWidth)});
    }

    QSettings settings;
    settings.setValue(SettingsKeys::MainWindowFilterSidebarVisible, visible);
    settings.setValue(SettingsKeys::MainWindowFilterSidebarWidth,
                      m_filterSidebarWidth);
}

void WindowLayoutController::setToolBarButtonStyle(QAction *action)
{
    if (!action || !m_widgets.toolBar)
        return;
    const Qt::ToolButtonStyle style = toolButtonStyleFromVariant(
        action->data(), m_widgets.toolBar->toolButtonStyle());
    applyToolBarButtonStyle(style);
    QSettings().setValue(SettingsKeys::MainWindowToolBarStyle,
                         static_cast<int>(style));
}

void WindowLayoutController::applyToolBarButtonStyle(
    Qt::ToolButtonStyle style)
{
    if (!isSupportedToolBarButtonStyle(style))
        style = DefaultToolBarButtonStyle;
    if (m_widgets.toolBar)
        m_widgets.toolBar->setToolButtonStyle(style);
    syncToolBarStyleActions(m_toolBarStyleActionGroup, style);
}

QMenu *WindowLayoutController::createToolBarPopupMenu()
{
    auto *menu = new QMenu(m_window);
    QAction *visibilityAction = menu->addAction(
        m_widgets.toolBar && m_widgets.toolBar->isVisible()
            ? tr("Hide Toolbar") : tr("Show Toolbar"));
    connect(visibilityAction, &QAction::triggered, this, [this]() {
        if (m_showToolBarAction && m_widgets.toolBar) {
            m_showToolBarAction->setChecked(
                !m_widgets.toolBar->isVisible());
        }
    });

    menu->addSeparator();
    QMenu *styleMenu = menu->addMenu(tr("Toolbar Display"));
    auto *styleGroup = new QActionGroup(styleMenu);
    styleGroup->setExclusive(true);
    const auto addStyle =
        [this, styleMenu, styleGroup](const QString &text,
                                     Qt::ToolButtonStyle style) {
            QAction *action = styleMenu->addAction(text);
            action->setCheckable(true);
            action->setData(static_cast<int>(style));
            action->setChecked(
                m_widgets.toolBar
                && m_widgets.toolBar->toolButtonStyle() == style);
            styleGroup->addAction(action);
        };
    addStyle(tr("Icons Only"), Qt::ToolButtonIconOnly);
    addStyle(tr("Icons and Text"), Qt::ToolButtonTextBesideIcon);
    addStyle(tr("Text Only"), Qt::ToolButtonTextOnly);
    connect(styleGroup, &QActionGroup::triggered,
            this, &WindowLayoutController::setToolBarButtonStyle);
    return menu;
}
