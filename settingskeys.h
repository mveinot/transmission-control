#ifndef SETTINGSKEYS_H
#define SETTINGSKEYS_H

namespace SettingsKeys {

inline constexpr const char *UpdateInterval =               "app/updateIntervalSeconds";
inline constexpr const char *DeleteTorrentOnAdd =           "app/deleteTorrentFileOnSuccessfulAdd";
inline constexpr const char *ShowTrayIcon =                 "app/tray/showIcon";
inline constexpr const char *ShowTrayNotifications =        "app/tray/showNotifications";
inline constexpr const char *HideApplicationIcon =          "app/tray/hideApplicationIcon";
inline constexpr const char *WatchFolderEnabled =           "app/watchFolder/enabled";
inline constexpr const char *WatchFolderPath =              "app/watchFolder/path";
inline constexpr const char *WatchFolderScanIntervalMs =    "app/watchFolder/scanIntervalMs";
inline constexpr const char *WatchFolderStableChecks =      "app/watchFolder/stableChecks";

}

#endif // SETTINGSKEYS_H