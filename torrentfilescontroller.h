#ifndef TORRENTFILESCONTROLLER_H
#define TORRENTFILESCONTROLLER_H

#include <QObject>
#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QPoint>
#include <QString>
#include <functional>

#include "foldermapping.h"

class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
class rpc_client;

class TorrentFilesController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentFilesController(QTreeWidget *fileTreeWidget,
                                    rpc_client *client,
                                    QWidget *dialogParent,
                                    QObject *parent = nullptr);

    void setup();
    void clear();
    void populate(const QJsonArray &files,
                  const QJsonArray &wanted,
                  const QJsonArray &priorities);
    void setTorrentContext(int torrentId, const QString &downloadDir);
    void setFolderMappingsProvider(
        std::function<QList<FolderMapping>()> folderMappingsProvider);

signals:
    void statusMessageRequested(const QString &message, int timeoutMs);
    void torrentDetailsRefreshRequested(int torrentId);

private:
    enum FileTreeColumn {
        FileNameColumn = 0,
        FilePriorityColumn,
        FileSizeColumn,
        FileDoneColumn,
        FilePercentColumn,
        FileColumnCount
    };

    enum FileTreeRole {
        FileKindRole = Qt::UserRole,
        FileIndexRole,
        FileWantedRole,
        FilePriorityRole
    };

    static bool jsonValueToBool(const QJsonValue &value,
                                bool defaultValue = false);
    static QString priorityToString(int priority);

    QTreeWidgetItem *findOrCreateTopLevelItem(const QString &name);
    QTreeWidgetItem *findOrCreateChild(QTreeWidgetItem *parent,
                                       const QString &name,
                                       bool isFolder);
    void updateFolderPriorityStates();
    void updateFolderPriorityState(QTreeWidgetItem *item);
    QList<int> fileIndicesForItem(QTreeWidgetItem *item) const;
    QList<int> selectedFileIndicesForContextItem(QTreeWidgetItem *item) const;
    QString torrentPathForFileTreeItem(QTreeWidgetItem *item) const;
    void renameFileTreeItem(QTreeWidgetItem *item);
    void showContextMenu(const QPoint &pos);
    void openFileFromContextMenu(const QList<int> &fileIndices);
    void openContainingFolderFromContextMenu(const QList<int> &fileIndices);
    bool resolveMappedLocalPathForSingleFile(const QList<int> &fileIndices,
                                             const QString &dialogTitle,
                                             QString *localPath,
                                             QString *remotePath = nullptr,
                                             bool requireFileExists = true);
    QString mapRemotePathToLocalPath(const QString &remotePath,
                                     const QList<FolderMapping> &mappings) const;
    void setSelectedFilesPriorityState(int priority, bool wanted);

    QTreeWidget *fileTreeWidget = nullptr;
    rpc_client *client = nullptr;
    QWidget *dialogParent = nullptr;
    int torrentId = -1;
    QString torrentDownloadDir;
    QHash<int, QString> torrentFilePaths;
    std::function<QList<FolderMapping>()> folderMappingsProvider;
};

#endif // TORRENTFILESCONTROLLER_H
