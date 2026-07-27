#ifndef MACDEFAULTHANDLERBACKEND_H
#define MACDEFAULTHANDLERBACKEND_H

#include <QString>

#include <functional>

enum class MacDefaultHandlerKind
{
    MagnetLinks,
    TorrentFiles
};

struct MacDefaultHandlerStatus
{
    bool supported = false;
    bool magnetLinks = false;
    bool torrentFiles = false;
};

/*
 * Queries and requests Launch Services defaults through NSWorkspace. Requests
 * may trigger a system-owned consent prompt; completion is delivered on the
 * Qt main thread after that flow finishes.
 */
MacDefaultHandlerStatus macDefaultHandlerStatus();
void requestMacDefaultHandler(
    MacDefaultHandlerKind kind,
    const std::function<void(const QString &error)> &completion);

#endif // MACDEFAULTHANDLERBACKEND_H
