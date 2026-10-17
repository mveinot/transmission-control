#ifndef TORRENTADDCONTROLLER_H
#define TORRENTADDCONTROLLER_H

#include "torrentadddialog.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QQueue>

class QWidget;
class TorrentBackend;

// Orchestrates validation, metadata preview, remembered add options, and the
// final RPC call for file and magnet sources.
class TorrentAddController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentAddController(TorrentBackend *client,
                                  QWidget *dialogParent,
                                  QObject *parent = nullptr);

    void setDefaultDownloadDir(const QString &downloadDir);
    QStringList userDefinedDownloadDirs() const;
    bool setUserDefinedDownloadDirs(const QStringList &downloadDirs);

public slots:
    void addTorrentFile(const QString &filePath);
    void addTorrentFiles(const QStringList &filePaths);
    void addMagnetLink(const QString &magnetLink);
    void addTorrentFileUsingDefaults(const QString &filePath);

signals:
    void addStarted();
    void addCancelled();
    void addFailed(const QString &message);

private:
    struct PendingAdd {
        QString downloadDir;
        bool userDefinedDownloadDir = false;
        QString originalName;
        QString newName;
    };

    TorrentBackend *m_client = nullptr;
    QWidget *m_dialogParent = nullptr;

    QString m_defaultDownloadDir;
    QStringList m_knownDownloadDirs;
    QQueue<PendingAdd> m_pendingAdds;

    bool deleteTorrentFileOnSuccessfulAdd() const;
    bool showOptionsDialog(TorrentAddDialog::SourceType sourceType) const;
    bool promptAndAdd(TorrentAddDialog::SourceType sourceType,
                      const QString &source);
    bool addUsingDefaults(TorrentAddDialog::SourceType sourceType,
                          const QString &source);

    QString savedDownloadDir() const;
    QString lastAcceptedDownloadDir() const;
    QStringList acceptedDownloadDirs() const;
    bool savedStartPaused() const;
    QString downloadDirSettingKey() const;
    QString lastAcceptedDownloadDirSettingKey() const;
    QString acceptedDownloadDirsSettingKey() const;

    void saveOptions(bool startPaused);
    void submitAdd(TorrentAddDialog::SourceType sourceType,
                   const QString &source,
                   const QString &downloadDir,
                   bool startPaused,
                   const QList<int> &unwantedFileIndices = {},
                   const QList<int> &lowPriorityFileIndices = {},
                   const QList<int> &highPriorityFileIndices = {},
                   bool userDefinedDownloadDir = false,
                   const QString &originalName = {},
                   const QString &newName = {});
};

#endif // TORRENTADDCONTROLLER_H
