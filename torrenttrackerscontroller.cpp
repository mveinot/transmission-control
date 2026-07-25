#include "torrenttrackerscontroller.h"

#include "torrentbackend.h"
#include "settingskeys.h"
#include "tablecolumncontroller.h"
#include "tableplaceholdercontroller.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>

TorrentTrackersController::TorrentTrackersController(QTableWidget *trackerTableWidget,
                                                     TorrentBackend *client,
                                                     QWidget *dialogParent,
                                                     QObject *parent)
    : QObject(parent)
    , trackerTableWidget(trackerTableWidget)
    , client(client)
    , dialogParent(dialogParent)
{
}

TorrentTrackersController::~TorrentTrackersController() = default;

void TorrentTrackersController::setup()
{
    if (!trackerTableWidget)
        return;

    trackerTableWidget->setAlternatingRowColors(true);
    trackerTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    trackerTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    trackerTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trackerTableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    trackerTableWidget->horizontalHeader()->setStretchLastSection(true);
    trackerTableWidget->setColumnCount(TrackerColumnCount);
    trackerTableWidget->setHorizontalHeaderLabels({
        tr("Tier"),
        tr("Host"),
        tr("Site"),
        tr("Announce"),
        tr("Scrape"),
        tr("Announce State"),
        tr("Scrape State"),
        tr("Seeds"),
        tr("Leechers"),
        tr("Downloads"),
        tr("Last Announce"),
        tr("Next Announce"),
        tr("Last Scrape"),
        tr("Next Scrape"),
        tr("Announce Result"),
        tr("Scrape Result")
    });

    columnController = std::make_unique<TableColumnController>(
        trackerTableWidget->horizontalHeader(),
        QString::fromLatin1(SettingsKeys::TrackerTableHeaderState),
        QString::fromLatin1(SettingsKeys::TrackerTableVisibleColumns),
        QVector<TableColumnController::ColumnDefinition> {
            { TierColumn, QStringLiteral("tier"), true, true, false },
            { HostColumn, QStringLiteral("host"), true, false, true },
            { SiteColumn, QStringLiteral("site"), false, true, false },
            { AnnounceColumn, QStringLiteral("announce"), true, true, false },
            { ScrapeColumn, QStringLiteral("scrape"), false, true, false },
            { AnnounceStateColumn, QStringLiteral("announceState"), true, true, false },
            { ScrapeStateColumn, QStringLiteral("scrapeState"), false, true, false },
            { SeedsColumn, QStringLiteral("seeds"), true, true, false },
            { LeechersColumn, QStringLiteral("leechers"), true, true, false },
            { DownloadsColumn, QStringLiteral("downloads"), false, true, false },
            { LastAnnounceColumn, QStringLiteral("lastAnnounce"), true, true, false },
            { NextAnnounceColumn, QStringLiteral("nextAnnounce"), true, true, false },
            { LastScrapeColumn, QStringLiteral("lastScrape"), false, true, false },
            { NextScrapeColumn, QStringLiteral("nextScrape"), false, true, false },
            { LastAnnounceResultColumn, QStringLiteral("announceResult"), true, true, false },
            { LastScrapeResultColumn, QStringLiteral("scrapeResult"), false, true, false },
        },
        this);
    columnController->setup();

    placeholderController = std::make_unique<TablePlaceholderController>(trackerTableWidget, this);
    placeholderController->setMessage(tr("No torrent selected."));

    connect(trackerTableWidget, &QTableWidget::customContextMenuRequested,
            this, &TorrentTrackersController::showContextMenu);
}

void TorrentTrackersController::saveViewState() const
{
    if (columnController)
        columnController->saveState();
}

void TorrentTrackersController::restoreViewState()
{
    if (columnController)
        columnController->restoreState();
}

void TorrentTrackersController::clear()
{
    if (!trackerTableWidget)
        return;

    trackerTableWidget->clearContents();
    trackerTableWidget->setRowCount(0);

    if (placeholderController)
        placeholderController->setMessage(tr("No torrent selected."));
}

void TorrentTrackersController::setLoading()
{
    if (!trackerTableWidget)
        return;

    trackerTableWidget->clearContents();
    trackerTableWidget->setRowCount(0);

    if (placeholderController)
        placeholderController->setMessage(tr("Loading trackers…"));
}

void TorrentTrackersController::setTorrentKey(TorrentKey torrentKey)
{
    this->torrentKey = torrentKey;
}

void TorrentTrackersController::populate(const TorrentTrackers &snapshot)
{
    if (!trackerTableWidget)
        return;

    if (placeholderController)
        placeholderController->setMessage(snapshot.trackers.isEmpty() ? tr("No trackers reported for this torrent.") : QString());

    trackerTableWidget->setSortingEnabled(false);
    trackerTableWidget->clearContents();
    trackerTableWidget->setRowCount(snapshot.trackers.size());

    int row = 0;

    for (const TorrentTracker &tracker : snapshot.trackers) {
        const int tier = tracker.tier;
        const QString host = tracker.host;
        const QString siteName = tracker.siteName;
        const QString announce = tracker.announceUrl;
        const QString scrape = tracker.scrapeUrl;
        const int trackerId = tracker.id;
        const int announceState = tracker.announceState;
        const int scrapeState = tracker.scrapeState;
        const int seeders = tracker.seederCount;
        const int leechers = tracker.leecherCount;
        const int downloads = tracker.downloadCount;
        const qint64 lastAnnounceSeconds = tracker.lastAnnounceTime;
        const qint64 nextAnnounceSeconds = tracker.nextAnnounceTime;
        const qint64 lastScrapeSeconds = tracker.lastScrapeTime;
        const qint64 nextScrapeSeconds = tracker.nextScrapeTime;

        const QString lastAnnounceResult = displayTrackerResult(
            tracker.lastAnnounceResult,
            tracker.lastAnnounceSucceeded,
            tracker.lastAnnounceTimedOut);
        const QString lastScrapeResult = displayTrackerResult(
            tracker.lastScrapeResult,
            tracker.lastScrapeSucceeded,
            tracker.lastScrapeTimedOut);

        auto *tierItem = makeTextItem(tier >= 0 ? QString::number(tier) : tr("Unknown"), tier);
        auto *hostItem = makeTextItem(host);
        auto *siteItem = makeTextItem(siteName.isEmpty() ? tr("—") : siteName);
        auto *announceItem = makeTextItem(announce);
        auto *scrapeItem = makeTextItem(scrape.isEmpty() ? tr("—") : scrape);
        auto *announceStateItem = makeTextItem(formatTrackerState(announceState), announceState);
        auto *scrapeStateItem = makeTextItem(formatTrackerState(scrapeState), scrapeState);
        auto *seedersItem = makeTextItem(formatTrackerCount(seeders), seeders);
        auto *leechersItem = makeTextItem(formatTrackerCount(leechers), leechers);
        auto *downloadsItem = makeTextItem(formatTrackerCount(downloads), downloads);
        auto *lastAnnounceItem = makeTextItem(formatTrackerTime(lastAnnounceSeconds, tr("Never")), lastAnnounceSeconds);
        auto *nextAnnounceItem = makeTextItem(formatTrackerTime(nextAnnounceSeconds, tr("Unknown")), nextAnnounceSeconds);
        auto *lastScrapeItem = makeTextItem(formatTrackerTime(lastScrapeSeconds, tr("Never")), lastScrapeSeconds);
        auto *nextScrapeItem = makeTextItem(formatTrackerTime(nextScrapeSeconds, tr("Unknown")), nextScrapeSeconds);
        auto *lastAnnounceResultItem = makeTextItem(lastAnnounceResult);
        auto *lastScrapeResultItem = makeTextItem(lastScrapeResult);

        announceItem->setData(TrackerAnnounceRole, announce);
        announceItem->setData(TrackerIdRole, trackerId);
        hostItem->setData(TrackerIdRole, trackerId);

        const QString tooltip = tr("Host: %1\nAnnounce: %2\nScrape: %3\nTier: %4")
                                .arg(host.isEmpty() ? tr("Unknown") : host,
                                     announce.isEmpty() ? tr("Unknown") : announce,
                                     scrape.isEmpty() ? tr("Unknown") : scrape,
                                     tier >= 0 ? QString::number(tier) : tr("Unknown"));

        for (QTableWidgetItem *tableItem : {
                 tierItem,
                 hostItem,
                 siteItem,
                 announceItem,
                 scrapeItem,
                 announceStateItem,
                 scrapeStateItem,
                 seedersItem,
                 leechersItem,
                 downloadsItem,
                 lastAnnounceItem,
                 nextAnnounceItem,
                 lastScrapeItem,
                 nextScrapeItem,
                 lastAnnounceResultItem,
                 lastScrapeResultItem
             }) {
            tableItem->setToolTip(tooltip);
        }

        trackerTableWidget->setItem(row, TierColumn, tierItem);
        trackerTableWidget->setItem(row, HostColumn, hostItem);
        trackerTableWidget->setItem(row, SiteColumn, siteItem);
        trackerTableWidget->setItem(row, AnnounceColumn, announceItem);
        trackerTableWidget->setItem(row, ScrapeColumn, scrapeItem);
        trackerTableWidget->setItem(row, AnnounceStateColumn, announceStateItem);
        trackerTableWidget->setItem(row, ScrapeStateColumn, scrapeStateItem);
        trackerTableWidget->setItem(row, SeedsColumn, seedersItem);
        trackerTableWidget->setItem(row, LeechersColumn, leechersItem);
        trackerTableWidget->setItem(row, DownloadsColumn, downloadsItem);
        trackerTableWidget->setItem(row, LastAnnounceColumn, lastAnnounceItem);
        trackerTableWidget->setItem(row, NextAnnounceColumn, nextAnnounceItem);
        trackerTableWidget->setItem(row, LastScrapeColumn, lastScrapeItem);
        trackerTableWidget->setItem(row, NextScrapeColumn, nextScrapeItem);
        trackerTableWidget->setItem(row, LastAnnounceResultColumn, lastAnnounceResultItem);
        trackerTableWidget->setItem(row, LastScrapeResultColumn, lastScrapeResultItem);

        ++row;
    }

    trackerTableWidget->setSortingEnabled(true);
}

int TorrentTrackersController::trackerIdForRow(int row) const
{
    if (!trackerTableWidget || row < 0)
        return -1;

    QTableWidgetItem *announceItem = trackerTableWidget->item(row, AnnounceColumn);

    if (!announceItem)
        return -1;

    bool ok = false;
    const int trackerId = announceItem->data(TrackerIdRole).toInt(&ok);
    return ok ? trackerId : -1;
}

QString TorrentTrackersController::trackerAnnounceUrlForRow(int row) const
{
    if (!trackerTableWidget || row < 0)
        return QString();

    QTableWidgetItem *announceItem = trackerTableWidget->item(row, AnnounceColumn);

    if (!announceItem)
        return QString();

    QString trackerUrl =
        announceItem->data(TrackerAnnounceRole).toString().trimmed();

    if (trackerUrl.isEmpty())
        trackerUrl = announceItem->text().trimmed();

    return trackerUrl;
}

void TorrentTrackersController::addTrackerFromContextMenu()
{
    if (!client || !isValidTorrentKey(torrentKey))
        return;

    bool ok = false;
    const QString trackerUrl = QInputDialog::getText(
        dialogParent,
        tr("Add Tracker"),
        tr("Announce URL:"),
        QLineEdit::Normal,
        QString(),
        &ok
        ).trimmed();

    if (!ok)
        return;

    if (trackerUrl.isEmpty()) {
        QMessageBox::warning(
            dialogParent,
            tr("Add Tracker"),
            tr("The tracker announce URL cannot be empty.")
            );
        return;
    }

    client->addTorrentTracker(torrentKey, trackerUrl);
    emit statusMessageRequested(tr("Adding tracker..."), 3000);
}

void TorrentTrackersController::editTrackerFromContextMenu(int row)
{
    if (!client || !isValidTorrentKey(torrentKey))
        return;

    const int trackerId = trackerIdForRow(row);
    const QString oldTrackerUrl = trackerAnnounceUrlForRow(row);

    if (trackerId < 0 || oldTrackerUrl.isEmpty())
        return;

    bool ok = false;
    const QString trackerUrl = QInputDialog::getText(
        dialogParent,
        tr("Edit Tracker"),
        tr("Announce URL:"),
        QLineEdit::Normal,
        oldTrackerUrl,
        &ok
        ).trimmed();

    if (!ok)
        return;

    if (trackerUrl.isEmpty()) {
        QMessageBox::warning(
            dialogParent,
            tr("Edit Tracker"),
            tr("The tracker announce URL cannot be empty.")
            );
        return;
    }

    if (trackerUrl == oldTrackerUrl)
        return;

    client->editTorrentTracker(torrentKey, trackerId, trackerUrl);
    emit statusMessageRequested(tr("Updating tracker..."), 3000);
}

void TorrentTrackersController::removeTrackerFromContextMenu(int row)
{
    if (!client || !isValidTorrentKey(torrentKey))
        return;

    const int trackerId = trackerIdForRow(row);
    const QString trackerUrl = trackerAnnounceUrlForRow(row);

    if (trackerId < 0 || trackerUrl.isEmpty())
        return;

    const QMessageBox::StandardButton result = QMessageBox::question(
        dialogParent,
        tr("Remove Tracker"),
        tr("Remove this tracker from the torrent?\n\n%1").arg(trackerUrl),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
        );

    if (result != QMessageBox::Yes)
        return;

    client->removeTorrentTracker(torrentKey, trackerId);
    emit statusMessageRequested(tr("Removing tracker..."), 3000);
}

void TorrentTrackersController::showContextMenu(const QPoint &pos)
{
    if (!trackerTableWidget)
        return;

    QTableWidgetItem *item = trackerTableWidget->itemAt(pos);
    const int row = item ? item->row() : -1;

    if (item)
        trackerTableWidget->selectRow(row);

    const QString trackerUrl = trackerAnnounceUrlForRow(row);
    const int trackerId = trackerIdForRow(row);

    QMenu menu(dialogParent);

    QAction *addTrackerAction = menu.addAction(tr("Add Tracker…"));
    addTrackerAction->setEnabled(isValidTorrentKey(torrentKey));

    QAction *editTrackerAction = menu.addAction(tr("Edit Tracker…"));
    editTrackerAction->setEnabled(isValidTorrentKey(torrentKey) && row >= 0 && trackerId >= 0);

    QAction *removeTrackerAction = menu.addAction(tr("Remove Tracker"));
    removeTrackerAction->setEnabled(isValidTorrentKey(torrentKey) && row >= 0 && trackerId >= 0);

    menu.addSeparator();

    QAction *copyTrackerUrlAction = menu.addAction(tr("Copy Tracker URL"));
    copyTrackerUrlAction->setEnabled(!trackerUrl.isEmpty());

    connect(addTrackerAction, &QAction::triggered,
            this, &TorrentTrackersController::addTrackerFromContextMenu);

    connect(editTrackerAction, &QAction::triggered,
            this, [this, row]() { editTrackerFromContextMenu(row); });

    connect(removeTrackerAction, &QAction::triggered,
            this, [this, row]() { removeTrackerFromContextMenu(row); });

    connect(copyTrackerUrlAction, &QAction::triggered,
            this, [this, trackerUrl]() { copyTrackerUrlToClipboard(trackerUrl); });

    menu.exec(trackerTableWidget->viewport()->mapToGlobal(pos));
}

QString TorrentTrackersController::formatTrackerTime(qint64 seconds, const QString &emptyText) const
{
    if (seconds <= 0)
        return emptyText;

    return QLocale().toString(QDateTime::fromSecsSinceEpoch(seconds), QLocale::ShortFormat);
}

QString TorrentTrackersController::formatTrackerCount(int count) const
{
    return count >= 0 ? QString::number(count) : tr("Unknown");
}

QString TorrentTrackersController::formatTrackerState(int state) const
{
    switch (state) {
    case 0:
        return tr("Inactive");
    case 1:
        return tr("Waiting");
    case 2:
        return tr("Queued");
    case 3:
        return tr("Active");
    default:
        return tr("Unknown");
    }
}

QString TorrentTrackersController::displayTrackerResult(const QString &result,
                                                        bool succeeded,
                                                        bool timedOut) const
{
    if (timedOut)
        return tr("Timed out");

    if (!result.trimmed().isEmpty())
        return result.trimmed();

    if (succeeded)
        return tr("Success");

    return tr("—");
}

QTableWidgetItem *TorrentTrackersController::makeTextItem(const QString &text,
                                                          const QVariant &sortValue) const
{
    auto *item = new QTableWidgetItem(text);

    if (sortValue.isValid())
        item->setData(Qt::UserRole, sortValue);

    return item;
}

void TorrentTrackersController::copyTrackerUrlToClipboard(const QString &trackerUrl)
{
    if (trackerUrl.isEmpty())
        return;

    QApplication::clipboard()->setText(trackerUrl);
    emit statusMessageRequested(tr("Tracker URL copied to clipboard."), 3000);
}
