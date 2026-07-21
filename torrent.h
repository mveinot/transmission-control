#ifndef TORRENT_H
#define TORRENT_H

#include <QObject>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QJsonArray>
#include <QSet>
#include <QUrl>

// Immutable-by-convention snapshot of the fields used by the torrent list and
// filters. Construct a replacement from each RPC snapshot rather than patching.
class torrent
{

public:
    enum class Status : int
    {
        Paused = 0,
        WaitingToVerify = 1,
        Verifying = 2,
        Queued = 3,
        Downloading = 4,
        WaitingToSeed = 5,
        Seeding = 6,
        Unknown = -1
    };
    explicit torrent(const QJsonValue &val);
    QString getName() const;
    QString getHashString() const;
    double getPercentDone() const;
    QString getStatus() const;
    int getStatusValue() const;
    bool hasError() const;
    bool isStalled() const;
    int getErrorCode() const;
    QString getErrorString() const;
    QString getRateDownload() const;
    double getRateDownloadBytesPerSecond() const;
    QString getRateUpload() const;
    double getRateUploadBytesPerSecond() const;
    QString getUploadRatio() const;
    double getUploadRatioValue() const;
    QString getEta() const;
    int getEtaSeconds() const;
    QString getSize() const;
    qint64 getSizeBytes() const;
    QString getAddedDate() const;
    qint64 getAddedDateSecs() const;
    QString getDownloadedEver() const;
    qint64 getDownloadedEverBytes() const;
    QString getUploadedEver() const;
    qint64 getUploadedEverBytes() const;
    QString getDownloadDir() const;
    QString getSeedsSummary() const;
    int getConnectedSeeds() const;
    int getTotalSeeds() const;
    QString getPeersSummary() const;
    int getConnectedPeers() const;
    int getTotalPeers() const;
    qint64 getSeedsSortValue() const;
    qint64 getPeersSortValue() const;
    QString getHealth() const;
    int getHealthScore() const;
    QString getHealthDetails() const;
    int getQueuePosition() const;
    bool sameDisplayData(const torrent &other) const;
    int getId() const;
    QJsonArray getFiles() const;
    QJsonArray getPeers() const;
    QString getPrimaryTrackerHost() const;
    QStringList getTrackerHosts() const;

private:
    static Status statusFromInt(int value);
    static QString statusToString(Status status);
    int id = 0;
    QString name;
    QString hashString;
    double percentDone = 0.0;
    double rateDownload = 0.0;
    double rateUpload = 0.0;
    double uploadRatio = 0.0;
    QJsonValue files;
    QJsonValue peers;
    Status status = Status::Unknown;
    int eta = 0;
    double sizeWhenDone = 0.0;
    qint64 addedDate = 0;
    qint64 downloadedEver = 0;
    qint64 uploadedEver = 0;
    QString downloadDir;
    int errorCode = 0;
    QString errorString;
    bool stalled = false;
    int peersConnected = 0;
    int peersSendingToUs = 0;
    int peersGettingFromUs = 0;
    int totalSeeders = -1;
    int totalLeechers = -1;
    int queuePosition = 0;
    qint64 desiredAvailable = 0;
    qint64 leftUntilDone = 0;
    QString primaryTrackerHost;
    QStringList trackerHosts;

signals:

};

#endif // TORRENT_H
