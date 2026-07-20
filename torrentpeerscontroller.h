#ifndef TORRENTPEERSCONTROLLER_H
#define TORRENTPEERSCONTROLLER_H

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QSet>
#include <memory>

class GeoIpService;
class QHostInfo;
class QTableWidget;
class TableColumnController;
class TablePlaceholderController;

// Rebuilds the peer snapshot table and enriches addresses with cached local
// GeoIP results and asynchronous reverse-DNS names.
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
    void populate(const QJsonArray &peers);
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

    void updateHostnameItem(int row, const QString &address);
    void startHostnameLookupIfNeeded(const QString &address);
    void applyHostnameLookupResult(const QString &address, const QString &hostname);
    QString hostnameDisplayText(const QString &address) const;
    QString hostnameToolTip(const QString &address) const;

    QTableWidget *peerTableWidget = nullptr;
    GeoIpService *geoIpService = nullptr;
    QHash<QString, QString> hostnameCache;
    QSet<QString> pendingHostnameLookups;
    QHash<int, QString> hostnameLookupIds;
    std::unique_ptr<TableColumnController> columnController;
    std::unique_ptr<TablePlaceholderController> placeholderController;
};

#endif // TORRENTPEERSCONTROLLER_H
