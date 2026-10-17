#ifndef GEOIPSERVICE_H
#define GEOIPSERVICE_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>

#ifdef PLANETARY_HAVE_MAXMINDDB
#include <maxminddb.h>
#endif

struct GeoIpDatabaseInfo
{
    bool maxMindDbSupport = false;
    bool loaded = false;
    bool fallbackLookupActive = true;

    QString path;
    QString errorMessage;

    QString databaseType;
    QString description;
    QString buildDateUtc;
    int ipVersion = 0;
    quint32 nodeCount = 0;
    int recordSize = 0;
    int binaryFormatMajor = 0;
    int binaryFormatMinor = 0;
    int cacheEntries = 0;
};

struct GeoIpResult
{
    QString countryCode;
    QString countryName;
    bool found = false;
    bool isPrivateAddress = false;

    QString displayText() const
    {
        if (isPrivateAddress)
            return QStringLiteral("Private");

        if (!found)
            return QStringLiteral("Unknown");

        if (!countryCode.isEmpty())
            return countryCode;

        return QStringLiteral("Unknown");
    }
};

// Synchronous local GeoIP resolver backed by a memory-mapped MMDB. Results,
// including misses and private addresses, are cached for the process lifetime.
class GeoIpService : public QObject
{
    Q_OBJECT

public:
    explicit GeoIpService(QObject *parent = nullptr);
    ~GeoIpService();

    bool loadDatabase(const QString &path);
    bool loadDefaultDatabase();
    static QStringList candidateDatabasePaths();
    bool isDatabaseLoaded() const;
    GeoIpDatabaseInfo databaseInfo() const;

    GeoIpResult lookup(const QString &ipAddress);

private:
    QHash<QString, GeoIpResult> cache;
    GeoIpDatabaseInfo dbInfo;

#ifdef PLANETARY_HAVE_MAXMINDDB
    MMDB_s mmdb {};
    bool mmdbOpen = false;
#endif

    GeoIpResult lookupInDatabase(const QString &ipAddress) const;
    GeoIpResult dummyLookup(const QString &ipAddress) const;

    static bool isPrivateOrLocalAddress(const QString &ipAddress);
    void updateCacheEntryCount();

#ifdef PLANETARY_HAVE_MAXMINDDB
    static QString readUtf8Value(MMDB_entry_s *entry,
                                 const char *firstKey,
                                 const char *secondKey = nullptr,
                                 const char *thirdKey = nullptr);
#endif
};

#endif // GEOIPSERVICE_H
