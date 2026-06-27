#ifndef TRAYCONTROLLER_H
#define TRAYCONTROLLER_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QSystemTrayIcon>
#include <QVector>

class QAction;
class QMainWindow;
class QMenu;
class QCloseEvent;
class torrent;

class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QMainWindow *window, QObject *parent = nullptr);

    void setup();
    void applySettings();
    void showMainWindow();
    void quitApplication();
    bool handleCloseEvent(QCloseEvent *event);
    void processTorrentList(const QVector<torrent> &torrents);

    bool isTrayAvailable() const;
    bool isTrayVisible() const;

signals:
    void statusMessageRequested(const QString &message, int timeoutMs = 3000);

private:
    bool trayIconEnabled() const;
    bool trayNotificationsEnabled() const;
    void updateTrayIconVisibility();
    void showNotification(const QString &title,
                          const QString &message,
                          QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                          int millisecondsTimeoutHint = 5000);
    static bool isTorrentCompleteForNotification(const torrent &torrentItem);

    QMainWindow *m_window = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    bool m_reallyQuit = false;
    bool m_completedTorrentNotificationBaselineLoaded = false;
    QSet<int> m_knownCompletedTorrentIds;
};

#endif // TRAYCONTROLLER_H
