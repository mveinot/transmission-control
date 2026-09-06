#include "appicons.h"

#include <iterator>

namespace AppIcons {
namespace {

struct SemanticIcon
{
    Id id;
    const char *name;
};

constexpr SemanticIcon SemanticIcons[] = {
    {Id::ActionAddTorrent, "action-add-torrent"},
    {Id::ActionAddMagnet, "action-add-magnet"},
    {Id::ActionStart, "action-start"},
    {Id::ActionStop, "action-stop"},
    {Id::ActionStartAll, "action-start-all"},
    {Id::ActionStopAll, "action-stop-all"},
    {Id::ActionForceStart, "action-force-start"},
    {Id::ActionVerify, "action-verify"},
    {Id::ActionReannounce, "action-reannounce"},
    {Id::ActionDelete, "action-delete"},
    {Id::QueueTop, "queue-top"},
    {Id::QueueUp, "queue-up"},
    {Id::QueueDown, "queue-down"},
    {Id::QueueBottom, "queue-bottom"},
    {Id::FilterAll, "filter-all"},
    {Id::FilterTracker, "filter-tracker"},
    {Id::FilterFolder, "filter-folder"},
    {Id::StatusDownloading, "status-downloading"},
    {Id::StatusSeeding, "status-seeding"},
    {Id::StatusComplete, "status-complete"},
    {Id::StatusActive, "status-active"},
    {Id::StatusInactive, "status-inactive"},
    {Id::StatusStopped, "status-stopped"},
    {Id::StatusError, "status-error"},
    {Id::StatusVerifying, "status-verifying"},
    {Id::StatusQueued, "status-queued"},
    {Id::StatusUnknown, "status-unknown"}
};

} // namespace

QString semanticName(Id iconId)
{
    for (const SemanticIcon &icon : SemanticIcons) {
        if (icon.id == iconId)
            return QString::fromLatin1(icon.name);
    }
    return QString();
}

std::optional<Id> idFromSemanticName(const QString &name)
{
    const QString candidate = name.trimmed().toLower();
    for (const SemanticIcon &icon : SemanticIcons) {
        if (QString::fromLatin1(icon.name) == candidate)
            return icon.id;
    }
    return std::nullopt;
}

QList<Id> allIds()
{
    QList<Id> result;
    result.reserve(static_cast<int>(std::size(SemanticIcons)));
    for (const SemanticIcon &icon : SemanticIcons)
        result.append(icon.id);
    return result;
}

} // namespace AppIcons
