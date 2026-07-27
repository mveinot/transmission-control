#ifndef SESSIONSETTINGSDIALOG_H
#define SESSIONSETTINGSDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QString>

class QLabel;
class QPushButton;
class QWidget;
struct TorrentBackendCapabilities;

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
    void configureBackend(const QString &backendName,
                          const TorrentBackendCapabilities &capabilities);
    QJsonObject changedSettings() const;

public slots:
    void setPortTestRunning();
    void setPortTestResult(bool portIsOpen, const QString &ipProtocol = QString());
    void setPortTestFailed(const QString &message);
    void setBlocklistUpdateResult(int ruleCount);
    void setBlocklistUpdateFailed(const QString &message);

signals:
    void portTestRequested();
    void blocklistUpdateRequested(const QJsonObject &changedSettings);

private:
    Ui::SessionSettingsDialog *ui = nullptr;

    QJsonObject originalSettings;
    QPushButton *testPortButton = nullptr;
    QLabel *portTestResultLabel = nullptr;
    QWidget *portTestContainer = nullptr;
    bool supportsDisabledEncryption = false;

    void populateEncryptionCombo(const QString &currentValue);
    void setComboCurrentData(const QString &value);
    void updateEnabledStates();
    void setupPortTestControls();
    bool blocklistUpdateRunning = false;
};

#endif // SESSIONSETTINGSDIALOG_H
