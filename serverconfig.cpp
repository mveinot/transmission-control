#include "serverconfig.h"
#include "ui_serverconfig.h"

#include "foldermappingsdialog.h"

#include <QItemSelectionModel>
#include <QMessageBox>
#include <QPushButton>
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
    ui->buttonConfigureFolderMappings->setEnabled(false);
    updateFolderMappingsSummary();

    loadServers();
    refreshServerList();

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
                    ui->buttonConfigureFolderMappings->setEnabled(false);
                    updateFolderMappingsSummary();
                    return;
                }

                loadServerIntoEditor(index);
                setEditorEnabled(true);
                ui->buttonSaveServer->setEnabled(true);
                ui->buttonRemoveServer->setEnabled(true);
                ui->buttonSetDefaultServer->setEnabled(true);
                ui->buttonConfigureFolderMappings->setEnabled(true);
                updateFolderMappingsSummary();
            });

    connect(ui->buttonAddServer, &QPushButton::clicked,
            this, [this]() {
                addServer();
            });

    connect(ui->buttonRemoveServer, &QPushButton::clicked,
            this, [this]() {
                removeSelectedServer();
            });

    connect(ui->buttonSetDefaultServer, &QPushButton::clicked,
            this, [this]() {
                setSelectedServerAsDefault();
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
    ui->buttonConfigureFolderMappings->setEnabled(true);
    updateFolderMappingsSummary();

    ui->editServerName->setFocus();
    ui->editServerName->selectAll();
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