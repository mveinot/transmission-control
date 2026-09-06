#include "iconthemeregistry.h"

#include <QSet>

namespace AppIcons {
namespace {

IconTheme::IconFiles builtInIconFiles()
{
    return {
        {Id::ActionAddTorrent, QStringLiteral("action-add-torrent.png")},
        {Id::ActionAddMagnet, QStringLiteral("action-add-magnet.png")},
        {Id::ActionStart, QStringLiteral("action-start.png")},
        {Id::ActionStop, QStringLiteral("action-stop.png")},
        {Id::ActionStartAll, QStringLiteral("action-start-all.png")},
        {Id::ActionStopAll, QStringLiteral("action-stop-all.png")},
        {Id::ActionForceStart, QStringLiteral("action-force-start.png")},
        {Id::ActionVerify, QStringLiteral("action-verify.png")},
        {Id::ActionReannounce, QStringLiteral("action-reannounce.png")},
        {Id::ActionDelete, QStringLiteral("action-delete.png")},
        {Id::QueueTop, QStringLiteral("queue-top.png")},
        {Id::QueueUp, QStringLiteral("queue-up.png")},
        {Id::QueueDown, QStringLiteral("queue-down.png")},
        {Id::QueueBottom, QStringLiteral("queue-bottom.png")},
        {Id::FilterAll, QStringLiteral("filter-all.png")},
        {Id::FilterTracker, QStringLiteral("filter-tracker.png")},
        {Id::FilterFolder, QStringLiteral("filter-folder.png")},
        {Id::StatusDownloading, QStringLiteral("status-downloading.png")},
        {Id::StatusSeeding, QStringLiteral("status-seeding.png")},
        {Id::StatusComplete, QStringLiteral("status-complete.png")},
        {Id::StatusActive, QStringLiteral("status-active.png")},
        {Id::StatusInactive, QStringLiteral("status-inactive.png")},
        {Id::StatusStopped, QStringLiteral("status-stopped.png")},
        {Id::StatusError, QStringLiteral("status-error.png")},
        {Id::StatusVerifying, QStringLiteral("status-verifying.png")},
        {Id::StatusQueued, QStringLiteral("status-queued.png")},
        {Id::StatusUnknown, QStringLiteral("status-unknown.png")}
    };
}

} // namespace

IconThemeRegistry &IconThemeRegistry::instance()
{
    static IconThemeRegistry registry;
    return registry;
}

IconThemeRegistry::IconThemeRegistry()
{
    registerBuiltInThemes();
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
    emit registryChanged(canonical);
    return true;
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
