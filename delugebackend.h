#ifndef DELUGEBACKEND_H
#define DELUGEBACKEND_H

#include "torrentbackend.h"

#include <QHash>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// Deluge Web JSON-RPC transport. Torrent projections and commands are added
// independently; this layer owns authentication, daemon readiness, request
// correlation, retry, and server-switch cancellation.
class DelugeBackend : public TorrentBackend
{
    Q_OBJECT

public:
    explicit DelugeBackend(QObject *parent = nullptr);

    QString backendName() const override;
    QString serverDisplayName() const override;
    QString endpointUrl() const override;
    TorrentBackendCapabilities capabilities() const override;
    bool loadCurrentServerFromSettings() override;
    bool setServerFromSettingsIndex(int index) override;
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
    void setTorrentFilesWantedAndPriority(const TorrentKey &,
                                          const QList<int> &, bool,
                                          int) override;
    void addTorrentTracker(const TorrentKey &, const QString &) override;
    void editTorrentTracker(const TorrentKey &, int, const QString &) override;
    void removeTorrentTracker(const TorrentKey &, int) override;
    void renameTorrentPath(const TorrentKey &, const QString &,
                           const QString &) override;
    void setTorrentProperties(const TorrentKey &,
                              const TorrentPropertyChanges &) override;
    void setTorrentsSequentialDownload(const QList<TorrentKey> &,
                                       bool) override;
    void setTorrentsBandwidthPriority(const QList<TorrentKey> &,
                                      int) override;
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
    enum class RequestKind {
        Login,
        DaemonConnectionCheck
    };

    struct RequestContext {
        RequestKind kind = RequestKind::Login;
        qint64 id = 0;
        QString method;
        QJsonArray parameters;
        quint64 generation = 0;
        bool retriedAuthentication = false;
    };

    QNetworkAccessManager m_network;
    QHash<QNetworkReply *, RequestContext> m_requests;
    QString m_serverName;
    QString m_baseUrl;
    QString m_rpcUrl;
    QString m_password;
    qint64 m_nextRequestId = 1;
    quint64 m_generation = 0;
    bool m_authenticated = false;
    bool m_ready = false;
    bool m_authenticationPending = false;
    bool m_connectionCheckPending = false;
    bool m_connectionCheckRetriedAuthentication = false;

    void setServer(const QString &name, const QString &url,
                   const QString &password);
    void authenticate(bool preserveConnectionRetry = false);
    void checkDaemonConnection();
    void postRpc(RequestContext context);
    QNetworkRequest makeRequest() const;
    void handleReply(QNetworkReply *reply);
    void handleAuthenticationFailure(const QString &reason);
    void retryAfterAuthentication();
    void abortRequests();
    void emitUnsupported(const QString &operation);

    static bool isAuthenticationError(const QJsonObject &error);
    static QString rpcErrorMessage(const QJsonObject &error);
};

#endif // DELUGEBACKEND_H
