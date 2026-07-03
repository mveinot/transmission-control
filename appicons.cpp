#include "appicons.h"

#include <QString>

namespace {

QString iconPath(AppIcons::Icon icon)
{
    switch (icon) {
    case AppIcons::Icon::ActionAddTorrent:
        return QStringLiteral(":/icons/ui/action-add-torrent.png");
    case AppIcons::Icon::ActionAddMagnet:
        return QStringLiteral(":/icons/ui/action-add-magnet.png");
    case AppIcons::Icon::ActionStart:
        return QStringLiteral(":/icons/ui/action-start.png");
    case AppIcons::Icon::ActionStop:
        return QStringLiteral(":/icons/ui/action-stop.png");
    case AppIcons::Icon::ActionStartAll:
        return QStringLiteral(":/icons/ui/action-start-all.png");
    case AppIcons::Icon::ActionStopAll:
        return QStringLiteral(":/icons/ui/action-stop-all.png");
    case AppIcons::Icon::ActionForceStart:
        return QStringLiteral(":/icons/ui/action-force-start.png");
    case AppIcons::Icon::ActionVerify:
        return QStringLiteral(":/icons/ui/action-verify.png");
    case AppIcons::Icon::ActionReannounce:
        return QStringLiteral(":/icons/ui/action-reannounce.png");
    case AppIcons::Icon::ActionDelete:
        return QStringLiteral(":/icons/ui/action-delete.png");
    case AppIcons::Icon::QueueTop:
        return QStringLiteral(":/icons/ui/queue-top.png");
    case AppIcons::Icon::QueueUp:
        return QStringLiteral(":/icons/ui/queue-up.png");
    case AppIcons::Icon::QueueDown:
        return QStringLiteral(":/icons/ui/queue-down.png");
    case AppIcons::Icon::QueueBottom:
        return QStringLiteral(":/icons/ui/queue-bottom.png");
    case AppIcons::Icon::FilterAll:
        return QStringLiteral(":/icons/ui/filter-all.png");
    case AppIcons::Icon::FilterTracker:
        return QStringLiteral(":/icons/ui/filter-tracker.png");
    case AppIcons::Icon::FilterFolder:
        return QStringLiteral(":/icons/ui/filter-folder.png");
    case AppIcons::Icon::StatusDownloading:
        return QStringLiteral(":/icons/ui/status-downloading.png");
    case AppIcons::Icon::StatusSeeding:
        return QStringLiteral(":/icons/ui/status-seeding.png");
    case AppIcons::Icon::StatusComplete:
        return QStringLiteral(":/icons/ui/status-complete.png");
    case AppIcons::Icon::StatusActive:
        return QStringLiteral(":/icons/ui/status-active.png");
    case AppIcons::Icon::StatusInactive:
        return QStringLiteral(":/icons/ui/status-inactive.png");
    case AppIcons::Icon::StatusStopped:
        return QStringLiteral(":/icons/ui/status-stopped.png");
    case AppIcons::Icon::StatusError:
        return QStringLiteral(":/icons/ui/status-error.png");
    case AppIcons::Icon::StatusVerifying:
        return QStringLiteral(":/icons/ui/status-verifying.png");
    case AppIcons::Icon::StatusQueued:
        return QStringLiteral(":/icons/ui/status-queued.png");
    case AppIcons::Icon::StatusUnknown:
        return QStringLiteral(":/icons/ui/status-unknown.png");
    }

    return QString();
}

} // namespace

namespace AppIcons {

QIcon icon(Icon icon)
{
    return QIcon(iconPath(icon));
}

} // namespace AppIcons
