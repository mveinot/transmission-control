#ifndef TORRENTBACKENDROUTER_H
#define TORRENTBACKENDROUTER_H

#include "torrentbackend.h"

// Stable facade retained by the UI while concrete backend instances are
// replaced as the selected server type changes.
class TorrentBackendRouter : public TorrentBackend {
  Q_OBJECT

public:
  explicit TorrentBackendRouter(QObject *parent = nullptr);

  QString backendName() const override;
  QString serverDisplayName() const override;
  QString endpointUrl() const override;
  QString protocolDescription() const override;
  TorrentBackendCapabilities capabilities() const override;
  bool setServerProfile(const ServerProfile &profile) override;
  void init() override;
  void getTorrentList() override;
  void getTorrentTrackerMetadata() override;
  void getTorrentDetails(const TorrentKey &) override;
  void getTorrentFiles(const TorrentKey &) override;
  void getTorrentPeers(const TorrentKey &) override;
  void getTorrentTrackers(const TorrentKey &) override;
  void getTorrentPieces(const TorrentKey &) override;
  void getTorrentProperties(const TorrentKey &) override;
  void cancelTorrentDetailRequests() override;
  void addTorrentFromFile(const QString &, bool) override;
  void addTorrentFromMagnet(const QString &) override;
  void addTorrentFile(const QString &, const QString &, bool,
                      const QList<int> &, const QList<int> &,
                      const QList<int> &, bool) override;
  void addMagnetLink(const QString &, const QString &, bool) override;
  void startTorrents(const QList<TorrentKey> &) override;
  void startAllTorrents() override;
  void startTorrentsNow(const QList<TorrentKey> &) override;
  void stopTorrents(const QList<TorrentKey> &) override;
  void stopAllTorrents() override;
  void removeTorrents(const QList<TorrentKey> &, bool) override;
  void verifyTorrents(const QList<TorrentKey> &) override;
  void reannounceTorrents(const QList<TorrentKey> &) override;
  void setTorrentLocation(const QList<TorrentKey> &, const QString &,
                          bool) override;
  void setTorrentFilesWanted(const TorrentKey &, const QList<int> &,
                             bool) override;
  void setTorrentFilesPriority(const TorrentKey &, const QList<int> &,
                               int) override;
  void setTorrentFilesWantedAndPriority(const TorrentKey &, const QList<int> &,
                                        bool, int) override;
  void addTorrentTracker(const TorrentKey &, const QString &) override;
  void editTorrentTracker(const TorrentKey &, int, const QString &) override;
  void removeTorrentTracker(const TorrentKey &, int) override;
  void renameTorrentPath(const TorrentKey &, const QString &,
                         const QString &) override;
  void setTorrentProperties(const TorrentKey &,
                            const TorrentPropertyChanges &) override;
  void setTorrentsSequentialDownload(const QList<TorrentKey> &, bool) override;
  void setTorrentsBandwidthPriority(const QList<TorrentKey> &, int) override;
  void queueMoveTop(const QList<TorrentKey> &) override;
  void queueMoveUp(const QList<TorrentKey> &) override;
  void queueMoveDown(const QList<TorrentKey> &) override;
  void queueMoveBottom(const QList<TorrentKey> &) override;
  void getSessionSettings() override;
  void getSessionStatistics() override;
  void setSessionSettings(const QJsonObject &) override;
  void getFreeSpace(const QString &) override;
  void testPortForwarding() override;
  void updateBlocklist(const QJsonObject &) override;

private:
  TorrentBackend *m_backend = nullptr;

  TorrentBackend *createBackend(const QString &type);
  void replaceBackend(TorrentBackend *backend);
  void forwardSignals(TorrentBackend *backend);
};

#endif // TORRENTBACKENDROUTER_H
