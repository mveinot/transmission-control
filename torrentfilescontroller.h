#ifndef TORRENTFILESCONTROLLER_H
#define TORRENTFILESCONTROLLER_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include <memory>

#include "foldermapping.h"
#include "torrentdomain.h"
#include "torrentkey.h"

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
class TorrentBackend;
class TableColumnController;
class TablePlaceholderController;

// Maintains the selected torrent's file records and projects them as either a
// hierarchy or a sortable flat view. File indices remain RPC-authoritative.
class TorrentFilesController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentFilesController(QTreeWidget *fileTreeWidget,
                                    QLineEdit *filterEdit,
                                    TorrentBackend *client,
                                    QWidget *dialogParent,
                                    QObject *parent = nullptr);
    ~TorrentFilesController() override;

    void setup();
    void clear();
    void populate(const TorrentFiles &files);
    void setTorrentContext(TorrentKey torrentKey, const QString &downloadDir);
    void setFolderMappingsProvider(
        std::function<QList<FolderMapping>()> folderMappingsProvider);
    void saveViewState() const;
    void restoreViewState();
    void setLoading();
    bool hasSelection() const;

signals:
    void statusMessageRequested(const QString &message, int timeoutMs);
    void selectionActiveChanged(bool active);

private:
    enum FileTreeColumn {
        FileNameColumn = 0,
        FilePriorityColumn,
        FileSizeColumn,
        FileDoneColumn,
        FileRemainingColumn,
        FilePercentColumn,
        FileColumnCount
    };

    enum FileTreeRole {
        FileKindRole = Qt::UserRole,
        FileIndexRole,
        FileWantedRole,
        FilePriorityRole
    };

    struct FileRecord {
        int index = -1;
        QString path;
        qint64 length = 0;
        qint64 bytesCompleted = 0;
        bool wanted = true;
        int priority = 0;
    };

    enum class FileTransferVisualState {
        Complete,
        Transferring,
        Skipped,
        Mixed,
        Unknown
    };

    static QString priorityToString(int priority);

    void setItemVisualState(QTreeWidgetItem *item, FileTransferVisualState state);
    void rebuildView();
    void populateTreeView();
    void populateFlatView();
    void populateFileItem(QTreeWidgetItem *item, const FileRecord &record, bool showFullPath);
    void handleSortChanged(int logicalIndex, Qt::SortOrder order);
    void updateFolderVisualStates();
    FileTransferVisualState updateFolderVisualState(QTreeWidgetItem *item);

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
    void updateSelectionState();
    void finishPendingFileMutation(const QString &method);
    void applyFilter(const QString &text);
    bool filterItem(QTreeWidgetItem *item,
                    const QString &text,
                    bool ancestorMatches);

    QTreeWidget *fileTreeWidget = nullptr;
    QLineEdit *filterEdit = nullptr;
    TorrentBackend *client = nullptr;
    QWidget *dialogParent = nullptr;
    TorrentKey torrentKey;
    QString torrentDownloadDir;
    QHash<int, QString> torrentFilePaths;
    QVector<FileRecord> fileRecords;
    int sortColumn = FileNameColumn;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    bool rebuildingView = false;
    bool selectionActive = false;
    QStringList pendingFileMutationMethods;
    std::function<QList<FolderMapping>()> folderMappingsProvider;
    std::unique_ptr<TableColumnController> columnController;
    std::unique_ptr<TablePlaceholderController> placeholderController;
};

#endif // TORRENTFILESCONTROLLER_H
