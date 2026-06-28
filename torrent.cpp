#include "torrent.h"
#include <QDateTime>
#include <algorithm>
#include <QUrl>
#include <QtGlobal>

static QString normalizedTrackerHost(QString host)
{
    host = host.trimmed().toLower();

    if (host.isEmpty())
        return QString();

    // If this is actually a URL, parse it as one.
    if (host.contains(QStringLiteral("://"))) {
        const QUrl url(host);
        return url.host().toLower();
    }

    // trackerStats["host"] may be "example.com:6969".
    // QUrl needs a scheme to parse host/port reliably.
    const QUrl url(QStringLiteral("scheme://") + host);

    if (!url.host().isEmpty())
        return url.host().toLower();

    return host;
}

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
    addedDate = static_cast<qint64>(obj.value("addedDate").toDouble());
    downloadedEver = static_cast<qint64>(obj.value("downloadedEver").toDouble());
    uploadedEver = static_cast<qint64>(obj.value("uploadedEver").toDouble());
    downloadDir = obj.value("downloadDir").toString().trimmed();
    peersConnected = obj.value("peersConnected").toInt();
    peersSendingToUs = obj.value("peersSendingToUs").toInt();
    peersGettingFromUs = obj.value("peersGettingFromUs").toInt();
    queuePosition = obj.value("queuePosition").toInt();

    trackerHosts.clear();
    primaryTrackerHost.clear();

    QSet<QString> uniqueHosts;

    auto addTrackerHost = [&uniqueHosts, this](const QString &host) {
        const QString normalized = normalizedTrackerHost(host);

        if (normalized.isEmpty())
            return;

        if (uniqueHosts.contains(normalized))
            return;

        uniqueHosts.insert(normalized);
        trackerHosts.append(normalized);
    };

    const QJsonArray trackersArray =
        obj.value(QStringLiteral("trackers")).toArray();

    for (const QJsonValue &trackerValue : trackersArray) {
        const QJsonObject trackerObject = trackerValue.toObject();

        QString announce =
            trackerObject.value(QStringLiteral("announce")).toString();

        if (announce.isEmpty()) {
            announce =
                trackerObject.value(QStringLiteral("scrape")).toString();
        }

        const QString host = QUrl(announce).host();
        addTrackerHost(host);
    }

    const QJsonArray trackerStatsArray =
        obj.value(QStringLiteral("trackerStats")).toArray();

    for (const QJsonValue &trackerValue : trackerStatsArray) {
        const QJsonObject trackerObject = trackerValue.toObject();

        QString host =
            trackerObject.value(QStringLiteral("host")).toString();

        if (host.isEmpty()) {
            const QString announce =
                trackerObject.value(QStringLiteral("announce")).toString();

            host = QUrl(announce).host();
        }

        addTrackerHost(host);

        const int seeders =
            trackerObject.value(QStringLiteral("seederCount")).toInt(-1);
        const int leechers =
            trackerObject.value(QStringLiteral("leecherCount")).toInt(-1);

        if (seeders >= 0)
            totalSeeders = qMax(totalSeeders, seeders);

        if (leechers >= 0)
            totalLeechers = qMax(totalLeechers, leechers);
    }

    std::sort(trackerHosts.begin(), trackerHosts.end());

    if (!trackerHosts.isEmpty())
        primaryTrackerHost = trackerHosts.first();
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

double torrent::getRateDownloadBytesPerSecond() const
{
    return rateDownload;
}

QString torrent::getRateUpload() const
{
    if (rateUpload == 0)
        return "";

    QLocale cLocale(QLocale::C);
    QString rateUploadStr = cLocale.formattedDataSize(rateUpload)+"/s";
    return rateUploadStr;
}

double torrent::getRateUploadBytesPerSecond() const
{
    return rateUpload;
}

QString torrent::getUploadRatio() const
{
    return QString("%1").arg(uploadRatio,5, 'f', 3);
}

double torrent::getUploadRatioValue() const
{
    return uploadRatio;
}

QString torrent::getSize() const
{
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

QString torrent::getAddedDate() const
{
    if (addedDate <= 0)
        return QString();

    return QDateTime::fromSecsSinceEpoch(addedDate).toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

qint64 torrent::getAddedDateSecs() const
{
    return addedDate;
}

QString torrent::getDownloadedEver() const
{
    if (downloadedEver <= 0)
        return QString();

    return QLocale().formattedDataSize(
        downloadedEver,
        1,
        QLocale::DataSizeIecFormat
        );
}

qint64 torrent::getDownloadedEverBytes() const
{
    return downloadedEver;
}

QString torrent::getUploadedEver() const
{
    if (uploadedEver <= 0)
        return QString();

    return QLocale().formattedDataSize(
        uploadedEver,
        1,
        QLocale::DataSizeIecFormat
        );
}

qint64 torrent::getUploadedEverBytes() const
{
    return uploadedEver;
}

QString torrent::getDownloadDir() const
{
    return downloadDir;
}

namespace {

QString connectedTotalSummary(int connected, int total)
{
    if (total >= 0)
        return QStringLiteral("%1/%2").arg(connected).arg(total);

    if (connected > 0)
        return QStringLiteral("%1/?").arg(connected);

    return QString();
}

qint64 connectedTotalSortValue(int connected, int total)
{
    const qint64 safeTotal = total >= 0 ? total : 0;
    return (static_cast<qint64>(connected) << 32) | safeTotal;
}

} // namespace

QString torrent::getSeedsSummary() const
{
    return connectedTotalSummary(peersSendingToUs, totalSeeders);
}

int torrent::getConnectedSeeds() const
{
    return peersSendingToUs;
}

int torrent::getTotalSeeds() const
{
    return totalSeeders;
}

QString torrent::getPeersSummary() const
{
    return connectedTotalSummary(peersGettingFromUs, totalLeechers);
}

int torrent::getConnectedPeers() const
{
    return peersGettingFromUs;
}

int torrent::getTotalPeers() const
{
    return totalLeechers;
}

qint64 torrent::getSeedsSortValue() const
{
    return connectedTotalSortValue(peersSendingToUs, totalSeeders);
}

qint64 torrent::getPeersSortValue() const
{
    return connectedTotalSortValue(peersGettingFromUs, totalLeechers);
}

int torrent::getEtaSeconds() const
{
    return eta;
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
           && sizeWhenDone == other.sizeWhenDone
           && addedDate == other.addedDate
           && downloadedEver == other.downloadedEver
           && uploadedEver == other.uploadedEver
           && downloadDir == other.downloadDir
           && peersConnected == other.peersConnected
           && peersSendingToUs == other.peersSendingToUs
           && peersGettingFromUs == other.peersGettingFromUs
           && totalSeeders == other.totalSeeders
           && totalLeechers == other.totalLeechers
           && queuePosition == other.queuePosition
           && primaryTrackerHost == other.primaryTrackerHost
           && trackerHosts == other.trackerHosts;
}

QString torrent::getPrimaryTrackerHost() const
{
    return primaryTrackerHost;
}

QStringList torrent::getTrackerHosts() const
{
    return trackerHosts;
}