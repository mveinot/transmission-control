#ifndef TORRENTDOMAIN_H
#define TORRENTDOMAIN_H

#include "torrentkey.h"

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

// Backend-neutral snapshots passed across the service/UI boundary. Transport
// adapters normalize their native field names and units before constructing
// these values.
struct TorrentDetails
{
    TorrentKey key;
    QString name;
    QString comment;
    QString creator;
    QString downloadDirectory;
    QString hashString;
    QString magnetLink;
    qint64 totalSize = 0;
    qint64 creationTime = 0;
    bool sequentialDownload = false;
    bool hasSequentialDownload = false;
    int bandwidthPriority = 0;
    bool hasBandwidthPriority = false;
    // Preserves normalized optional fields for the technical Details tab
    // without exposing a backend's transport representation.
    QVariantMap fields;
};

struct TorrentFile
{
    int index = -1;
    QString path;
    qint64 length = 0;
    qint64 bytesCompleted = 0;
    bool wanted = true;
    int priority = 0;
};

struct TorrentFiles
{
    TorrentKey key;
    QString downloadDirectory;
    QVector<TorrentFile> files;
};

struct TorrentPeer
{
    QString address;
    int port = 0;
    QString clientName;
    double progress = 0.0;
    qint64 downloadRate = 0;
    qint64 uploadRate = 0;
    bool encrypted = false;
    bool incoming = false;
};

struct TorrentPeers
{
    TorrentKey key;
    QVector<TorrentPeer> peers;
};

struct TorrentTracker
{
    int id = -1;
    int tier = -1;
    QString host;
    QString siteName;
    QString announceUrl;
    QString scrapeUrl;
    int announceState = -1;
    int scrapeState = -1;
    int seederCount = -1;
    int leecherCount = -1;
    int downloadCount = -1;
    qint64 lastAnnounceTime = 0;
    qint64 nextAnnounceTime = 0;
    qint64 lastScrapeTime = 0;
    qint64 nextScrapeTime = 0;
    QString lastAnnounceResult;
    QString lastScrapeResult;
    bool lastAnnounceSucceeded = false;
    bool lastAnnounceTimedOut = false;
    bool lastScrapeSucceeded = false;
    bool lastScrapeTimedOut = false;
};

struct TorrentTrackers
{
    TorrentKey key;
    QVector<TorrentTracker> trackers;
};

struct TorrentPieces
{
    TorrentKey key;
    int pieceCount = 0;
    QByteArray completedPieces;
    double percentDone = 0.0;
};

struct TorrentProperties
{
    TorrentKey key;
    QString name;
    QString hashString;
    int bandwidthPriority = 0;
    bool honorsSessionLimits = true;
    int queuePosition = 0;
    int peerLimit = -1;
    bool downloadLimited = false;
    int downloadLimit = 0;
    bool uploadLimited = false;
    int uploadLimit = 0;
    int seedRatioMode = 0;
    double seedRatioLimit = 0.0;
    int seedIdleMode = 0;
    int seedIdleLimit = 0;
    QStringList labels;
    QString group;
    bool hasGroup = false;
    QVariantMap fields;
};

struct TorrentPropertyChanges
{
    int bandwidthPriority = 0;
    bool honorsSessionLimits = true;
    int queuePosition = 0;
    int peerLimit = -1;
    bool downloadLimited = false;
    int downloadLimit = 0;
    bool uploadLimited = false;
    int uploadLimit = 0;
    int seedRatioMode = 0;
    double seedRatioLimit = 0.0;
    int seedIdleMode = 0;
    int seedIdleLimit = 0;
    QStringList labels;
    QString group;
    bool setGroup = false;
};

Q_DECLARE_METATYPE(TorrentDetails)
Q_DECLARE_METATYPE(TorrentFiles)
Q_DECLARE_METATYPE(TorrentPeers)
Q_DECLARE_METATYPE(TorrentTrackers)
Q_DECLARE_METATYPE(TorrentPieces)
Q_DECLARE_METATYPE(TorrentProperties)
Q_DECLARE_METATYPE(TorrentPropertyChanges)

#endif // TORRENTDOMAIN_H
