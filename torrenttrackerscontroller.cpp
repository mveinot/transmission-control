#include "torrenttrackerscontroller.h"

#include "rpc_client.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>

TorrentTrackersController::TorrentTrackersController(QTableWidget *trackerTableWidget,
                                                     rpc_client *client,
                                                     QWidget *dialogParent,
                                                     QObject *parent)
    : QObject(parent)
    , trackerTableWidget(trackerTableWidget)
    , client(client)
    , dialogParent(dialogParent)
{
}

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
        tr("Host"),
        tr("Announce"),
        tr("Seeds"),
        tr("Leechers"),
        tr("Last Announce"),
        tr("Result")
    });

    connect(trackerTableWidget, &QTableWidget::customContextMenuRequested,
            this, &TorrentTrackersController::showContextMenu);
}

void TorrentTrackersController::clear()
{
    if (!trackerTableWidget)
        return;

    trackerTableWidget->clearContents();
    trackerTableWidget->setRowCount(0);
}

void TorrentTrackersController::setTorrentId(int torrentId)
{
    this->torrentId = torrentId;
}

void TorrentTrackersController::populate(const QJsonObject &details)
{
    if (!trackerTableWidget)
        return;

    const QJsonArray trackerStats = details.value("trackerStats").toArray();
    const QJsonArray trackers = details.value("trackers").toArray();

    trackerTableWidget->setSortingEnabled(false);
    trackerTableWidget->clearContents();
    trackerTableWidget->setRowCount(trackerStats.size());

    int row = 0;

    for (const QJsonValue &value : trackerStats) {
        const QJsonObject tracker = value.toObject();

        const QString host = tracker.value("host").toString();
        const QString announce = tracker.value("announce").toString();

        int trackerId = tracker.value("id").toInt(-1);

        if (trackerId < 0 && row < trackers.size())
            trackerId = trackers.at(row).toObject().value("id").toInt(-1);

        if (trackerId < 0) {
            for (const QJsonValue &trackerValue : trackers) {
                const QJsonObject trackerObject = trackerValue.toObject();

                if (trackerObject.value("announce").toString() == announce) {
                    trackerId = trackerObject.value("id").toInt(-1);
                    break;
                }
            }
        }

        const int seeders = tracker.value("seederCount").toInt(-1);
        const int leechers = tracker.value("leecherCount").toInt(-1);

        const qint64 lastAnnounceSeconds =
            tracker.value("lastAnnounceTime").toVariant().toLongLong();

        const QString lastAnnounceTime =
            lastAnnounceSeconds > 0
                ? QLocale().toString(QDateTime::fromSecsSinceEpoch(lastAnnounceSeconds),
                                      QLocale::ShortFormat)
                : tr("Never");

        const QString lastAnnounceResult =
            tracker.value("lastAnnounceResult").toString();

        auto *hostItem = new QTableWidgetItem(host);
        auto *announceItem = new QTableWidgetItem(announce);
        announceItem->setData(TrackerAnnounceRole, announce);
        announceItem->setData(TrackerIdRole, trackerId);
        hostItem->setData(TrackerIdRole, trackerId);

        auto *seedersItem = new QTableWidgetItem(
            seeders >= 0 ? QString::number(seeders) : tr("Unknown")
            );

        auto *leechersItem = new QTableWidgetItem(
            leechers >= 0 ? QString::number(leechers) : tr("Unknown")
            );

        auto *lastAnnounceItem = new QTableWidgetItem(lastAnnounceTime);
        auto *resultItem = new QTableWidgetItem(lastAnnounceResult);

        seedersItem->setData(Qt::UserRole, seeders);
        leechersItem->setData(Qt::UserRole, leechers);

        trackerTableWidget->setItem(row, HostColumn, hostItem);
        trackerTableWidget->setItem(row, AnnounceColumn, announceItem);
        trackerTableWidget->setItem(row, SeedsColumn, seedersItem);
        trackerTableWidget->setItem(row, LeechersColumn, leechersItem);
        trackerTableWidget->setItem(row, LastAnnounceColumn, lastAnnounceItem);
        trackerTableWidget->setItem(row, ResultColumn, resultItem);

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
    if (!client || torrentId < 0)
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

    client->addTorrentTracker(torrentId, trackerUrl);
    emit statusMessageRequested(tr("Adding tracker..."), 3000);
}

void TorrentTrackersController::editTrackerFromContextMenu(int row)
{
    if (!client || torrentId < 0)
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

    client->editTorrentTracker(torrentId, trackerId, trackerUrl);
    emit statusMessageRequested(tr("Updating tracker..."), 3000);
}

void TorrentTrackersController::removeTrackerFromContextMenu(int row)
{
    if (!client || torrentId < 0)
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

    client->removeTorrentTracker(torrentId, trackerId);
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
    addTrackerAction->setEnabled(torrentId >= 0);

    QAction *editTrackerAction = menu.addAction(tr("Edit Tracker…"));
    editTrackerAction->setEnabled(torrentId >= 0 && row >= 0 && trackerId >= 0);

    QAction *removeTrackerAction = menu.addAction(tr("Remove Tracker"));
    removeTrackerAction->setEnabled(torrentId >= 0 && row >= 0 && trackerId >= 0);

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

void TorrentTrackersController::copyTrackerUrlToClipboard(const QString &trackerUrl)
{
    if (trackerUrl.isEmpty())
        return;

    QApplication::clipboard()->setText(trackerUrl);
    emit statusMessageRequested(tr("Tracker URL copied to clipboard."), 3000);
}
