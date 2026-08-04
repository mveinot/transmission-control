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
    QString m_initialAppearance;

    void populateLanguageOptions();
    void loadSettings();
    void saveSettings();
    QString selectedAppearance() const;
    void updateNotificationOptionAvailability();
    void refreshDefaultHandlerStatus();
    void requestDefaultHandler(bool magnetLinks);
};

#endif // APPSETTINGS_H
