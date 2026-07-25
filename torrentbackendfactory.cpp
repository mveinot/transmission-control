#include "torrentbackendfactory.h"

#include "transmissionbackend.h"

TorrentBackend *createConfiguredTorrentBackend(QObject *parent)
{
    // Transmission is currently the only registered implementation. Server
    // profiles already persist a backend identifier for future dispatch.
    return new TransmissionBackend(parent);
}
