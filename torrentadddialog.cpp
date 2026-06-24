#include "torrentadddialog.h"
#include "ui_torrentadddialog.h"

#include <QHash>
#include <QHeaderView>
#include <QLocale>
#include <QObject>
#include <QStringList>
#include <QTreeWidgetItem>

namespace {

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
        const qint64 currentSize = item->data(0, Qt::UserRole).toLongLong();
        item->setData(0, Qt::UserRole, currentSize + size);
        item = item->parent();
    }
}

void updateFolderSizes(QTreeWidgetItem *item)
{
    if (!item)
        return;

    if (item->childCount() > 0)
        item->setText(1, formatByteSize(item->data(0, Qt::UserRole).toLongLong()));

    for (int i = 0; i < item->childCount(); ++i)
        updateFolderSizes(item->child(i));
}

} // namespace

TorrentAddDialog::TorrentAddDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TorrentAddDialog)
{
    ui->setupUi(this);

    ui->treeTorrentContents->setHeaderLabels({
        tr("Name"),
        tr("Size")
    });
    ui->treeTorrentContents->header()->setStretchLastSection(false);
    ui->treeTorrentContents->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->treeTorrentContents->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
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
    ui->treeTorrentContents->clear();
    ui->groupContents->show();

    if (!metadata.isValid()) {
        ui->labelContentsSummary->setText(
            tr("Torrent contents could not be read: %1")
                .arg(metadata.errorString)
        );
        ui->treeTorrentContents->hide();
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
                folderItem = new QTreeWidgetItem(QStringList{parts.at(i), QString()});
                folderItem->setData(0, Qt::UserRole, qint64(0));

                if (parentItem)
                    parentItem->addChild(folderItem);
                else
                    ui->treeTorrentContents->addTopLevelItem(folderItem);

                folderItems.insert(folderKey, folderItem);
            }

            parentItem = folderItem;
        }

        auto *fileItem = new QTreeWidgetItem(QStringList{
            parts.last(),
            formatByteSize(file.length)
        });
        fileItem->setData(0, Qt::UserRole, file.length);
        fileItem->setData(0, Qt::UserRole + 1, file.index);
        fileItem->setToolTip(0, file.path);

        if (parentItem) {
            parentItem->addChild(fileItem);
            addSizeToItem(parentItem, file.length);
        } else {
            ui->treeTorrentContents->addTopLevelItem(fileItem);
        }
    }

    for (int i = 0; i < ui->treeTorrentContents->topLevelItemCount(); ++i)
        updateFolderSizes(ui->treeTorrentContents->topLevelItem(i));

    ui->treeTorrentContents->expandToDepth(0);
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