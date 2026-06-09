#include "geoipservice.h"

#include <QHostAddress>
#include <QHash>
#include <QStringList>

namespace {

struct DummyCountry
{
    const char *code;
    const char *name;
};

const QList<DummyCountry> DummyCountries {
    { "CA", "Canada" },
    { "US", "United States" },
    { "GB", "United Kingdom" },
    { "DE", "Germany" },
    { "FR", "France" },
    { "NL", "Netherlands" },
    { "SE", "Sweden" },
    { "NO", "Norway" },
    { "JP", "Japan" },
    { "AU", "Australia" },
    { "BR", "Brazil" },
    { "IN", "India" },
    { "ES", "Spain" },
    { "IT", "Italy" },
    { "PL", "Poland" }
};

}

GeoIpService::GeoIpService(QObject *parent)
    : QObject(parent)
{
}

GeoIpResult GeoIpService::lookup(const QString &ipAddress)
{
    const QString key = ipAddress.trimmed();

    if (key.isEmpty())
        return {};

    const auto cached = cache.constFind(key);

    if (cached != cache.constEnd())
        return cached.value();

    GeoIpResult result = dummyLookup(key);
    cache.insert(key, result);

    return result;
}

GeoIpResult GeoIpService::dummyLookup(const QString &ipAddress) const
{
    GeoIpResult result;

    if (isPrivateOrLocalAddress(ipAddress)) {
        result.countryCode = QStringLiteral("LAN");
        result.countryName = QStringLiteral("Private / Local");
        result.found = false;
        result.isPrivateAddress = true;
        return result;
    }

    if (DummyCountries.isEmpty())
        return result;

    const uint hash = qHash(ipAddress);
    const int index = static_cast<int>(hash % DummyCountries.size());

    const DummyCountry &country = DummyCountries.at(index);

    result.countryCode = QString::fromLatin1(country.code);
    result.countryName = QString::fromLatin1(country.name);
    result.flagEmoji = countryCodeToFlagEmoji(result.countryCode);
    result.found = true;

    return result;
}

bool GeoIpService::isPrivateOrLocalAddress(const QString &ipAddress)
{
    QHostAddress address(ipAddress);

    if (address.isNull())
        return false;

    if (address == QHostAddress::LocalHost ||
        address == QHostAddress::LocalHostIPv6) {
        return true;
    }

    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 ip = address.toIPv4Address();

        const quint8 a = static_cast<quint8>((ip >> 24) & 0xff);
        const quint8 b = static_cast<quint8>((ip >> 16) & 0xff);

        // 10.0.0.0/8
        if (a == 10)
            return true;

        // 172.16.0.0/12
        if (a == 172 && b >= 16 && b <= 31)
            return true;

        // 192.168.0.0/16
        if (a == 192 && b == 168)
            return true;

        // 169.254.0.0/16 link-local
        if (a == 169 && b == 254)
            return true;

        return false;
    }

    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        const QString text = ipAddress.toLower();

        return text == QStringLiteral("::1") ||
               text.startsWith(QStringLiteral("fc")) ||
               text.startsWith(QStringLiteral("fd")) ||
               text.startsWith(QStringLiteral("fe80"));
    }

    return false;
}

QString GeoIpService::countryCodeToFlagEmoji(const QString &countryCode)
{
    const QString code = countryCode.trimmed().toUpper();

    if (code.size() != 2)
        return {};

    const QChar first = code.at(0);
    const QChar second = code.at(1);

    if (first < QLatin1Char('A') || first > QLatin1Char('Z') ||
        second < QLatin1Char('A') || second > QLatin1Char('Z')) {
        return {};
    }

    const char32_t regionalIndicatorA = 0x1F1E6;

    char32_t flagChars[2] = {
        regionalIndicatorA + static_cast<char32_t>(first.unicode() - 'A'),
        regionalIndicatorA + static_cast<char32_t>(second.unicode() - 'A')
    };

    return QString::fromUcs4(flagChars, 2);
}