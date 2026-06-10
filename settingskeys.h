#ifndef SETTINGSKEYS_H
#define SETTINGSKEYS_H

namespace SettingsKeys {

inline constexpr const char *UpdateInterval =
    "app/updateIntervalSeconds";

inline constexpr const char *DeleteTorrentOnAdd =
    "app/deleteTorrentFileOnSuccessfulAdd";

inline constexpr const char *ShowTrayIcon =
    "app/tray/showIcon";

inline constexpr const char *ShowTrayNotifications =
    "app/tray/showNotifications";

inline constexpr const char *HideApplicationIcon =
    "app/tray/hideApplicationIcon";

}

#endif // SETTINGSKEYS_H