#ifndef TRAYCONTROLLER_H
#define TRAYCONTROLLER_H

#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

class QAction;
class QMainWindow;
class QMenu;
class QCloseEvent;

class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QMainWindow *window, QObject *parent = nullptr);

    void setup();
    void setTorrentGlobalActions(QAction *startAllAction, QAction *stopAllAction);
    void applySettings();
    void showMainWindow();
    void quitApplication();
    bool handleCloseEvent(QCloseEvent *event);
    bool isTrayAvailable() const;
    bool isTrayVisible() const;

signals:
    void statusMessageRequested(const QString &message, int timeoutMs = 3000);

private:
    bool trayIconEnabled() const;
    bool shouldCloseToTray() const;
    void updateTrayIconVisibility();
    void rebuildTrayMenu();

    QMainWindow *m_window = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_showAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_startAllAction = nullptr;
    QAction *m_stopAllAction = nullptr;
    bool m_reallyQuit = false;
};

#endif // TRAYCONTROLLER_H
