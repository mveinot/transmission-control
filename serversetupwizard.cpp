#include "serversetupwizard.h"

#include "serverconfig.h"
#include "serverconnectionprobe.h"
#include "serverprofile.h"

#include <QColor>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPalette>
#include <QUrl>
#include <QVBoxLayout>
#include <QWizardPage>

#include <algorithm>

namespace {
class ServerDetailsPage final : public QWizardPage
{
public:
    explicit ServerDetailsPage(QWidget *parent = nullptr)
        : QWizardPage(parent)
    {
    }

    void registerRequiredField(const QString &name, QWidget *widget,
                               const char *property, const char *signal)
    {
        registerField(name + QLatin1Char('*'), widget, property, signal);
    }
};
}

ServerSetupWizard::ServerSetupWizard(bool appendToExisting, QWidget *parent)
    : QWizard(parent)
    , m_connectionProbe(new ServerConnectionProbe(this))
    , m_appendToExisting(appendToExisting)
{
    setWindowTitle(tr("Set Up Planetary"));
#ifdef Q_OS_WIN
    // AeroStyle bypasses the active widget style on composited Windows
    // desktops and can paint light page/button surfaces under a forced dark
    // palette. ModernStyle follows the Qt widget palette consistently.
    setWizardStyle(QWizard::ModernStyle);
#endif
    setOption(QWizard::NoBackButtonOnStartPage);
    resize(620, 390);

    auto *welcomePage = new QWizardPage(this);
    welcomePage->setTitle(tr("Welcome to Planetary"));
    welcomePage->setSubTitle(
        appendToExisting
            ? tr("Add another torrent server to Planetary.")
            : tr("Planetary connects to an existing torrent server. "
                 "Add your first server to begin."));
    auto *welcomeLayout = new QVBoxLayout(welcomePage);
    auto *welcomeText = new QLabel(
        tr("You will need the Web UI or RPC address for a Transmission, "
           "qBittorrent, or Deluge server. Credentials are optional when "
           "the server does not require authentication."),
        welcomePage);
    welcomeText->setWordWrap(true);
    welcomeLayout->addWidget(welcomeText);
    welcomeLayout->addStretch();
    auto *importButton =
        new QPushButton(tr("Import Saved Server…"), welcomePage);
    auto *importLayout = new QHBoxLayout;
    importLayout->addWidget(importButton);
    importLayout->addStretch();
    welcomeLayout->addLayout(importLayout);
    addPage(welcomePage);

    auto *detailsPage = new ServerDetailsPage(this);
    detailsPage->setTitle(tr("Torrent Server"));
    detailsPage->setSubTitle(
        tr("Enter the connection details supplied by your torrent server."));
    auto *detailsLayout = new QVBoxLayout(detailsPage);
    auto *form = new QFormLayout;
    // Form-layout defaults differ by platform; connection fields should use
    // all horizontal space available when the wizard is resized.
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_backendCombo = new QComboBox(detailsPage);
    m_backendCombo->addItem(tr("Transmission"), QStringLiteral("transmission"));
    m_backendCombo->addItem(tr("qBittorrent"), QStringLiteral("qbittorrent"));
    m_backendCombo->addItem(tr("Deluge"), QStringLiteral("deluge"));
    form->addRow(tr("Server type:"), m_backendCombo);

    m_nameEdit = new QLineEdit(detailsPage);
    m_nameEdit->setPlaceholderText(tr("Home server"));
    form->addRow(tr("Name:"), m_nameEdit);

    m_urlLabel = new QLabel(detailsPage);
    m_urlEdit = new QLineEdit(detailsPage);
    form->addRow(m_urlLabel, m_urlEdit);

    m_usernameLabel = new QLabel(tr("Username:"), detailsPage);
    m_usernameEdit = new QLineEdit(detailsPage);
    m_usernameLabel->setBuddy(m_usernameEdit);
    form->addRow(m_usernameLabel, m_usernameEdit);

    m_passwordEdit = new QLineEdit(detailsPage);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Password:"), m_passwordEdit);

    detailsLayout->addLayout(form);

    auto *testLayout = new QHBoxLayout;
    m_testButton = new QPushButton(tr("Test Connection"), detailsPage);
    m_testStatus = new QLabel(detailsPage);
    m_testStatus->setWordWrap(true);
    testLayout->addWidget(m_testButton);
    testLayout->addWidget(m_testStatus, 1);
    detailsLayout->addLayout(testLayout);
    detailsLayout->addStretch();

    detailsPage->registerRequiredField(
        QStringLiteral("serverName"), m_nameEdit, "text", SIGNAL(textChanged(QString)));
    detailsPage->registerRequiredField(
        QStringLiteral("serverUrl"), m_urlEdit, "text", SIGNAL(textChanged(QString)));
    addPage(detailsPage);

    connect(m_backendCombo, &QComboBox::currentIndexChanged,
            this, &ServerSetupWizard::updateBackendFields);
    connect(m_testButton, &QPushButton::clicked,
            this, &ServerSetupWizard::testConnection);
    connect(importButton, &QPushButton::clicked,
            this, &ServerSetupWizard::importServer);
    connect(m_connectionProbe, &ServerConnectionProbe::connectionSucceeded,
            this, [this](const QUrl &workingUrl, bool adjusted) {
                if (adjusted) {
                    const auto result = QMessageBox::question(
                        this,
                        tr("Use Working Server URL"),
                        tr("Connection succeeded using:\n%1\n\n"
                           "Use this corrected URL?")
                            .arg(workingUrl.toString()),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::Yes);
                    if (result == QMessageBox::Yes)
                        m_urlEdit->setText(workingUrl.toString());
                }
                setTestResult(
                    adjusted
                        ? tr("Connection successful using the suggested URL.")
                        : tr("Connection successful."),
                    true);
            });
    connect(m_connectionProbe, &ServerConnectionProbe::connectionFailed,
            this, [this](const QString &message,
                         ServerConnectionProbe::FailureKind) {
                setTestResult(message, false);
            });

    const auto clearTestResult = [this]() {
        m_connectionProbe->cancel();
        m_testButton->setEnabled(true);
        m_testStatus->clear();
    };
    connect(m_nameEdit, &QLineEdit::textChanged, this, clearTestResult);
    connect(m_urlEdit, &QLineEdit::textChanged, this, clearTestResult);
    connect(m_usernameEdit, &QLineEdit::textChanged, this, clearTestResult);
    connect(m_passwordEdit, &QLineEdit::textChanged, this, clearTestResult);

    updateBackendFields();
}

int ServerSetupWizard::savedServerIndex() const
{
    return m_savedServerIndex;
}

bool ServerSetupWizard::hasConfiguredServer()
{
    const QVector<ServerProfile> profiles =
        ServerProfileRepository().loadProfiles();
    return std::any_of(profiles.cbegin(), profiles.cend(),
                       [](const ServerProfile &profile) {
                           return profile.isValid();
                       });
}

QString ServerSetupWizard::backendType() const
{
    return m_backendCombo->currentData().toString();
}

void ServerSetupWizard::updateBackendFields()
{
    m_connectionProbe->cancel();
    m_testButton->setEnabled(true);
    const bool qBittorrent = backendType() == QStringLiteral("qbittorrent");
    const bool deluge = backendType() == QStringLiteral("deluge");
    m_urlLabel->setText(qBittorrent || deluge ? tr("Web UI URL:")
                                              : tr("RPC URL:"));
    if (qBittorrent)
        m_urlEdit->setPlaceholderText(QStringLiteral("http://server:8080"));
    else if (deluge)
        m_urlEdit->setPlaceholderText(QStringLiteral("http://server:8112"));
    else
        m_urlEdit->setPlaceholderText(
            QStringLiteral("http://server:9091/transmission/rpc"));
    m_usernameEdit->setVisible(!deluge);
    m_usernameLabel->setVisible(!deluge);
    m_passwordEdit->setPlaceholderText(
        deluge ? tr("Required") : tr("Optional"));
    m_testStatus->clear();
}

void ServerSetupWizard::accept()
{
    const QString name = m_nameEdit->text().trimmed();
    QString endpoint = m_urlEdit->text().trimmed();
    if (!endpoint.contains(QStringLiteral("://")) && !endpoint.isEmpty()) {
        const QString suggested = QStringLiteral("http://") + endpoint;
        const auto result = QMessageBox::question(
            this,
            tr("Add URL Scheme"),
            tr("The server URL has no HTTP or HTTPS scheme. Use %1?")
                .arg(suggested),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (result != QMessageBox::Yes)
            return;
        endpoint = suggested;
        m_urlEdit->setText(endpoint);
    }
    const QUrl url(endpoint);

    if (name.isEmpty() || endpoint.isEmpty()
        || !url.isValid()
        || url.host().isEmpty()
        || (url.scheme() != QStringLiteral("http")
            && url.scheme() != QStringLiteral("https"))) {
        setTestResult(tr("Enter a name and a valid HTTP or HTTPS server URL."),
                      false);
        return;
    }

    ServerProfileRepository repository;
    QVector<ServerProfile> profiles =
        m_appendToExisting ? repository.loadProfiles()
                           : QVector<ServerProfile>{};
    const int serverIndex = profiles.size();

    ServerProfile profile;
    profile.settingsIndex = serverIndex;
    profile.name = name;
    profile.backendType = backendType();
    profile.rpcUrl = endpoint;
    profile.username = m_usernameEdit->text().trimmed();
    profile.password = m_passwordEdit->text();
    profiles.append(profile);
    repository.saveProfiles(profiles);

    if (!m_appendToExisting)
        repository.setDefaultIndex(serverIndex);
    repository.setCurrentIndex(serverIndex);

    m_savedServerIndex = serverIndex;
    QWizard::accept();
}

void ServerSetupWizard::importServer()
{
    ServerConfig importer(this);
    if (!importer.importServerFromFile())
        return;

    m_savedServerIndex =
        ServerProfileRepository().loadProfiles().size() - 1;
    QWizard::accept();
}

void ServerSetupWizard::testConnection()
{
    QString endpoint = m_urlEdit->text().trimmed();
    if (!endpoint.contains(QStringLiteral("://")) && !endpoint.isEmpty()) {
        const QString suggested = QStringLiteral("http://") + endpoint;
        const auto result = QMessageBox::question(
            this,
            tr("Add URL Scheme"),
            tr("The server URL has no HTTP or HTTPS scheme. Test using %1?")
                .arg(suggested),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (result != QMessageBox::Yes)
            return;
        endpoint = suggested;
        m_urlEdit->setText(endpoint);
    }

    const QUrl url(endpoint);
    if (!url.isValid()
        || url.host().isEmpty()
        || (url.scheme() != QStringLiteral("http")
            && url.scheme() != QStringLiteral("https"))) {
        setTestResult(tr("Enter a valid HTTP or HTTPS server URL."), false);
        return;
    }

    m_testButton->setEnabled(false);
    m_testStatus->setText(tr("Testing…"));
    m_connectionProbe->start(
        backendType(), url, m_usernameEdit->text(), m_passwordEdit->text());
}

void ServerSetupWizard::setTestResult(const QString &message, bool success)
{
    m_testStatus->setText(message);
    const QColor color =
        success ? m_testStatus->palette().color(QPalette::Link)
                : QColor(180, 55, 55);
    m_testStatus->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }").arg(color.name()));
    m_testButton->setEnabled(true);
}
