#include "torrent.h"

torrent::torrent(QJsonValue val, QObject *parent)
{
    id = val["id"].toInt();
    name = val["name"].toString();
    eta = val["eta"].toInt();
    percentDone = val["percentDone"].toDouble()*100;
    status = val["status"].toInt(0);
    rateDownload = val["rateDownload"].toDouble();
    rateUpload = val["rateUpload"].toDouble();
    uploadRatio = val["uploadRatio"].toDouble();
    files = val["files"];
    peers = val["peers"];
}

QString torrent::getName()
{
    return name;
}

double torrent::getPercentDone()
{
    return percentDone;
}

QString torrent::getStatus()
{
    switch(status) {
    case 0:
        return "Paused";
    case 1:
        return "Waiting to Verify";
    case 2:
        return "Verifying";
    case 3:
        return "Queued";
    case 4:
        return "Downloading";
    case 5:
        return "Waiting to Seed";
    case 6:
        return "Seeding";
    default:
        return "Unknown";
    }

    return "Error";
}

QString torrent::getRateDownload()
{
    if (rateDownload == 0)
        return "";

    QLocale cLocale(QLocale::C);
    QString rateDownloadStr = cLocale.formattedDataSize(rateDownload)+"/s";
    return rateDownloadStr;
}

QString torrent::getRateUpload()
{
    if (rateUpload == 0)
        return "";

    QLocale cLocale(QLocale::C);
    QString rateUploadStr = cLocale.formattedDataSize(rateUpload)+"/s";
    return rateUploadStr;
}

QString torrent::getUploadRatio()
{
    //QString ret;
    //ret.spr
    return QString("%1").arg(uploadRatio,5, 'f', 3);
}

int torrent::getId()
{
    return id;
}

QString torrent::getEta()
{
    int seconds = 0;
    int minutes = 0;
    int hours = 0;
    int days = 0;
    int n = eta;

    if (n <= 0)
        return "";

    //qDebug() << "eta" << n;

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
