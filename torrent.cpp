#include "torrent.h"
#include <QUrl>

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
    }

    std::sort(trackerHosts.begin(), trackerHosts.end());

    if (!trackerHosts.isEmpty())
        primaryTrackerHost = trackerHosts.first();

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
           && sizeWhenDone == other.sizeWhenDone
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