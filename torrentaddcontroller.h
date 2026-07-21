#ifndef TORRENTADDCONTROLLER_H
#define TORRENTADDCONTROLLER_H

#include "torrentadddialog.h"

#include <QObject>
#include <QString>
#include <QStringList>

class QWidget;
class rpc_client;

// Orchestrates validation, metadata preview, remembered add options, and the
// final RPC call for file and magnet sources.
class TorrentAddController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentAddController(rpc_client *client,
                                  QWidget *dialogParent,
                                  QObject *parent = nullptr);

    void setDefaultDownloadDir(const QString &downloadDir);

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
    rpc_client *m_client = nullptr;
    QWidget *m_dialogParent = nullptr;

    QString m_defaultDownloadDir;

    bool deleteTorrentFileOnSuccessfulAdd() const;
    bool promptAndAdd(TorrentAddDialog::SourceType sourceType,
                      const QString &source);

    QString savedDownloadDir() const;
    bool savedStartPaused() const;

    void saveOptions(const QString &downloadDir, bool startPaused);
};

#endif // TORRENTADDCONTROLLER_H
