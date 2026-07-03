#ifndef APPICONS_H
#define APPICONS_H

#include <QIcon>

namespace AppIcons {

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

QIcon icon(Icon icon);

} // namespace AppIcons

#endif // APPICONS_H
