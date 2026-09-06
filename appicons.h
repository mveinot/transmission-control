#ifndef APPICONS_H
#define APPICONS_H

namespace AppIcons {

inline constexpr const char *ClassicTheme = "classic";
inline constexpr const char *GlassTheme = "glass";

// Semantic icon identifiers isolate controllers from resource paths and keep
// action/status artwork consistent across menus, toolbars, and item views.
enum class Id {
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

} // namespace AppIcons

#endif // APPICONS_H
