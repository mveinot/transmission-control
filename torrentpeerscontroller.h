#ifndef TORRENTPEERSCONTROLLER_H
#define TORRENTPEERSCONTROLLER_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <memory>

#include "peerreconciler.h"
#include "torrentdomain.h"

class GeoIpService;
class QHostInfo;
class QTableWidget;
class QTableWidgetItem;
class TableColumnController;
class TablePlaceholderController;

// Applies endpoint-keyed peer snapshot changes to the table and enriches new
// addresses with cached local GeoIP results and asynchronous reverse DNS.
class TorrentPeersController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentPeersController(QTableWidget *peerTableWidget,
                                    GeoIpService *geoIpService,
                                    QObject *parent = nullptr);
    ~TorrentPeersController() override;

    void setup();
    void clear();
    void populate(const TorrentPeers &peers);
    void saveViewState() const;
    void restoreViewState();
    void setLoading();

private slots:
    void handleHostLookup(const QHostInfo &hostInfo);

private:
    enum PeerColumn {
        CountryColumn = 0,
        AddressColumn,
        HostnameColumn,
        PortColumn,
        ClientColumn,
        ProgressColumn,
        DownloadColumn,
        UploadColumn,
        EncryptedColumn,
        IncomingColumn,
        PeerColumnCount
    };

    void resetRows();
    void insertPeerRow(const PeerRowKey &key, const TorrentPeer &peer);
    void updatePeerRow(const PeerRowChange &change);
    void removePeerRow(const PeerRowKey &key);
    int rowForKey(const PeerRowKey &key) const;
    bool changeAffectsColumn(const PeerRowChange &change, int column) const;
    void setClientItem(int row, const QString &clientName);
    void setProgressItem(int row, double progress);
    void setRateItem(int row, int column, qint64 rate);
    void setBooleanItem(int row, int column, bool value);
    void updateHostnameItem(int row, const QString &address);
    void startHostnameLookupIfNeeded(const QString &address);
    void startQueuedHostnameLookups();
    void cacheHostnameResult(const QString &address, const QString &hostname);
    void applyHostnameLookupResult(const QString &address, const QString &hostname);
    QString hostnameDisplayText(const QString &address) const;
    QString hostnameToolTip(const QString &address) const;

    QTableWidget *peerTableWidget = nullptr;
    GeoIpService *geoIpService = nullptr;
    PeerSnapshotReconciler reconciler;
    QHash<PeerRowKey, QTableWidgetItem *> rowAnchors;
    QHash<QString, QString> hostnameCache;
    QSet<QString> pendingHostnameLookups;
    QHash<int, QString> hostnameLookupIds;
    QStringList hostnameLookupQueue;
    QStringList hostnameCacheOrder;
    std::unique_ptr<TableColumnController> columnController;
    std::unique_ptr<TablePlaceholderController> placeholderController;
};

#endif // TORRENTPEERSCONTROLLER_H
