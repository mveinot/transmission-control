#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QDialog>

namespace Ui {
class AppSettings;
}

// Edits application-local behavior. Transmission session settings are handled
// separately because they belong to the currently selected remote server.
class AppSettings : public QDialog
{
    Q_OBJECT

public:
    explicit AppSettings(QWidget *parent = nullptr);
    ~AppSettings();

signals:
    void clearWatchFolderHistoryRequested();
    void testNotificationRequested();
    void testExternalCommandRequested(const QString &executable, const QString &arguments);

private:
    Ui::AppSettings *ui;

    void loadSettings();
    void saveSettings();
    void updateNotificationOptionAvailability();
};

#endif // APPSETTINGS_H
