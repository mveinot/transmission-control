#include "serverconfig.h"
#include "ui_serverconfig.h"

#include "foldermappingsdialog.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>

ServerConfig::ServerConfig(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ServerConfig)
    , serverListModel(new QStringListModel(this))
{
    ui->setupUi(this);
    setFixedSize(size());

    setWindowTitle(tr("Server Configuration"));

    ui->listServers->setModel(serverListModel);

    setEditorEnabled(false);
    ui->buttonSaveServer->setEnabled(false);
    ui->buttonRemoveServer->setEnabled(false);
    ui->buttonSetDefaultServer->setEnabled(false);
    ui->buttonExportServer->setEnabled(false);
    ui->buttonConfigureFolderMappings->setEnabled(false);
    updateFolderMappingsSummary();

    loadServers();
    refreshServerList();

    auto *addServerMenu = new QMenu(ui->buttonAddServer);
    addServerMenu->addAction(tr("New Server"), this, [this]() {
        addServer();
    });
    addServerMenu->addAction(tr("From File…"), this, [this]() {
        importServerFromFile();
    });
    ui->buttonAddServer->setMenu(addServerMenu);

    connect(ui->listServers->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            [this](const QModelIndex &current, const QModelIndex &) {
                const int index = current.row();

                if (index < 0 || index >= servers.size()) {
                    clearEditor();
                    setEditorEnabled(false);
                    ui->buttonSaveServer->setEnabled(false);
                    ui->buttonRemoveServer->setEnabled(false);
                    ui->buttonSetDefaultServer->setEnabled(false);
                    ui->buttonExportServer->setEnabled(false);
                    ui->buttonConfigureFolderMappings->setEnabled(false);
                    updateFolderMappingsSummary();
                    return;
                }

                loadServerIntoEditor(index);
                setEditorEnabled(true);
                ui->buttonSaveServer->setEnabled(true);
                ui->buttonRemoveServer->setEnabled(true);
                ui->buttonSetDefaultServer->setEnabled(true);
                ui->buttonExportServer->setEnabled(true);
                ui->buttonConfigureFolderMappings->setEnabled(true);
                updateFolderMappingsSummary();
            });

    connect(ui->buttonRemoveServer, &QPushButton::clicked,
            this, [this]() {
                removeSelectedServer();
            });

    connect(ui->buttonSetDefaultServer, &QPushButton::clicked,
            this, [this]() {
                setSelectedServerAsDefault();
            });

    connect(ui->buttonExportServer, &QPushButton::clicked,
            this, [this]() {
                exportSelectedServer();
            });

    connect(ui->buttonSaveServer, &QPushButton::clicked,
            this, [this]() {
                saveSelectedServer();
            });

    connect(ui->buttonConfigureFolderMappings, &QPushButton::clicked,
            this, [this]() {
                configureFolderMappings();
            });

    connect(ui->scButtonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (saveSelectedServer())
            accept();
    });

    connect(ui->scButtonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    if (!servers.isEmpty()) {
        ui->listServers->setCurrentIndex(serverListModel->index(0, 0));
    }
}

ServerConfig::~ServerConfig()
{
    delete ui;
}

int ServerConfig::currentServerIndex() const
{
    const QModelIndex index = ui->listServers->currentIndex();

    if (!index.isValid())
        return -1;

    const int row = index.row();

    if (row < 0 || row >= servers.size())
        return -1;

    return row;
}

void ServerConfig::loadServers()
{
    servers.clear();

    QSettings settings;

    const int count = settings.beginReadArray("servers");

    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);

        TransmissionServer server;
        server.name = settings.value("name").toString();
        server.rpcUrl = settings.value("rpcUrl").toString();
        server.username = settings.value("username").toString();
        server.password = settings.value("password").toString();

        const int mappingCount = settings.beginReadArray("folderMappings");

        for (int mappingIndex = 0; mappingIndex < mappingCount; ++mappingIndex) {
            settings.setArrayIndex(mappingIndex);

            FolderMapping mapping;
            mapping.remotePath = settings.value("remotePath").toString().trimmed();
            mapping.localPath = settings.value("localPath").toString().trimmed();

            if (!mapping.remotePath.isEmpty() || !mapping.localPath.isEmpty())
                server.folderMappings.append(mapping);
        }

        settings.endArray();

        if (!server.name.trimmed().isEmpty() ||
            !server.rpcUrl.trimmed().isEmpty()) {
            servers.append(server);
        }
    }

    settings.endArray();

    defaultServerIndex = settings.value("servers/defaultIndex", -1).toInt();

    if (defaultServerIndex >= servers.size())
        defaultServerIndex = servers.isEmpty() ? -1 : 0;
}

void ServerConfig::saveServers()
{
    QSettings settings;

    settings.beginWriteArray("servers");

    for (int i = 0; i < servers.size(); ++i) {
        settings.setArrayIndex(i);

        settings.setValue("name", servers.at(i).name);
        settings.setValue("rpcUrl", servers.at(i).rpcUrl);
        settings.setValue("username", servers.at(i).username);
        settings.setValue("password", servers.at(i).password);

        settings.beginWriteArray("folderMappings");

        const QList<FolderMapping> mappings = servers.at(i).folderMappings;

        for (int mappingIndex = 0; mappingIndex < mappings.size(); ++mappingIndex) {
            settings.setArrayIndex(mappingIndex);
            settings.setValue("remotePath", mappings.at(mappingIndex).remotePath);
            settings.setValue("localPath", mappings.at(mappingIndex).localPath);
        }

        settings.endArray();
    }

    settings.endArray();

    /*
    const int current = currentServerIndex();
    if (current >= 0)
        settings.setValue("servers/currentIndex", current);
*/

    settings.setValue("servers/defaultIndex", defaultServerIndex);

    settings.sync();
}

void ServerConfig::refreshServerList()
{
    QStringList names;

    for (int i = 0; i < servers.size(); ++i) {
        QString name = servers.at(i).name.trimmed();

        if (name.isEmpty())
            name = tr("(unnamed server)");

        if (i == defaultServerIndex)
            name += tr(" (default)");

        names.append(name);
    }

    serverListModel->setStringList(names);
}

void ServerConfig::loadServerIntoEditor(int index)
{
    if (index < 0 || index >= servers.size())
        return;

    const TransmissionServer &server = servers.at(index);

    QSignalBlocker blockerName(ui->editServerName);
    QSignalBlocker blockerUrl(ui->editServerUrl);
    QSignalBlocker blockerUsername(ui->editServerUsername);
    QSignalBlocker blockerPassword(ui->editServerPassword);

    ui->editServerName->setText(server.name);
    ui->editServerUrl->setText(server.rpcUrl);
    ui->editServerUsername->setText(server.username);
    ui->editServerPassword->setText(server.password);

    updateFolderMappingsSummary();
}

void ServerConfig::saveEditorToServer(int index)
{
    if (index < 0 || index >= servers.size())
        return;

    servers[index].name = ui->editServerName->text().trimmed();
    servers[index].rpcUrl = ui->editServerUrl->text().trimmed();
    servers[index].username = ui->editServerUsername->text().trimmed();
    servers[index].password = ui->editServerPassword->text();

    refreshServerList();

    ui->listServers->setCurrentIndex(serverListModel->index(index, 0));
}

void ServerConfig::clearEditor()
{
    ui->editServerName->clear();
    ui->editServerUrl->clear();
    ui->editServerUsername->clear();
    ui->editServerPassword->clear();
    updateFolderMappingsSummary();
}

void ServerConfig::setEditorEnabled(bool enabled)
{
    ui->editServerName->setEnabled(enabled);
    ui->editServerUrl->setEnabled(enabled);
    ui->editServerUsername->setEnabled(enabled);
    ui->editServerPassword->setEnabled(enabled);
    ui->buttonConfigureFolderMappings->setEnabled(enabled);
}

void ServerConfig::addServer()
{
    TransmissionServer server;
    server.name = tr("New Server");
    server.rpcUrl = "http://server:9091/transmission/rpc";
    server.username = "";
    server.password = "";

    servers.append(server);

    if (defaultServerIndex < 0)
        defaultServerIndex = 0;

    refreshServerList();

    const int newIndex = servers.size() - 1;
    ui->listServers->setCurrentIndex(serverListModel->index(newIndex, 0));

    setEditorEnabled(true);
    ui->buttonSaveServer->setEnabled(true);
    ui->buttonRemoveServer->setEnabled(true);
    ui->buttonSetDefaultServer->setEnabled(true);
    ui->buttonExportServer->setEnabled(true);
    ui->buttonConfigureFolderMappings->setEnabled(true);
    updateFolderMappingsSummary();

    ui->editServerName->setFocus();
    ui->editServerName->selectAll();
}


QJsonObject ServerConfig::serverToJson(const TransmissionServer &server, bool includePassword) const
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), server.name);
    object.insert(QStringLiteral("rpcUrl"), server.rpcUrl);
    object.insert(QStringLiteral("username"), server.username);

    if (includePassword)
        object.insert(QStringLiteral("password"), server.password);

    QJsonArray mappings;

    for (const FolderMapping &mapping : server.folderMappings) {
        QJsonObject mappingObject;
        mappingObject.insert(QStringLiteral("remotePath"), mapping.remotePath);
        mappingObject.insert(QStringLiteral("localPath"), mapping.localPath);
        mappings.append(mappingObject);
    }

    object.insert(QStringLiteral("folderMappings"), mappings);

    return object;
}

bool ServerConfig::serverFromJson(const QJsonObject &object,
                                  TransmissionServer *server,
                                  QString *errorMessage) const
{
    if (!server)
        return false;

    TransmissionServer parsed;
    parsed.name = object.value(QStringLiteral("name")).toString().trimmed();
    parsed.rpcUrl = object.value(QStringLiteral("rpcUrl")).toString().trimmed();
    parsed.username = object.value(QStringLiteral("username")).toString();
    parsed.password = object.value(QStringLiteral("password")).toString();

    if (parsed.name.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("The server file does not contain a server name.");
        return false;
    }

    if (parsed.rpcUrl.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("The server file does not contain an RPC URL.");
        return false;
    }

    const QJsonArray mappings = object.value(QStringLiteral("folderMappings")).toArray();

    for (const QJsonValue &value : mappings) {
        if (!value.isObject())
            continue;

        const QJsonObject mappingObject = value.toObject();

        FolderMapping mapping;
        mapping.remotePath = mappingObject.value(QStringLiteral("remotePath")).toString().trimmed();
        mapping.localPath = mappingObject.value(QStringLiteral("localPath")).toString().trimmed();

        if (!mapping.remotePath.isEmpty() || !mapping.localPath.isEmpty())
            parsed.folderMappings.append(mapping);
    }

    *server = parsed;
    return true;
}

QString ServerConfig::suggestedExportFileName(const TransmissionServer &server) const
{
    QString name = server.name.trimmed().toLower();

    if (name.isEmpty())
        name = QStringLiteral("planetary-server");

    name.replace(QRegularExpression(QStringLiteral("[^a-z0-9._-]+")), QStringLiteral("-"));
    name.replace(QRegularExpression(QStringLiteral("-+")), QStringLiteral("-"));
    name = name.trimmed();

    while (name.startsWith(QLatin1Char('-')) || name.startsWith(QLatin1Char('.')))
        name.remove(0, 1);

    while (name.endsWith(QLatin1Char('-')) || name.endsWith(QLatin1Char('.')))
        name.chop(1);

    if (name.isEmpty())
        name = QStringLiteral("planetary-server");

    return name + QStringLiteral(".planetary-server.json");
}

QString ServerConfig::uniqueServerName(const QString &baseName) const
{
    QString candidate = baseName.trimmed();

    if (candidate.isEmpty())
        candidate = tr("Imported Server");

    auto nameExists = [this](const QString &name) {
        for (const TransmissionServer &server : servers) {
            if (server.name.compare(name, Qt::CaseInsensitive) == 0)
                return true;
        }

        return false;
    };

    if (!nameExists(candidate))
        return candidate;

    const QString original = candidate;

    for (int suffix = 2; suffix < 1000; ++suffix) {
        candidate = tr("%1 (%2)").arg(original).arg(suffix);

        if (!nameExists(candidate))
            return candidate;
    }

    return tr("%1 (imported)").arg(original);
}

void ServerConfig::importServerFromFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Import Server"),
        QString(),
        tr("Planetary server files (*.planetary-server.json *.json);;JSON files (*.json);;All files (*)")
        );

    if (filePath.isEmpty())
        return;

    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(
            this,
            tr("Import Server Failed"),
            tr("Could not read server file:\n%1").arg(filePath)
            );
        return;
    }

    const QByteArray data = file.readAll();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        QMessageBox::warning(
            this,
            tr("Import Server Failed"),
            tr("The selected file is not valid JSON:\n%1").arg(parseError.errorString())
            );
        return;
    }

    const QJsonObject root = document.object();

    if (root.value(QStringLiteral("application")).toString() != QStringLiteral("Planetary") ||
        root.value(QStringLiteral("kind")).toString() != QStringLiteral("server")) {
        QMessageBox::warning(
            this,
            tr("Import Server Failed"),
            tr("The selected file does not appear to be a Planetary server export.")
            );
        return;
    }

    const QJsonObject serverObject = root.value(QStringLiteral("server")).toObject();

    if (serverObject.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Import Server Failed"),
            tr("The selected file does not contain a server configuration.")
            );
        return;
    }

    TransmissionServer server;
    QString errorMessage;

    if (!serverFromJson(serverObject, &server, &errorMessage)) {
        QMessageBox::warning(
            this,
            tr("Import Server Failed"),
            errorMessage.isEmpty() ? tr("The selected file contains an invalid server configuration.") : errorMessage
            );
        return;
    }

    server.name = uniqueServerName(server.name);

    const int current = currentServerIndex();

    if (current >= 0)
        saveEditorToServer(current);

    servers.append(server);

    if (defaultServerIndex < 0)
        defaultServerIndex = 0;

    refreshServerList();

    const int newIndex = servers.size() - 1;
    ui->listServers->setCurrentIndex(serverListModel->index(newIndex, 0));

    setEditorEnabled(true);
    ui->buttonSaveServer->setEnabled(true);
    ui->buttonRemoveServer->setEnabled(true);
    ui->buttonSetDefaultServer->setEnabled(true);
    ui->buttonExportServer->setEnabled(true);
    ui->buttonConfigureFolderMappings->setEnabled(true);
    updateFolderMappingsSummary();
    saveServers();
}

void ServerConfig::exportSelectedServer()
{
    const int index = currentServerIndex();

    if (index < 0 || index >= servers.size())
        return;

    if (!saveSelectedServer())
        return;

    const TransmissionServer &server = servers.at(index);

    QMessageBox passwordPrompt(this);
    passwordPrompt.setIcon(QMessageBox::Warning);
    passwordPrompt.setWindowTitle(tr("Export Server"));
    passwordPrompt.setText(tr("Export server \"%1\"?").arg(server.name));
    passwordPrompt.setInformativeText(
        tr("The export can include this server's saved password. Only include it if the file will be stored securely.")
        );

    QPushButton *includePasswordButton = passwordPrompt.addButton(tr("Include Password"), QMessageBox::AcceptRole);
    QPushButton *omitPasswordButton = passwordPrompt.addButton(tr("Omit Password"), QMessageBox::DestructiveRole);
    passwordPrompt.addButton(QMessageBox::Cancel);
    passwordPrompt.setDefaultButton(omitPasswordButton);
    passwordPrompt.exec();

    if (passwordPrompt.clickedButton() == nullptr ||
        passwordPrompt.standardButton(passwordPrompt.clickedButton()) == QMessageBox::Cancel) {
        return;
    }

    const bool includePassword = (passwordPrompt.clickedButton() == includePasswordButton);

    QString selectedPath = QFileDialog::getSaveFileName(
        this,
        tr("Export Server"),
        suggestedExportFileName(server),
        tr("Planetary server files (*.planetary-server.json);;JSON files (*.json);;All files (*)")
        );

    if (selectedPath.isEmpty())
        return;

    QFileInfo fileInfo(selectedPath);

    if (fileInfo.suffix().isEmpty())
        selectedPath += QStringLiteral(".planetary-server.json");

    QJsonObject root;
    root.insert(QStringLiteral("application"), QStringLiteral("Planetary"));
    root.insert(QStringLiteral("kind"), QStringLiteral("server"));
    root.insert(QStringLiteral("formatVersion"), 1);
    root.insert(QStringLiteral("server"), serverToJson(server, includePassword));

    QFile file(selectedPath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(
            this,
            tr("Export Server Failed"),
            tr("Could not write server file:\n%1").arg(selectedPath)
            );
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
}

void ServerConfig::removeSelectedServer()
{
    const int index = currentServerIndex();

    if (index < 0)
        return;

    const QString name = servers.at(index).name.isEmpty()
                             ? tr("(unnamed server)")
                             : servers.at(index).name;

    const auto result = QMessageBox::question(
        this,
        tr("Remove Server"),
        tr("Remove server \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
        );

    if (result != QMessageBox::Yes)
        return;

    servers.removeAt(index);

    if (servers.isEmpty()) {
        defaultServerIndex = -1;
    } else if (index == defaultServerIndex) {
        defaultServerIndex = qMin(index, servers.size() - 1);
    } else if (index < defaultServerIndex) {
        --defaultServerIndex;
    }

    refreshServerList();

    if (servers.isEmpty()) {
        clearEditor();
        setEditorEnabled(false);
        ui->buttonSaveServer->setEnabled(false);
        ui->buttonRemoveServer->setEnabled(false);
        ui->buttonSetDefaultServer->setEnabled(false);
        ui->buttonExportServer->setEnabled(false);
        ui->buttonConfigureFolderMappings->setEnabled(false);
        updateFolderMappingsSummary();
        return;
    }

    const int nextIndex = qMin(index, servers.size() - 1);
    ui->listServers->setCurrentIndex(serverListModel->index(nextIndex, 0));
}

bool ServerConfig::saveSelectedServer()
{
    const int index = currentServerIndex();

    if (index < 0)
        return true;

    if (ui->editServerName->text().trimmed().isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Server Configuration"),
            tr("Server name cannot be empty.")
            );
        return false;
    }

    if (ui->editServerUrl->text().trimmed().isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Server Configuration"),
            tr("RPC URL cannot be empty.")
            );
        return false;
    }

    saveEditorToServer(index);
    updateFolderMappingsSummary();
    saveServers();

    return true;
}

void ServerConfig::updateFolderMappingsSummary()
{
    const int index = currentServerIndex();

    if (index < 0 || index >= servers.size()) {
        ui->labelFolderMappingsSummary->setText(tr("No server selected"));
        return;
    }

    const int count = servers.at(index).folderMappings.size();

    if (count == 0) {
        ui->labelFolderMappingsSummary->setText(tr("No folder mappings configured"));
        return;
    }

    ui->labelFolderMappingsSummary->setText(
        tr("%n folder mapping(s) configured", nullptr, count)
    );
}

void ServerConfig::configureFolderMappings()
{
    const int index = currentServerIndex();

    if (index < 0 || index >= servers.size())
        return;

    saveEditorToServer(index);

    FolderMappingsDialog dialog(this);
    dialog.setServerName(servers.at(index).name);
    dialog.setMappings(servers.at(index).folderMappings);

    if (dialog.exec() != QDialog::Accepted)
        return;

    servers[index].folderMappings = dialog.mappings();
    updateFolderMappingsSummary();
    refreshServerList();
    ui->listServers->setCurrentIndex(serverListModel->index(index, 0));
}

void ServerConfig::setSelectedServerAsDefault()
{
    const int index = currentServerIndex();

    if (index < 0)
        return;

    saveEditorToServer(index);

    defaultServerIndex = index;

    refreshServerList();
    ui->listServers->setCurrentIndex(serverListModel->index(index, 0));

    saveServers();
}