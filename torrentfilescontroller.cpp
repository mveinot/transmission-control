#include "torrentfilescontroller.h"

#include "appicons.h"
#include "percentfilldelegate.h"
#include "torrentbackend.h"
#include "settingskeys.h"
#include "tablecolumncontroller.h"
#include "tableplaceholdercontroller.h"

#include <QCoreApplication>
#include <QIcon>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QInputDialog>
#include <QJsonObject>
#include <QJsonValue>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMetaType>
#include <QSet>
#include <QSize>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <algorithm>
#include <functional>
#include <utility>

namespace {

class SortableFileTreeItem final : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem &other) const override
    {
        const int column = treeWidget() ? treeWidget()->sortColumn() : 0;
        const QVariant left = data(column, Qt::UserRole);
        const QVariant right = other.data(column, Qt::UserRole);

        if (left.isValid() && right.isValid()) {
            const int leftType = left.metaType().id();
            const int rightType = right.metaType().id();
            const bool floatingPoint =
                leftType == QMetaType::Double || leftType == QMetaType::Float
                || rightType == QMetaType::Double || rightType == QMetaType::Float;
            const bool integral =
                (leftType >= QMetaType::Bool && leftType <= QMetaType::ULongLong)
                && (rightType >= QMetaType::Bool && rightType <= QMetaType::ULongLong);

            if (floatingPoint)
                return left.toDouble() < right.toDouble();

            if (integral)
                return left.toLongLong() < right.toLongLong();
        }

        return text(column).localeAwareCompare(other.text(column)) < 0;
    }
};

bool isComplete(qint64 length, qint64 bytesCompleted)
{
    return length <= 0 || bytesCompleted >= length;
}

} // namespace

TorrentFilesController::TorrentFilesController(QTreeWidget *fileTreeWidget,
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

    fileTreeWidget->setColumnCount(FileColumnCount);
    fileTreeWidget->setHeaderLabels({
        tr("Name"),
        tr("Priority"),
        tr("Size"),
        tr("Done"),
        tr("Remaining"),
        tr("Completed")
    });
    fileTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    fileTreeWidget->setAlternatingRowColors(false);
    fileTreeWidget->setRootIsDecorated(true);
    fileTreeWidget->setSortingEnabled(true);
    fileTreeWidget->setIconSize(QSize(16, 16));
    fileTreeWidget->setItemDelegateForColumn(
        FilePercentColumn,
        new PercentFillDelegate(FilePercentColumn, Qt::UserRole, fileTreeWidget)
        );

    columnController = std::make_unique<TableColumnController>(
        fileTreeWidget->header(),
        QString::fromLatin1(SettingsKeys::FileTreeHeaderState),
        QString::fromLatin1(SettingsKeys::FileTreeVisibleColumns),
        QVector<TableColumnController::ColumnDefinition> {
            { FileNameColumn, QStringLiteral("name"), true, false, true },
            { FilePriorityColumn, QStringLiteral("priority"), true, true, false },
            { FileSizeColumn, QStringLiteral("size"), true, true, false },
            { FileDoneColumn, QStringLiteral("done"), true, true, false },
            { FileRemainingColumn, QStringLiteral("remaining"), false, true, false },
            { FilePercentColumn, QStringLiteral("completed"), true, true, false },
        },
        this);
    columnController->setup();

    placeholderController = std::make_unique<TablePlaceholderController>(fileTreeWidget, this);
    placeholderController->setMessage(tr("No torrent selected."));

    connect(fileTreeWidget, &QTreeWidget::customContextMenuRequested,
            this, &TorrentFilesController::showContextMenu);

    connect(fileTreeWidget->header(), &QHeaderView::sortIndicatorChanged,
            this, &TorrentFilesController::handleSortChanged);

    fileTreeWidget->sortItems(FileNameColumn, Qt::AscendingOrder);

    if (filterEdit) {
        filterEdit->setClearButtonEnabled(true);
        filterEdit->setPlaceholderText(tr("Search files..."));

        connect(filterEdit, &QLineEdit::textChanged,
                this, &TorrentFilesController::applyFilter);
    }
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

    if (!fileRecords.isEmpty())
        rebuildView();
}

void TorrentFilesController::clear()
{
    if (fileTreeWidget)
        fileTreeWidget->clear();

    torrentFilePaths.clear();
    fileRecords.clear();

    if (placeholderController)
        placeholderController->setMessage(tr("No torrent selected."));
}

void TorrentFilesController::setLoading()
{
    if (fileTreeWidget)
        fileTreeWidget->clear();

    torrentFilePaths.clear();
    fileRecords.clear();

    if (placeholderController)
        placeholderController->setMessage(tr("Loading files…"));
}

void TorrentFilesController::setTorrentContext(int torrentId,
                                               const QString &downloadDir)
{
    this->torrentId = torrentId;
    torrentDownloadDir = downloadDir;
}

void TorrentFilesController::setFolderMappingsProvider(
    std::function<QList<FolderMapping>()> folderMappingsProvider)
{
    this->folderMappingsProvider = std::move(folderMappingsProvider);
}

bool TorrentFilesController::jsonValueToBool(const QJsonValue &value,
                                             bool defaultValue)
{
    if (value.isBool())
        return value.toBool();

    if (value.isDouble())
        return value.toInt(defaultValue ? 1 : 0) != 0;

    if (value.isString()) {
        const QString text = value.toString().trimmed().toLower();

        if (text == QStringLiteral("true")
            || text == QStringLiteral("yes")
            || text == QStringLiteral("1")) {
            return true;
        }

        if (text == QStringLiteral("false")
            || text == QStringLiteral("no")
            || text == QStringLiteral("0")) {
            return false;
        }
    }

    return defaultValue;
}

QString TorrentFilesController::priorityToString(int priority)
{
    switch (priority) {
    case 1:
        return QCoreApplication::translate("TorrentFilesController", "High");
    case -1:
        return QCoreApplication::translate("TorrentFilesController", "Low");
    case 0:
    default:
        return QCoreApplication::translate("TorrentFilesController", "Normal");
    }
}

void TorrentFilesController::setItemVisualState(QTreeWidgetItem *item,
                                                 FileTransferVisualState state)
{
    if (!item)
        return;

    QIcon icon;
    QString tooltip;

    switch (state) {
    case FileTransferVisualState::Complete:
        icon = AppIcons::icon(AppIcons::Icon::StatusComplete);
        tooltip = tr("Complete");
        break;
    case FileTransferVisualState::Transferring:
        icon = AppIcons::icon(AppIcons::Icon::StatusDownloading);
        tooltip = tr("Transferring");
        break;
    case FileTransferVisualState::Skipped:
        icon = AppIcons::icon(AppIcons::Icon::StatusStopped);
        tooltip = tr("Skipped");
        break;
    case FileTransferVisualState::Mixed:
        icon = AppIcons::icon(AppIcons::Icon::StatusActive);
        tooltip = tr("Mixed");
        break;
    case FileTransferVisualState::Unknown:
    default:
        icon = AppIcons::icon(AppIcons::Icon::StatusUnknown);
        tooltip = tr("Unknown");
        break;
    }

    item->setIcon(FileNameColumn, icon);
    item->setToolTip(FileNameColumn, tooltip);
}

void TorrentFilesController::updateFolderVisualStates()
{
    if (!fileTreeWidget)
        return;

    for (int i = 0; i < fileTreeWidget->topLevelItemCount(); ++i)
        updateFolderVisualState(fileTreeWidget->topLevelItem(i));
}

TorrentFilesController::FileTransferVisualState TorrentFilesController::updateFolderVisualState(
    QTreeWidgetItem *item)
{
    if (!item)
        return FileTransferVisualState::Unknown;

    const QString kind = item->data(FileNameColumn, FileKindRole).toString();

    if (kind == QStringLiteral("file")) {
        const bool wanted = item->data(FileNameColumn, FileWantedRole).toBool();
        const qint64 length = item->data(FileSizeColumn, Qt::UserRole).toLongLong();
        const qint64 bytesCompleted = item->data(FileDoneColumn, Qt::UserRole).toLongLong();

        if (!wanted)
            return FileTransferVisualState::Skipped;

        return isComplete(length, bytesCompleted)
            ? FileTransferVisualState::Complete
            : FileTransferVisualState::Transferring;
    }

    bool sawComplete = false;
    bool sawTransferring = false;
    bool sawSkipped = false;
    bool sawUnknown = false;

    for (int i = 0; i < item->childCount(); ++i) {
        switch (updateFolderVisualState(item->child(i))) {
        case FileTransferVisualState::Complete:
            sawComplete = true;
            break;
        case FileTransferVisualState::Transferring:
            sawTransferring = true;
            break;
        case FileTransferVisualState::Skipped:
            sawSkipped = true;
            break;
        case FileTransferVisualState::Mixed:
            sawComplete = true;
            sawSkipped = true;
            break;
        case FileTransferVisualState::Unknown:
        default:
            sawUnknown = true;
            break;
        }
    }

    FileTransferVisualState state = FileTransferVisualState::Unknown;

    if (sawTransferring)
        state = FileTransferVisualState::Transferring;
    else if (sawComplete && !sawSkipped && !sawUnknown)
        state = FileTransferVisualState::Complete;
    else if (sawSkipped && !sawComplete && !sawUnknown)
        state = FileTransferVisualState::Skipped;
    else if (sawComplete || sawSkipped)
        state = FileTransferVisualState::Mixed;

    setItemVisualState(item, state);

    return state;
}

QTreeWidgetItem *TorrentFilesController::findOrCreateTopLevelItem(
    const QString &name)
{
    if (!fileTreeWidget)
        return nullptr;

    for (int i = 0; i < fileTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = fileTreeWidget->topLevelItem(i);

        if (item->text(FileNameColumn) == name)
            return item;
    }

    auto *item = new SortableFileTreeItem(fileTreeWidget);
    item->setText(FileNameColumn, name);
    item->setData(FileNameColumn, FileKindRole, QStringLiteral("folder"));

    return item;
}

QTreeWidgetItem *TorrentFilesController::findOrCreateChild(
    QTreeWidgetItem *parent,
    const QString &name,
    bool isFolder)
{
    if (!parent)
        return nullptr;

    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);

        if (child->text(FileNameColumn) == name)
            return child;
    }

    auto *child = new SortableFileTreeItem(parent);
    child->setText(FileNameColumn, name);
    child->setData(FileNameColumn,
                   FileKindRole,
                   isFolder ? QStringLiteral("folder") : QStringLiteral("file"));

    return child;
}

void TorrentFilesController::populate(const QJsonArray &files,
                                      const QJsonArray &wanted,
                                      const QJsonArray &priorities)
{
    if (!fileTreeWidget)
        return;

    fileTreeWidget->clear();
    torrentFilePaths.clear();
    fileRecords.clear();

    if (placeholderController)
        placeholderController->setMessage(files.isEmpty() ? tr("No files reported for this torrent.") : QString());

    fileRecords.reserve(files.size());

    for (int fileIndex = 0; fileIndex < files.size(); ++fileIndex) {
        const QJsonObject file = files.at(fileIndex).toObject();

        FileRecord record;
        record.index = fileIndex;
        record.path = file.value(QStringLiteral("name")).toString();
        record.length = file.value(QStringLiteral("length")).toVariant().toLongLong();
        record.bytesCompleted =
            file.value(QStringLiteral("bytesCompleted")).toVariant().toLongLong();
        record.wanted =
            fileIndex < wanted.size()
                ? jsonValueToBool(wanted.at(fileIndex), true)
                : true;
        record.priority =
            fileIndex < priorities.size()
                ? priorities.at(fileIndex).toInt(0)
                : 0;

        torrentFilePaths.insert(fileIndex, record.path);
        fileRecords.append(record);
    }

    rebuildView();
}

void TorrentFilesController::populateFileItem(QTreeWidgetItem *item,
                                               const FileRecord &record,
                                               bool showFullPath)
{
    if (!item)
        return;

    const double percentDone =
        record.length > 0
            ? (static_cast<double>(record.bytesCompleted)
               / static_cast<double>(record.length)) * 100.0
            : 0.0;
    const qint64 remaining = std::max<qint64>(0, record.length - record.bytesCompleted);

    item->setText(FileNameColumn,
                  showFullPath ? record.path : QFileInfo(record.path).fileName());
    item->setData(FileNameColumn, Qt::UserRole, record.path.toCaseFolded());
    item->setData(FileNameColumn, FileKindRole, QStringLiteral("file"));
    item->setData(FileNameColumn, FileIndexRole, record.index);
    item->setData(FileNameColumn, FileWantedRole, record.wanted);
    item->setData(FileNameColumn, FilePriorityRole, record.priority);

    setItemVisualState(
        item,
        !record.wanted
            ? FileTransferVisualState::Skipped
            : (isComplete(record.length, record.bytesCompleted)
                   ? FileTransferVisualState::Complete
                   : FileTransferVisualState::Transferring));

    item->setText(FilePriorityColumn,
                  record.wanted ? priorityToString(record.priority) : tr("Skip"));
    item->setData(FilePriorityColumn, Qt::UserRole,
                  record.wanted ? record.priority + 1 : -1);

    item->setText(FileSizeColumn,
                  QLocale().formattedDataSize(
                      record.length, 1, QLocale::DataSizeIecFormat));
    item->setText(FileDoneColumn,
                  QLocale().formattedDataSize(
                      record.bytesCompleted, 1, QLocale::DataSizeIecFormat));
    item->setText(FileRemainingColumn,
                  QLocale().formattedDataSize(
                      remaining, 1, QLocale::DataSizeIecFormat));
    item->setText(FilePercentColumn,
                  QStringLiteral("%1%").arg(percentDone, 0, 'f', 1));

    item->setData(FileSizeColumn, Qt::UserRole, record.length);
    item->setData(FileDoneColumn, Qt::UserRole, record.bytesCompleted);
    item->setData(FileRemainingColumn, Qt::UserRole, remaining);
    item->setData(FilePercentColumn, Qt::UserRole, percentDone);
}

void TorrentFilesController::populateTreeView()
{
    for (const FileRecord &record : std::as_const(fileRecords)) {
        const QStringList parts = record.path.split('/', Qt::SkipEmptyParts);

        if (parts.isEmpty())
            continue;

        QTreeWidgetItem *current = findOrCreateTopLevelItem(parts.first());

        for (int i = 1; i < parts.size(); ++i) {
            const bool isLast = (i == parts.size() - 1);
            current = findOrCreateChild(current, parts.at(i), !isLast);
        }

        populateFileItem(current, record, false);
    }

    updateFolderPriorityStates();
    updateFolderVisualStates();
    fileTreeWidget->setRootIsDecorated(true);
    fileTreeWidget->expandToDepth(0);
}

void TorrentFilesController::populateFlatView()
{
    for (const FileRecord &record : std::as_const(fileRecords)) {
        auto *item = new SortableFileTreeItem(fileTreeWidget);
        populateFileItem(item, record, true);
    }

    fileTreeWidget->setRootIsDecorated(false);
}

void TorrentFilesController::rebuildView()
{
    if (!fileTreeWidget || rebuildingView)
        return;

    // Projection changes replace items; preserve selection by stable file ID.
    QSet<int> selectedIndices;
    for (QTreeWidgetItem *selectedItem : fileTreeWidget->selectedItems()) {
        const QList<int> indices = fileIndicesForItem(selectedItem);
        for (int index : indices)
            selectedIndices.insert(index);
    }

    rebuildingView = true;
    const QSignalBlocker blocker(fileTreeWidget->header());
    fileTreeWidget->setSortingEnabled(false);
    fileTreeWidget->clear();

    if (sortColumn == FileNameColumn)
        populateTreeView();
    else
        populateFlatView();

    fileTreeWidget->setSortingEnabled(true);
    fileTreeWidget->sortItems(sortColumn, sortOrder);

    std::function<void(QTreeWidgetItem *)> restoreSelection =
        [&](QTreeWidgetItem *item) {
            if (!item)
                return;

            const QVariant indexValue = item->data(FileNameColumn, FileIndexRole);
            if (indexValue.isValid() && selectedIndices.contains(indexValue.toInt()))
                item->setSelected(true);

            for (int i = 0; i < item->childCount(); ++i)
                restoreSelection(item->child(i));
        };

    for (int i = 0; i < fileTreeWidget->topLevelItemCount(); ++i)
        restoreSelection(fileTreeWidget->topLevelItem(i));

    fileTreeWidget->resizeColumnToContents(FileNameColumn);
    applyFilter(filterEdit ? filterEdit->text() : QString());
    rebuildingView = false;
}

void TorrentFilesController::handleSortChanged(int logicalIndex,
                                               Qt::SortOrder order)
{
    if (rebuildingView || logicalIndex < 0 || logicalIndex >= FileColumnCount)
        return;

    const bool viewShapeChanged =
        (sortColumn == FileNameColumn) != (logicalIndex == FileNameColumn);
    sortColumn = logicalIndex;
    sortOrder = order;

    if (viewShapeChanged)
        rebuildView();
}

void TorrentFilesController::applyFilter(const QString &text)
{
    if (!fileTreeWidget)
        return;

    const QString filterText = text.trimmed();

    for (int i = 0; i < fileTreeWidget->topLevelItemCount(); ++i)
        filterItem(fileTreeWidget->topLevelItem(i), filterText, false);
}

bool TorrentFilesController::filterItem(QTreeWidgetItem *item,
                                        const QString &text,
                                        bool ancestorMatches)
{
    if (!item)
        return false;

    const bool itemMatches = text.isEmpty()
        || item->text(FileNameColumn).contains(text, Qt::CaseInsensitive);
    const bool showDescendants = ancestorMatches || itemMatches;
    bool descendantMatches = false;

    for (int i = 0; i < item->childCount(); ++i) {
        if (filterItem(item->child(i), text, showDescendants))
            descendantMatches = true;
    }

    const bool visible = showDescendants || descendantMatches;
    item->setHidden(!visible);

    if (!text.isEmpty() && descendantMatches)
        item->setExpanded(true);

    return itemMatches || descendantMatches;
}

void TorrentFilesController::updateFolderPriorityStates()
{
    if (!fileTreeWidget)
        return;

    for (int i = 0; i < fileTreeWidget->topLevelItemCount(); ++i)
        updateFolderPriorityState(fileTreeWidget->topLevelItem(i));
}

void TorrentFilesController::updateFolderPriorityState(QTreeWidgetItem *item)
{
    if (!item)
        return;

    const QString kind = item->data(FileNameColumn, FileKindRole).toString();

    if (kind == QStringLiteral("file"))
        return;

    QSet<QString> effectivePriorities;

    std::function<void(QTreeWidgetItem *)> scan = [&](QTreeWidgetItem *node) {
        if (!node)
            return;

        const QString nodeKind = node->data(FileNameColumn, FileKindRole).toString();

        if (nodeKind == QStringLiteral("file")) {
            const QVariant wantedValue = node->data(FileNameColumn, FileWantedRole);
            const bool wanted = wantedValue.isValid() ? wantedValue.toBool() : true;

            if (!wanted) {
                effectivePriorities.insert(tr("Skip"));
                return;
            }

            const int priority = node->data(FileNameColumn, FilePriorityRole).toInt();
            effectivePriorities.insert(priorityToString(priority));
            return;
        }

        for (int i = 0; i < node->childCount(); ++i)
            scan(node->child(i));
    };

    scan(item);

    if (effectivePriorities.size() == 1)
        item->setText(FilePriorityColumn, *effectivePriorities.constBegin());
    else if (effectivePriorities.size() > 1)
        item->setText(FilePriorityColumn, tr("Mixed"));
    else
        item->setText(FilePriorityColumn, QString());
}

QList<int> TorrentFilesController::fileIndicesForItem(QTreeWidgetItem *item) const
{
    QList<int> indices;

    if (!item)
        return indices;

    const QString kind = item->data(FileNameColumn, FileKindRole).toString();

    if (kind == QStringLiteral("file")) {
        const QVariant fileIndexValue = item->data(FileNameColumn, FileIndexRole);
        const int fileIndex = fileIndexValue.isValid() ? fileIndexValue.toInt() : -1;

        if (fileIndex >= 0)
            indices.append(fileIndex);

        return indices;
    }

    for (int i = 0; i < item->childCount(); ++i)
        indices.append(fileIndicesForItem(item->child(i)));

    return indices;
}

QList<int> TorrentFilesController::selectedFileIndicesForContextItem(
    QTreeWidgetItem *item) const
{
    if (!item)
        return {};

    QList<QTreeWidgetItem *> items;

    if (item->isSelected())
        items = fileTreeWidget->selectedItems();
    else
        items.append(item);

    QSet<int> uniqueIndices;

    for (QTreeWidgetItem *selectedItem : std::as_const(items)) {
        const QList<int> indices = fileIndicesForItem(selectedItem);

        for (int index : indices)
            uniqueIndices.insert(index);
    }

    QList<int> result = uniqueIndices.values();
    std::sort(result.begin(), result.end());

    return result;
}

QString TorrentFilesController::torrentPathForFileTreeItem(
    QTreeWidgetItem *item) const
{
    if (!item)
        return {};

    const QString kind = item->data(FileNameColumn, FileKindRole).toString();

    if (kind == QStringLiteral("file")) {
        bool ok = false;
        const int fileIndex = item->data(FileNameColumn, FileIndexRole).toInt(&ok);

        if (ok && fileIndex >= 0)
            return torrentFilePaths.value(fileIndex).trimmed();

        return {};
    }

    QStringList parts;
    QTreeWidgetItem *current = item;

    while (current) {
        parts.prepend(current->text(FileNameColumn));
        current = current->parent();
    }

    return parts.join(QLatin1Char('/')).trimmed();
}

void TorrentFilesController::renameFileTreeItem(QTreeWidgetItem *item)
{
    if (torrentId < 0 || !item || !client)
        return;

    const QString oldPath = torrentPathForFileTreeItem(item);

    if (oldPath.isEmpty())
        return;

    const QString oldName = item->text(FileNameColumn).trimmed();

    bool ok = false;
    const QString newName = QInputDialog::getText(
        dialogParent,
        tr("Rename Path"),
        tr("New name:"),
        QLineEdit::Normal,
        oldName,
        &ok
        ).trimmed();

    if (!ok)
        return;

    if (newName.isEmpty()) {
        QMessageBox::warning(dialogParent,
                             tr("Rename Path"),
                             tr("The new name cannot be empty."));
        return;
    }

    if (newName.contains(QLatin1Char('/'))
        || newName.contains(QLatin1Char('\\'))) {
        QMessageBox::warning(dialogParent,
                             tr("Rename Path"),
                             tr("Enter a file or folder name, not a path."));
        return;
    }

    if (newName == oldName)
        return;

    client->renameTorrentPath(torrentId, oldPath, newName);

    emit statusMessageRequested(tr("Renaming %1...").arg(oldName), 3000);
    emit torrentDetailsRefreshRequested(torrentId);
}

void TorrentFilesController::showContextMenu(const QPoint &pos)
{
    if (!fileTreeWidget)
        return;

    QTreeWidgetItem *item = fileTreeWidget->itemAt(pos);

    if (!item)
        return;

    if (!item->isSelected()) {
        fileTreeWidget->clearSelection();
        item->setSelected(true);
        fileTreeWidget->setCurrentItem(item);
    }

    const QList<int> fileIndices = selectedFileIndicesForContextItem(item);

    if (fileIndices.isEmpty())
        return;

    QMenu menu(dialogParent);

    QAction *openAction = menu.addAction(tr("Open"));
    openAction->setEnabled(fileIndices.size() == 1);

    connect(openAction, &QAction::triggered,
            this, [this, fileIndices]() { openFileFromContextMenu(fileIndices); });

    QAction *openContainingFolderAction = menu.addAction(tr("Open Containing Folder"));
    openContainingFolderAction->setEnabled(fileIndices.size() == 1);

    connect(openContainingFolderAction, &QAction::triggered,
            this, [this, fileIndices]() {
                openContainingFolderFromContextMenu(fileIndices);
            });

    QAction *renameAction = menu.addAction(tr("Rename…"));
    renameAction->setEnabled(fileTreeWidget->selectedItems().size() == 1);

    connect(renameAction, &QAction::triggered,
            this, [this, item]() { renameFileTreeItem(item); });

    menu.addSeparator();

    QMenu *priorityMenu = menu.addMenu(tr("Priority"));

    QAction *skipPriorityAction = priorityMenu->addAction(tr("Skip"));
    QAction *lowPriorityAction = priorityMenu->addAction(tr("Low"));
    QAction *normalPriorityAction = priorityMenu->addAction(tr("Normal"));
    QAction *highPriorityAction = priorityMenu->addAction(tr("High"));

    connect(skipPriorityAction, &QAction::triggered,
            this, [this]() { setSelectedFilesPriorityState(0, false); });

    connect(lowPriorityAction, &QAction::triggered,
            this, [this]() { setSelectedFilesPriorityState(-1, true); });

    connect(normalPriorityAction, &QAction::triggered,
            this, [this]() { setSelectedFilesPriorityState(0, true); });

    connect(highPriorityAction, &QAction::triggered,
            this, [this]() { setSelectedFilesPriorityState(1, true); });

    menu.exec(fileTreeWidget->viewport()->mapToGlobal(pos));
}

QString TorrentFilesController::mapRemotePathToLocalPath(
    const QString &remotePath,
    const QList<FolderMapping> &mappings) const
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

        const bool exactMatch = cleanRemotePath == remotePrefix;
        const bool childMatch =
            cleanRemotePath.startsWith(remotePrefix + QLatin1Char('/'));

        if (!exactMatch && !childMatch)
            continue;

        if (!bestMatch || remotePrefix.length() > bestRemotePrefix.length()) {
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

    if (suffix.isEmpty())
        return localPrefix;

    return QDir(localPrefix).filePath(suffix);
}

bool TorrentFilesController::resolveMappedLocalPathForSingleFile(
    const QList<int> &fileIndices,
    const QString &dialogTitle,
    QString *localPath,
    QString *remotePath,
    bool requireFileExists)
{
    if (localPath)
        localPath->clear();

    if (remotePath)
        remotePath->clear();

    if (fileIndices.isEmpty())
        return false;

    if (fileIndices.size() != 1) {
        QMessageBox::information(dialogParent,
                                 dialogTitle,
                                 tr("Please select a single file."));
        return false;
    }

    const int fileIndex = fileIndices.first();
    const QString relativeFilePath = torrentFilePaths.value(fileIndex).trimmed();

    if (relativeFilePath.isEmpty()) {
        QMessageBox::warning(
            dialogParent,
            dialogTitle,
            tr("Planetary could not determine the selected file path."));
        return false;
    }

    if (torrentDownloadDir.trimmed().isEmpty()) {
        QMessageBox::warning(
            dialogParent,
            dialogTitle,
            tr("Planetary could not determine the torrent download directory."));
        return false;
    }

    const QString resolvedRemotePath =
        QDir::cleanPath(torrentDownloadDir + QLatin1Char('/') + relativeFilePath);

    const QList<FolderMapping> mappings =
        folderMappingsProvider ? folderMappingsProvider() : QList<FolderMapping>();

    const QString resolvedLocalPath =
        mapRemotePathToLocalPath(resolvedRemotePath, mappings);

    if (remotePath)
        *remotePath = resolvedRemotePath;

    if (resolvedLocalPath.isEmpty()) {
        QMessageBox::information(
            dialogParent,
            tr("No Folder Mapping"),
            tr("Planetary could not map this remote file path to a local file path.\n\n"
               "Remote path:\n%1")
                .arg(resolvedRemotePath));
        return false;
    }

    if (requireFileExists && !QFileInfo::exists(resolvedLocalPath)) {
        QMessageBox::warning(
            dialogParent,
            tr("File Not Found"),
            tr("The mapped local file does not exist.\n\n"
               "Remote path:\n%1\n\n"
               "Local path:\n%2")
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

    if (!resolveMappedLocalPathForSingleFile(
            fileIndices,
            tr("Open File"),
            &localPath,
            nullptr,
            true)) {
        return;
    }

    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));

    if (!opened) {
        QMessageBox::warning(
            dialogParent,
            tr("Open File Failed"),
            tr("The operating system could not open this file:\n\n%1")
                .arg(localPath));
        return;
    }

    emit statusMessageRequested(
        tr("Opening %1").arg(QFileInfo(localPath).fileName()),
        3000);
}

void TorrentFilesController::openContainingFolderFromContextMenu(
    const QList<int> &fileIndices)
{
    QString localPath;
    QString remotePath;

    if (!resolveMappedLocalPathForSingleFile(
            fileIndices,
            tr("Open Containing Folder"),
            &localPath,
            &remotePath,
            false)) {
        return;
    }

    const QFileInfo localInfo(localPath);
    const QString folderPath = localInfo.isDir()
        ? localInfo.absoluteFilePath()
        : localInfo.absolutePath();

    if (folderPath.isEmpty() || !QFileInfo::exists(folderPath)) {
        QMessageBox::warning(
            dialogParent,
            tr("Folder Not Found"),
            tr("The mapped containing folder does not exist.\n\n"
               "Remote path:\n%1\n\n"
               "Local folder:\n%2")
                .arg(remotePath, folderPath));
        return;
    }

    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));

    if (!opened) {
        QMessageBox::warning(
            dialogParent,
            tr("Open Folder Failed"),
            tr("The operating system could not open this folder:\n\n%1")
                .arg(folderPath));
        return;
    }

    emit statusMessageRequested(
        tr("Opening folder %1").arg(QDir::toNativeSeparators(folderPath)),
        3000);
}

void TorrentFilesController::setSelectedFilesPriorityState(int priority,
                                                           bool wanted)
{
    if (torrentId < 0 || !client || !fileTreeWidget)
        return;

    QTreeWidgetItem *item = fileTreeWidget->currentItem();

    if (!item)
        return;

    const QList<int> fileIndices = selectedFileIndicesForContextItem(item);

    if (fileIndices.isEmpty())
        return;

    client->setTorrentFilesWantedAndPriority(torrentId, fileIndices, wanted, priority);

    const QString priorityText = wanted ? priorityToString(priority) : tr("Skip");

    emit statusMessageRequested(
        tr("Setting %1 file(s) to %2...")
            .arg(fileIndices.size())
            .arg(priorityText),
        3000);

    emit torrentDetailsRefreshRequested(torrentId);
}
