#ifndef TORRENTPEERSCONTROLLER_H
#define TORRENTPEERSCONTROLLER_H

#include <QObject>
#include <QJsonArray>

class GeoIpService;
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

private:
    enum PeerColumn {
        CountryColumn = 0,
        AddressColumn,
        PortColumn,
        ClientColumn,
        ProgressColumn,
        DownloadColumn,
        UploadColumn,
        EncryptedColumn,
        IncomingColumn,
        PeerColumnCount
    };

    QTableWidget *peerTableWidget = nullptr;
    GeoIpService *geoIpService = nullptr;
};

#endif // TORRENTPEERSCONTROLLER_H
