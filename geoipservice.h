#ifndef GEOIPSERVICE_H
#define GEOIPSERVICE_H

#include <QObject>
#include <QHash>
#include <QString>

#ifdef PLANETARY_HAVE_MAXMINDDB
#include <maxminddb.h>
#endif

struct GeoIpResult
{
    QString countryCode;
    QString countryName;
    QString flagEmoji;

    bool found = false;
    bool isPrivateAddress = false;

    QString displayText() const
    {
        if (isPrivateAddress)
            return QStringLiteral("Private");

        if (!found)
            return QStringLiteral("Unknown");

        if (!flagEmoji.isEmpty() && !countryCode.isEmpty())
            return QStringLiteral("%1 %2").arg(flagEmoji, countryCode);

        if (!countryCode.isEmpty())
            return countryCode;

        return QStringLiteral("Unknown");
    }
};

class GeoIpService : public QObject
{
    Q_OBJECT

public:
    explicit GeoIpService(QObject *parent = nullptr);
    ~GeoIpService();

    bool loadDatabase(const QString &path);
    bool isDatabaseLoaded() const;

    GeoIpResult lookup(const QString &ipAddress);

private:
    QHash<QString, GeoIpResult> cache;

#ifdef PLANETARY_HAVE_MAXMINDDB
    MMDB_s mmdb {};
    bool mmdbOpen = false;
#endif

    GeoIpResult lookupInDatabase(const QString &ipAddress) const;
    GeoIpResult dummyLookup(const QString &ipAddress) const;

    static bool isPrivateOrLocalAddress(const QString &ipAddress);
    static QString countryCodeToFlagEmoji(const QString &countryCode);

#ifdef PLANETARY_HAVE_MAXMINDDB
    static QString readUtf8Value(MMDB_entry_s *entry,
                                 const char *firstKey,
                                 const char *secondKey = nullptr,
                                 const char *thirdKey = nullptr);
#endif
};

#endif // GEOIPSERVICE_H