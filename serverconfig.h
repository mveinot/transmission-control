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

class ServerConfig : public QDialog
{
    Q_OBJECT

public:
    explicit ServerConfig(QWidget *parent = nullptr);
    ~ServerConfig();

private:
    struct TransmissionServer
    {
        QString name;
        QString rpcUrl;
        QString username;
        QString password;
        QList<FolderMapping> folderMappings;
    };

    Ui::ServerConfig *ui;

    QVector<TransmissionServer> servers;
    QStringListModel *serverListModel = nullptr;

    int currentServerIndex() const;

    void loadServers();
    void saveServers();

    void refreshServerList();
    void loadServerIntoEditor(int index);
    void saveEditorToServer(int index);
    void clearEditor();
    void setEditorEnabled(bool enabled);
    void updateFolderMappingsSummary();

    void addServer();
    void importServerFromFile();
    void exportSelectedServer();
    void removeSelectedServer();
    bool saveSelectedServer();
    void configureFolderMappings();
    int defaultServerIndex = -1;
    void setSelectedServerAsDefault();

    QJsonObject serverToJson(const TransmissionServer &server, bool includePassword) const;
    bool serverFromJson(const QJsonObject &object, TransmissionServer *server, QString *errorMessage) const;
    QString suggestedExportFileName(const TransmissionServer &server) const;
    QString uniqueServerName(const QString &baseName) const;
};

#endif // SERVERCONFIG_H