#include "geoipservice.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QTimeZone>
#include <QStringList>

namespace {

#ifdef PLANETARY_HAVE_MAXMINDDB
QString mmdbCString(const char *text)
{
    if (!text || *text == '\0')
        return {};

    return QString::fromUtf8(text);
}
#endif

}

GeoIpService::GeoIpService(QObject *parent)
    : QObject(parent)
{
#ifdef PLANETARY_HAVE_MAXMINDDB
    dbInfo.maxMindDbSupport = true;
#else
    dbInfo.maxMindDbSupport = false;
    dbInfo.errorMessage = tr("Planetary was built without libmaxminddb support.");
#endif
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


QStringList GeoIpService::candidateDatabasePaths()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QString databasePath;

#ifdef Q_OS_MACOS
    // Planetary.app/Contents/MacOS -> Planetary.app/Contents/Resources/geoip
    databasePath = QDir(appDir).filePath(
        QStringLiteral("../Resources/geoip/country.mmdb"));
#elif defined(Q_OS_WIN)
    // Portable Windows distribution: Planetary.exe beside geoip/country.mmdb.
    databasePath = QDir(appDir).filePath(
        QStringLiteral("geoip/country.mmdb"));
#else
    // Linux install and AppImage layout: bin/Planetary -> share/planetary/geoip.
    databasePath = QDir(appDir).filePath(
        QStringLiteral("../share/planetary/geoip/country.mmdb"));
#endif

    return {QDir::cleanPath(QFileInfo(databasePath).absoluteFilePath())};
}

bool GeoIpService::loadDefaultDatabase()
{
    const QStringList candidates = candidateDatabasePaths();
    QStringList attemptedPaths;
    QStringList failureMessages;

    for (const QString &candidate : candidates) {
        if (!QFileInfo::exists(candidate))
            continue;

        attemptedPaths.append(candidate);

        if (loadDatabase(candidate))
            return true;

        const GeoIpDatabaseInfo failedInfo = databaseInfo();
        if (!failedInfo.errorMessage.isEmpty())
            failureMessages.append(tr("%1: %2").arg(candidate, failedInfo.errorMessage));
    }

    cache.clear();
    updateCacheEntryCount();

    dbInfo = {};
#ifdef PLANETARY_HAVE_MAXMINDDB
    dbInfo.maxMindDbSupport = true;
#else
    dbInfo.maxMindDbSupport = false;
#endif
    dbInfo.fallbackLookupActive = true;
    if (!candidates.isEmpty())
        dbInfo.path = candidates.first();

#ifndef PLANETARY_HAVE_MAXMINDDB
    dbInfo.errorMessage = tr("Planetary was built without libmaxminddb support.");
#else
    if (attemptedPaths.isEmpty()) {
        dbInfo.errorMessage = tr("GeoIP database file was not found. Checked: %1")
                                  .arg(candidates.join(QStringLiteral("; ")));
    } else {
        dbInfo.errorMessage = tr("GeoIP database could not be loaded. %1")
                                  .arg(failureMessages.join(QStringLiteral("; ")));
    }
#endif

    return false;
}


bool GeoIpService::loadDatabase(const QString &path)
{
    cache.clear();
    updateCacheEntryCount();

    dbInfo = {};

#ifdef PLANETARY_HAVE_MAXMINDDB
    dbInfo.maxMindDbSupport = true;
#else
    dbInfo.maxMindDbSupport = false;
#endif

#ifdef PLANETARY_HAVE_MAXMINDDB
    if (mmdbOpen) {
        MMDB_close(&mmdb);
        mmdbOpen = false;
    }

    const QString cleanPath = path.trimmed();
    dbInfo.path = cleanPath;

    if (cleanPath.isEmpty()) {
        dbInfo.errorMessage = tr("GeoIP database path is empty.");
        return false;
    }

    if (!QFileInfo::exists(cleanPath)) {
        dbInfo.errorMessage = tr("GeoIP database file was not found.");
        return false;
    }

    const int status = MMDB_open(
        cleanPath.toUtf8().constData(),
        MMDB_MODE_MMAP,
        &mmdb
        );

    if (status != MMDB_SUCCESS) {
        dbInfo.errorMessage = tr("Could not open GeoIP database: %1")
                                  .arg(QString::fromUtf8(MMDB_strerror(status)));
        return false;
    }

    mmdbOpen = true;

    dbInfo.loaded = true;
    dbInfo.fallbackLookupActive = false;
    dbInfo.databaseType = mmdbCString(mmdb.metadata.database_type);
    dbInfo.ipVersion = static_cast<int>(mmdb.metadata.ip_version);
    dbInfo.nodeCount = mmdb.metadata.node_count;
    dbInfo.recordSize = static_cast<int>(mmdb.metadata.record_size);
    dbInfo.binaryFormatMajor = static_cast<int>(mmdb.metadata.binary_format_major_version);
    dbInfo.binaryFormatMinor = static_cast<int>(mmdb.metadata.binary_format_minor_version);

    if (mmdb.metadata.build_epoch > 0) {
        dbInfo.buildDateUtc =
            QDateTime::fromSecsSinceEpoch(
                static_cast<qint64>(mmdb.metadata.build_epoch),
                QTimeZone::UTC
                ).toString(Qt::ISODate);
    }

    for (uint32_t i = 0; i < mmdb.metadata.description.count; ++i) {
        const MMDB_description_s *description = mmdb.metadata.description.descriptions[i];

        if (!description)
            continue;

        const QString language = mmdbCString(description->language);
        const QString text = mmdbCString(description->description);

        if (text.isEmpty())
            continue;

        if (language == QStringLiteral("en")) {
            dbInfo.description = text;
            break;
        }

        if (dbInfo.description.isEmpty())
            dbInfo.description = text;
    }

    return true;
#else
    Q_UNUSED(path);
    dbInfo.errorMessage = tr("Planetary was built without libmaxminddb support.");
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

GeoIpDatabaseInfo GeoIpService::databaseInfo() const
{
    GeoIpDatabaseInfo info = dbInfo;
    info.cacheEntries = cache.size();
    return info;
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
        result.countryCode = tr("LAN");
        result.countryName = tr("Private / Local");
        result.found = false;
        result.isPrivateAddress = true;
    } else if (isDatabaseLoaded()) {
        result = lookupInDatabase(key);
    }

    // Cache misses and private-address classifications too; peer snapshots
    // repeatedly contain the same endpoints.
    cache.insert(key, result);
    updateCacheEntryCount();
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
        return result;
    }

    if (mmdbError != MMDB_SUCCESS) {
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
     * so we try a couple of reasonable paths before giving up.
     */
    QString countryCode =
        readUtf8Value(&lookupResult.entry, "country", "iso_code");

    QString countryName =
        readUtf8Value(&lookupResult.entry, "country", "names", "en");

    if (countryCode.isEmpty())
        return result;

    result.countryCode = countryCode.toUpper();
    result.countryName = countryName.isEmpty() ? result.countryCode : countryName;
    result.found = true;
#else
    Q_UNUSED(ipAddress);
#endif

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

void GeoIpService::updateCacheEntryCount()
{
    dbInfo.cacheEntries = cache.size();
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
