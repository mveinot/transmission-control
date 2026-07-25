#ifndef SERVERCONFIG_H
#define SERVERCONFIG_H

#include "foldermapping.h"

#include <QDialog>
#include <QList>
#include <QJsonObject>
#include <QStringListModel>
#include <QVector>

namespace Ui {
class ServerConfig;
}

// Transactional editor for the persisted server array. Changes remain local to
// the dialog until acceptance, including imported definitions and mappings.
class ServerConfig : public QDialog
{
    Q_OBJECT

public:
    explicit ServerConfig(QWidget *parent = nullptr);
    ~ServerConfig();

private:
    struct ServerDefinition
    {
        QString backendType = QStringLiteral("transmission");
        QString name;
        QString rpcUrl;
        QString username;
        QString password;
        QList<FolderMapping> folderMappings;
    };

    Ui::ServerConfig *ui;

    QVector<ServerDefinition> servers;
    QStringListModel *serverListModel = nullptr;

    int currentServerIndex() const;

    void loadServers();
    void saveServers();

    void refreshServerList();
    void loadServerIntoEditor(int index);
    void saveEditorToServer(int index);
    void clearEditor();
    void setEditorEnabled(bool enabled);
    void updateEditorForServerType();
    void updateFolderMappingsSummary();

    void addServer();
    void importServerFromFile();
    void exportSelectedServer();
    void removeSelectedServer();
    bool saveSelectedServer();
    void configureFolderMappings();
    int defaultServerIndex = -1;
    void setSelectedServerAsDefault();

    QJsonObject serverToJson(const ServerDefinition &server, bool includePassword) const;
    bool serverFromJson(const QJsonObject &object, ServerDefinition *server, QString *errorMessage) const;
    QString suggestedExportFileName(const ServerDefinition &server) const;
    QString uniqueServerName(const QString &baseName) const;
};

#endif // SERVERCONFIG_H
