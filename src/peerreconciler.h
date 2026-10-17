#ifndef PEERRECONCILER_H
#define PEERRECONCILER_H

#include "torrentdomain.h"

#include <QFlags>
#include <QHash>
#include <QtGlobal>
#include <QVector>

// Stable row identity derived from the endpoint returned by torrent backends.
// The occurrence disambiguates duplicate endpoint rows without imposing a
// backend-specific peer identifier.
struct PeerRowKey
{
    QString address;
    int port = 0;
    int occurrence = 0;

    bool operator==(const PeerRowKey &other) const
    {
        return address == other.address
               && port == other.port
               && occurrence == other.occurrence;
    }
};

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
size_t qHash(const PeerRowKey &key, size_t seed = 0) noexcept;
#else
uint qHash(const PeerRowKey &key, uint seed = 0) noexcept;
#endif

enum class PeerField {
    None = 0,
    Client = 1 << 0,
    Progress = 1 << 1,
    DownloadRate = 1 << 2,
    UploadRate = 1 << 3,
    Encrypted = 1 << 4,
    Incoming = 1 << 5,
};
Q_DECLARE_FLAGS(PeerFields, PeerField)
Q_DECLARE_OPERATORS_FOR_FLAGS(PeerFields)

struct PeerRowChange
{
    enum class Kind {
        Insert,
        Update,
        Remove,
    };

    Kind kind = Kind::Update;
    PeerRowKey key;
    TorrentPeer peer;
    PeerFields fields;
};

// Compares backend snapshots independently of any item view. A future table
// model can consume these same row operations and field masks.
class PeerSnapshotReconciler
{
public:
    QVector<PeerRowChange> reconcile(const QVector<TorrentPeer> &peers);
    void clear();

private:
    QHash<PeerRowKey, TorrentPeer> currentPeers;
};

#endif // PEERRECONCILER_H
