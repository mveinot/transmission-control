#include "geoipservice.h"

#include <QDebug>
#include <QFileInfo>
#include <QHostAddress>
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

GeoIpService::~GeoIpService()
{
#ifdef PLANETARY_HAVE_MAXMINDDB
    if (mmdbOpen) {
        MMDB_close(&mmdb);
        mmdbOpen = false;
    }
#endif
}

bool GeoIpService::loadDatabase(const QString &path)
{
    cache.clear();

#ifdef PLANETARY_HAVE_MAXMINDDB
    if (mmdbOpen) {
        MMDB_close(&mmdb);
        mmdbOpen = false;
    }

    const QString cleanPath = path.trimmed();

    if (cleanPath.isEmpty()) {
        qWarning() << "GeoIP database path is empty";
        return false;
    }

    if (!QFileInfo::exists(cleanPath)) {
        qWarning() << "GeoIP database not found:" << cleanPath;
        return false;
    }

    const int status = MMDB_open(
        cleanPath.toUtf8().constData(),
        MMDB_MODE_MMAP,
        &mmdb
        );

    if (status != MMDB_SUCCESS) {
        qWarning() << "Could not open GeoIP database:"
                   << cleanPath
                   << MMDB_strerror(status);
        return false;
    }

    mmdbOpen = true;

    qDebug() << "Loaded GeoIP database:" << cleanPath;
    return true;
#else
    Q_UNUSED(path);
    qWarning() << "Planetary was built without libmaxminddb support";
    return false;
#endif
}

bool GeoIpService::isDatabaseLoaded() const
{
#ifdef PLANETARY_HAVE_MAXMINDDB
    return mmdbOpen;
#else
    return false;
#endif
}

GeoIpResult GeoIpService::lookup(const QString &ipAddress)
{
    const QString key = ipAddress.trimmed();

    if (key.isEmpty())
        return {};

    const auto cached = cache.constFind(key);

    if (cached != cache.constEnd())
        return cached.value();

    GeoIpResult result;

    if (isPrivateOrLocalAddress(key)) {
        result.countryCode = QStringLiteral("LAN");
        result.countryName = QStringLiteral("Private / Local");
        result.found = false;
        result.isPrivateAddress = true;
    } else if (isDatabaseLoaded()) {
        result = lookupInDatabase(key);
    } else {
        result = dummyLookup(key);
    }

    cache.insert(key, result);
    return result;
}

GeoIpResult GeoIpService::lookupInDatabase(const QString &ipAddress) const
{
    GeoIpResult result;

#ifdef PLANETARY_HAVE_MAXMINDDB
    if (!mmdbOpen)
        return result;

    int gaiError = 0;
    int mmdbError = 0;

    MMDB_lookup_result_s lookupResult =
        MMDB_lookup_string(
            &mmdb,
            ipAddress.toUtf8().constData(),
            &gaiError,
            &mmdbError
            );

    if (gaiError != 0) {
        qWarning() << "GeoIP getaddrinfo error for"
                   << ipAddress
                   << gai_strerror(gaiError);
        return result;
    }

    if (mmdbError != MMDB_SUCCESS) {
        qWarning() << "GeoIP lookup error for"
                   << ipAddress
                   << MMDB_strerror(mmdbError);
        return result;
    }

    if (!lookupResult.found_entry)
        return result;

    /*
     * DB-IP country MMDB commonly exposes country data under:
     *
     *   country.iso_code
     *   country.names.en
     *
     * Some generated country-only MMDBs may also have slightly flatter layouts,
     * so we try a couple of reasonable paths before giving up. Because database
     * schemas enjoy being tiny cultural arguments.
     */
    QString countryCode =
        readUtf8Value(&lookupResult.entry, "country", "iso_code");

    QString countryName =
        readUtf8Value(&lookupResult.entry, "country", "names", "en");

    if (countryCode.isEmpty())
        countryCode = readUtf8Value(&lookupResult.entry, "country", "iso_code");

    if (countryName.isEmpty())
        countryName = readUtf8Value(&lookupResult.entry, "country", "names", "en");

    if (countryCode.isEmpty())
        return result;

    result.countryCode = countryCode.toUpper();
    result.countryName = countryName.isEmpty() ? result.countryCode : countryName;
    result.flagEmoji = countryCodeToFlagEmoji(result.countryCode);
    result.found = true;
#else
    Q_UNUSED(ipAddress);
#endif

    return result;
}

GeoIpResult GeoIpService::dummyLookup(const QString &ipAddress) const
{
    GeoIpResult result;

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

        if (a == 10)
            return true;

        if (a == 172 && b >= 16 && b <= 31)
            return true;

        if (a == 192 && b == 168)
            return true;

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

#ifdef PLANETARY_HAVE_MAXMINDDB
QString GeoIpService::readUtf8Value(MMDB_entry_s *entry,
                                    const char *firstKey,
                                    const char *secondKey,
                                    const char *thirdKey)
{
    MMDB_entry_data_s data;

    int status = MMDB_SUCCESS;

    if (thirdKey) {
        status = MMDB_get_value(entry, &data, firstKey, secondKey, thirdKey, nullptr);
    } else if (secondKey) {
        status = MMDB_get_value(entry, &data, firstKey, secondKey, nullptr);
    } else {
        status = MMDB_get_value(entry, &data, firstKey, nullptr);
    }

    if (status != MMDB_SUCCESS)
        return {};

    if (!data.has_data)
        return {};

    if (data.type != MMDB_DATA_TYPE_UTF8_STRING)
        return {};

    return QString::fromUtf8(
        data.utf8_string,
        static_cast<int>(data.data_size)
        );
}
#endif