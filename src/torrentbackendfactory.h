#ifndef TORRENTBACKENDFACTORY_H
#define TORRENTBACKENDFACTORY_H

class QObject;
class TorrentBackend;

// Composition boundary for remote torrent implementations. Server metadata
// can select among concrete backends here without exposing those classes to
// MainWindow or feature controllers.
TorrentBackend *createConfiguredTorrentBackend(QObject *parent);

#endif // TORRENTBACKENDFACTORY_H
