#include "serverconfig.h"
#include "ui_serverconfig.h"

#include <QItemSelectionModel>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>

ServerConfig::ServerConfig(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ServerConfig)
    , serverListModel(new QStringListModel(this))
{
    ui->setupUi(this);
    setFixedSize(size());

    setWindowTitle("Server Configuration");

    ui->listServers->setModel(serverListModel);

    setEditorEnabled(false);
    ui->buttonSaveServer->setEnabled(false);
    ui->buttonRemoveServer->setEnabled(false);
    ui->buttonSetDefaultServer->setEnabled(false);

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
                    return;
                }

                loadServerIntoEditor(index);
                setEditorEnabled(true);
                ui->buttonSaveServer->setEnabled(true);
                ui->buttonRemoveServer->setEnabled(true);
                ui->buttonSetDefaultServer->setEnabled(true);
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

    connect(ui->scButtonBox, &QDialogButtonBox::accepted,
            this, [this]() {
                saveSelectedServer();
                saveServers();
                accept();
            });

    connect(ui->scButtonBox, &QDialogButtonBox::rejected,
            this, [this]() {
                reject();
            });

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
    }

    settings.endArray();

    const int current = currentServerIndex();
    if (current >= 0)
        settings.setValue("servers/currentIndex", current);

    settings.setValue("servers/defaultIndex", defaultServerIndex);

    settings.sync();
}

void ServerConfig::refreshServerList()
{
    QStringList names;

    for (int i = 0; i < servers.size(); ++i) {
        QString name = servers.at(i).name.trimmed();

        if (name.isEmpty())
            name = "(unnamed server)";

        if (i == defaultServerIndex)
            name += " (default)";

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
}

void ServerConfig::setEditorEnabled(bool enabled)
{
    ui->editServerName->setEnabled(enabled);
    ui->editServerUrl->setEnabled(enabled);
    ui->editServerUsername->setEnabled(enabled);
    ui->editServerPassword->setEnabled(enabled);
}

void ServerConfig::addServer()
{
    TransmissionServer server;
    server.name = "New Server";
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

    ui->editServerName->setFocus();
    ui->editServerName->selectAll();
}

void ServerConfig::removeSelectedServer()
{
    const int index = currentServerIndex();

    if (index < 0)
        return;

    const QString name = servers.at(index).name.isEmpty()
                             ? QStringLiteral("(unnamed server)")
                             : servers.at(index).name;

    const auto result = QMessageBox::question(
        this,
        "Remove Server",
        QString("Remove server \"%1\"?").arg(name),
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
        return;
    }

    const int nextIndex = qMin(index, servers.size() - 1);
    ui->listServers->setCurrentIndex(serverListModel->index(nextIndex, 0));
}

void ServerConfig::saveSelectedServer()
{
    const int index = currentServerIndex();

    if (index < 0)
        return;

    if (ui->editServerName->text().trimmed().isEmpty()) {
        QMessageBox::warning(
            this,
            "Server Configuration",
            "Server name cannot be empty."
            );
        return;
    }

    if (ui->editServerUrl->text().trimmed().isEmpty()) {
        QMessageBox::warning(
            this,
            "Server Configuration",
            "RPC URL cannot be empty."
            );
        return;
    }

    saveEditorToServer(index);
    saveServers();
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