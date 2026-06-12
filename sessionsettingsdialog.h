#ifndef SESSIONSETTINGSDIALOG_H
#define SESSIONSETTINGSDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QString>

namespace Ui {
class SessionSettingsDialog;
}

class SessionSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SessionSettingsDialog(QWidget *parent = nullptr);
    ~SessionSettingsDialog() override;

    void setSessionSettings(const QJsonObject &settings);
    QJsonObject changedSettings() const;

private:
    Ui::SessionSettingsDialog *ui = nullptr;

    QJsonObject originalSettings;

    void populateEncryptionCombo(const QString &currentValue);
    void setComboCurrentData(const QString &value);
    void updateEnabledStates();
};

#endif // SESSIONSETTINGSDIALOG_H