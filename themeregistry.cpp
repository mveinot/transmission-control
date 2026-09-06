#include "themeregistry.h"

#include "thememanifest.h"

#include <QDebug>
#include <QDir>
#include <QFileInfoList>
#include <QSet>
#include <QStandardPaths>

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

ThemeRegistry::ThemeRegistry(const QString &themeDirectory, QObject *parent)
    : QObject(parent)
    , m_themeDirectory(themeDirectory.isEmpty()
                           ? QString()
                           : QDir::cleanPath(themeDirectory))
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
    return m_themes.value(canonicalId(themeId));
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

    QSet<QString> discoveredIds;
    for (const QString &manifestPath : manifestPaths) {
        const ThemeManifestResult result =
            ThemeManifestParser::parseFile(manifestPath);
        if (!result.succeeded()) {
            qWarning().noquote()
                << QStringLiteral("Skipping theme manifest %1: %2")
                       .arg(manifestPath, result.error);
            continue;
        }

        const QString themeId = canonicalId(result.theme.id());
        if (discoveredIds.contains(themeId)) {
            qWarning().noquote()
                << QStringLiteral("Skipping duplicate theme id '%1' in %2")
                       .arg(themeId, manifestPath);
            continue;
        }

        if (registerTheme(result.theme))
            discoveredIds.insert(themeId);
    }

    const QSet<QString> removedIds = m_scannedThemeIds - discoveredIds;
    for (const QString &themeId : removedIds)
        unregisterTheme(themeId);
    m_scannedThemeIds = discoveredIds;
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
