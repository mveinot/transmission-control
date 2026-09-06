#include "iconthemeregistry.h"

#include "iconthememanifest.h"

#include <QDebug>
#include <QDir>
#include <QFileInfoList>
#include <QSet>
#include <QStandardPaths>

namespace AppIcons {
namespace {

IconTheme::IconFiles builtInIconFiles()
{
    IconTheme::IconFiles files;
    for (Id iconId : allIds())
        files.insert(iconId, semanticName(iconId) + QStringLiteral(".png"));
    return files;
}

} // namespace

IconThemeRegistry &IconThemeRegistry::instance()
{
    static IconThemeRegistry registry(standardThemeDirectory());
    return registry;
}

QString IconThemeRegistry::standardThemeDirectory()
{
    const QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData.isEmpty()
               ? QString()
               : QDir(appData).filePath(QStringLiteral("icon-themes"));
}

IconThemeRegistry::IconThemeRegistry(const QString &themeDirectory,
                                     QObject *parent)
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

QList<IconTheme> IconThemeRegistry::themes() const
{
    QList<IconTheme> result;
    result.reserve(m_themeOrder.size());
    for (const QString &themeId : m_themeOrder)
        result.append(m_themes.value(themeId));
    return result;
}

IconTheme IconThemeRegistry::theme(const QString &themeId) const
{
    return m_themes.value(canonicalId(themeId));
}

bool IconThemeRegistry::contains(const QString &themeId) const
{
    return m_themes.contains(canonicalId(themeId));
}

QString IconThemeRegistry::resolvedThemeId(const QString &themeId) const
{
    const QString candidate = canonicalId(themeId);
    return m_themes.contains(candidate) ? candidate : defaultThemeId();
}

QString IconThemeRegistry::defaultThemeId() const
{
    return QString::fromLatin1(GlassTheme);
}

QString IconThemeRegistry::themeDirectory() const
{
    return m_themeDirectory;
}

QIcon IconThemeRegistry::icon(const QString &themeId, Id iconId) const
{
    QString candidate = resolvedThemeId(themeId);
    QSet<QString> visited;

    while (!candidate.isEmpty() && !visited.contains(candidate)) {
        visited.insert(candidate);
        const IconTheme current = m_themes.value(candidate);
        if (current.hasIcon(iconId)) {
            const QIcon result(current.iconPath(iconId));
            if (!result.isNull())
                return result;
        }
        candidate = canonicalId(current.fallbackThemeId());
    }

    const IconTheme fallback = m_themes.value(defaultThemeId());
    return QIcon(fallback.iconPath(iconId));
}

bool IconThemeRegistry::registerTheme(const IconTheme &theme)
{
    if (!theme.isValid())
        return false;

    const QString themeId = canonicalId(theme.id());
    const IconTheme existing = m_themes.value(themeId);
    if (existing.isBuiltIn() && !theme.isBuiltIn())
        return false;

    if (!m_themes.contains(themeId))
        m_themeOrder.append(themeId);
    m_themes.insert(themeId, theme);
    emit registryChanged(themeId);
    return true;
}

bool IconThemeRegistry::unregisterTheme(const QString &themeId)
{
    const QString canonical = canonicalId(themeId);
    const IconTheme existing = m_themes.value(canonical);
    if (!existing.isValid() || existing.isBuiltIn())
        return false;

    m_themes.remove(canonical);
    m_themeOrder.removeAll(canonical);
    m_scannedThemeIds.remove(canonical);
    emit registryChanged(canonical);
    return true;
}

void IconThemeRegistry::rescanExternalThemes()
{
    if (m_themeDirectory.isEmpty())
        return;

    const QDir root(m_themeDirectory);
    const QFileInfoList directories = root.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);

    QSet<QString> discoveredIds;
    for (const QFileInfo &directory : directories) {
        const QString manifestPath = QDir(directory.absoluteFilePath())
                                         .filePath(QString::fromLatin1(
                                             IconThemeManifestParser::FileName));
        if (!QFileInfo::exists(manifestPath))
            continue;

        const IconThemeManifestResult result =
            IconThemeManifestParser::parseFile(manifestPath);
        if (!result.succeeded()) {
            qWarning().noquote()
                << QStringLiteral("Skipping icon theme manifest %1: %2")
                       .arg(manifestPath, result.error);
            continue;
        }

        const QString themeId = canonicalId(result.theme.id());
        if (discoveredIds.contains(themeId)) {
            qWarning().noquote()
                << QStringLiteral("Skipping duplicate icon theme id '%1' in %2")
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

void IconThemeRegistry::registerBuiltInThemes()
{
    const IconTheme::IconFiles files = builtInIconFiles();
    const IconTheme glass(
        QString::fromLatin1(GlassTheme),
        QStringLiteral("Glass"),
        QStringLiteral(":/icons/ui/glass"),
        files,
        QString(),
        true);
    const IconTheme classic(
        QString::fromLatin1(ClassicTheme),
        QStringLiteral("Classic"),
        QStringLiteral(":/icons/ui/classic"),
        files,
        QString::fromLatin1(GlassTheme),
        true);

    m_themes.insert(glass.id(), glass);
    m_themes.insert(classic.id(), classic);
    m_themeOrder = {glass.id(), classic.id()};
}

QString IconThemeRegistry::canonicalId(const QString &themeId) const
{
    return themeId.trimmed().toLower();
}

} // namespace AppIcons
