#include "peerreconciler.h"

#include <utility>

namespace {

QString normalizedAddress(const QString &address)
{
    return address.trimmed();
}

PeerFields changedFields(const TorrentPeer &before, const TorrentPeer &after)
{
    PeerFields fields;

    if (before.clientName != after.clientName)
        fields |= PeerField::Client;
    if (before.progress != after.progress)
        fields |= PeerField::Progress;
    if (before.downloadRate != after.downloadRate)
        fields |= PeerField::DownloadRate;
    if (before.uploadRate != after.uploadRate)
        fields |= PeerField::UploadRate;
    if (before.encrypted != after.encrypted)
        fields |= PeerField::Encrypted;
    if (before.incoming != after.incoming)
        fields |= PeerField::Incoming;

    return fields;
}

}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
size_t qHash(const PeerRowKey &key, size_t seed) noexcept
#else
uint qHash(const PeerRowKey &key, uint seed) noexcept
#endif
{
    seed = qHash(key.address, seed);
    seed = qHash(key.port, seed);
    return qHash(key.occurrence, seed);
}

QVector<PeerRowChange> PeerSnapshotReconciler::reconcile(
    const QVector<TorrentPeer> &peers)
{
    QHash<PeerRowKey, TorrentPeer> nextPeers;
    QHash<QString, int> endpointOccurrences;
    nextPeers.reserve(peers.size());

    for (TorrentPeer peer : peers) {
        peer.address = normalizedAddress(peer.address);
        const QString endpoint =
            QStringLiteral("%1\x1f%2").arg(peer.address).arg(peer.port);
        const int occurrence = endpointOccurrences.value(endpoint);
        endpointOccurrences.insert(endpoint, occurrence + 1);

        const PeerRowKey key { peer.address, peer.port, occurrence };
        nextPeers.insert(key, peer);
    }

    QVector<PeerRowChange> changes;
    changes.reserve(currentPeers.size() + nextPeers.size());

    for (auto it = currentPeers.cbegin(); it != currentPeers.cend(); ++it) {
        if (!nextPeers.contains(it.key())) {
            changes.append({
                PeerRowChange::Kind::Remove,
                it.key(),
                it.value(),
                PeerField::None,
            });
        }
    }

    for (auto it = nextPeers.cbegin(); it != nextPeers.cend(); ++it) {
        const auto previous = currentPeers.constFind(it.key());

        if (previous == currentPeers.cend()) {
            changes.append({
                PeerRowChange::Kind::Insert,
                it.key(),
                it.value(),
                PeerField::None,
            });
            continue;
        }

        const PeerFields fields = changedFields(previous.value(), it.value());
        if (fields != PeerField::None) {
            changes.append({
                PeerRowChange::Kind::Update,
                it.key(),
                it.value(),
                fields,
            });
        }
    }

    currentPeers = std::move(nextPeers);
    return changes;
}

void PeerSnapshotReconciler::clear()
{
    currentPeers.clear();
}
