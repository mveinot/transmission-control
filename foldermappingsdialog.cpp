#include "foldermappingsdialog.h"
#include "ui_foldermappingsdialog.h"

FolderMappingsDialog::FolderMappingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FolderMappingsDialog)
{
    ui->setupUi(this);
}

FolderMappingsDialog::~FolderMappingsDialog()
{
    delete ui;
}
