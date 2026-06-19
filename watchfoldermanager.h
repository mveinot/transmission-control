#ifndef WATCHFOLDERMANAGER_H
#define WATCHFOLDERMANAGER_H

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QtCore/qfileinfo.h>

class WatchFolderManager : public QObject
{
    Q_OBJECT

public:
    explicit WatchFolderManager(QObject *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setWatchFolder(const QString &folderPath);
    QString watchFolder() const;

    void setScanIntervalMs(int intervalMs);
    int scanIntervalMs() const;

    void setRequiredStableChecks(int checks);
    int requiredStableChecks() const;

    void clearProcessedHistory();

signals:
    void torrentFileReady(const QString &filePath);
    void statusMessage(const QString &message);
    void warningMessage(const QString &message);

private slots:
    void handleDirectoryChanged(const QString &path);
    void scanWatchFolder();

private:
    struct CandidateFile
    {
        qint64 size = -1;
        QDateTime lastModified;
        int stableChecks = 0;
    };

    QFileSystemWatcher m_watcher;
    QTimer m_scanTimer;

    QString m_watchFolder;
    bool m_enabled = false;

    int m_scanIntervalMs = 1000;
    int m_requiredStableChecks = 2;

    QHash<QString, CandidateFile> m_candidates;
    QSet<QString> m_processedFingerprints;

    void restartWatcher();
    void stopWatcher();

    QStringList torrentFilesInWatchFolder() const;
    bool isTorrentFile(const QString &filePath) const;

    void observeCandidate(const QString &filePath);
    bool candidateIsStable(const QString &filePath,
                           const QFileInfo &fileInfo,
                           CandidateFile &candidate);

    QString fingerprintForFile(const QString &filePath,
                               const QFileInfo &fileInfo) const;

    bool alreadyProcessed(const QString &fingerprint) const;
    void markProcessed(const QString &fingerprint);

    void loadProcessedFingerprints();
    void saveProcessedFingerprints() const;
};

#endif // WATCHFOLDERMANAGER_H