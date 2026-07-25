#ifndef TORRENTKEY_H
#define TORRENTKEY_H

#include <QString>

// Stable, backend-neutral torrent identity. Implementations normalize their
// native handles to an info hash (or another persistent opaque string) before
// exposing torrents to the rest of Planetary.
using TorrentKey = QString;

inline bool isValidTorrentKey(const TorrentKey &key)
{
    return !key.trimmed().isEmpty();
}

#endif // TORRENTKEY_H
