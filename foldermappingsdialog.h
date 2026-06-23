#ifndef FOLDERMAPPINGSDIALOG_H
#define FOLDERMAPPINGSDIALOG_H

#include <QDialog>

namespace Ui {
class FolderMappingsDialog;
}

class FolderMappingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FolderMappingsDialog(QWidget *parent = nullptr);
    ~FolderMappingsDialog();

private:
    Ui::FolderMappingsDialog *ui;
};

#endif // FOLDERMAPPINGSDIALOG_H
