#include "torrent.h"

torrent::torrent(const QJsonValue &val)
{
    const QJsonObject obj = val.toObject();

    id = obj.value("id").toInt();
    name = obj.value("name").toString();
    eta = obj.value("eta").toInt();
    percentDone = obj.value("percentDone").toDouble() * 100.0;
    status = statusFromInt(obj.value("status").toInt());
    rateDownload = obj.value("rateDownload").toDouble();
    rateUpload = obj.value("rateUpload").toDouble();
    uploadRatio = obj.value("uploadRatio").toDouble();
    files = obj.value("files");
    peers = obj.value("peers");
    sizeWhenDone = obj.value("sizeWhenDone").toDouble();
    queuePosition = obj.value("queuePosition").toInt();
}

torrent::Status torrent::statusFromInt(int value)
{
    switch (value) {
    case 0: return Status::Paused;
    case 1: return Status::WaitingToVerify;
    case 2: return Status::Verifying;
    case 3: return Status::Queued;
    case 4: return Status::Downloading;
    case 5: return Status::WaitingToSeed;
    case 6: return Status::Seeding;
    default: return Status::Unknown;
    }
}

int torrent::getId() const { return id; }
QString torrent::getName() const { return name; }
double torrent::getPercentDone() const { return percentDone; }
int torrent::getQueuePosition() const { return queuePosition; };

QString torrent::statusToString(Status status)
{
    switch (status) {
    case Status::Paused:
        return "Paused";
    case Status::WaitingToVerify:
        return "Waiting to Verify";
    case Status::Verifying:
        return "Verifying";
    case Status::Queued:
        return "Queued";
    case Status::Downloading:
        return "Downloading";
    case Status::WaitingToSeed:
        return "Waiting to Seed";
    case Status::Seeding:
        return "Seeding";
    case Status::Unknown:
    default:
        return "Unknown";
    }
}

QString torrent::getStatus() const
{
    return statusToString(status);
}

QString torrent::getRateDownload() const
{
    if (rateDownload == 0)
        return "";

    QLocale cLocale(QLocale::C);
    QString rateDownloadStr = cLocale.formattedDataSize(rateDownload)+"/s";
    return rateDownloadStr;
}

QString torrent::getRateUpload() const
{
    if (rateUpload == 0)
        return "";

    QLocale cLocale(QLocale::C);
    QString rateUploadStr = cLocale.formattedDataSize(rateUpload)+"/s";
    return rateUploadStr;
}

QString torrent::getUploadRatio() const
{
    return QString("%1").arg(uploadRatio,5, 'f', 3);
}

QString torrent::getSize() const
{
    //return QString("%1").arg(sizeWhenDone,5, 'f', 1);
    return QLocale().formattedDataSize(
        sizeWhenDone,
        1,
        QLocale::DataSizeIecFormat
        );
}

qint64 torrent::getSizeBytes() const
{
    return sizeWhenDone;
}

QString torrent::getEta() const
{
    int seconds = 0;
    int minutes = 0;
    int hours = 0;
    int days = 0;
    int n = eta;

    if (n <= 0)
        return "";

    days = n / (24 * 3600);
    n = n % (24 * 3600);
    hours = n / 3600;
    n %= 3600;
    minutes = n / 60 ;
    n %= 60;
    seconds = n;

    if (days > 1)
        return QString::number(days)+"d "+QString::number(hours)+"h";

    if (hours > 1)
        return QString::number(hours)+"h "+QString::number(minutes)+"m";

    if (minutes > 1)
        return QString::number(minutes)+"m "+QString::number(seconds)+"s";

    return QString::number(seconds)+"s";
}

QJsonArray torrent::getFiles() const
{
    return files.toArray();
}

QJsonArray torrent::getPeers() const
{
    return peers.toArray();
}

bool torrent::sameDisplayData(const torrent &other) const
{
    return id == other.id
           && name == other.name
           && percentDone == other.percentDone
           && rateDownload == other.rateDownload
           && rateUpload == other.rateUpload
           && uploadRatio == other.uploadRatio
           && status == other.status
           && eta == other.eta
           && sizeWhenDone == other.sizeWhenDone;
}


