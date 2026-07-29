#include "serversetupwizard.h"

#include "serverconfig.h"
#include "settingskeys.h"

#include <QColor>
#include <QComboBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QPalette>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWizardPage>

namespace {
constexpr int ConnectionTestTimeoutMs = 10000;

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
    , m_network(new QNetworkAccessManager(this))
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
        tr("You will need the Web UI or RPC address for a Transmission or "
           "qBittorrent server. Credentials are optional when the server "
           "does not require authentication."),
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

    m_backendCombo = new QComboBox(detailsPage);
    m_backendCombo->addItem(tr("Transmission"), QStringLiteral("transmission"));
    m_backendCombo->addItem(tr("qBittorrent"), QStringLiteral("qbittorrent"));
    form->addRow(tr("Server type:"), m_backendCombo);

    m_nameEdit = new QLineEdit(detailsPage);
    m_nameEdit->setPlaceholderText(tr("Home server"));
    form->addRow(tr("Name:"), m_nameEdit);

    m_urlLabel = new QLabel(detailsPage);
    m_urlEdit = new QLineEdit(detailsPage);
    form->addRow(m_urlLabel, m_urlEdit);

    m_usernameEdit = new QLineEdit(detailsPage);
    form->addRow(tr("Username:"), m_usernameEdit);

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
    connect(m_network, &QNetworkAccessManager::finished,
            this, &ServerSetupWizard::handleConnectionTestReply);

    const auto clearTestResult = [this]() {
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
    QSettings settings;
    const int count = settings.beginReadArray(SettingsKeys::ServersArray);
    bool found = false;

    for (int index = 0; index < count && !found; ++index) {
        settings.setArrayIndex(index);
        found = !settings.value(SettingsKeys::ServerRpcUrl)
                     .toString().trimmed().isEmpty();
    }

    settings.endArray();
    return found;
}

QString ServerSetupWizard::backendType() const
{
    return m_backendCombo->currentData().toString();
}

void ServerSetupWizard::updateBackendFields()
{
    const bool qBittorrent = backendType() == QStringLiteral("qbittorrent");
    m_urlLabel->setText(qBittorrent ? tr("Web UI URL:") : tr("RPC URL:"));
    m_urlEdit->setPlaceholderText(
        qBittorrent ? QStringLiteral("http://server:8080")
                    : QStringLiteral("http://server:9091/transmission/rpc"));
    m_testStatus->clear();
}

void ServerSetupWizard::accept()
{
    const QString name = m_nameEdit->text().trimmed();
    const QString endpoint = m_urlEdit->text().trimmed();
    const QUrl url(endpoint);

    if (name.isEmpty() || endpoint.isEmpty()
        || !url.isValid()
        || (url.scheme() != QStringLiteral("http")
            && url.scheme() != QStringLiteral("https"))) {
        setTestResult(tr("Enter a name and a valid HTTP or HTTPS server URL."),
                      false);
        return;
    }

    QSettings settings;
    int serverIndex = 0;

    if (m_appendToExisting) {
        serverIndex = settings.beginReadArray(SettingsKeys::ServersArray);
        settings.endArray();
    } else {
        settings.remove(SettingsKeys::ServersArray);
    }

    settings.beginWriteArray(SettingsKeys::ServersArray, serverIndex + 1);
    settings.setArrayIndex(serverIndex);
    settings.setValue(SettingsKeys::ServerName, name);
    settings.setValue(SettingsKeys::ServerBackendType, backendType());
    settings.setValue(SettingsKeys::ServerRpcUrl, endpoint);
    settings.setValue(SettingsKeys::ServerUsername,
                      m_usernameEdit->text().trimmed());
    settings.setValue(SettingsKeys::ServerPassword, m_passwordEdit->text());
    settings.endArray();
    if (!m_appendToExisting)
        settings.setValue(SettingsKeys::ServersDefaultIndex, serverIndex);
    settings.setValue(SettingsKeys::ServersCurrentIndex, serverIndex);
    settings.sync();

    m_savedServerIndex = serverIndex;
    QWizard::accept();
}

void ServerSetupWizard::importServer()
{
    ServerConfig importer(this);
    if (!importer.importServerFromFile())
        return;

    QSettings settings;
    m_savedServerIndex = settings.beginReadArray(SettingsKeys::ServersArray) - 1;
    settings.endArray();
    QWizard::accept();
}

void ServerSetupWizard::testConnection()
{
    const QUrl url(m_urlEdit->text().trimmed());
    if (!url.isValid()
        || (url.scheme() != QStringLiteral("http")
            && url.scheme() != QStringLiteral("https"))) {
        setTestResult(tr("Enter a valid HTTP or HTTPS server URL."), false);
        return;
    }

    m_testButton->setEnabled(false);
    m_testStatus->setText(tr("Testing…"));
    m_transmissionRetry = false;

    if (backendType() == QStringLiteral("qbittorrent")) {
        QUrl loginUrl = url;
        QString path = loginUrl.path();
        if (path.endsWith(QLatin1Char('/')))
            path.chop(1);
        loginUrl.setPath(path + QStringLiteral("/api/v2/auth/login"));

        QNetworkRequest request(loginUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/x-www-form-urlencoded"));
        request.setTransferTimeout(ConnectionTestTimeoutMs);

        QUrlQuery form;
        form.addQueryItem(QStringLiteral("username"),
                          m_usernameEdit->text().trimmed());
        form.addQueryItem(QStringLiteral("password"), m_passwordEdit->text());
        QNetworkReply *reply =
            m_network->post(request, form.toString(QUrl::FullyEncoded).toUtf8());
        reply->setProperty("planetaryBackend", QStringLiteral("qbittorrent"));
    } else {
        sendTransmissionTest();
    }
}

void ServerSetupWizard::sendTransmissionTest(const QByteArray &sessionToken)
{
    QNetworkRequest request(QUrl(m_urlEdit->text().trimmed()));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setTransferTimeout(ConnectionTestTimeoutMs);

    if (!sessionToken.isEmpty())
        request.setRawHeader("X-Transmission-Session-Id", sessionToken);

    const QString username = m_usernameEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    if (!username.isEmpty() || !password.isEmpty()) {
        request.setRawHeader(
            "Authorization",
            QByteArrayLiteral("Basic ")
                + (username + QLatin1Char(':') + password).toUtf8().toBase64());
    }

    const QJsonObject payload{
        {QStringLiteral("method"), QStringLiteral("session-get")},
        {QStringLiteral("arguments"),
         QJsonObject{{QStringLiteral("fields"),
                      QJsonArray{QStringLiteral("version")}}}}};
    QNetworkReply *reply =
        m_network->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    reply->setProperty("planetaryBackend", QStringLiteral("transmission"));
}

void ServerSetupWizard::handleConnectionTestReply(QNetworkReply *reply)
{
    if (!reply)
        return;

    const QString backend =
        reply->property("planetaryBackend").toString();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray response = reply->readAll().trimmed();

    if (backend == QStringLiteral("transmission")
        && status == 409
        && !m_transmissionRetry) {
        const QByteArray sessionToken =
            reply->rawHeader("X-Transmission-Session-Id");
        reply->deleteLater();

        if (!sessionToken.isEmpty()) {
            m_transmissionRetry = true;
            sendTransmissionTest(sessionToken);
            return;
        }
    }

    const bool networkSucceeded =
        reply->error() == QNetworkReply::NoError;
    bool succeeded = networkSucceeded && status >= 200 && status < 300;

    if (backend == QStringLiteral("qbittorrent")) {
        // Current qBittorrent versions return "Ok."; some releases return an
        // empty HTTP 204 response after establishing the authenticated cookie.
        succeeded = succeeded
                    && (response == QByteArrayLiteral("Ok.")
                        || (status == 204 && response.isEmpty()));
    }

    if (succeeded) {
        setTestResult(tr("Connection successful."), true);
    } else if (status == 401 || status == 403
               || response == QByteArrayLiteral("Fails.")) {
        setTestResult(tr("Authentication failed."), false);
    } else {
        const QString reason =
            reply->errorString().trimmed().isEmpty()
                ? tr("The server returned HTTP %1.").arg(status)
                : reply->errorString();
        setTestResult(tr("Connection failed: %1").arg(reason), false);
    }

    reply->deleteLater();
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
