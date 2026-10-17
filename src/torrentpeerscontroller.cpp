#include "torrentpeerscontroller.h"

#include "geoipservice.h"
#include "settingskeys.h"
#include "tablecolumncontroller.h"
#include "tableplaceholdercontroller.h"

#include <QAbstractItemView>
#include <QFile>
#include <QHostInfo>
#include <QIcon>
#include <QLocale>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QSize>
#include <QHeaderView>

namespace {

constexpr int MaximumConcurrentHostnameLookups = 8;
constexpr int MaximumHostnameCacheEntries = 256;
QString normalizedAddress(const QString &address)
{
    return address.trimmed();
}

// provide a path to a SVG flag resource for a given country code
QString flagResourcePathForCountryCode(QString countryCode)
{
    countryCode = countryCode.trimmed().toLower();

    if (countryCode.size() != 2)
        return {};

    for (const QChar ch : std::as_const(countryCode)) {
        if (ch < QLatin1Char('a') || ch > QLatin1Char('z'))
            return {};
    }

    const QString path = QStringLiteral(":/flags/%1.svg").arg(countryCode);
    return QFile::exists(path) ? path : QString {};
}

// return a qicon for a given country code
QIcon flagIconForCountryCode(const QString &countryCode)
{
    const QString path = flagResourcePathForCountryCode(countryCode);
    return path.isEmpty() ? QIcon {} : QIcon(path);
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
    peerTableWidget->setIconSize(QSize(24, 16));

    columnController = std::make_unique<TableColumnController>(
        peerTableWidget->horizontalHeader(),
        QString::fromLatin1(SettingsKeys::PeerTableHeaderState),
        QString::fromLatin1(SettingsKeys::PeerTableVisibleColumns),
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

    placeholderController = std::make_unique<TablePlaceholderController>(peerTableWidget, this);
    placeholderController->setMessage(tr("No torrent selected."));
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

    resetRows();

    if (placeholderController)
        placeholderController->setMessage(tr("No torrent selected."));
}

void TorrentPeersController::setLoading()
{
    if (!peerTableWidget)
        return;

    resetRows();

    if (placeholderController)
        placeholderController->setMessage(tr("Loading peers…"));
}

void TorrentPeersController::populate(const TorrentPeers &snapshot)
{
    if (!peerTableWidget)
        return;

    if (placeholderController)
        placeholderController->setMessage(snapshot.peers.isEmpty() ? tr("No peers connected.") : QString());

    const QVector<PeerRowChange> changes = reconciler.reconcile(snapshot.peers);
    if (changes.isEmpty())
        return;

    PeerRowKey selectedKey;
    bool hasSelectedKey = false;
    const int selectedRow = peerTableWidget->currentRow();
    if (selectedRow >= 0) {
        QTableWidgetItem *selectedAnchor =
            peerTableWidget->item(selectedRow, AddressColumn);
        for (auto it = rowAnchors.cbegin(); it != rowAnchors.cend(); ++it) {
            if (it.value() == selectedAnchor) {
                selectedKey = it.key();
                hasSelectedKey = true;
                break;
            }
        }
    }

    const bool sortingWasEnabled = peerTableWidget->isSortingEnabled();
    const int sortedColumn =
        peerTableWidget->horizontalHeader()->sortIndicatorSection();
    bool suspendSorting = false;

    for (const PeerRowChange &change : changes) {
        if (change.kind != PeerRowChange::Kind::Update
            || changeAffectsColumn(change, sortedColumn)) {
            suspendSorting = sortingWasEnabled;
            break;
        }
    }

    if (suspendSorting)
        peerTableWidget->setSortingEnabled(false);

    // Reconciler output orders removals before inserts and updates. Removing
    // first avoids transient duplicate rows when an endpoint disappears and
    // another peer is added in the same snapshot.
    for (const PeerRowChange &change : changes) {
        switch (change.kind) {
        case PeerRowChange::Kind::Remove:
            removePeerRow(change.key);
            break;
        case PeerRowChange::Kind::Insert:
            insertPeerRow(change.key, change.peer);
            break;
        case PeerRowChange::Kind::Update:
            updatePeerRow(change);
            break;
        }
    }

    if (suspendSorting)
        peerTableWidget->setSortingEnabled(true);

    if (hasSelectedKey) {
        const int row = rowForKey(selectedKey);
        if (row >= 0)
            peerTableWidget->selectRow(row);
    }
}

void TorrentPeersController::resetRows()
{
    reconciler.clear();
    rowAnchors.clear();
    peerTableWidget->clearContents();
    peerTableWidget->setRowCount(0);
}

void TorrentPeersController::insertPeerRow(const PeerRowKey &key,
                                           const TorrentPeer &peer)
{
    const QString address = key.address;
    const GeoIpResult geoIp =
        geoIpService ? geoIpService->lookup(address) : GeoIpResult {};
    const int row = peerTableWidget->rowCount();
    peerTableWidget->insertRow(row);

    auto *countryItem = new QTableWidgetItem(geoIp.displayText());
    countryItem->setIcon(flagIconForCountryCode(geoIp.countryCode));
    countryItem->setToolTip(
        geoIp.found
            ? QStringLiteral("%1 (%2) - %3")
                  .arg(geoIp.countryName, geoIp.countryCode, address)
            : address);
    countryItem->setData(Qt::UserRole, geoIp.countryCode);

    auto *addressItem = new QTableWidgetItem(address);
    addressItem->setToolTip(address);

    startHostnameLookupIfNeeded(address);
    const QString hostnameText = hostnameDisplayText(address);
    auto *hostnameItem = new QTableWidgetItem(hostnameText);
    hostnameItem->setToolTip(hostnameToolTip(address));
    hostnameItem->setData(Qt::UserRole, hostnameText.toCaseFolded());

    auto *portItem = new QTableWidgetItem(QString::number(peer.port));
    portItem->setData(Qt::UserRole, peer.port);

    peerTableWidget->setItem(row, CountryColumn, countryItem);
    peerTableWidget->setItem(row, AddressColumn, addressItem);
    peerTableWidget->setItem(row, HostnameColumn, hostnameItem);
    peerTableWidget->setItem(row, PortColumn, portItem);
    peerTableWidget->setItem(row, ClientColumn, new QTableWidgetItem);
    peerTableWidget->setItem(row, ProgressColumn, new QTableWidgetItem);
    peerTableWidget->setItem(row, DownloadColumn, new QTableWidgetItem);
    peerTableWidget->setItem(row, UploadColumn, new QTableWidgetItem);
    peerTableWidget->setItem(row, EncryptedColumn, new QTableWidgetItem);
    peerTableWidget->setItem(row, IncomingColumn, new QTableWidgetItem);
    rowAnchors.insert(key, addressItem);

    setClientItem(row, peer.clientName);
    setProgressItem(row, peer.progress);
    setRateItem(row, DownloadColumn, peer.downloadRate);
    setRateItem(row, UploadColumn, peer.uploadRate);
    setBooleanItem(row, EncryptedColumn, peer.encrypted);
    setBooleanItem(row, IncomingColumn, peer.incoming);
}

void TorrentPeersController::updatePeerRow(const PeerRowChange &change)
{
    const int row = rowForKey(change.key);
    if (row < 0)
        return;

    if (change.fields.testFlag(PeerField::Client))
        setClientItem(row, change.peer.clientName);
    if (change.fields.testFlag(PeerField::Progress))
        setProgressItem(row, change.peer.progress);
    if (change.fields.testFlag(PeerField::DownloadRate))
        setRateItem(row, DownloadColumn, change.peer.downloadRate);
    if (change.fields.testFlag(PeerField::UploadRate))
        setRateItem(row, UploadColumn, change.peer.uploadRate);
    if (change.fields.testFlag(PeerField::Encrypted))
        setBooleanItem(row, EncryptedColumn, change.peer.encrypted);
    if (change.fields.testFlag(PeerField::Incoming))
        setBooleanItem(row, IncomingColumn, change.peer.incoming);
}

void TorrentPeersController::removePeerRow(const PeerRowKey &key)
{
    const int row = rowForKey(key);
    rowAnchors.remove(key);
    if (row >= 0)
        peerTableWidget->removeRow(row);
}

int TorrentPeersController::rowForKey(const PeerRowKey &key) const
{
    QTableWidgetItem *anchor = rowAnchors.value(key, nullptr);
    return anchor ? peerTableWidget->row(anchor) : -1;
}

bool TorrentPeersController::changeAffectsColumn(
    const PeerRowChange &change,
    int column) const
{
    if (change.kind != PeerRowChange::Kind::Update)
        return true;

    switch (column) {
    case ClientColumn:
        return change.fields.testFlag(PeerField::Client);
    case ProgressColumn:
        return change.fields.testFlag(PeerField::Progress);
    case DownloadColumn:
        return change.fields.testFlag(PeerField::DownloadRate);
    case UploadColumn:
        return change.fields.testFlag(PeerField::UploadRate);
    case EncryptedColumn:
        return change.fields.testFlag(PeerField::Encrypted);
    case IncomingColumn:
        return change.fields.testFlag(PeerField::Incoming);
    default:
        return false;
    }
}

void TorrentPeersController::setClientItem(int row, const QString &clientName)
{
    peerTableWidget->item(row, ClientColumn)->setText(
        clientName.isEmpty() ? QStringLiteral("(unknown)") : clientName);
}

void TorrentPeersController::setProgressItem(int row, double progress)
{
    const double percent = progress * 100.0;
    QTableWidgetItem *item = peerTableWidget->item(row, ProgressColumn);
    item->setText(QStringLiteral("%1%").arg(percent, 0, 'f', 1));
    item->setData(Qt::UserRole, percent);
}

void TorrentPeersController::setRateItem(int row, int column, qint64 rate)
{
    QTableWidgetItem *item = peerTableWidget->item(row, column);
    item->setText(
        QLocale().formattedDataSize(rate, 1, QLocale::DataSizeIecFormat)
        + QStringLiteral("/s"));
    item->setData(Qt::UserRole, rate);
}

void TorrentPeersController::setBooleanItem(int row, int column, bool value)
{
    QTableWidgetItem *item = peerTableWidget->item(row, column);
    item->setText(value ? tr("Yes") : tr("No"));
    item->setData(Qt::UserRole, value);
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
    startQueuedHostnameLookups();
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

    // QHostInfo does not coalesce identical requests. Pending addresses are
    // tracked separately from cached results, including negative results.
    pendingHostnameLookups.insert(address);
    hostnameLookupQueue.append(address);
    startQueuedHostnameLookups();
}

void TorrentPeersController::startQueuedHostnameLookups()
{
    while (hostnameLookupIds.size() < MaximumConcurrentHostnameLookups
           && !hostnameLookupQueue.isEmpty()) {
        const QString address = hostnameLookupQueue.takeFirst();

        const int lookupId = QHostInfo::lookupHost(
            address,
            this,
            SLOT(handleHostLookup(QHostInfo))
            );

        hostnameLookupIds.insert(lookupId, address);
    }
}

void TorrentPeersController::cacheHostnameResult(const QString &address,
                                                 const QString &hostname)
{
    hostnameCache.insert(address, hostname);
    hostnameCacheOrder.removeAll(address);
    hostnameCacheOrder.append(address);

    while (hostnameCacheOrder.size() > MaximumHostnameCacheEntries) {
        const QString evictedAddress = hostnameCacheOrder.takeFirst();
        hostnameCache.remove(evictedAddress);
    }
}

void TorrentPeersController::applyHostnameLookupResult(const QString &address,
                                                       const QString &hostname)
{
    // Successful and negative answers share the same bounded cache so dead
    // addresses cannot cause unbounded retries or memory growth.
    cacheHostnameResult(address, hostname);

    if (!peerTableWidget)
        return;

    const bool suspendSorting =
        peerTableWidget->isSortingEnabled()
        && peerTableWidget->horizontalHeader()->sortIndicatorSection()
               == HostnameColumn;
    if (suspendSorting)
        peerTableWidget->setSortingEnabled(false);

    for (auto it = rowAnchors.cbegin(); it != rowAnchors.cend(); ++it) {
        if (it.key().address != address)
            continue;

        const int row = peerTableWidget->row(it.value());
        if (row >= 0)
            updateHostnameItem(row, address);
    }

    if (suspendSorting)
        peerTableWidget->setSortingEnabled(true);
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
