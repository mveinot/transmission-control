#include "torrentpeerscontroller.h"

#include "geoipservice.h"

#include <QAbstractItemView>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QTableWidget>
#include <QTableWidgetItem>

TorrentPeersController::TorrentPeersController(QTableWidget *peerTableWidget,
                                               GeoIpService *geoIpService,
                                               QObject *parent)
    : QObject(parent)
    , peerTableWidget(peerTableWidget)
    , geoIpService(geoIpService)
{
}

void TorrentPeersController::setup()
{
    if (!peerTableWidget)
        return;

    peerTableWidget->setColumnCount(PeerColumnCount);
    peerTableWidget->setHorizontalHeaderLabels({
        tr("Country"),
        tr("Address"),
        tr("Port"),
        tr("Client"),
        tr("Progress"),
        tr("Download"),
        tr("Upload"),
        tr("Encrypted"),
        tr("Incoming")
    });
    peerTableWidget->setAlternatingRowColors(true);
    peerTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    peerTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    peerTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    peerTableWidget->setSortingEnabled(true);
}

void TorrentPeersController::clear()
{
    if (!peerTableWidget)
        return;

    peerTableWidget->clearContents();
    peerTableWidget->setRowCount(0);
}

void TorrentPeersController::populate(const QJsonArray &peers)
{
    if (!peerTableWidget)
        return;

    peerTableWidget->setSortingEnabled(false);
    peerTableWidget->clearContents();
    peerTableWidget->setRowCount(peers.size());

    int row = 0;

    for (const QJsonValue &peerValue : peers) {
        const QJsonObject peer = peerValue.toObject();

        const QString address = peer.value("address").toString();

        const GeoIpResult geoIp =
            geoIpService ? geoIpService->lookup(address) : GeoIpResult {};

        const int port = peer.value("port").toInt();

        const QString clientName =
            peer.value("clientName").toString().isEmpty()
                ? QStringLiteral("(unknown)")
                : peer.value("clientName").toString();

        const double progress = peer.value("progress").toDouble() * 100.0;

        const qint64 rateToClient =
            peer.value("rateToClient").toVariant().toLongLong();

        const qint64 rateToPeer =
            peer.value("rateToPeer").toVariant().toLongLong();

        const bool isEncrypted = peer.value("isEncrypted").toBool();
        const bool isIncoming = peer.value("isIncoming").toBool();

        auto *countryItem = new QTableWidgetItem(geoIp.displayText());
        countryItem->setToolTip(
            geoIp.found
                ? QString("%1 (%2)").arg(geoIp.countryName, address)
                : QString("%1").arg(address)
            );
        countryItem->setData(Qt::UserRole, geoIp.countryCode);

        auto *addressItem = new QTableWidgetItem(address);

        auto *portItem = new QTableWidgetItem(QString::number(port));
        portItem->setData(Qt::UserRole, port);

        auto *clientItem = new QTableWidgetItem(clientName);

        auto *progressItem =
            new QTableWidgetItem(QString("%1%").arg(progress, 0, 'f', 1));
        progressItem->setData(Qt::UserRole, progress);

        auto *downloadItem =
            new QTableWidgetItem(
                QLocale().formattedDataSize(
                    rateToClient,
                    1,
                    QLocale::DataSizeIecFormat
                    ) + "/s"
                );
        downloadItem->setData(Qt::UserRole, rateToClient);

        auto *uploadItem =
            new QTableWidgetItem(
                QLocale().formattedDataSize(
                    rateToPeer,
                    1,
                    QLocale::DataSizeIecFormat
                    ) + "/s"
                );
        uploadItem->setData(Qt::UserRole, rateToPeer);

        auto *encryptedItem =
            new QTableWidgetItem(isEncrypted ? tr("Yes") : tr("No"));
        encryptedItem->setData(Qt::UserRole, isEncrypted);

        auto *incomingItem =
            new QTableWidgetItem(isIncoming ? tr("Yes") : tr("No"));
        incomingItem->setData(Qt::UserRole, isIncoming);

        peerTableWidget->setItem(row, CountryColumn, countryItem);
        peerTableWidget->setItem(row, AddressColumn, addressItem);
        peerTableWidget->setItem(row, PortColumn, portItem);
        peerTableWidget->setItem(row, ClientColumn, clientItem);
        peerTableWidget->setItem(row, ProgressColumn, progressItem);
        peerTableWidget->setItem(row, DownloadColumn, downloadItem);
        peerTableWidget->setItem(row, UploadColumn, uploadItem);
        peerTableWidget->setItem(row, EncryptedColumn, encryptedItem);
        peerTableWidget->setItem(row, IncomingColumn, incomingItem);

        ++row;
    }

    peerTableWidget->setSortingEnabled(true);
}
