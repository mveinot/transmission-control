#include "torrentpeerscontroller.h"

#include "geoipservice.h"
#include "tablecolumncontroller.h"

#include <QAbstractItemView>
#include <QHostInfo>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>

namespace {
QString normalizedAddress(const QString &address)
{
    return address.trimmed();
}
}

TorrentPeersController::TorrentPeersController(QTableWidget *peerTableWidget,
                                               GeoIpService *geoIpService,
                                               QObject *parent)
    : QObject(parent)
    , peerTableWidget(peerTableWidget)
    , geoIpService(geoIpService)
{
}

TorrentPeersController::~TorrentPeersController() = default;

void TorrentPeersController::setup()
{
    if (!peerTableWidget)
        return;

    peerTableWidget->setColumnCount(PeerColumnCount);
    peerTableWidget->setHorizontalHeaderLabels({
        tr("Country"),
        tr("Address"),
        tr("Host"),
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

    columnController = std::make_unique<TableColumnController>(
        peerTableWidget->horizontalHeader(),
        QStringLiteral("ui/peerTableWidget/horizontalHeaderState/v4"),
        QStringLiteral("ui/peerTableWidget/visibleColumns/v1"),
        QVector<TableColumnController::ColumnDefinition> {
            { CountryColumn, QStringLiteral("country"), true, true, false },
            { AddressColumn, QStringLiteral("address"), true, false, true },
            { HostnameColumn, QStringLiteral("host"), true, true, false },
            { PortColumn, QStringLiteral("port"), true, true, false },
            { ClientColumn, QStringLiteral("client"), true, true, false },
            { ProgressColumn, QStringLiteral("progress"), true, true, false },
            { DownloadColumn, QStringLiteral("download"), true, true, false },
            { UploadColumn, QStringLiteral("upload"), true, true, false },
            { EncryptedColumn, QStringLiteral("encrypted"), true, true, false },
            { IncomingColumn, QStringLiteral("incoming"), true, true, false },
        },
        this);
    columnController->setup();
}

void TorrentPeersController::saveViewState() const
{
    if (columnController)
        columnController->saveState();
}

void TorrentPeersController::restoreViewState()
{
    if (columnController)
        columnController->restoreState();
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

        const QString address = normalizedAddress(peer.value("address").toString());

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
        addressItem->setToolTip(address);

        startHostnameLookupIfNeeded(address);

        const QString hostnameText = hostnameDisplayText(address);
        auto *hostnameItem = new QTableWidgetItem(hostnameText);
        hostnameItem->setToolTip(hostnameToolTip(address));
        hostnameItem->setData(Qt::UserRole, hostnameText.toCaseFolded());

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
        peerTableWidget->setItem(row, HostnameColumn, hostnameItem);
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

void TorrentPeersController::handleHostLookup(const QHostInfo &hostInfo)
{
    const int lookupId = hostInfo.lookupId();
    const QString address = hostnameLookupIds.take(lookupId);

    if (address.isEmpty())
        return;

    pendingHostnameLookups.remove(address);

    QString hostname;

    if (hostInfo.error() == QHostInfo::NoError) {
        hostname = hostInfo.hostName().trimmed();

        if (hostname == address)
            hostname.clear();
    }

    applyHostnameLookupResult(address, hostname);
}

void TorrentPeersController::updateHostnameItem(int row, const QString &address)
{
    if (!peerTableWidget)
        return;

    QTableWidgetItem *hostnameItem = peerTableWidget->item(row, HostnameColumn);

    if (!hostnameItem) {
        hostnameItem = new QTableWidgetItem;
        peerTableWidget->setItem(row, HostnameColumn, hostnameItem);
    }

    const QString displayText = hostnameDisplayText(address);

    hostnameItem->setText(displayText);
    hostnameItem->setToolTip(hostnameToolTip(address));
    hostnameItem->setData(Qt::UserRole, displayText.toCaseFolded());
}

void TorrentPeersController::startHostnameLookupIfNeeded(const QString &address)
{
    if (address.isEmpty())
        return;

    if (hostnameCache.contains(address) || pendingHostnameLookups.contains(address))
        return;

    pendingHostnameLookups.insert(address);

    const int lookupId = QHostInfo::lookupHost(
        address,
        this,
        SLOT(handleHostLookup(QHostInfo))
        );

    hostnameLookupIds.insert(lookupId, address);
}

void TorrentPeersController::applyHostnameLookupResult(const QString &address,
                                                       const QString &hostname)
{
    hostnameCache.insert(address, hostname);

    if (!peerTableWidget)
        return;

    const bool sortingWasEnabled = peerTableWidget->isSortingEnabled();
    peerTableWidget->setSortingEnabled(false);

    for (int row = 0; row < peerTableWidget->rowCount(); ++row) {
        const QTableWidgetItem *addressItem = peerTableWidget->item(row, AddressColumn);

        if (!addressItem || addressItem->text() != address)
            continue;

        updateHostnameItem(row, address);
    }

    peerTableWidget->setSortingEnabled(sortingWasEnabled);
}

QString TorrentPeersController::hostnameDisplayText(const QString &address) const
{
    if (address.isEmpty())
        return QString();

    if (!hostnameCache.contains(address))
        return pendingHostnameLookups.contains(address) ? tr("Resolving…") : QString();

    const QString hostname = hostnameCache.value(address);

    return hostname.isEmpty() ? QStringLiteral("—") : hostname;
}

QString TorrentPeersController::hostnameToolTip(const QString &address) const
{
    if (address.isEmpty())
        return QString();

    if (!hostnameCache.contains(address)) {
        return pendingHostnameLookups.contains(address)
            ? tr("Reverse DNS lookup pending for %1").arg(address)
            : address;
    }

    const QString hostname = hostnameCache.value(address);

    if (hostname.isEmpty())
        return tr("No reverse DNS hostname found for %1").arg(address);

    return QStringLiteral("%1 (%2)").arg(hostname, address);
}
