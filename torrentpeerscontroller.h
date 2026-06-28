#ifndef TORRENTPEERSCONTROLLER_H
#define TORRENTPEERSCONTROLLER_H

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QSet>

class GeoIpService;
class QHostInfo;
class QTableWidget;

class TorrentPeersController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentPeersController(QTableWidget *peerTableWidget,
                                    GeoIpService *geoIpService,
                                    QObject *parent = nullptr);

    void setup();
    void clear();
    void populate(const QJsonArray &peers);

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
};

#endif // TORRENTPEERSCONTROLLER_H
