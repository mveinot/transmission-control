#ifndef SETTINGSKEYS_H
#define SETTINGSKEYS_H

namespace SettingsKeys {

// Persistent keys are compatibility identifiers and must not be renamed as
// part of ordinary C++ symbol refactoring.
inline constexpr const char *UpdateInterval =               "app/updateIntervalSeconds";
inline constexpr const char *DeleteTorrentOnAdd =           "app/deleteTorrentFileOnSuccessfulAdd";
inline constexpr const char *TorrentOpenDirectory =         "app/torrentOpenDirectory";
inline constexpr const char *ShowTrayIcon =                 "app/tray/showIcon";
inline constexpr const char *ShowNotifications =            "app/notifications/showNotifications";
inline constexpr const char *NotifyTorrentAdded =             "app/notifications/torrentAdded";
inline constexpr const char *NotifyTorrentCompleted =         "app/notifications/torrentCompleted";
inline constexpr const char *NotifyTorrentError =             "app/notifications/torrentError";
inline constexpr const char *NotifyTorrentStalled =           "app/notifications/torrentStalled";
inline constexpr const char *DesktopNotificationsEnabled =    "app/notifications/desktopEnabled";
inline constexpr const char *ExternalCommandEnabled =         "app/notifications/externalCommandEnabled";
inline constexpr const char *ExternalCommandExecutable =      "app/notifications/externalCommandExecutable";
inline constexpr const char *ExternalCommandArguments =       "app/notifications/externalCommandArguments";
inline constexpr const char *ShowTrayNotifications =        "app/tray/showNotifications"; // legacy setting
inline constexpr const char *HideApplicationIcon =          "app/tray/hideApplicationIcon";
inline constexpr const char *WatchFolderEnabled =           "app/watchFolder/enabled";
inline constexpr const char *WatchFolderPath =              "app/watchFolder/path";
inline constexpr const char *WatchFolderScanIntervalMs =    "app/watchFolder/scanIntervalMs";
inline constexpr const char *WatchFolderStableChecks =      "app/watchFolder/stableChecks";

}

#endif // SETTINGSKEYS_H
