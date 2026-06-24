#ifndef FOLDERMAPPINGSDIALOG_H
#define FOLDERMAPPINGSDIALOG_H

#include "foldermapping.h"

#include <QDialog>
#include <QList>

namespace Ui {
class FolderMappingsDialog;
}

class FolderMappingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FolderMappingsDialog(QWidget *parent = nullptr);
    ~FolderMappingsDialog() override;

    void setServerName(const QString &serverName);

    void setMappings(const QList<FolderMapping> &mappings);
    QList<FolderMapping> mappings() const;

private slots:
    void addMapping();
    void removeSelectedMapping();
    void browseLocalPath();
    void updateButtonStates();

private:
    void setupTable();
    void addMappingRow(const QString &remotePath = QString(),
                       const QString &localPath = QString());
    bool validateMappings();
    QString normalizedPathForDisplay(const QString &path) const;

    Ui::FolderMappingsDialog *ui;
};

#endif // FOLDERMAPPINGSDIALOG_H
