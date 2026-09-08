#include "themeregistry.h"

#include "themearchive.h"
#include "thememanifest.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace AppThemes {
namespace {

AppIcons::IconTheme::IconFiles builtInIconFiles()
{
    AppIcons::IconTheme::IconFiles files;
    for (AppIcons::Id iconId : AppIcons::allIds()) {
        files.insert(iconId,
                     AppIcons::semanticName(iconId) + QStringLiteral(".png"));
    }
    return files;
}

} // namespace

ThemeRegistry &ThemeRegistry::instance()
{
    static ThemeRegistry registry(standardThemeDirectory());
    return registry;
}

QString ThemeRegistry::standardThemeDirectory()
{
    const QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData.isEmpty()
               ? QString()
               : QDir(appData).filePath(QStringLiteral("icon-themes"));
}

QString ThemeRegistry::standardThemeCacheDirectory()
{
    const QString cache =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return cache.isEmpty()
               ? QString()
               : QDir(cache).filePath(QStringLiteral("theme-packs"));
}

ThemeRegistry::ThemeRegistry(const QString &themeDirectory,
                             QObject *parent,
                             const QString &themeCacheDirectory)
    : QObject(parent)
    , m_themeDirectory(themeDirectory.isEmpty()
                           ? QString()
                           : QDir::cleanPath(themeDirectory))
    , m_themeCacheDirectory(themeCacheDirectory.isEmpty()
                                ? standardThemeCacheDirectory()
                                : QDir::cleanPath(themeCacheDirectory))
{
    registerBuiltInThemes();
    if (!m_themeDirectory.isEmpty()) {
        QDir().mkpath(m_themeDirectory);
        rescanExternalThemes();
    }
}

QList<Theme> ThemeRegistry::themes() const
{
    QList<Theme> result;
    result.reserve(m_themeOrder.size());
    for (const QString &themeId : m_themeOrder)
        result.append(m_themes.value(themeId));
    return result;
}

QList<AppIcons::IconTheme> ThemeRegistry::iconThemes() const
{
    QList<AppIcons::IconTheme> result;
    for (const QString &themeId : m_themeOrder) {
        const Theme current = m_themes.value(themeId);
        if (current.hasIconTheme())
            result.append(current.iconTheme());
    }
    return result;
}

QList<AppColors::ColorTheme> ThemeRegistry::colorThemes() const
{
    QList<AppColors::ColorTheme> result;
    for (const QString &themeId : m_themeOrder) {
        const Theme current = m_themes.value(themeId);
        if (current.hasColorTheme())
            result.append(current.colorTheme());
    }
    return result;
}

Theme ThemeRegistry::theme(const QString &themeId) const
{
    const QString candidate = canonicalId(themeId);
    ensureThemeMaterialized(candidate);
    return m_themes.value(candidate);
}

AppIcons::IconTheme ThemeRegistry::iconTheme(const QString &themeId) const
{
    return theme(themeId).iconTheme();
}

AppColors::ColorTheme ThemeRegistry::colorTheme(const QString &themeId) const
{
    return theme(themeId).colorTheme();
}

bool ThemeRegistry::contains(const QString &themeId) const
{
    return m_themes.contains(canonicalId(themeId));
}

QString ThemeRegistry::resolvedIconThemeId(const QString &themeId) const
{
    const QString candidate = canonicalId(themeId);
    return m_themes.value(candidate).hasIconTheme()
               ? candidate
               : defaultIconThemeId();
}

QString ThemeRegistry::resolvedColorThemeId(const QString &themeId) const
{
    const QString candidate = canonicalId(themeId);
    return m_themes.value(candidate).hasColorTheme()
               ? candidate
               : defaultColorThemeId();
}

QString ThemeRegistry::defaultIconThemeId() const
{
    return QString::fromLatin1(AppIcons::GlassTheme);
}

QString ThemeRegistry::defaultColorThemeId() const
{
    return QString::fromLatin1(AppColors::SystemTheme);
}

QString ThemeRegistry::themeDirectory() const
{
    return m_themeDirectory;
}

QIcon ThemeRegistry::icon(const QString &themeId, AppIcons::Id iconId) const
{
    QString candidate = resolvedIconThemeId(themeId);
    QSet<QString> visited;

    while (!candidate.isEmpty() && !visited.contains(candidate)) {
        visited.insert(candidate);
        const AppIcons::IconTheme current = iconTheme(candidate);
        if (current.hasIcon(iconId)) {
            const QIcon result(current.iconPath(iconId));
            if (!result.isNull())
                return result;
        }
        candidate = canonicalId(current.fallbackThemeId());
    }

    const AppIcons::IconTheme fallback = iconTheme(defaultIconThemeId());
    return QIcon(fallback.iconPath(iconId));
}

bool ThemeRegistry::registerTheme(const Theme &theme)
{
    if (!theme.isValid())
        return false;

    const QString themeId = canonicalId(theme.id());
    const Theme existing = m_themes.value(themeId);
    if (existing.isBuiltIn() && !theme.isBuiltIn())
        return false;

    if (!m_themes.contains(themeId))
        m_themeOrder.append(themeId);
    m_themes.insert(themeId, theme);
    m_archiveSources.remove(themeId);
    m_failedArchiveIds.remove(themeId);
    m_materializedArchiveIds.remove(themeId);
    emit registryChanged(themeId);
    return true;
}

bool ThemeRegistry::unregisterTheme(const QString &themeId)
{
    const QString canonical = canonicalId(themeId);
    const Theme existing = m_themes.value(canonical);
    if (!existing.isValid() || existing.isBuiltIn())
        return false;

    m_themes.remove(canonical);
    m_themeOrder.removeAll(canonical);
    m_scannedThemeIds.remove(canonical);
    m_archiveSources.remove(canonical);
    m_failedArchiveIds.remove(canonical);
    m_materializedArchiveIds.remove(canonical);
    emit registryChanged(canonical);
    return true;
}

void ThemeRegistry::rescanExternalThemes()
{
    if (m_themeDirectory.isEmpty())
        return;

    const QDir root(m_themeDirectory);
    QStringList manifestPaths;
    const QFileInfoList manifestFiles = root.entryInfoList(
        {QStringLiteral("*.json")}, QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &manifest : manifestFiles)
        manifestPaths.append(manifest.absoluteFilePath());

    const QFileInfoList directories = root.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &directory : directories) {
        const QString manifestPath = QDir(directory.absoluteFilePath())
                                         .filePath(QString::fromLatin1(
                                             ThemeManifestParser::FileName));
        if (QFileInfo::exists(manifestPath))
            manifestPaths.append(manifestPath);
    }

    struct Candidate {
        QString path;
        bool archive = false;
    };
    QList<Candidate> candidates;
    candidates.reserve(manifestPaths.size());
    for (const QString &manifestPath : manifestPaths)
        candidates.append({manifestPath, false});

    const QFileInfoList archiveFiles = root.entryInfoList(
        QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &file : archiveFiles) {
        if (file.fileName().endsWith(QStringLiteral(".planetarytheme"),
                                     Qt::CaseInsensitive)) {
            candidates.append({file.absoluteFilePath(), true});
        }
    }

    QSet<QString> discoveredIds;
    QHash<QString, ArchiveSource> discoveredArchives;
    for (const Candidate &candidate : candidates) {
        ThemeManifestResult result;
        ThemeArchiveManifest archiveManifest;
        QString cachePath;
        if (candidate.archive) {
            archiveManifest = ThemeArchive::readManifest(candidate.path);
            if (!archiveManifest.succeeded()) {
                qWarning().noquote()
                    << QStringLiteral("Skipping theme archive %1: %2")
                           .arg(candidate.path, archiveManifest.error);
                continue;
            }

            // Parse metadata without touching the cache. Relative paths are
            // rebased to the eventual extraction directory.
            result = ThemeManifestParser::parseData(
                archiveManifest.data,
                m_themeCacheDirectory.isEmpty()
                    ? m_themeDirectory
                    : m_themeCacheDirectory,
                false);
            if (result.succeeded()) {
                cachePath = archiveCachePath(candidate.path, result.theme.id());
                result = ThemeManifestParser::parseData(
                    archiveManifest.data, cachePath, false);
            }
        } else {
            result = ThemeManifestParser::parseFile(candidate.path);
        }
        if (!result.succeeded()) {
            qWarning().noquote()
                << QStringLiteral("Skipping theme %1: %2")
                       .arg(candidate.path, result.error);
            continue;
        }

        const QString themeId = canonicalId(result.theme.id());
        if (discoveredIds.contains(themeId)) {
            qWarning().noquote()
                << QStringLiteral("Skipping duplicate theme id '%1' in %2")
                       .arg(themeId, candidate.path);
            continue;
        }

        const Theme existing = m_themes.value(themeId);
        if (existing.isBuiltIn())
            continue;

        const bool changed = !m_themes.contains(themeId) || !(existing == result.theme);

        if (!m_themes.contains(themeId))
            m_themeOrder.append(themeId);
        m_themes.insert(themeId, result.theme);
        m_failedArchiveIds.remove(themeId);
        m_materializedArchiveIds.remove(themeId);
        if (candidate.archive) {
            const ArchiveSource source {
                candidate.path, archiveManifest.entryPrefix, cachePath};
            discoveredArchives.insert(themeId, source);
            m_archiveSources.insert(themeId, source);
        } else {
            m_archiveSources.remove(themeId);
        }
        discoveredIds.insert(themeId);
        if (changed)
            emit registryChanged(themeId);
    }

    const QSet<QString> removedIds = m_scannedThemeIds - discoveredIds;
    for (const QString &themeId : removedIds)
        unregisterTheme(themeId);
    m_scannedThemeIds = discoveredIds;
    m_archiveSources = discoveredArchives;
}

bool ThemeRegistry::ensureThemeMaterialized(const QString &themeId) const
{
    const auto sourceIt = m_archiveSources.constFind(themeId);
    if (sourceIt == m_archiveSources.constEnd())
        return true;
    if (m_materializedArchiveIds.contains(themeId))
        return true;
    if (m_failedArchiveIds.contains(themeId))
        return false;

    const ArchiveSource source = sourceIt.value();
    const QString manifestPath = QDir(source.cachePath).filePath(
        QString::fromLatin1(ThemeManifestParser::FileName));
    ThemeManifestResult cached = ThemeManifestParser::parseFile(manifestPath);
    if (cached.succeeded() && canonicalId(cached.theme.id()) == themeId) {
        m_themes.insert(themeId, cached.theme);
        m_materializedArchiveIds.insert(themeId);
        return true;
    }

    const QString cacheParent = QFileInfo(source.cachePath).absolutePath();
    if (!QDir().mkpath(cacheParent)) {
        qWarning().noquote() << "Could not create theme cache:" << cacheParent;
        m_failedArchiveIds.insert(themeId);
        return false;
    }

    QTemporaryDir staging(QDir(cacheParent).filePath(QStringLiteral(".extract-XXXXXX")));
    if (!staging.isValid()) {
        qWarning().noquote() << "Could not create temporary theme cache directory";
        m_failedArchiveIds.insert(themeId);
        return false;
    }

    QString extractionError;
    if (!ThemeArchive::extract(source.archivePath, source.entryPrefix,
                               staging.path(), &extractionError)) {
        qWarning().noquote()
            << QStringLiteral("Could not extract theme archive %1: %2")
                   .arg(source.archivePath, extractionError);
        m_failedArchiveIds.insert(themeId);
        return false;
    }

    const ThemeManifestResult staged = ThemeManifestParser::parseFile(
        QDir(staging.path()).filePath(
            QString::fromLatin1(ThemeManifestParser::FileName)));
    if (!staged.succeeded() || canonicalId(staged.theme.id()) != themeId) {
        qWarning().noquote()
            << QStringLiteral("Extracted theme '%1' failed validation: %2")
                   .arg(themeId, staged.error);
        m_failedArchiveIds.insert(themeId);
        return false;
    }

    if (!QDir().rename(staging.path(), source.cachePath)) {
        // Another instance may have populated the same immutable cache while
        // this one was extracting it.
        cached = ThemeManifestParser::parseFile(manifestPath);
        if (!cached.succeeded() || canonicalId(cached.theme.id()) != themeId) {
            qWarning().noquote() << "Could not finalize cached theme:" << themeId;
            m_failedArchiveIds.insert(themeId);
            return false;
        }
        m_themes.insert(themeId, cached.theme);
        m_materializedArchiveIds.insert(themeId);
        return true;
    }

    staging.setAutoRemove(false);
    cached = ThemeManifestParser::parseFile(manifestPath);
    if (!cached.succeeded() || canonicalId(cached.theme.id()) != themeId) {
        qWarning().noquote() << "Finalized theme cache failed validation:" << themeId;
        m_failedArchiveIds.insert(themeId);
        return false;
    }
    m_themes.insert(themeId, cached.theme);
    m_materializedArchiveIds.insert(themeId);
    return true;
}

QString ThemeRegistry::archiveCachePath(const QString &archivePath,
                                        const QString &themeId) const
{
    const QFileInfo info(archivePath);
    const QByteArray identity = info.canonicalFilePath().toUtf8()
                                + QByteArray::number(info.size())
                                + QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    const QString fingerprint = QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(16));
    return QDir(m_themeCacheDirectory)
        .filePath(themeId + QLatin1Char('/') + fingerprint);
}

void ThemeRegistry::registerBuiltInThemes()
{
    const AppIcons::IconTheme::IconFiles files = builtInIconFiles();
    const AppIcons::IconTheme glass(
        QString::fromLatin1(AppIcons::GlassTheme),
        QStringLiteral("Glass"),
        QStringLiteral(":/icons/ui/glass"),
        files,
        QString(),
        true);
    const AppIcons::IconTheme classic(
        QString::fromLatin1(AppIcons::ClassicTheme),
        QStringLiteral("Classic"),
        QStringLiteral(":/icons/ui/classic"),
        files,
        QString::fromLatin1(AppIcons::GlassTheme),
        true);
    const AppColors::ColorTheme system(
        QString::fromLatin1(AppColors::SystemTheme),
        QStringLiteral("Follow System"),
        AppColors::Mode::System,
        {}, {}, true);
    const AppColors::ColorTheme light(
        QString::fromLatin1(AppColors::LightTheme),
        QStringLiteral("Light"),
        AppColors::Mode::Light,
        {}, {}, true);
    const AppColors::ColorTheme dark(
        QString::fromLatin1(AppColors::DarkTheme),
        QStringLiteral("Dark"),
        AppColors::Mode::Dark,
        {}, {}, true);

    const auto addBuiltIn = [this](const Theme &theme) {
        m_themes.insert(theme.id(), theme);
        m_themeOrder.append(theme.id());
    };
    addBuiltIn(Theme(glass.id(), glass.displayName(), glass, std::nullopt, true));
    addBuiltIn(Theme(classic.id(), classic.displayName(), classic, std::nullopt, true));
    addBuiltIn(Theme(system.id(), system.displayName(), std::nullopt, system, true));
    addBuiltIn(Theme(light.id(), light.displayName(), std::nullopt, light, true));
    addBuiltIn(Theme(dark.id(), dark.displayName(), std::nullopt, dark, true));
}

QString ThemeRegistry::canonicalId(const QString &themeId) const
{
    return themeId.trimmed().toLower();
}

} // namespace AppThemes
