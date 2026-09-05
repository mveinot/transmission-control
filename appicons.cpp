#include "appicons.h"

#include "settingskeys.h"

#include <QSettings>
#include <QString>

namespace {

QString iconFileName(AppIcons::Icon icon)
{
    switch (icon) {
    case AppIcons::Icon::ActionAddTorrent:
        return QStringLiteral("action-add-torrent.png");
    case AppIcons::Icon::ActionAddMagnet:
        return QStringLiteral("action-add-magnet.png");
    case AppIcons::Icon::ActionStart:
        return QStringLiteral("action-start.png");
    case AppIcons::Icon::ActionStop:
        return QStringLiteral("action-stop.png");
    case AppIcons::Icon::ActionStartAll:
        return QStringLiteral("action-start-all.png");
    case AppIcons::Icon::ActionStopAll:
        return QStringLiteral("action-stop-all.png");
    case AppIcons::Icon::ActionForceStart:
        return QStringLiteral("action-force-start.png");
    case AppIcons::Icon::ActionVerify:
        return QStringLiteral("action-verify.png");
    case AppIcons::Icon::ActionReannounce:
        return QStringLiteral("action-reannounce.png");
    case AppIcons::Icon::ActionDelete:
        return QStringLiteral("action-delete.png");
    case AppIcons::Icon::QueueTop:
        return QStringLiteral("queue-top.png");
    case AppIcons::Icon::QueueUp:
        return QStringLiteral("queue-up.png");
    case AppIcons::Icon::QueueDown:
        return QStringLiteral("queue-down.png");
    case AppIcons::Icon::QueueBottom:
        return QStringLiteral("queue-bottom.png");
    case AppIcons::Icon::FilterAll:
        return QStringLiteral("filter-all.png");
    case AppIcons::Icon::FilterTracker:
        return QStringLiteral("filter-tracker.png");
    case AppIcons::Icon::FilterFolder:
        return QStringLiteral("filter-folder.png");
    case AppIcons::Icon::StatusDownloading:
        return QStringLiteral("status-downloading.png");
    case AppIcons::Icon::StatusSeeding:
        return QStringLiteral("status-seeding.png");
    case AppIcons::Icon::StatusComplete:
        return QStringLiteral("status-complete.png");
    case AppIcons::Icon::StatusActive:
        return QStringLiteral("status-active.png");
    case AppIcons::Icon::StatusInactive:
        return QStringLiteral("status-inactive.png");
    case AppIcons::Icon::StatusStopped:
        return QStringLiteral("status-stopped.png");
    case AppIcons::Icon::StatusError:
        return QStringLiteral("status-error.png");
    case AppIcons::Icon::StatusVerifying:
        return QStringLiteral("status-verifying.png");
    case AppIcons::Icon::StatusQueued:
        return QStringLiteral("status-queued.png");
    case AppIcons::Icon::StatusUnknown:
        return QStringLiteral("status-unknown.png");
    }

    return QString();
}

} // namespace

namespace AppIcons {

QString normalizedThemeId(const QString &themeId)
{
    const QString normalized = themeId.trimmed().toLower();
    if (normalized == QString::fromLatin1(ClassicTheme))
        return normalized;

    return QString::fromLatin1(GlassTheme);
}

QString selectedThemeId()
{
    return normalizedThemeId(
        QSettings().value(SettingsKeys::IconTheme,
                          QString::fromLatin1(GlassTheme)).toString());
}

QIcon icon(Icon icon)
{
    return AppIcons::icon(icon, selectedThemeId());
}

QIcon icon(Icon icon, const QString &themeId)
{
    const QString path = QStringLiteral(":/icons/ui/%1/%2")
                             .arg(normalizedThemeId(themeId), iconFileName(icon));
    return QIcon(path);
}

} // namespace AppIcons
