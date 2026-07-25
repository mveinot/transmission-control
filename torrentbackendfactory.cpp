#include "torrentbackendfactory.h"

#include "torrentbackendrouter.h"

TorrentBackend *createConfiguredTorrentBackend(QObject *parent) {
  return new TorrentBackendRouter(parent);
}
