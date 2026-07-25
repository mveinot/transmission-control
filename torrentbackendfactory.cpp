#include "torrentbackendfactory.h"

#include "transmissionbackend.h"

TorrentBackend *createConfiguredTorrentBackend(QObject *parent)
{
    // Existing server records predate backend typing and are therefore
    // Transmission profiles. A later migration can dispatch on a persisted
    // backend identifier while preserving this compatibility default.
    return new TransmissionBackend(parent);
}
