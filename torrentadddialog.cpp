#include "torrentadddialog.h"
#include "ui_torrentadddialog.h"

#include <QComboBox>
#include <QHash>
#include <QHeaderView>
#include <QLocale>
#include <QObject>
#include <QStringList>
#include <QTreeWidgetItem>

namespace {

constexpr int NameColumn = 0;
constexpr int PriorityColumn = 1;
constexpr int SizeColumn = 2;

constexpr int PrioritySkip = -2;
constexpr int PriorityLow = -1;
constexpr int PriorityNormal = 0;
constexpr int PriorityHigh = 1;

constexpr int SizeRole = Qt::UserRole;
constexpr int FileIndexRole = Qt::UserRole + 1;
constexpr int IsFileRole = Qt::UserRole + 2;

QString formatByteSize(qint64 bytes)
{
    static constexpr qint64 KiB = 1024;
    static constexpr qint64 MiB = KiB * 1024;
    static constexpr qint64 GiB = MiB * 1024;

    const QLocale locale;

    if (bytes >= GiB)
        return locale.toString(bytes / static_cast<double>(GiB), 'f', 2) + QObject::tr(" GiB");

    if (bytes >= MiB)
        return locale.toString(bytes / static_cast<double>(MiB), 'f', 2) + QObject::tr(" MiB");

    if (bytes >= KiB)
        return locale.toString(bytes / static_cast<double>(KiB), 'f', 1) + QObject::tr(" KiB");

    return locale.toString(bytes) + QObject::tr(" bytes");
}

qint64 totalTorrentSize(const TorrentMetadata &metadata)
{
    qint64 totalSize = 0;

    for (const TorrentFileMetadata &file : metadata.files)
        totalSize += file.length;

    return totalSize;
}

void addSizeToItem(QTreeWidgetItem *item, qint64 size)
{
    while (item) {
        const qint64 currentSize = item->data(NameColumn, SizeRole).toLongLong();
        item->setData(NameColumn, SizeRole, currentSize + size);
        item = item->parent();
    }
}

void updateFolderSizes(QTreeWidgetItem *item)
{
    if (!item)
        return;

    if (item->childCount() > 0)
        item->setText(SizeColumn, formatByteSize(item->data(NameColumn, SizeRole).toLongLong()));

    for (int i = 0; i < item->childCount(); ++i)
        updateFolderSizes(item->child(i));
}

void collectFileItems(QTreeWidgetItem *item, QList<QTreeWidgetItem *> *items)
{
    if (!item || !items)
        return;

    if (item->data(NameColumn, IsFileRole).toBool()) {
        items->append(item);
        return;
    }

    for (int i = 0; i < item->childCount(); ++i)
        collectFileItems(item->child(i), items);
}

} // namespace

TorrentAddDialog::TorrentAddDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TorrentAddDialog)
{
    ui->setupUi(this);

    setupContentsTree();
    ui->groupContents->hide();

    ui->editSource->setTextMargins(0, 0, 0, 0);
    ui->editSource->setCursorPosition(0);

    QPalette sourcePalette = ui->editSource->palette();
    sourcePalette.setColor(QPalette::Base, sourcePalette.color(QPalette::Window));
    sourcePalette.setColor(QPalette::Text, sourcePalette.color(QPalette::WindowText));
    ui->editSource->setPalette(sourcePalette);

    ui->editSource->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0px;"
        "}"
        ));
}

TorrentAddDialog::~TorrentAddDialog()
{
    delete ui;
}

void TorrentAddDialog::setupContentsTree()
{
    ui->treeTorrentContents->setColumnCount(3);
    ui->treeTorrentContents->setHeaderLabels({
        tr("Name"),
        tr("Priority"),
        tr("Size")
    });

    ui->treeTorrentContents->header()->setStretchLastSection(false);
    ui->treeTorrentContents->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    ui->treeTorrentContents->header()->setSectionResizeMode(PriorityColumn, QHeaderView::ResizeToContents);
    ui->treeTorrentContents->header()->setSectionResizeMode(SizeColumn, QHeaderView::ResizeToContents);
}

void TorrentAddDialog::setSource(SourceType type, const QString &source)
{
    m_sourceType = type;
    m_source = source;

    switch (type) {
    case SourceType::TorrentFile:
        ui->labelSourceTypeValue->setText(tr("Torrent file"));
        break;

    case SourceType::MagnetLink:
        ui->labelSourceTypeValue->setText(tr("Magnet link"));
        clearTorrentMetadata();
        break;
    }

    ui->editSource->setText(source);
    ui->editSource->setCursorPosition(0);
}

void TorrentAddDialog::setDownloadDir(const QString &downloadDir)
{
    ui->editDownloadDir->setText(downloadDir);
}

void TorrentAddDialog::setStartPaused(bool paused)
{
    ui->checkStartPaused->setChecked(paused);
}

void TorrentAddDialog::setRememberOptions(bool remember)
{
    ui->checkRememberOptions->setChecked(remember);
}

void TorrentAddDialog::setTorrentMetadata(const TorrentMetadata &metadata)
{
    // Preserve metainfo indices so hierarchical choices map to flat RPC arrays.
    m_updatingPriorities = true;

    ui->treeTorrentContents->clear();
    ui->groupContents->show();

    if (!metadata.isValid()) {
        ui->labelContentsSummary->setText(
            tr("Torrent contents could not be read: %1")
                .arg(metadata.errorString)
        );
        ui->treeTorrentContents->hide();
        m_updatingPriorities = false;
        return;
    }

    ui->treeTorrentContents->show();
    ui->labelContentsSummary->setText(
        tr("%1 · %2 file(s) · %3")
            .arg(metadata.name)
            .arg(metadata.files.size())
            .arg(formatByteSize(totalTorrentSize(metadata)))
    );

    QHash<QString, QTreeWidgetItem *> folderItems;

    for (const TorrentFileMetadata &file : metadata.files) {
        const QStringList parts = file.path.split(QLatin1Char('/'), Qt::SkipEmptyParts);

        if (parts.isEmpty())
            continue;

        QTreeWidgetItem *parentItem = nullptr;
        QString folderKey;

        for (int i = 0; i < parts.size() - 1; ++i) {
            if (!folderKey.isEmpty())
                folderKey += QLatin1Char('/');

            folderKey += parts.at(i);

            QTreeWidgetItem *folderItem = folderItems.value(folderKey, nullptr);

            if (!folderItem) {
                folderItem = new QTreeWidgetItem(QStringList{
                    parts.at(i),
                    QString(),
                    QString()
                });

                addTorrentFolderItem(folderItem);

                if (parentItem)
                    parentItem->addChild(folderItem);
                else
                    ui->treeTorrentContents->addTopLevelItem(folderItem);

                ui->treeTorrentContents->setItemWidget(folderItem,
                                                       PriorityColumn,
                                                       createPriorityCombo(0));

                folderItems.insert(folderKey, folderItem);
            }

            parentItem = folderItem;
        }

        addTorrentFileItem(file, parentItem, parts.last());
    }

    for (int i = 0; i < ui->treeTorrentContents->topLevelItemCount(); ++i)
        updateFolderSizes(ui->treeTorrentContents->topLevelItem(i));

    m_updatingPriorities = false;

    ui->treeTorrentContents->expandToDepth(0);
}

void TorrentAddDialog::addTorrentFolderItem(QTreeWidgetItem *item)
{
    item->setData(NameColumn, SizeRole, qint64(0));
    item->setData(NameColumn, IsFileRole, false);

}

void TorrentAddDialog::addTorrentFileItem(const TorrentFileMetadata &file,
                                          QTreeWidgetItem *parentItem,
                                          const QString &displayName)
{
    auto *fileItem = new QTreeWidgetItem(QStringList{
        displayName,
        QString(),
        formatByteSize(file.length)
    });

    fileItem->setData(NameColumn, SizeRole, file.length);
    fileItem->setData(NameColumn, FileIndexRole, file.index);
    fileItem->setData(NameColumn, IsFileRole, true);
    fileItem->setToolTip(NameColumn, file.path);

    if (parentItem) {
        parentItem->addChild(fileItem);
        addSizeToItem(parentItem, file.length);
    } else {
        ui->treeTorrentContents->addTopLevelItem(fileItem);
    }

    ui->treeTorrentContents->setItemWidget(fileItem,
                                           PriorityColumn,
                                           createPriorityCombo(0));
}

QComboBox *TorrentAddDialog::createPriorityCombo(int priority)
{
    auto *combo = new QComboBox(ui->treeTorrentContents);

    combo->addItem(tr("Skip"), PrioritySkip);
    combo->addItem(tr("Low"), PriorityLow);
    combo->addItem(tr("Normal"), PriorityNormal);
    combo->addItem(tr("High"), PriorityHigh);

    const int index = combo->findData(priority);
    combo->setCurrentIndex(index >= 0 ? index : combo->findData(PriorityNormal));

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, combo](int) {
                if (m_updatingPriorities)
                    return;

                QTreeWidgetItem *item = nullptr;

                for (int i = 0; i < ui->treeTorrentContents->topLevelItemCount() && !item; ++i) {
                    QList<QTreeWidgetItem *> stack;
                    stack.append(ui->treeTorrentContents->topLevelItem(i));

                    while (!stack.isEmpty() && !item) {
                        QTreeWidgetItem *candidate = stack.takeLast();

                        if (ui->treeTorrentContents->itemWidget(candidate, PriorityColumn) == combo) {
                            item = candidate;
                            break;
                        }

                        for (int childIndex = 0; childIndex < candidate->childCount(); ++childIndex)
                            stack.append(candidate->child(childIndex));
                    }
                }

                if (!item || item->data(NameColumn, IsFileRole).toBool())
                    return;

                const int priority = combo->currentData().toInt();
                setPriorityForChildren(item, priority);
            });

    return combo;
}

void TorrentAddDialog::setPriorityForChildren(QTreeWidgetItem *item, int priority)
{
    if (!item)
        return;

    const bool wasUpdating = m_updatingPriorities;
    m_updatingPriorities = true;

    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem *child = item->child(i);

        if (auto *combo = qobject_cast<QComboBox *>(ui->treeTorrentContents->itemWidget(child, PriorityColumn))) {
            const int index = combo->findData(priority);

            if (index >= 0)
                combo->setCurrentIndex(index);
        }

        setPriorityForChildren(child, priority);
    }

    m_updatingPriorities = wasUpdating;
}

void TorrentAddDialog::clearTorrentMetadata()
{
    ui->treeTorrentContents->clear();
    ui->groupContents->hide();
}

TorrentAddDialog::SourceType TorrentAddDialog::sourceType() const
{
    return m_sourceType;
}

QString TorrentAddDialog::source() const
{
    return m_source;
}

QString TorrentAddDialog::downloadDir() const
{
    return ui->editDownloadDir->text().trimmed();
}

bool TorrentAddDialog::startPaused() const
{
    return ui->checkStartPaused->isChecked();
}

bool TorrentAddDialog::rememberOptions() const
{
    return ui->checkRememberOptions->isChecked();
}

QList<int> TorrentAddDialog::unwantedFileIndices() const
{
    return fileIndicesForPriority(PrioritySkip);
}

QList<int> TorrentAddDialog::lowPriorityFileIndices() const
{
    return fileIndicesForPriority(PriorityLow);
}

QList<int> TorrentAddDialog::highPriorityFileIndices() const
{
    return fileIndicesForPriority(PriorityHigh);
}

QList<int> TorrentAddDialog::fileIndicesForPriority(int priority) const
{
    QList<int> result;

    for (int i = 0; i < ui->treeTorrentContents->topLevelItemCount(); ++i) {
        QList<QTreeWidgetItem *> fileItems;
        collectFileItems(ui->treeTorrentContents->topLevelItem(i), &fileItems);

        for (QTreeWidgetItem *item : fileItems) {
            auto *combo = qobject_cast<QComboBox *>(
                ui->treeTorrentContents->itemWidget(item, PriorityColumn));

            if (!combo || combo->currentData().toInt() != priority)
                continue;

            const int fileIndex = item->data(NameColumn, FileIndexRole).toInt();

            if (fileIndex >= 0)
                result.append(fileIndex);
        }
    }

    return result;
}
