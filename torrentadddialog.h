#ifndef TORRENTADDDIALOG_H
#define TORRENTADDDIALOG_H

#include <QDialog>
#include <QList>
#include <QString>

#include "torrentmetadataparser.h"

namespace Ui {
class TorrentAddDialog;
}

class QComboBox;
class QTreeWidgetItem;

class TorrentAddDialog : public QDialog
{
    Q_OBJECT

public:
    enum class SourceType {
        TorrentFile,
        MagnetLink
    };

    explicit TorrentAddDialog(QWidget *parent = nullptr);
    ~TorrentAddDialog() override;

    void setSource(SourceType type, const QString &source);
    void setDownloadDir(const QString &downloadDir);
    void setStartPaused(bool paused);
    void setRememberOptions(bool remember);
    void setTorrentMetadata(const TorrentMetadata &metadata);
    void clearTorrentMetadata();

    SourceType sourceType() const;
    QString source() const;
    QString downloadDir() const;
    bool startPaused() const;
    bool rememberOptions() const;

    QList<int> unwantedFileIndices() const;
    QList<int> lowPriorityFileIndices() const;
    QList<int> highPriorityFileIndices() const;

private:
    Ui::TorrentAddDialog *ui = nullptr;

    SourceType m_sourceType = SourceType::TorrentFile;
    QString m_source;

    bool m_updatingPriorities = false;

    void setupContentsTree();
    void addTorrentFileItem(const TorrentFileMetadata &file,
                            QTreeWidgetItem *parentItem,
                            const QString &displayName);
    void addTorrentFolderItem(QTreeWidgetItem *item);

    QComboBox *createPriorityCombo(int priority = 0);
    void setPriorityForChildren(QTreeWidgetItem *item, int priority);

    QList<int> fileIndicesForPriority(int priority) const;
};

#endif // TORRENTADDDIALOG_H
