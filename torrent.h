#ifndef TORRENT_H
#define TORRENT_H

#include <QObject>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>

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
    bool sameDisplayData(const torrent &other) const;
    int getId() const;

private:
    static Status statusFromInt(int value);
    static QString statusToString(Status status);
    int id = 0;
    QString name;
    double percentDone = 0.0;
    double rateDownload = 0.0;
    double rateUpload = 0.0;
    double uploadRatio =0.0;
    QJsonValue files;
    QJsonValue peers;
    Status status = Status::Unknown;
    int eta = 0;

signals:

};

#endif // TORRENT_H
