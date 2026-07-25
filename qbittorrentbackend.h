#ifndef QBITTORRENTBACKEND_H
#define QBITTORRENTBACKEND_H

#include "torrentbackend.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>

// qBittorrent WebUI API adapter. This initial implementation covers session
// authentication, list snapshots, and the General details surface.
class QBittorrentBackend : public TorrentBackend {
  Q_OBJECT

public:
  explicit QBittorrentBackend(QObject *parent = nullptr);

  QString backendName() const override;
  QString serverDisplayName() const override;
  QString endpointUrl() const override;
  TorrentBackendCapabilities capabilities() const override;
  bool loadCurrentServerFromSettings() override;
  bool setServerFromSettingsIndex(int index) override;
  void init() override;

  void getTorrentList() override;
  void getTorrentTrackerMetadata() override;
  void getTorrentDetails(const TorrentKey &torrentKey) override;
  void getTorrentFiles(const TorrentKey &torrentKey) override;
  void getTorrentPeers(const TorrentKey &torrentKey) override;
  void getTorrentTrackers(const TorrentKey &torrentKey) override;
  void getTorrentPieces(const TorrentKey &torrentKey) override;
  void getTorrentProperties(const TorrentKey &torrentKey) override;
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
  enum class RequestKind { Login, TorrentList, TorrentInfo, TorrentProperties };
  struct RequestContext {
    RequestKind kind;
    TorrentKey key;
  };

  QNetworkAccessManager m_network;
  QHash<QNetworkReply *, RequestContext> m_requests;
  QHash<TorrentKey, QVariantMap> m_infoByKey;
  QString m_serverName;
  QString m_baseUrl;
  QString m_username;
  QString m_password;
  bool m_authenticated = false;
  bool m_authenticationPending = false;
  bool m_listPendingAfterLogin = false;

  void authenticate();
  void sendGet(RequestKind kind, const QString &path,
               const QUrlQuery &query = {}, const TorrentKey &key = {});
  QNetworkRequest makeRequest(const QString &path,
                              const QUrlQuery &query = {}) const;
  void handleReply(QNetworkReply *reply);
  void setServer(const QString &name, const QString &url,
                 const QString &username, const QString &password);
  void abortRequests();
  void emitUnsupported(const QString &operation);

  static QJsonObject normalizeTorrent(const QJsonObject &source);
  static TorrentDetails normalizeDetails(const QVariantMap &info,
                                         const QVariantMap &properties);
};

#endif // QBITTORRENTBACKEND_H
