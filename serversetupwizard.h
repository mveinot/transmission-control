#ifndef SERVERSETUPWIZARD_H
#define SERVERSETUPWIZARD_H

#include <QWizard>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class ServerConnectionProbe;

// Collects the minimum information required for Planetary's first server.
// The definition is persisted only when the wizard is finished.
class ServerSetupWizard : public QWizard
{
    Q_OBJECT

public:
    explicit ServerSetupWizard(bool appendToExisting = false,
                               QWidget *parent = nullptr);

    static bool hasConfiguredServer();
    int savedServerIndex() const;

protected:
    void accept() override;

private:
    QComboBox *m_backendCombo = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLabel *m_urlLabel = nullptr;
    QLabel *m_usernameLabel = nullptr;
    QLabel *m_testStatus = nullptr;
    QPushButton *m_testButton = nullptr;
    ServerConnectionProbe *m_connectionProbe = nullptr;
    bool m_appendToExisting = false;
    int m_savedServerIndex = -1;

    QString backendType() const;
    void updateBackendFields();
    void importServer();
    void testConnection();
    void setTestResult(const QString &message, bool success);
};

#endif // SERVERSETUPWIZARD_H
