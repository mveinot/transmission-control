#include "torrentfilescontroller.h"

#include "percentfilldelegate.h"
#include "settingskeys.h"
#include "tablecolumncontroller.h"
#include "tableplaceholdercontroller.h"
#include "torrentbackend.h"
#include "torrentfilemodel.h"

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QUrl>
#include <algorithm>
#include <utility>

TorrentFilesController::TorrentFilesController(QTreeView *fileTreeWidget,
                                               QLineEdit *filterEdit,
                                               TorrentBackend *client,
                                               QWidget *dialogParent,
                                               QObject *parent)
    : QObject(parent)
    , fileTreeWidget(fileTreeWidget)
    , filterEdit(filterEdit)
    , client(client)
    , dialogParent(dialogParent)
{
}

TorrentFilesController::~TorrentFilesController() = default;

void TorrentFilesController::setup()
{
    if (!fileTreeWidget)
        return;

    fileModel = new TorrentFileModel(this);
    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(fileModel);
    proxyModel->setSortRole(TorrentFileModel::SortRole);
    proxyModel->setFilterKeyColumn(TorrentFileModel::NameColumn);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setRecursiveFilteringEnabled(true);
    proxyModel->setAutoAcceptChildRows(true);
    proxyModel->setDynamicSortFilter(true);
    fileTreeWidget->setModel(proxyModel);

    fileTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    fileTreeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    fileTreeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    fileTreeWidget->setAlternatingRowColors(false);
    fileTreeWidget->setRootIsDecorated(true);
    fileTreeWidget->setSortingEnabled(true);
    fileTreeWidget->setIconSize(QSize(16, 16));
    fileTreeWidget->setItemDelegateForColumn(
        TorrentFileModel::PercentColumn,
        new PercentFillDelegate(TorrentFileModel::PercentColumn,
                                TorrentFileModel::SortRole,
                                fileTreeWidget));

    columnController = std::make_unique<TableColumnController>(
        fileTreeWidget->header(),
        QString::fromLatin1(SettingsKeys::FileTreeHeaderState),
        QString::fromLatin1(SettingsKeys::FileTreeVisibleColumns),
        QVector<TableColumnController::ColumnDefinition> {
            { TorrentFileModel::NameColumn, QStringLiteral("name"), true, false, true },
            { TorrentFileModel::PriorityColumn, QStringLiteral("priority"), true, true, false },
            { TorrentFileModel::SizeColumn, QStringLiteral("size"), true, true, false },
            { TorrentFileModel::DoneColumn, QStringLiteral("done"), true, true, false },
            { TorrentFileModel::RemainingColumn, QStringLiteral("remaining"), false, true, false },
            { TorrentFileModel::PercentColumn, QStringLiteral("completed"), true, true, false },
        }, this);
    columnController->setup();

    placeholderController = std::make_unique<TablePlaceholderController>(fileTreeWidget, this);
    placeholderController->setMessage(tr("No torrent selected."));

    connect(fileTreeWidget, &QTreeView::customContextMenuRequested,
            this, &TorrentFilesController::showContextMenu);
    connect(fileTreeWidget->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() { updateSelectionState(); });
    connect(fileTreeWidget->header(), &QHeaderView::sortIndicatorChanged,
            this, &TorrentFilesController::handleSortChanged);
    connect(client, &TorrentBackend::commandSucceeded,
            this, &TorrentFilesController::finishPendingFileMutation);
    connect(client, &TorrentBackend::commandFailed,
            this, [this](const QString &method, const QString &) {
                finishPendingFileMutation(method);
            });

    fileTreeWidget->sortByColumn(TorrentFileModel::NameColumn, Qt::AscendingOrder);

    if (filterEdit) {
        filterEdit->setClearButtonEnabled(true);
        filterEdit->setPlaceholderText(tr("Search files..."));
        connect(filterEdit, &QLineEdit::textChanged,
                this, &TorrentFilesController::applyFilter);
    }
}

bool TorrentFilesController::hasSelection() const { return selectionActive; }

void TorrentFilesController::updateSelectionState()
{
    if (!fileTreeWidget || !fileTreeWidget->selectionModel())
        return;
    const bool active = fileTreeWidget->selectionModel()->hasSelection();
    if (selectionActive == active)
        return;
    selectionActive = active;
    emit selectionActiveChanged(active);
}

void TorrentFilesController::clearSelectionState()
{
    if (fileTreeWidget)
        fileTreeWidget->clearSelection();

    /*
     * A model reset invalidates selected indexes, but QItemSelectionModel is
     * not required to emit selectionChanged for that invalidation. Keep the
     * polling gate authoritative so a new torrent cannot inherit suppression
     * from the file selection belonging to the previous torrent.
     */
    if (!selectionActive)
        return;

    selectionActive = false;
    emit selectionActiveChanged(false);
}

void TorrentFilesController::finishPendingFileMutation(const QString &method)
{
    if (!pendingFileMutationMethods.contains(method))
        return;
    pendingFileMutationMethods.clear();
    if (fileTreeWidget)
        fileTreeWidget->clearSelection();
}

void TorrentFilesController::saveViewState() const
{
    if (columnController)
        columnController->saveState();
}

void TorrentFilesController::restoreViewState()
{
    if (columnController)
        columnController->restoreState();
    if (!fileTreeWidget)
        return;
    sortColumn = fileTreeWidget->header()->sortIndicatorSection();
    sortOrder = fileTreeWidget->header()->sortIndicatorOrder();
    handleSortChanged(sortColumn, sortOrder);
}

void TorrentFilesController::clear()
{
    clearSelectionState();

    if (fileModel)
        fileModel->clear();
    torrentFilePaths.clear();
    if (placeholderController)
        placeholderController->setMessage(tr("No torrent selected."));
}

void TorrentFilesController::setLoading()
{
    clearSelectionState();

    if (fileModel)
        fileModel->clear();
    torrentFilePaths.clear();
    if (placeholderController)
        placeholderController->setMessage(tr("Loading files…"));
}

void TorrentFilesController::setTorrentContext(TorrentKey key, const QString &downloadDir)
{
    torrentKey = key;
    torrentDownloadDir = downloadDir;
}

void TorrentFilesController::setFolderMappingsProvider(
    std::function<QList<FolderMapping>()> provider)
{
    folderMappingsProvider = std::move(provider);
}

void TorrentFilesController::populate(const TorrentFiles &snapshot)
{
    if (!fileModel || !fileTreeWidget)
        return;

    const bool initiallyEmpty = fileModel->rowCount() == 0;
    QSet<QString> expandedPaths;
    std::function<void(const QModelIndex &)> capture = [&](const QModelIndex &parent) {
        for (int row = 0; row < proxyModel->rowCount(parent); ++row) {
            const QModelIndex index = proxyModel->index(row, 0, parent);
            if (fileTreeWidget->isExpanded(index))
                expandedPaths.insert(torrentPathForIndex(index));
            capture(index);
        }
    };
    if (!initiallyEmpty && !fileModel->isFlat())
        capture({});

    const int verticalPosition = fileTreeWidget->verticalScrollBar()->value();
    const int horizontalPosition = fileTreeWidget->horizontalScrollBar()->value();
    torrentFilePaths.clear();
    for (const TorrentFile &file : snapshot.files)
        torrentFilePaths.insert(file.index, file.path);
    fileModel->reconcile(snapshot.files);

    if (placeholderController) {
        placeholderController->setMessage(snapshot.files.isEmpty()
            ? tr("No files reported for this torrent.") : QString());
    }

    if (initiallyEmpty && !snapshot.files.isEmpty() && !fileModel->isFlat()) {
        fileTreeWidget->expandToDepth(0);
    } else if (!expandedPaths.isEmpty()) {
        std::function<void(const QModelIndex &)> restore = [&](const QModelIndex &parent) {
            for (int row = 0; row < proxyModel->rowCount(parent); ++row) {
                const QModelIndex index = proxyModel->index(row, 0, parent);
                if (expandedPaths.contains(torrentPathForIndex(index)))
                    fileTreeWidget->setExpanded(index, true);
                restore(index);
            }
        };
        restore({});
    }
    fileTreeWidget->verticalScrollBar()->setValue(verticalPosition);
    fileTreeWidget->horizontalScrollBar()->setValue(horizontalPosition);
}

QString TorrentFilesController::priorityToString(int priority)
{
    switch (priority) {
    case 1: return tr("High");
    case -1: return tr("Low");
    default: return tr("Normal");
    }
}

void TorrentFilesController::handleSortChanged(int logicalIndex, Qt::SortOrder order)
{
    if (!fileModel || !proxyModel || logicalIndex < 0
        || logicalIndex >= TorrentFileModel::ColumnCount)
        return;
    sortColumn = logicalIndex;
    sortOrder = order;
    const bool flat = logicalIndex != TorrentFileModel::NameColumn;

    // Changing projection resets the model shape; release any selection and
    // its polling suppression before Qt invalidates the selected indexes.
    if (fileModel->isFlat() != flat)
        clearSelectionState();

    fileModel->setFlat(flat);
    fileTreeWidget->setRootIsDecorated(!flat);
    proxyModel->sort(logicalIndex, order);
}

void TorrentFilesController::applyFilter(const QString &text)
{
    if (proxyModel)
        proxyModel->setFilterFixedString(text.trimmed());
}

QModelIndex TorrentFilesController::sourceIndex(const QModelIndex &viewIndex) const
{
    return proxyModel && viewIndex.isValid()
        ? proxyModel->mapToSource(viewIndex) : QModelIndex();
}

QList<int> TorrentFilesController::fileIndicesForIndex(const QModelIndex &index) const
{
    return fileModel ? fileModel->fileIndices(sourceIndex(index)) : QList<int>();
}

QList<int> TorrentFilesController::selectedFileIndicesForContextIndex(
    const QModelIndex &index) const
{
    if (!index.isValid() || !fileTreeWidget || !fileTreeWidget->selectionModel())
        return {};
    QModelIndexList indexes;
    if (fileTreeWidget->selectionModel()->isSelected(index))
        indexes = fileTreeWidget->selectionModel()->selectedRows(TorrentFileModel::NameColumn);
    else
        indexes.append(index.siblingAtColumn(TorrentFileModel::NameColumn));

    QSet<int> unique;
    for (const QModelIndex &selected : std::as_const(indexes)) {
        const QList<int> indices = fileIndicesForIndex(selected);
        for (int fileIndex : indices)
            unique.insert(fileIndex);
    }
    QList<int> result = unique.values();
    std::sort(result.begin(), result.end());
    return result;
}

QString TorrentFilesController::torrentPathForIndex(const QModelIndex &index) const
{
    return fileModel ? fileModel->torrentPath(sourceIndex(index)) : QString();
}

void TorrentFilesController::renameFileIndex(const QModelIndex &index)
{
    if (!isValidTorrentKey(torrentKey) || !index.isValid() || !client)
        return;
    const QString oldPath = torrentPathForIndex(index);
    const QString oldName = index.data(Qt::DisplayRole).toString().trimmed();
    if (oldPath.isEmpty() || oldName.isEmpty())
        return;

    QInputDialog dialog(dialogParent);
    dialog.setWindowTitle(tr("Rename Path"));
    dialog.setLabelText(tr("New name:"));
    dialog.setTextEchoMode(QLineEdit::Normal);
    dialog.setTextValue(oldName);
    const QSize naturalSize = dialog.sizeHint();
    dialog.resize(naturalSize.width() * 2, naturalSize.height());
    if (QLineEdit *editor = dialog.findChild<QLineEdit *>())
        editor->selectAll();
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString newName = dialog.textValue().trimmed();
    if (newName.isEmpty()) {
        QMessageBox::warning(dialogParent, tr("Rename Path"), tr("The new name cannot be empty."));
        return;
    }
    if (newName.contains(QLatin1Char('/')) || newName.contains(QLatin1Char('\\'))) {
        QMessageBox::warning(dialogParent, tr("Rename Path"),
                             tr("Enter a file or folder name, not a path."));
        return;
    }
    if (newName == oldName)
        return;

    pendingFileMutationMethods = {QStringLiteral("torrent-rename-path"),
                                  QStringLiteral("torrent-set")};
    client->renameTorrentPath(torrentKey, oldPath, newName);
    emit statusMessageRequested(tr("Renaming %1...").arg(oldName), 3000);
}

void TorrentFilesController::showContextMenu(const QPoint &pos)
{
    if (!fileTreeWidget)
        return;
    QModelIndex index = fileTreeWidget->indexAt(pos).siblingAtColumn(TorrentFileModel::NameColumn);
    if (!index.isValid())
        return;
    if (!fileTreeWidget->selectionModel()->isSelected(index)) {
        fileTreeWidget->clearSelection();
        fileTreeWidget->selectionModel()->select(
            index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        fileTreeWidget->setCurrentIndex(index);
    }
    const QList<int> fileIndices = selectedFileIndicesForContextIndex(index);
    if (fileIndices.isEmpty())
        return;

    QMenu menu(dialogParent);
    QAction *openAction = menu.addAction(tr("Open"));
    openAction->setEnabled(fileIndices.size() == 1);
    connect(openAction, &QAction::triggered, this,
            [this, fileIndices]() { openFileFromContextMenu(fileIndices); });
    QAction *openFolderAction = menu.addAction(tr("Open Containing Folder"));
    openFolderAction->setEnabled(fileIndices.size() == 1);
    connect(openFolderAction, &QAction::triggered, this,
            [this, fileIndices]() { openContainingFolderFromContextMenu(fileIndices); });

    const TorrentBackendCapabilities capabilities =
        client ? client->capabilities() : TorrentBackendCapabilities{};
    QAction *renameAction = menu.addAction(tr("Rename…"));
    renameAction->setVisible(capabilities.pathRenaming);
    renameAction->setEnabled(capabilities.pathRenaming
        && fileTreeWidget->selectionModel()->selectedRows(0).size() == 1);
    const QPersistentModelIndex persistentIndex(index);
    connect(renameAction, &QAction::triggered, this,
            [this, persistentIndex]() { renameFileIndex(persistentIndex); });

    menu.addSeparator();
    QMenu *priorityMenu = menu.addMenu(tr("Priority"));
    priorityMenu->menuAction()->setVisible(capabilities.filePriorities);
    priorityMenu->setEnabled(capabilities.filePriorities);
    QAction *skipAction = priorityMenu->addAction(tr("Skip"));
    QAction *lowAction = priorityMenu->addAction(tr("Low"));
    QAction *normalAction = priorityMenu->addAction(tr("Normal"));
    QAction *highAction = priorityMenu->addAction(tr("High"));
    lowAction->setVisible(capabilities.fileLowPriority);
    connect(skipAction, &QAction::triggered, this,
            [this]() { setSelectedFilesPriorityState(0, false); });
    connect(lowAction, &QAction::triggered, this,
            [this]() { setSelectedFilesPriorityState(-1, true); });
    connect(normalAction, &QAction::triggered, this,
            [this]() { setSelectedFilesPriorityState(0, true); });
    connect(highAction, &QAction::triggered, this,
            [this]() { setSelectedFilesPriorityState(1, true); });
    menu.exec(fileTreeWidget->viewport()->mapToGlobal(pos));
}

QString TorrentFilesController::mapRemotePathToLocalPath(
    const QString &remotePath, const QList<FolderMapping> &mappings) const
{
    const QString cleanRemotePath = QDir::cleanPath(remotePath.trimmed());
    if (cleanRemotePath.isEmpty())
        return {};
    const FolderMapping *bestMatch = nullptr;
    QString bestRemotePrefix;
    for (const FolderMapping &mapping : mappings) {
        const QString remotePrefix = QDir::cleanPath(mapping.remotePath.trimmed());
        if (remotePrefix.isEmpty())
            continue;
        const bool exact = cleanRemotePath == remotePrefix;
        const bool child = cleanRemotePath.startsWith(remotePrefix + QLatin1Char('/'));
        if ((exact || child) && (!bestMatch || remotePrefix.length() > bestRemotePrefix.length())) {
            bestMatch = &mapping;
            bestRemotePrefix = remotePrefix;
        }
    }
    if (!bestMatch)
        return {};
    const QString localPrefix = QDir::cleanPath(bestMatch->localPath.trimmed());
    QString suffix = cleanRemotePath.mid(bestRemotePrefix.length());
    if (suffix.startsWith(QLatin1Char('/')))
        suffix.remove(0, 1);
    return suffix.isEmpty() ? localPrefix : QDir(localPrefix).filePath(suffix);
}

bool TorrentFilesController::resolveMappedLocalPathForSingleFile(
    const QList<int> &fileIndices, const QString &dialogTitle,
    QString *localPath, QString *remotePath, bool requireFileExists)
{
    if (localPath)
        localPath->clear();
    if (remotePath)
        remotePath->clear();
    if (fileIndices.isEmpty())
        return false;
    if (fileIndices.size() != 1) {
        QMessageBox::information(dialogParent, dialogTitle, tr("Please select a single file."));
        return false;
    }
    const QString relativePath = torrentFilePaths.value(fileIndices.first()).trimmed();
    if (relativePath.isEmpty()) {
        QMessageBox::warning(dialogParent, dialogTitle,
                             tr("Planetary could not determine the selected file path."));
        return false;
    }
    if (torrentDownloadDir.trimmed().isEmpty()) {
        QMessageBox::warning(dialogParent, dialogTitle,
                             tr("Planetary could not determine the torrent download directory."));
        return false;
    }
    const QString resolvedRemotePath =
        QDir::cleanPath(torrentDownloadDir + QLatin1Char('/') + relativePath);
    const QList<FolderMapping> mappings = folderMappingsProvider
        ? folderMappingsProvider() : QList<FolderMapping>();
    const QString resolvedLocalPath = mapRemotePathToLocalPath(resolvedRemotePath, mappings);
    if (remotePath)
        *remotePath = resolvedRemotePath;
    if (resolvedLocalPath.isEmpty()) {
        QMessageBox::information(dialogParent, tr("No Folder Mapping"),
            tr("Planetary could not map this remote file path to a local file path.\n\nRemote path:\n%1")
                .arg(resolvedRemotePath));
        return false;
    }
    if (requireFileExists && !QFileInfo::exists(resolvedLocalPath)) {
        QMessageBox::warning(dialogParent, tr("File Not Found"),
            tr("The mapped local file does not exist.\n\nRemote path:\n%1\n\nLocal path:\n%2")
                .arg(resolvedRemotePath, resolvedLocalPath));
        return false;
    }
    if (localPath)
        *localPath = resolvedLocalPath;
    return true;
}

void TorrentFilesController::openFileFromContextMenu(const QList<int> &fileIndices)
{
    QString localPath;
    if (!resolveMappedLocalPathForSingleFile(fileIndices, tr("Open File"), &localPath))
        return;
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(localPath))) {
        QMessageBox::warning(dialogParent, tr("Open File Failed"),
                             tr("The operating system could not open this file:\n\n%1").arg(localPath));
        return;
    }
    emit statusMessageRequested(tr("Opening %1").arg(QFileInfo(localPath).fileName()), 3000);
}

void TorrentFilesController::openContainingFolderFromContextMenu(
    const QList<int> &fileIndices)
{
    QString localPath;
    QString remotePath;
    if (!resolveMappedLocalPathForSingleFile(fileIndices, tr("Open Containing Folder"),
                                             &localPath, &remotePath, false))
        return;
    const QFileInfo localInfo(localPath);
    const QString folderPath = localInfo.isDir()
        ? localInfo.absoluteFilePath() : localInfo.absolutePath();
    if (folderPath.isEmpty() || !QFileInfo::exists(folderPath)) {
        QMessageBox::warning(dialogParent, tr("Folder Not Found"),
            tr("The mapped containing folder does not exist.\n\nRemote path:\n%1\n\nLocal folder:\n%2")
                .arg(remotePath, folderPath));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath))) {
        QMessageBox::warning(dialogParent, tr("Open Folder Failed"),
            tr("The operating system could not open this folder:\n\n%1").arg(folderPath));
        return;
    }
    emit statusMessageRequested(
        tr("Opening folder %1").arg(QDir::toNativeSeparators(folderPath)), 3000);
}

void TorrentFilesController::setSelectedFilesPriorityState(int priority, bool wanted)
{
    if (!isValidTorrentKey(torrentKey) || !client || !fileTreeWidget)
        return;
    const QModelIndex current = fileTreeWidget->currentIndex().siblingAtColumn(0);
    const QList<int> fileIndices = selectedFileIndicesForContextIndex(current);
    if (fileIndices.isEmpty())
        return;
    pendingFileMutationMethods = {QStringLiteral("torrent-set")};
    client->setTorrentFilesWantedAndPriority(torrentKey, fileIndices, wanted, priority);
    const QString priorityText = wanted ? priorityToString(priority) : tr("Skip");
    emit statusMessageRequested(
        tr("Setting %1 file(s) to %2...").arg(fileIndices.size()).arg(priorityText), 3000);
}
