#ifndef GEOIPSERVICE_H
#define GEOIPSERVICE_H

#include <QObject>
#include <QHash>
#include <QString>

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

        if (!flagEmoji.isEmpty())
            return QStringLiteral("%1 %2").arg(flagEmoji, countryCode);

        return countryCode;
    }
};

class GeoIpService : public QObject
{
    Q_OBJECT

public:
    explicit GeoIpService(QObject *parent = nullptr);

    GeoIpResult lookup(const QString &ipAddress);

private:
    QHash<QString, GeoIpResult> cache;

    GeoIpResult dummyLookup(const QString &ipAddress) const;

    static bool isPrivateOrLocalAddress(const QString &ipAddress);
    static QString countryCodeToFlagEmoji(const QString &countryCode);
};

#endif // GEOIPSERVICE_H