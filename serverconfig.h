#ifndef SERVERCONFIG_H
#define SERVERCONFIG_H

#include "foldermapping.h"

#include <QDialog>
#include <QList>
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
    void removeSelectedServer();
    bool saveSelectedServer();
    void configureFolderMappings();
    int defaultServerIndex = -1;
    void setSelectedServerAsDefault();
};

#endif // SERVERCONFIG_H