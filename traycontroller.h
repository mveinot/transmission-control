#ifndef TRAYCONTROLLER_H
#define TRAYCONTROLLER_H

#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

class QAction;
class QMainWindow;
class QMenu;
class QCloseEvent;

// Encapsulates tray lifetime and close-to-tray policy so MainWindow can remain
// agnostic about platform availability and user tray preferences.
class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QMainWindow *window, QObject *parent = nullptr);

    void setup();
    void setTorrentGlobalActions(QAction *startAllAction, QAction *stopAllAction);
    void setTorrentCounts(int totalCount, int downloadingCount, int seedingCount);
    void applySettings();
    void showMainWindow();
    void quitApplication();
    bool handleCloseEvent(QCloseEvent *event);
    bool isTrayAvailable() const;
    bool isTrayVisible() const;
    bool showNotification(const QString &title,
                          const QString &message,
                          int millisecondsTimeoutHint = 5000);

signals:
    void statusMessageRequested(const QString &message, int timeoutMs = 3000);

private:
    bool trayIconEnabled() const;
    bool shouldCloseToTray() const;
    void updateTrayIconVisibility();
    void updateTorrentCountActions();
    void rebuildTrayMenu();

    QMainWindow *m_window = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_showAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_startAllAction = nullptr;
    QAction *m_stopAllAction = nullptr;
    QAction *m_totalCountAction = nullptr;
    QAction *m_downloadingCountAction = nullptr;
    QAction *m_seedingCountAction = nullptr;
    int m_totalCount = 0;
    int m_downloadingCount = 0;
    int m_seedingCount = 0;
    bool m_reallyQuit = false;
};

#endif // TRAYCONTROLLER_H
