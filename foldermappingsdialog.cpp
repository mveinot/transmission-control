#include "foldermappingsdialog.h"
#include "ui_foldermappingsdialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace {
constexpr int RemotePathColumn = 0;
constexpr int LocalPathColumn = 1;
}

FolderMappingsDialog::FolderMappingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FolderMappingsDialog)
{
    ui->setupUi(this);

    setupTable();

    connect(ui->addButton, &QPushButton::clicked,
            this, &FolderMappingsDialog::addMapping);

    connect(ui->removeButton, &QPushButton::clicked,
            this, &FolderMappingsDialog::removeSelectedMapping);

    connect(ui->browseLocalButton, &QPushButton::clicked,
            this, &FolderMappingsDialog::browseLocalPath);

    connect(ui->mappingsTable, &QTableWidget::itemSelectionChanged,
            this, &FolderMappingsDialog::updateButtonStates);

    connect(ui->mappingsTable, &QTableWidget::itemChanged,
            this, &FolderMappingsDialog::updateButtonStates);

    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, [this]() {
                if (validateMappings())
                    accept();
            });

    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &FolderMappingsDialog::reject);

    updateButtonStates();
}

FolderMappingsDialog::~FolderMappingsDialog()
{
    delete ui;
}

void FolderMappingsDialog::setServerName(const QString &serverName)
{
    const QString trimmedServerName = serverName.trimmed();

    if (trimmedServerName.isEmpty()) {
        ui->serverLabel->setText(
            tr("Configure remote-to-local folder mappings for this server.")
        );
        return;
    }

    ui->serverLabel->setText(
        tr("Configure remote-to-local folder mappings for \u201c%1\u201d.")
            .arg(trimmedServerName)
    );
}

void FolderMappingsDialog::setupTable()
{
    ui->mappingsTable->setColumnCount(2);

    ui->mappingsTable->setHorizontalHeaderLabels({
        tr("Remote path"),
        tr("Local path")
    });

    ui->mappingsTable->horizontalHeader()->setStretchLastSection(true);
    ui->mappingsTable->horizontalHeader()->setSectionResizeMode(
        RemotePathColumn,
        QHeaderView::Stretch
    );
    ui->mappingsTable->horizontalHeader()->setSectionResizeMode(
        LocalPathColumn,
        QHeaderView::Stretch
    );

    ui->mappingsTable->verticalHeader()->setVisible(false);
    ui->mappingsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->mappingsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->mappingsTable->setEditTriggers(
        QAbstractItemView::DoubleClicked
        | QAbstractItemView::SelectedClicked
        | QAbstractItemView::EditKeyPressed
    );
}

void FolderMappingsDialog::setMappings(const QList<FolderMapping> &mappings)
{
    ui->mappingsTable->blockSignals(true);
    ui->mappingsTable->setRowCount(0);

    for (const FolderMapping &mapping : mappings)
        addMappingRow(mapping.remotePath, mapping.localPath);

    ui->mappingsTable->blockSignals(false);

    updateButtonStates();
}

QList<FolderMapping> FolderMappingsDialog::mappings() const
{
    QList<FolderMapping> result;

    for (int row = 0; row < ui->mappingsTable->rowCount(); ++row) {
        const QTableWidgetItem *remoteItem =
            ui->mappingsTable->item(row, RemotePathColumn);

        const QTableWidgetItem *localItem =
            ui->mappingsTable->item(row, LocalPathColumn);

        if (!remoteItem || !localItem)
            continue;

        FolderMapping mapping;
        mapping.remotePath = remoteItem->text().trimmed();
        mapping.localPath = localItem->text().trimmed();

        if (!mapping.remotePath.isEmpty() || !mapping.localPath.isEmpty())
            result.append(mapping);
    }

    return result;
}

void FolderMappingsDialog::addMapping()
{
    addMappingRow();

    const int row = ui->mappingsTable->rowCount() - 1;
    ui->mappingsTable->setCurrentCell(row, RemotePathColumn);
    ui->mappingsTable->editItem(ui->mappingsTable->item(row, RemotePathColumn));

    updateButtonStates();
}

void FolderMappingsDialog::addMappingRow(const QString &remotePath,
                                         const QString &localPath)
{
    const int row = ui->mappingsTable->rowCount();
    ui->mappingsTable->insertRow(row);

    auto *remoteItem =
        new QTableWidgetItem(normalizedPathForDisplay(remotePath));

    auto *localItem =
        new QTableWidgetItem(normalizedPathForDisplay(localPath));

    ui->mappingsTable->setItem(row, RemotePathColumn, remoteItem);
    ui->mappingsTable->setItem(row, LocalPathColumn, localItem);
}

void FolderMappingsDialog::removeSelectedMapping()
{
    const int row = ui->mappingsTable->currentRow();

    if (row < 0)
        return;

    ui->mappingsTable->removeRow(row);
    updateButtonStates();
}

void FolderMappingsDialog::browseLocalPath()
{
    int row = ui->mappingsTable->currentRow();

    if (row < 0) {
        addMappingRow();
        row = ui->mappingsTable->rowCount() - 1;
        ui->mappingsTable->setCurrentCell(row, LocalPathColumn);
    }

    QTableWidgetItem *localItem =
        ui->mappingsTable->item(row, LocalPathColumn);

    QString initialPath;

    if (localItem)
        initialPath = localItem->text().trimmed();

    const QString selectedPath =
        QFileDialog::getExistingDirectory(
            this,
            tr("Select Local Folder"),
            initialPath
        );

    if (selectedPath.isEmpty())
        return;

    if (!localItem) {
        localItem = new QTableWidgetItem;
        ui->mappingsTable->setItem(row, LocalPathColumn, localItem);
    }

    localItem->setText(QDir::cleanPath(selectedPath));

    updateButtonStates();
}

void FolderMappingsDialog::updateButtonStates()
{
    const bool hasSelection = ui->mappingsTable->currentRow() >= 0;

    ui->removeButton->setEnabled(hasSelection);
    ui->browseLocalButton->setEnabled(true);
}

bool FolderMappingsDialog::validateMappings()
{
    const QList<FolderMapping> currentMappings = mappings();

    for (int i = 0; i < currentMappings.size(); ++i) {
        const FolderMapping &mapping = currentMappings.at(i);

        if (mapping.remotePath.trimmed().isEmpty()) {
            QMessageBox::warning(
                this,
                tr("Invalid Folder Mapping"),
                tr("Remote path cannot be empty.")
            );

            ui->mappingsTable->setCurrentCell(i, RemotePathColumn);
            return false;
        }

        if (mapping.localPath.trimmed().isEmpty()) {
            QMessageBox::warning(
                this,
                tr("Invalid Folder Mapping"),
                tr("Local path cannot be empty.")
            );

            ui->mappingsTable->setCurrentCell(i, LocalPathColumn);
            return false;
        }

        for (int j = i + 1; j < currentMappings.size(); ++j) {
            if (QDir::cleanPath(mapping.remotePath)
                == QDir::cleanPath(currentMappings.at(j).remotePath)) {
                QMessageBox::warning(
                    this,
                    tr("Duplicate Folder Mapping"),
                    tr("The remote path \u201c%1\u201d is mapped more than once.")
                        .arg(mapping.remotePath)
                );

                ui->mappingsTable->setCurrentCell(j, RemotePathColumn);
                return false;
            }
        }

        if (!QDir(mapping.localPath).exists()) {
            const QMessageBox::StandardButton choice =
                QMessageBox::question(
                    this,
                    tr("Local Folder Not Found"),
                    tr("The local folder does not exist:\n\n%1\n\n"
                       "Save this mapping anyway?")
                        .arg(mapping.localPath),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                );

            if (choice != QMessageBox::Yes) {
                ui->mappingsTable->setCurrentCell(i, LocalPathColumn);
                return false;
            }
        }
    }

    return true;
}

QString FolderMappingsDialog::normalizedPathForDisplay(const QString &path) const
{
    const QString trimmed = path.trimmed();

    if (trimmed.isEmpty())
        return {};

    return QDir::cleanPath(trimmed);
}
