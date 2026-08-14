#ifndef TORRENTFILESCONTROLLER_H
#define TORRENTFILESCONTROLLER_H

#include "foldermapping.h"
#include "torrentdomain.h"
#include "torrentkey.h"

#include <QObject>
#include <QHash>
#include <QPoint>
#include <QStringList>
#include <functional>
#include <memory>

class QLineEdit;
class QModelIndex;
class QSortFilterProxyModel;
class QTreeView;
class QWidget;
class TorrentBackend;
class TorrentFileModel;
class TableColumnController;
class TablePlaceholderController;

// Coordinates the model-backed files view and translates selected model rows
// into backend file-index operations. The model owns all presentation data.
class TorrentFilesController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentFilesController(QTreeView *fileTreeWidget,
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
    static QString priorityToString(int priority);

    void handleSortChanged(int logicalIndex, Qt::SortOrder order);
    QList<int> fileIndicesForIndex(const QModelIndex &index) const;
    QList<int> selectedFileIndicesForContextIndex(const QModelIndex &index) const;
    QString torrentPathForIndex(const QModelIndex &index) const;
    void renameFileIndex(const QModelIndex &index);
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
    void clearSelectionState();
    void finishPendingFileMutation(const QString &method);
    void applyFilter(const QString &text);
    QModelIndex sourceIndex(const QModelIndex &viewIndex) const;

    QTreeView *fileTreeWidget = nullptr;
    QLineEdit *filterEdit = nullptr;
    TorrentBackend *client = nullptr;
    QWidget *dialogParent = nullptr;
    TorrentKey torrentKey;
    QString torrentDownloadDir;
    QHash<int, QString> torrentFilePaths;
    int sortColumn = 0;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    bool selectionActive = false;
    QStringList pendingFileMutationMethods;
    std::function<QList<FolderMapping>()> folderMappingsProvider;
    std::unique_ptr<TableColumnController> columnController;
    std::unique_ptr<TablePlaceholderController> placeholderController;
    TorrentFileModel *fileModel = nullptr;
    QSortFilterProxyModel *proxyModel = nullptr;
};

#endif // TORRENTFILESCONTROLLER_H
