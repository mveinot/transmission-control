#ifndef SETTINGSKEYS_H
#define SETTINGSKEYS_H

namespace SettingsKeys {

// Persistent keys are compatibility identifiers and must not be renamed as
// part of ordinary C++ symbol refactoring.
inline constexpr const char *UpdateInterval =               "app/updateIntervalSeconds";
inline constexpr const char *DeleteTorrentOnAdd =           "app/deleteTorrentFileOnSuccessfulAdd";
inline constexpr const char *TorrentOpenDirectory =         "app/torrentOpenDirectory";
inline constexpr const char *StartTorrentPaused =           "torrentAdd/startPaused";
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
inline constexpr const char *WatchFolderEnabled =           "app/watchFolder/enabled";
inline constexpr const char *WatchFolderPath =              "app/watchFolder/path";
inline constexpr const char *WatchFolderScanIntervalMs =    "app/watchFolder/scanIntervalMs";
inline constexpr const char *WatchFolderStableChecks =      "app/watchFolder/stableChecks";
inline constexpr const char *WatchFolderProcessedFingerprints =
    "watchFolder/processedFingerprints";
inline constexpr const char *UpdateCheckAutomatically =     "updates/checkAutomatically";
inline constexpr const char *UpdateLastCheck =              "updates/lastCheck";
inline constexpr const char *MainWindowToolBarVisible =     "mainWindow/toolBarVisible";
inline constexpr const char *MainWindowStatusBarVisible =   "mainWindow/statusBarVisible";
inline constexpr const char *MainWindowDetailsPaneVisible = "mainWindow/detailsPaneVisible";
inline constexpr const char *MainWindowDetailsPaneHeight =  "mainWindow/detailsPaneHeight";
inline constexpr const char *MainWindowFilterSidebarVisible =
    "mainWindow/filterSidebarVisible";
inline constexpr const char *MainWindowFilterSidebarWidth =
    "mainWindow/filterSidebarWidth";
inline constexpr const char *MainWindowToolBarStyle =       "mainWindow/toolBarButtonStyle";
inline constexpr const char *MainWindowState =              "mainWindow/stateV2";
inline constexpr const char *TorrentTableHeaderState =
    "ui/torrentTable/horizontalHeaderState/v4";
inline constexpr const char *TorrentTableVerticalHeaderState =
    "ui/torrentTable/verticalHeaderState/v1";
inline constexpr const char *TorrentTableVisibleColumns =
    "ui/torrentTable/visibleColumns/v2";
inline constexpr const char *FileTreeHeaderState =
    "ui/fileTreeWidget/headerState/v6";
inline constexpr const char *FileTreeVisibleColumns =
    "ui/fileTreeWidget/visibleColumns/v1";
inline constexpr const char *PeerTableHeaderState =
    "ui/peerTableWidget/horizontalHeaderState/v4";
inline constexpr const char *PeerTableVisibleColumns =
    "ui/peerTableWidget/visibleColumns/v1";
inline constexpr const char *TrackerTableHeaderState =
    "ui/trackerTableWidget/headerState/v4";
inline constexpr const char *TrackerTableVisibleColumns =
    "ui/trackerTableWidget/visibleColumns/v2";
inline constexpr const char *FilterTrackersCollapsed =
    "ui/filterSections/trackersCollapsed";
inline constexpr const char *FilterFoldersCollapsed =
    "ui/filterSections/foldersCollapsed";
inline constexpr const char *FilterLabelsCollapsed =
    "ui/filterSections/labelsCollapsed";
inline constexpr const char *FilterGroupsCollapsed =
    "ui/filterSections/groupsCollapsed";
inline constexpr const char *TorrentAddDownloadDir =        "torrentAdd/downloadDir";

// Server entries and folder mappings are QSettings arrays. The short field
// names below are relative to the active array element, while index keys are
// absolute and therefore include the array prefix.
inline constexpr const char *ServersArray =                 "servers";
inline constexpr const char *ServersDefaultIndex =          "servers/defaultIndex";
inline constexpr const char *ServersCurrentIndex =          "servers/currentIndex";
inline constexpr const char *ServerName =                   "name";
inline constexpr const char *ServerBackendType =            "backendType";
inline constexpr const char *ServerRpcUrl =                 "rpcUrl";
inline constexpr const char *ServerUsername =               "username";
inline constexpr const char *ServerPassword =               "password";
inline constexpr const char *ServerFolderMappingsArray =    "folderMappings";
inline constexpr const char *FolderMappingRemotePath =      "remotePath";
inline constexpr const char *FolderMappingLocalPath =       "localPath";

}

#endif // SETTINGSKEYS_H
