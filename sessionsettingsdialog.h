#ifndef SESSIONSETTINGSDIALOG_H
#define SESSIONSETTINGSDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QString>

class QLabel;
class QPushButton;

namespace Ui {
class SessionSettingsDialog;
}

// Edits a server-provided session snapshot and emits only fields whose values
// differ from that baseline.
class SessionSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SessionSettingsDialog(QWidget *parent = nullptr);
    ~SessionSettingsDialog() override;

    void setSessionSettings(const QJsonObject &settings);
    QJsonObject changedSettings() const;

public slots:
    void setPortTestRunning();
    void setPortTestResult(bool portIsOpen, const QString &ipProtocol = QString());
    void setPortTestFailed(const QString &message);

signals:
    void portTestRequested();

private:
    Ui::SessionSettingsDialog *ui = nullptr;

    QJsonObject originalSettings;
    QPushButton *testPortButton = nullptr;
    QLabel *portTestResultLabel = nullptr;

    void populateEncryptionCombo(const QString &currentValue);
    void setComboCurrentData(const QString &value);
    void updateEnabledStates();
    void setupPortTestControls();
};

#endif // SESSIONSETTINGSDIALOG_H
