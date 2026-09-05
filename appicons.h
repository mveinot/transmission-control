#ifndef APPICONS_H
#define APPICONS_H

#include <QIcon>
#include <QString>

namespace AppIcons {

inline constexpr const char *ClassicTheme = "classic";
inline constexpr const char *GlassTheme = "glass";

// Semantic icon identifiers isolate controllers from resource paths and keep
// action/status artwork consistent across menus, toolbars, and item views.
enum class Icon {
    ActionAddTorrent,
    ActionAddMagnet,
    ActionStart,
    ActionStop,
    ActionStartAll,
    ActionStopAll,
    ActionForceStart,
    ActionVerify,
    ActionReannounce,
    ActionDelete,
    QueueTop,
    QueueUp,
    QueueDown,
    QueueBottom,
    FilterAll,
    FilterTracker,
    FilterFolder,
    StatusDownloading,
    StatusSeeding,
    StatusComplete,
    StatusActive,
    StatusInactive,
    StatusStopped,
    StatusError,
    StatusVerifying,
    StatusQueued,
    StatusUnknown
};

QString normalizedThemeId(const QString &themeId);
QString selectedThemeId();
QIcon icon(Icon icon);
QIcon icon(Icon icon, const QString &themeId);

} // namespace AppIcons

#endif // APPICONS_H
