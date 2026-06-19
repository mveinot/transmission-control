#ifndef TORRENT_H
#define TORRENT_H

#include <QObject>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QJsonArray>
#include <QSet>
#include <QUrl>

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
    double getPercentDone() const;
    QString getStatus() const;
    QString getRateDownload() const;
    QString getRateUpload() const;
    QString getUploadRatio() const;
    QString getEta() const;
    QString getSize() const;
    qint64 getSizeBytes() const;
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
    double percentDone = 0.0;
    double rateDownload = 0.0;
    double rateUpload = 0.0;
    double uploadRatio = 0.0;
    QJsonValue files;
    QJsonValue peers;
    Status status = Status::Unknown;
    int eta = 0;
    double sizeWhenDone = 0.0;
    int queuePosition = 0;
    QString primaryTrackerHost;
    QStringList trackerHosts;

signals:

};

#endif // TORRENT_H
