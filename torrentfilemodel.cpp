#include "torrentfilemodel.h"

#include "iconthememanager.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLocale>
#include <QSet>
#include <algorithm>

namespace {
bool complete(qint64 length, qint64 done)
{
    return length <= 0 || done >= length;
}

QString fileViewText(const char *sourceText)
{
    // Retain the established context so the model/view refactor can reuse all
    // existing file-view translations without invalidating catalog entries.
    return QCoreApplication::translate("TorrentFilesController", sourceText);
}
}

int TorrentFileModel::Node::row() const
{
    if (!parent)
        return 0;
    for (int row = 0; row < static_cast<int>(parent->children.size()); ++row) {
        if (parent->children.at(row).get() == this)
            return row;
    }
    return -1;
}

TorrentFileModel::TorrentFileModel(QObject *parent)
    : QAbstractItemModel(parent), m_root(std::make_unique<Node>())
{
    connect(&AppIcons::IconThemeManager::instance(),
            &AppIcons::IconThemeManager::themeChanged,
            this,
            [this]() { refreshIcons(m_root.get()); });
}

QModelIndex TorrentFileModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column < 0 || column >= ColumnCount)
        return {};
    Node *parentNode = nodeForIndex(parent);
    if (!parentNode || row >= static_cast<int>(parentNode->children.size()))
        return {};
    return createIndex(row, column, parentNode->children.at(row).get());
}

QModelIndex TorrentFileModel::parent(const QModelIndex &child) const
{
    Node *node = nodeForIndex(child);
    if (!node || !node->parent || node->parent == m_root.get())
        return {};
    return createIndex(node->parent->row(), 0, node->parent);
}

int TorrentFileModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;
    Node *node = nodeForIndex(parent);
    return node ? static_cast<int>(node->children.size()) : 0;
}

int TorrentFileModel::columnCount(const QModelIndex &) const
{
    return ColumnCount;
}

QVariant TorrentFileModel::data(const QModelIndex &index, int role) const
{
    Node *node = nodeForIndex(index);
    if (!node || node == m_root.get())
        return {};

    if (role == KindRole)
        return node->isFile() ? QStringLiteral("file") : QStringLiteral("folder");
    if (role == FileIndexRole)
        return node->isFile() ? QVariant(node->fileIndex) : QVariant();
    if (role == WantedRole)
        return node->isFile() ? QVariant(node->wanted) : QVariant();
    if (role == PriorityRole)
        return node->isFile() ? QVariant(node->priority) : QVariant();
    if (role == PathRole)
        return node->path;

    if (role == Qt::DecorationRole && index.column() == NameColumn) {
        switch (node->state) {
        case TransferState::Complete:
            return AppIcons::IconThemeManager::instance().icon(AppIcons::Id::StatusComplete);
        case TransferState::Transferring:
            return AppIcons::IconThemeManager::instance().icon(AppIcons::Id::StatusDownloading);
        case TransferState::Skipped:
            return AppIcons::IconThemeManager::instance().icon(AppIcons::Id::StatusStopped);
        case TransferState::Mixed:
            return AppIcons::IconThemeManager::instance().icon(AppIcons::Id::StatusActive);
        case TransferState::Unknown:
            return AppIcons::IconThemeManager::instance().icon(AppIcons::Id::StatusUnknown);
        }
    }

    if (role == Qt::ToolTipRole && index.column() == NameColumn) {
        switch (node->state) {
        case TransferState::Complete: return fileViewText("Complete");
        case TransferState::Transferring: return fileViewText("Transferring");
        case TransferState::Skipped: return fileViewText("Skipped");
        case TransferState::Mixed: return fileViewText("Mixed");
        case TransferState::Unknown: return fileViewText("Unknown");
        }
    }

    if (!node->isFile()) {
        if (role == Qt::DisplayRole && index.column() == NameColumn)
            return node->name;
        if (role == Qt::DisplayRole && index.column() == PriorityColumn)
            return node->effectivePriority;
        if (role == SortRole && index.column() == NameColumn)
            return node->name.toCaseFolded();
        return {};
    }

    const qint64 remaining = std::max<qint64>(0, node->length - node->bytesCompleted);
    const double percent = node->length > 0
        ? static_cast<double>(node->bytesCompleted) / node->length * 100.0 : 0.0;

    if (role == SortRole) {
        switch (index.column()) {
        case NameColumn: return node->path.toCaseFolded();
        case PriorityColumn: return node->wanted ? node->priority + 1 : -1;
        case SizeColumn: return node->length;
        case DoneColumn: return node->bytesCompleted;
        case RemainingColumn: return remaining;
        case PercentColumn: return percent;
        }
    }

    if (role != Qt::DisplayRole)
        return {};
    switch (index.column()) {
    case NameColumn: return m_flat ? node->path : node->name;
    case PriorityColumn: return node->wanted ? priorityText(node->priority) : fileViewText("Skip");
    case SizeColumn: return QLocale().formattedDataSize(node->length, 1, QLocale::DataSizeIecFormat);
    case DoneColumn: return QLocale().formattedDataSize(node->bytesCompleted, 1, QLocale::DataSizeIecFormat);
    case RemainingColumn: return QLocale().formattedDataSize(remaining, 1, QLocale::DataSizeIecFormat);
    case PercentColumn: return QStringLiteral("%1%").arg(percent, 0, 'f', 1);
    }
    return {};
}

QVariant TorrentFileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case NameColumn: return fileViewText("Name");
    case PriorityColumn: return fileViewText("Priority");
    case SizeColumn: return fileViewText("Size");
    case DoneColumn: return fileViewText("Done");
    case RemainingColumn: return fileViewText("Remaining");
    case PercentColumn: return fileViewText("Completed");
    }
    return {};
}

Qt::ItemFlags TorrentFileModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

void TorrentFileModel::clear()
{
    if (m_files.isEmpty() && m_root->children.empty())
        return;
    beginResetModel();
    m_files.clear();
    m_filesByIndex.clear();
    m_root = std::make_unique<Node>();
    endResetModel();
}

bool TorrentFileModel::sameStructure(const QVector<TorrentFile> &files) const
{
    if (files.size() != m_files.size())
        return false;
    for (int i = 0; i < files.size(); ++i) {
        if (files.at(i).index != m_files.at(i).index || files.at(i).path != m_files.at(i).path)
            return false;
    }
    return true;
}

void TorrentFileModel::reconcile(const QVector<TorrentFile> &files)
{
    if (!sameStructure(files)) {
        beginResetModel();
        m_files = files;
        rebuild();
        endResetModel();
        return;
    }

    QSet<Node *> changedParents;
    for (int i = 0; i < files.size(); ++i) {
        const TorrentFile &incoming = files.at(i);
        const TorrentFile &old = m_files.at(i);
        if (incoming.length == old.length && incoming.bytesCompleted == old.bytesCompleted
            && incoming.wanted == old.wanted && incoming.priority == old.priority)
            continue;
        Node *node = m_filesByIndex.value(incoming.index);
        if (!node)
            continue;
        node->length = incoming.length;
        node->bytesCompleted = incoming.bytesCompleted;
        node->wanted = incoming.wanted;
        node->priority = incoming.priority;
        node->state = !incoming.wanted ? TransferState::Skipped
            : (complete(incoming.length, incoming.bytesCompleted)
                   ? TransferState::Complete : TransferState::Transferring);
        emit dataChanged(createIndex(node->row(), 0, node),
                         createIndex(node->row(), ColumnCount - 1, node));
        for (Node *parent = node->parent; parent && parent != m_root.get(); parent = parent->parent)
            changedParents.insert(parent);
    }
    m_files = files;
    // Recompute bottom-up once so nested folder states never depend on hash
    // iteration order; only the affected ancestors are subsequently repainted.
    if (!changedParents.isEmpty())
        updateAggregates(m_root.get());
    for (Node *node : std::as_const(changedParents)) {
        const QModelIndex left = createIndex(node->row(), 0, node);
        emit dataChanged(left, createIndex(node->row(), ColumnCount - 1, node));
    }
}

void TorrentFileModel::setFlat(bool flat)
{
    if (m_flat == flat)
        return;
    beginResetModel();
    m_flat = flat;
    rebuild();
    endResetModel();
}

bool TorrentFileModel::isFlat() const { return m_flat; }

void TorrentFileModel::rebuild()
{
    m_root = std::make_unique<Node>();
    m_filesByIndex.clear();
    for (const TorrentFile &file : std::as_const(m_files))
        appendFile(file);
    for (auto &child : m_root->children)
        updateAggregates(child.get());
}

TorrentFileModel::Node *TorrentFileModel::findFolder(Node *parent, const QString &name) const
{
    for (const auto &child : parent->children) {
        if (!child->isFile() && child->name == name)
            return child.get();
    }
    return nullptr;
}

void TorrentFileModel::appendFile(const TorrentFile &file)
{
    Node *parent = m_root.get();
    const QStringList parts = file.path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return;
    if (!m_flat) {
        QString path;
        for (int i = 0; i + 1 < parts.size(); ++i) {
            path = path.isEmpty() ? parts.at(i) : path + QLatin1Char('/') + parts.at(i);
            Node *folder = findFolder(parent, parts.at(i));
            if (!folder) {
                auto created = std::make_unique<Node>();
                created->parent = parent;
                created->name = parts.at(i);
                created->path = path;
                folder = created.get();
                parent->children.push_back(std::move(created));
            }
            parent = folder;
        }
    }
    auto node = std::make_unique<Node>();
    node->parent = parent;
    node->name = parts.last();
    node->path = file.path;
    node->fileIndex = file.index;
    node->length = file.length;
    node->bytesCompleted = file.bytesCompleted;
    node->wanted = file.wanted;
    node->priority = file.priority;
    node->state = !file.wanted ? TransferState::Skipped
        : (complete(file.length, file.bytesCompleted)
               ? TransferState::Complete : TransferState::Transferring);
    m_filesByIndex.insert(file.index, node.get());
    parent->children.push_back(std::move(node));
}

void TorrentFileModel::updateAggregates(Node *node)
{
    if (!node || node->isFile())
        return;
    bool completed = false, transferring = false, skipped = false, unknown = false;
    QSet<QString> priorities;
    for (auto &child : node->children) {
        updateAggregates(child.get());
        switch (child->state) {
        case TransferState::Complete: completed = true; break;
        case TransferState::Transferring: transferring = true; break;
        case TransferState::Skipped: skipped = true; break;
        case TransferState::Mixed: completed = skipped = true; break;
        case TransferState::Unknown: unknown = true; break;
        }
        if (child->isFile())
            priorities.insert(child->wanted ? priorityText(child->priority) : fileViewText("Skip"));
        else if (!child->effectivePriority.isEmpty())
            priorities.insert(child->effectivePriority);
    }
    node->state = transferring ? TransferState::Transferring
        : (completed && !skipped && !unknown ? TransferState::Complete
           : (skipped && !completed && !unknown ? TransferState::Skipped
              : (completed || skipped ? TransferState::Mixed : TransferState::Unknown)));
    node->effectivePriority = priorities.size() == 1 ? *priorities.constBegin()
        : (priorities.size() > 1 ? fileViewText("Mixed") : QString());
}

TorrentFileModel::Node *TorrentFileModel::nodeForIndex(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<Node *>(index.internalPointer()) : m_root.get();
}

void TorrentFileModel::collectFileIndices(const Node *node, QList<int> *indices) const
{
    if (node->isFile()) {
        indices->append(node->fileIndex);
        return;
    }
    for (const auto &child : node->children)
        collectFileIndices(child.get(), indices);
}

void TorrentFileModel::refreshIcons(Node *parentNode)
{
    if (!parentNode || parentNode->children.empty())
        return;

    Node *first = parentNode->children.front().get();
    Node *last = parentNode->children.back().get();
    emit dataChanged(createIndex(0, NameColumn, first),
                     createIndex(static_cast<int>(parentNode->children.size()) - 1,
                                 NameColumn,
                                 last),
                     QVector<int> {Qt::DecorationRole});

    for (const auto &child : parentNode->children) {
        if (!child->isFile())
            refreshIcons(child.get());
    }
}

QList<int> TorrentFileModel::fileIndices(const QModelIndex &index) const
{
    QList<int> result;
    Node *node = nodeForIndex(index);
    if (node && node != m_root.get())
        collectFileIndices(node, &result);
    return result;
}

QString TorrentFileModel::torrentPath(const QModelIndex &index) const
{
    Node *node = nodeForIndex(index);
    return node && node != m_root.get() ? node->path.trimmed() : QString();
}

QString TorrentFileModel::priorityText(int priority)
{
    switch (priority) {
    case 1: return QCoreApplication::translate("TorrentFilesController", "High");
    case -1: return QCoreApplication::translate("TorrentFilesController", "Low");
    default: return QCoreApplication::translate("TorrentFilesController", "Normal");
    }
}
