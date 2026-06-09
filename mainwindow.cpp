#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QTimer>
#include <QActionGroup>
#include <QHeaderView>
#include <QSettings>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLocale>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMessageBox>
#include <QPushButton>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenu>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QComboBox>
#include <QSystemTrayIcon>
#include <QAction>
#include <QCloseEvent>
#include <QEvent>
#include <QApplication>
#include <QIcon>
#include "rpc_client.h"
#include "dialogabout.h"
#include "serverconfig.h"
#include "appsettings.h"
#include "torrentsortproxymodel.h"
#include "percentfilldelegate.h"

namespace {
constexpr const char *DeleteTorrentOnAddKey = "app/deleteTorrentFileOnSuccessfulAdd";
constexpr const char *UpdateIntervalKey = "app/updateIntervalSeconds";
constexpr int DefaultUpdateIntervalSeconds = 10;
constexpr int MinimumUpdateIntervalSeconds = 1;
constexpr int MaximumUpdateIntervalSeconds = 3600;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    timer = new QTimer(this);
    client = new rpc_client(this);
    torrentModel = new TorrentModel(this);

    proxy = new TorrentSortProxyModel(this);
    proxy->setSourceModel(torrentModel);

    this->aboutAction = new QAction(0);
    this->aboutAction->setMenuRole(QAction::AboutRole);

    ui->setupUi(this);

    setupConnectionStatusIndicator();

    auto *stateGroup = new QActionGroup(this);
    stateGroup->setExclusive(true);

    MainWindow::setWindowTitle(QCoreApplication::applicationName());

    ui->tableView->setModel(proxy);

    updateTorrentActionState();

    connect(client, &rpc_client::torrentsReceived,
            torrentModel, &TorrentModel::applyUpdate);

    connect(torrentModel, &TorrentModel::listUpdated,
            this, &MainWindow::drawTorrentList);

    connect(ui->tableView->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            [this]() {
                updateTorrentActionState();
            });

    connect(ui->tableView->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            [this]() {
                updateTorrentActionState();
            });

    ui->fileTreeWidget->setColumnCount(FileColumnCount);
    ui->fileTreeWidget->setHeaderLabels({ "Name", "Size", "Done", "Completed" });
    ui->fileTreeWidget->setAlternatingRowColors(true);
    ui->fileTreeWidget->setRootIsDecorated(true);

    ui->actionAll->setCheckable(true);
    ui->actionDownloading->setCheckable(true);
    ui->actionCompleted->setCheckable(true);
    ui->actionActive->setCheckable(true);
    ui->actionInactive->setCheckable(true);
    ui->actionStopped->setCheckable(true);
    ui->actionError->setCheckable(true);

    stateGroup->addAction(ui->actionAll);
    stateGroup->addAction(ui->actionDownloading);
    stateGroup->addAction(ui->actionCompleted);
    stateGroup->addAction(ui->actionActive);
    stateGroup->addAction(ui->actionInactive);
    stateGroup->addAction(ui->actionStopped);
    stateGroup->addAction(ui->actionError);
    ui->actionAll->setChecked(true);

    ui->peerTableWidget->setColumnCount(8);
    ui->peerTableWidget->setHorizontalHeaderLabels({
        "Address",
        "Port",
        "Client",
        "Progress",
        "Download",
        "Upload",
        "Encrypted",
        "Incoming"
    });
    ui->peerTableWidget->setAlternatingRowColors(true);
    ui->peerTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->peerTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->peerTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->peerTableWidget->setSortingEnabled(true);

    ui->tableView->hideColumn(TorrentModel::IdColumn);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tableView->sortByColumn(TorrentModel::NameColumn, Qt::AscendingOrder);
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableView->setItemDelegateForColumn(
        TorrentModel::PercentDoneColumn,
        new PercentFillDelegate(
            TorrentModel::PercentDoneColumn,
            Qt::UserRole + 1,
            ui->tableView
            )
        );

    ui->trackerTableWidget->setColumnCount(6);
    ui->trackerTableWidget->setHorizontalHeaderLabels({
        "Host",
        "Announce",
        "Seeds",
        "Leechers",
        "Last Announce",
        "Result"
    });

    ui->trackerTableWidget->setAlternatingRowColors(true);
    ui->trackerTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->trackerTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->trackerTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->trackerTableWidget->horizontalHeader()->setStretchLastSection(true);

    ui->fileTreeWidget->setItemDelegateForColumn(
        FilePercentColumn,
        new PercentFillDelegate(FilePercentColumn, Qt::UserRole, ui->fileTreeWidget)
        );

    ui->statusbar->showMessage(client->getServer());

    connect(client, &rpc_client::serverChanged,
            torrentModel, &TorrentModel::clear);

    connect(ui->actionAll, &QAction::triggered, this, [this]() {
        setTorrentStateFilter(TorrentSortProxyModel::StateFilter::All);
    });

    connect(ui->actionDownloading, &QAction::triggered, this, [this]() {
        setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Downloading);
    });

    connect(ui->actionCompleted, &QAction::triggered, this, [this]() {
        setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Completed);
    });

    connect(ui->actionActive, &QAction::triggered, this, [this]() {
        setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Active);
    });

    connect(ui->actionInactive, &QAction::triggered, this, [this]() {
        setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Inactive);
    });

    connect(ui->actionStopped, &QAction::triggered, this, [this]() {
        setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Stopped);
    });

    connect(ui->actionError, &QAction::triggered, this, [this]() {
        setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Error);
    });

    connect(ui->actionServer_Setup, &QAction::triggered, this, &MainWindow::onServerSetupTriggered);

    connect(ui->listWidget, &QListWidget::currentRowChanged,
            this, [this](int row) {
                switch (row) {
                case 0:
                    setTorrentStateFilter(TorrentSortProxyModel::StateFilter::All);
                    break;

                case 1:
                    setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Downloading);
                    break;

                case 2:
                    setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Completed);
                    break;

                case 3:
                    setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Active);
                    break;

                case 4:
                    setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Inactive);
                    break;

                case 5:
                    setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Stopped);
                    break;

                case 6:
                    setTorrentStateFilter(TorrentSortProxyModel::StateFilter::Error);
                    break;

                default:
                    setTorrentStateFilter(TorrentSortProxyModel::StateFilter::All);
                    break;
                }
            });

    this->mainMenu = new QMenu(0);
    this->menuBar()->addMenu(this->mainMenu);
    this->mainMenu->addAction(this->aboutAction);
    this->setMenuBar(this->menuBar());

    connect(timer, &QTimer::timeout, this, &MainWindow::updateTorrentList);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    connect(client, &rpc_client::torrentDetailsReceived,
            this,
            [this](int torrentId, const QJsonObject &details) {
                if (torrentId != currentTorrentId())
                    return;

                populateGeneralTab(details);
                populateTrackerTable(details);
                populateFileTree(details.value("files").toArray());
                populatePeerTable(details.value("peers").toArray());

                // Later:
                // priorities = details.value("priorities").toArray();
                // wanted = details.value("wanted").toArray();
            });

    connect(client, &rpc_client::updateStarted,
            this,
            [this]() {
                connectionStatusLabel->setStyleSheet("");
                connectionStatusLabel->setText("Updating...");
            });

    connect(client, &rpc_client::updateFailed,
            this,
            [this](const QString &message) {
                connectionStatusLabel->setStyleSheet("color: #ff6b6b;");
                connectionStatusLabel->setText(
                    QString("Connection error: %1").arg(message)
                    );
            });

    connect(client, &rpc_client::torrentsReceived,
            this,
            [this](const QVector<torrent> &torrents) {
                connectionStatusLabel->setStyleSheet("");
                connectionStatusLabel->setText(
                    QString("Connected: %1 · %2 torrent(s)")
                        .arg(client->getServer())
                        .arg(torrents.size())
                    );
            });

    connect(ui->tableView, &QTableView::customContextMenuRequested,
            this, &MainWindow::showTorrentContextMenu);

    loadServerCombo();

    connect(ui->comboServers,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) {
                saveSelectedServerFromCombo();
            });

    restoreTableViewState();

    setWindowIcon(QIcon(":/icons/planetary.png"));

    setupTrayIcon();
    client->init();
    applyUpdateInterval();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateTorrentList()
{
    client->getTorrentList();
}

void MainWindow::drawTorrentList()
{
}

void MainWindow::showAbout()
{
    DialogAbout *about = new DialogAbout(this);
    about->show();
}

void MainWindow::on_tableView_clicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid())
        return;

    const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return;

    const int torrentId = sourceIndex.data(Qt::UserRole).toInt();

    ui->fileTreeWidget->clear();
    ui->peerTableWidget->clearContents();
    ui->peerTableWidget->setRowCount(0);

    clearGeneralTab();
    clearTrackerTable();
    ui->fileTreeWidget->clear();
    ui->peerTableWidget->clearContents();
    ui->peerTableWidget->setRowCount(0);

    client->getTorrentDetails(torrentId);
}

int MainWindow::currentTorrentId() const
{
    const QModelIndex proxyIndex = ui->tableView->currentIndex();

    if (!proxyIndex.isValid())
        return -1;

    const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return -1;

    return sourceIndex.data(Qt::UserRole).toInt();
}

void MainWindow::on_actionDelete_Torrent_triggered()
{
    const QList<int> ids = selectedTorrentIds();
    const QStringList names = selectedTorrentNames();

    if (ids.isEmpty()) {
        QMessageBox::information(
            this,
            "Delete Torrent",
            "No torrent is selected."
            );
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Delete Torrent");
    msgBox.setIcon(QMessageBox::Warning);

    if (ids.size() == 1) {
        msgBox.setText("Delete the selected torrent?");
        msgBox.setInformativeText(
            QString(
                "Torrent:\n%1\n\n"
                "Choose whether to remove only the torrent from Transmission, "
                "or also delete the downloaded data."
                ).arg(names.value(0))
            );
    } else {
        QString preview = names.mid(0, 8).join("\n");

        if (names.size() > 8)
            preview += QString("\n… and %1 more").arg(names.size() - 8);

        msgBox.setText(
            QString("Delete %1 selected torrents?").arg(ids.size())
            );

        msgBox.setInformativeText(
            QString(
                "Torrents:\n%1\n\n"
                "Choose whether to remove only the torrents from Transmission, "
                "or also delete the downloaded data."
                ).arg(preview)
            );
    }

    QPushButton *noButton =
        msgBox.addButton("No", QMessageBox::RejectRole);

    QPushButton *torrentOnlyButton =
        msgBox.addButton("Yes: torrent only", QMessageBox::AcceptRole);

    QPushButton *torrentAndDataButton =
        msgBox.addButton("Yes, torrent and data", QMessageBox::DestructiveRole);

    msgBox.setDefaultButton(noButton);
    msgBox.exec();

    if (msgBox.clickedButton() == noButton)
        return;

    if (msgBox.clickedButton() == torrentOnlyButton) {
        client->removeTorrents(ids, false);
        return;
    }

    if (msgBox.clickedButton() == torrentAndDataButton) {
        client->removeTorrents(ids, true);
        return;
    }
}

void MainWindow::saveTableViewState()
{
    QSettings settings;

    settings.setValue(
        "ui/tableView/horizontalHeaderState",
        ui->tableView->horizontalHeader()->saveState()
        );

    settings.setValue(
        "ui/tableView/verticalHeaderState",
        ui->tableView->verticalHeader()->saveState()
        );

    settings.setValue(
        "ui/fileTreeWidget/headerState",
        ui->fileTreeWidget->header()->saveState()
        );

    settings.setValue(
        "ui/peerTableWidget/horizontalHeaderState",
        ui->peerTableWidget->horizontalHeader()->saveState()
        );
}

void MainWindow::restoreTableViewState()
{
    QSettings settings;

    const QByteArray horizontalState =
        settings.value("ui/tableView/horizontalHeaderState").toByteArray();

    if (!horizontalState.isEmpty()) {
        ui->tableView->horizontalHeader()->restoreState(horizontalState);
    }

    const QByteArray verticalState =
        settings.value("ui/tableView/verticalHeaderState").toByteArray();

    if (!verticalState.isEmpty()) {
        ui->tableView->verticalHeader()->restoreState(verticalState);
    }

    const QByteArray fileTreeHeaderState =
        settings.value("ui/fileTreeWidget/headerState").toByteArray();

    if (!fileTreeHeaderState.isEmpty()) {
        ui->fileTreeWidget->header()->restoreState(fileTreeHeaderState);
    }

    const QByteArray peerTableHeaderState =
        settings.value("ui/peerTableWidget/horizontalHeaderState").toByteArray();

    if (!peerTableHeaderState.isEmpty()) {
        ui->peerTableWidget->horizontalHeader()->restoreState(peerTableHeaderState);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveTableViewState();

    if (reallyQuit) {
        event->accept();
        return;
    }

    hide();
    event->ignore();

    /*
    if (trayIcon) {
        trayIcon->showMessage(
            "Planetary",
            "Planetary is still running in the menu bar.",
            QSystemTrayIcon::Information,
            2500
            );
    }
*/
}

void MainWindow::onServerSetupTriggered()
{
    ServerConfig sc(this);

    if (sc.exec() == QDialog::Accepted) {
        loadServerCombo();

        const int serverIndex =
            ui->comboServers->currentData().toInt();

        if (serverIndex >= 0) {
            client->setServerFromSettingsIndex(serverIndex);
            statusBar()->showMessage(client->getServer());
            client->getTorrentList();
        }
    }
}

void MainWindow::setTorrentStateFilter(TorrentSortProxyModel::StateFilter filter)
{
    proxy->setStateFilter(filter);

    QSignalBlocker blocker(ui->listWidget);

    switch (filter) {
    case TorrentSortProxyModel::StateFilter::All:
        ui->actionAll->setChecked(true);
        ui->listWidget->setCurrentRow(0);
        break;

    case TorrentSortProxyModel::StateFilter::Downloading:
        ui->actionDownloading->setChecked(true);
        ui->listWidget->setCurrentRow(1);
        break;

    case TorrentSortProxyModel::StateFilter::Completed:
        ui->actionCompleted->setChecked(true);
        ui->listWidget->setCurrentRow(2);
        break;

    case TorrentSortProxyModel::StateFilter::Active:
        ui->actionActive->setChecked(true);
        ui->listWidget->setCurrentRow(3);
        break;

    case TorrentSortProxyModel::StateFilter::Inactive:
        ui->actionInactive->setChecked(true);
        ui->listWidget->setCurrentRow(4);
        break;

    case TorrentSortProxyModel::StateFilter::Stopped:
        ui->actionStopped->setChecked(true);
        ui->listWidget->setCurrentRow(5);
        break;

    case TorrentSortProxyModel::StateFilter::Error:
        ui->actionError->setChecked(true);
        ui->listWidget->setCurrentRow(6);
        break;
    }
}

QTreeWidgetItem *MainWindow::findOrCreateTopLevelItem(const QString &name)
{
    for (int i = 0; i < ui->fileTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = ui->fileTreeWidget->topLevelItem(i);

        if (item->text(0) == name)
            return item;
    }

    auto *item = new QTreeWidgetItem(ui->fileTreeWidget);
    item->setText(0, name);
    item->setData(0, Qt::UserRole, "folder");

    return item;
}

QTreeWidgetItem *MainWindow::findOrCreateChild(QTreeWidgetItem *parent,
                                               const QString &name,
                                               bool isFolder)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);

        if (child->text(0) == name)
            return child;
    }

    auto *child = new QTreeWidgetItem(parent);
    child->setText(0, name);
    child->setData(0, Qt::UserRole, isFolder ? "folder" : "file");

    return child;
}

void MainWindow::populateFileTree(const QJsonArray &files)
{
    ui->fileTreeWidget->clear();

    for (const QJsonValue &fileValue : files) {
        const QJsonObject file = fileValue.toObject();

        const QString path = file.value("name").toString();
        const qint64 length =
            file.value("length").toVariant().toLongLong();
        const qint64 bytesCompleted =
            file.value("bytesCompleted").toVariant().toLongLong();

        const QStringList parts = path.split('/', Qt::SkipEmptyParts);

        if (parts.isEmpty())
            continue;

        QTreeWidgetItem *current = findOrCreateTopLevelItem(parts.first());

        for (int i = 1; i < parts.size(); ++i) {
            const bool isLast = (i == parts.size() - 1);
            current = findOrCreateChild(current, parts.at(i), !isLast);
        }

        const double percentDone =
            length > 0
                ? (static_cast<double>(bytesCompleted) / static_cast<double>(length)) * 100.0
                : 0.0;

        current->setText(FileSizeColumn, QLocale().formattedDataSize(
                                             length,
                                             1,
                                             QLocale::DataSizeIecFormat
                                             ));

        current->setText(FileDoneColumn, QLocale().formattedDataSize(
                                             bytesCompleted,
                                             1,
                                             QLocale::DataSizeIecFormat
                                             ));

        current->setText(FilePercentColumn, QString("%1%").arg(percentDone, 0, 'f', 1));

        current->setData(FileSizeColumn, Qt::UserRole, length);
        current->setData(FileDoneColumn, Qt::UserRole, bytesCompleted);
        current->setData(FilePercentColumn, Qt::UserRole, percentDone);
    }

    ui->fileTreeWidget->expandToDepth(0);
    ui->fileTreeWidget->resizeColumnToContents(0);
}

void MainWindow::populatePeerTable(const QJsonArray &peers)
{
    ui->peerTableWidget->setSortingEnabled(false);
    ui->peerTableWidget->clearContents();
    ui->peerTableWidget->setRowCount(peers.size());

    int row = 0;

    for (const QJsonValue &peerValue : peers) {
        const QJsonObject peer = peerValue.toObject();

        const QString address = peer.value("address").toString();
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
            new QTableWidgetItem(isEncrypted ? "Yes" : "No");
        encryptedItem->setData(Qt::UserRole, isEncrypted);

        auto *incomingItem =
            new QTableWidgetItem(isIncoming ? "Yes" : "No");
        incomingItem->setData(Qt::UserRole, isIncoming);

        ui->peerTableWidget->setItem(row, 0, addressItem);
        ui->peerTableWidget->setItem(row, 1, portItem);
        ui->peerTableWidget->setItem(row, 2, clientItem);
        ui->peerTableWidget->setItem(row, 3, progressItem);
        ui->peerTableWidget->setItem(row, 4, downloadItem);
        ui->peerTableWidget->setItem(row, 5, uploadItem);
        ui->peerTableWidget->setItem(row, 6, encryptedItem);
        ui->peerTableWidget->setItem(row, 7, incomingItem);

        ++row;
    }

    ui->peerTableWidget->setSortingEnabled(true);
}

void MainWindow::showTorrentContextMenu(const QPoint &pos)
{
    const QModelIndex index = ui->tableView->indexAt(pos);

    if (!index.isValid())
        return;

    QItemSelectionModel *selection = ui->tableView->selectionModel();

    if (!selection->isSelected(index)) {
        selection->select(
            index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
            );

        ui->tableView->setCurrentIndex(index);
    }

    QMenu menu(this);

    menu.addAction(ui->actionStart_Torrent);
    menu.addAction(ui->actionStop_Torrent);
    menu.addSeparator();
    menu.addAction(ui->actionVerify_Torrent);
    menu.addAction(ui->actionReannounce);
    menu.addSeparator();
    menu.addAction(ui->actionDelete_Torrent);

    menu.exec(ui->tableView->viewport()->mapToGlobal(pos));
}

void MainWindow::startSelectedTorrent()
{
    const QList<int> ids = selectedTorrentIds();

    if (ids.isEmpty()) {
        statusBar()->showMessage("No torrent selected.", 3000);
        return;
    }

    client->startTorrents(ids);

    statusBar()->showMessage(
        QString("Starting %1 torrent(s)...").arg(ids.size()),
        3000
        );
}

void MainWindow::stopSelectedTorrent()
{
    const QList<int> ids = selectedTorrentIds();

    if (ids.isEmpty()) {
        statusBar()->showMessage("No torrent selected.", 3000);
        return;
    }

    client->stopTorrents(ids);

    statusBar()->showMessage(
        QString("Stopping %1 torrent(s)...").arg(ids.size()),
        3000
        );
}

void MainWindow::addTorrentFromFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Add Torrent File",
        QString(),
        "Torrent Files (*.torrent);;All Files (*)"
        );

    if (filePath.isEmpty())
        return;

    QSettings settings;

    const bool deleteTorrentOnAdd =
        settings.value("app/deleteTorrentFileOnSuccessfulAdd", false).toBool();

    client->addTorrentFromFile(filePath, deleteTorrentOnAdd);

    statusBar()->showMessage("Adding torrent...", 3000);
}

void MainWindow::addTorrentFromMagnet()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Add Torrent from Magnet Link");
    dialog.resize(720, 120);

    auto *layout = new QVBoxLayout(&dialog);

    auto *formLayout = new QFormLayout();
    auto *magnetEdit = new QLineEdit(&dialog);

    magnetEdit->setPlaceholderText("magnet:?xt=urn:btih:...");
    magnetEdit->setMinimumWidth(640);
    magnetEdit->setClearButtonEnabled(true);

    formLayout->addRow("Magnet link:", magnetEdit);
    layout->addLayout(formLayout);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog
        );

    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);

    connect(buttons, &QDialogButtonBox::rejected,
            &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString magnetLink = magnetEdit->text().trimmed();

    if (magnetLink.isEmpty()) {
        QMessageBox::information(
            this,
            "Add Torrent from Magnet Link",
            "No magnet link was entered."
            );
        return;
    }

    if (!magnetLink.startsWith("magnet:", Qt::CaseInsensitive)) {
        QMessageBox::warning(
            this,
            "Add Torrent from Magnet Link",
            "That does not look like a magnet link."
            );
        return;
    }

    client->addTorrentFromMagnet(magnetLink);

    statusBar()->showMessage("Adding torrent...", 3000);

    //QTimer::singleShot(750, client, &rpc_client::getTorrentList);
}

void MainWindow::on_actionStart_Torrent_triggered()
{
    startSelectedTorrent();
}

void MainWindow::on_actionStop_Torrent_triggered()
{
    stopSelectedTorrent();
}

void MainWindow::on_action_Open_Torrent_triggered()
{
    addTorrentFromFile();
}

QString MainWindow::currentTorrentName() const
{
    const QModelIndex proxyIndex = ui->tableView->currentIndex();

    if (!proxyIndex.isValid())
        return {};

    const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return {};

    return sourceIndex
        .siblingAtColumn(TorrentModel::NameColumn)
        .data(Qt::DisplayRole)
        .toString();
}

void MainWindow::on_actionAdd_Torrent_from_Magnet_Link_triggered()
{
    addTorrentFromMagnet();
}

void MainWindow::reannounceSelectedTorrent()
{
    const QList<int> ids = selectedTorrentIds();

    if (ids.isEmpty()) {
        statusBar()->showMessage("No torrent selected.", 3000);
        return;
    }

    client->reannounceTorrents(ids);

    statusBar()->showMessage(
        QString("Reannouncing %1 torrent(s)...").arg(ids.size()),
        3000
        );
}

void MainWindow::verifySelectedTorrent()
{
    const QList<int> ids = selectedTorrentIds();

    if (ids.isEmpty()) {
        statusBar()->showMessage("No torrent selected.", 3000);
        return;
    }

    client->verifyTorrents(ids);

    statusBar()->showMessage(
        QString("Verifying %1 torrent(s)...").arg(ids.size()),
        3000
        );
}

void MainWindow::on_actionReannounce_triggered()
{
    reannounceSelectedTorrent();
}

void MainWindow::on_actionAbout_triggered()
{
    DialogAbout *about = new DialogAbout(this);
    about->show();
}

void MainWindow::on_actionVerify_Torrent_triggered()
{
    verifySelectedTorrent();
}

void MainWindow::loadServerCombo()
{
    QSettings settings;

    const int previouslySelectedServerIndex =
        ui->comboServers->currentData().toInt();

    const bool hadPreviousSelection =
        ui->comboServers->currentIndex() >= 0 &&
        previouslySelectedServerIndex >= 0;

    const int defaultIndex =
        settings.value("servers/defaultIndex", -1).toInt();

    const int savedCurrentIndex =
        settings.value("servers/currentIndex", defaultIndex).toInt();

    QSignalBlocker blocker(ui->comboServers);

    ui->comboServers->clear();

    const int count = settings.beginReadArray("servers");

    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);

        QString name = settings.value("name").toString().trimmed();
        const QString rpcUrl = settings.value("rpcUrl").toString().trimmed();

        if (name.isEmpty()) {
            name = rpcUrl.isEmpty()
            ? QStringLiteral("(unnamed server)")
            : rpcUrl;
        }

        if (i == defaultIndex)
            name += QStringLiteral(" (default)");

        ui->comboServers->addItem(name, i);
    }

    settings.endArray();

    if (ui->comboServers->count() == 0) {
        ui->comboServers->addItem("No servers configured", -1);
        ui->comboServers->setEnabled(false);
        return;
    }

    ui->comboServers->setEnabled(true);

    int comboIndex = -1;

    // First preference: keep whatever the combo was already showing.
    if (hadPreviousSelection)
        comboIndex = ui->comboServers->findData(previouslySelectedServerIndex);

    // First-load fallback: saved current server.
    if (comboIndex < 0)
        comboIndex = ui->comboServers->findData(savedCurrentIndex);

    // Next fallback: default server.
    if (comboIndex < 0)
        comboIndex = ui->comboServers->findData(defaultIndex);

    // Last fallback: first server.
    if (comboIndex < 0)
        comboIndex = 0;

    ui->comboServers->setCurrentIndex(comboIndex);
}

void MainWindow::saveSelectedServerFromCombo()
{
    const int serverIndex =
        ui->comboServers->currentData().toInt();

    if (serverIndex < 0)
        return;

    QSettings settings;
    settings.setValue("servers/currentIndex", serverIndex);
    settings.sync();

    if (!client->setServerFromSettingsIndex(serverIndex)) {
        statusBar()->showMessage("Could not switch server.", 5000);
        return;
    }

    ui->fileTreeWidget->clear();
    ui->peerTableWidget->clearContents();
    ui->peerTableWidget->setRowCount(0);
    updateTorrentActionState();

    statusBar()->showMessage(
        QString("Selected server: %1").arg(client->getServer()),
        3000
        );

    client->getTorrentList();
}

void MainWindow::updateTorrentActionState()
{
    const bool hasSelection =
        ui->tableView->selectionModel() &&
        !ui->tableView->selectionModel()->selectedRows().isEmpty();

    ui->actionStart_Torrent->setEnabled(hasSelection);
    ui->actionStop_Torrent->setEnabled(hasSelection);
    ui->actionDelete_Torrent->setEnabled(hasSelection);
    ui->actionVerify_Torrent->setEnabled(hasSelection);
    ui->actionReannounce->setEnabled(hasSelection);
}
void MainWindow::setupTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "System tray is not available.";
        return;
    }

    trayMenu = new QMenu(this);

    QAction *showAction = trayMenu->addAction("Show Planetary");
    connect(showAction, &QAction::triggered,
            this, &MainWindow::showMainWindow);

    trayMenu->addSeparator();

    QAction *quitAction = trayMenu->addAction("Quit");
    connect(quitAction, &QAction::triggered,
            this, &MainWindow::quitApplication);

    trayIcon = new QSystemTrayIcon(this);

    // Uses the app/window icon. If this is blank, use QIcon(":/icons/planetary.png")
    trayIcon->setIcon(windowIcon());
    trayIcon->setToolTip("Planetary");
    trayIcon->setContextMenu(trayMenu);

    connect(trayIcon, &QSystemTrayIcon::activated,
            this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick) {
                    showMainWindow();
                }
            });

    trayIcon->show();
}

void MainWindow::showMainWindow()
{
    show();
    setWindowState(windowState() & ~Qt::WindowMinimized);
    raise();
    activateWindow();
}

void MainWindow::quitApplication()
{
    reallyQuit = true;

    if (trayIcon)
        trayIcon->hide();

    close();
    qApp->quit();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            QTimer::singleShot(0, this, [this]() {
                hide();
            });
        }
    }
}

int MainWindow::updateIntervalMs() const
{
    QSettings settings;

    const int seconds =
        settings.value(UpdateIntervalKey, DefaultUpdateIntervalSeconds).toInt();

    const int boundedSeconds =
        qBound(MinimumUpdateIntervalSeconds,
               seconds,
               MaximumUpdateIntervalSeconds);

    return boundedSeconds * 1000;
}

void MainWindow::applyUpdateInterval()
{
    timer->setInterval(updateIntervalMs());

    if (!timer->isActive())
        timer->start();

    statusBar()->showMessage(
        QString("Update interval: %1 seconds").arg(timer->interval() / 1000),
        3000
        );
}
void MainWindow::on_actionSettings_triggered()
{
    AppSettings dialog(this);

    if (dialog.exec() == QDialog::Accepted) {
        applyUpdateInterval();
    }
}

QList<int> MainWindow::selectedTorrentIds() const
{
    QList<int> ids;

    const QItemSelectionModel *selection = ui->tableView->selectionModel();

    if (!selection)
        return ids;

    const QModelIndexList proxyRows = selection->selectedRows();

    for (const QModelIndex &proxyIndex : proxyRows) {
        const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);

        if (!sourceIndex.isValid())
            continue;

        const int id = sourceIndex.data(Qt::UserRole).toInt();

        if (id >= 0)
            ids.append(id);
    }

    return ids;
}

QStringList MainWindow::selectedTorrentNames() const
{
    QStringList names;

    const QItemSelectionModel *selection = ui->tableView->selectionModel();

    if (!selection)
        return names;

    const QModelIndexList proxyRows = selection->selectedRows();

    for (const QModelIndex &proxyIndex : proxyRows) {
        const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);

        if (!sourceIndex.isValid())
            continue;

        const QString name = sourceIndex
                                 .siblingAtColumn(TorrentModel::NameColumn)
                                 .data(Qt::DisplayRole)
                                 .toString();

        if (!name.isEmpty())
            names.append(name);
    }

    return names;
}

void MainWindow::setupConnectionStatusIndicator()
{
    connectionStatusLabel = new QLabel(this);
    connectionStatusLabel->setText("Not connected");

    connectionStatusLabel->setMinimumWidth(260);
    connectionStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    ui->statusbar->addPermanentWidget(connectionStatusLabel);

    connect(client, &rpc_client::updateStarted,
            this,
            [this]() {
                connectionStatusLabel->setText("Updating...");
            });

    connect(client, &rpc_client::updateFailed,
            this,
            [this](const QString &message) {
                QString displayMessage = message;

                if (message.contains("timed out", Qt::CaseInsensitive)) {
                    displayMessage = "Timed out contacting Transmission";
                } else if (message.contains("connection refused", Qt::CaseInsensitive)) {
                    displayMessage = "Connection refused by Transmission";
                } else if (message.contains("host not found", Qt::CaseInsensitive)) {
                    displayMessage = "Host not found";
                } else if (message.contains("network", Qt::CaseInsensitive)) {
                    displayMessage = message;
                }

                connectionStatusLabel->setText(
                    QString("Connection error: %1").arg(displayMessage)
                    );
            });

    connect(client, &rpc_client::torrentsReceived,
            this,
            [this](const QVector<torrent> &torrents) {
                connectionStatusLabel->setText(
                    QString("Connected: %1 · %2 torrent(s)")
                        .arg(client->getServer())
                        .arg(torrents.size())
                    );
            });

    connect(client, &rpc_client::serverChanged,
            this,
            [this]() {
                connectionStatusLabel->setText(
                    QString("Server changed: %1").arg(client->getServer())
                    );
            });
}

void MainWindow::populateGeneralTab(const QJsonObject &details)
{
    const QString name =
        details.value("name").toString();

    const QString comment =
        details.value("comment").toString();

    const QString creator =
        details.value("creator").toString();

    const QString downloadDir =
        details.value("downloadDir").toString();

    const QString hashString =
        details.value("hashString").toString();

    const QString magnetLink =
        details.value("magnetLink").toString();

    const qint64 totalSize =
        details.value("totalSize").toVariant().toLongLong();

    const qint64 dateCreated =
        details.value("dateCreated").toVariant().toLongLong();

    ui->labelGeneralName->setText(name);
    ui->labelGeneralCreator->setText(creator);
    ui->labelGeneralDownloadDir->setText(downloadDir);
    ui->labelGeneralHash->setText(hashString);
    ui->lineGeneralMagnet->setText(magnetLink);

    ui->labelGeneralComment->setText(
        comment.isEmpty() ? QStringLiteral("None") : comment
        );

    ui->labelGeneralTotalSize->setText(
        QLocale().formattedDataSize(
            totalSize,
            1,
            QLocale::DataSizeIecFormat
            )
        );

    if (dateCreated > 0) {
        const QDateTime created =
            QDateTime::fromSecsSinceEpoch(dateCreated);

        ui->labelGeneralCreated->setText(
            QLocale().toString(created, QLocale::ShortFormat)
            );
    } else {
        ui->labelGeneralCreated->setText("Unknown");
    }
}

void MainWindow::clearGeneralTab()
{
    ui->labelGeneralName->clear();
    ui->labelGeneralTotalSize->clear();
    ui->labelGeneralCreator->clear();
    ui->labelGeneralCreated->clear();
    ui->labelGeneralDownloadDir->clear();
    ui->labelGeneralHash->clear();
    ui->lineGeneralMagnet->clear();
}

void MainWindow::clearTrackerTable()
{
    ui->trackerTableWidget->clearContents();
    ui->trackerTableWidget->setRowCount(0);
}

void MainWindow::populateTrackerTable(const QJsonObject &details)
{
    const QJsonArray trackerStats =
        details.value("trackerStats").toArray();

    ui->trackerTableWidget->setSortingEnabled(false);
    ui->trackerTableWidget->clearContents();
    ui->trackerTableWidget->setRowCount(trackerStats.size());

    int row = 0;

    for (const QJsonValue &value : trackerStats) {
        const QJsonObject tracker = value.toObject();

        const QString host =
            tracker.value("host").toString();

        const QString announce =
            tracker.value("announce").toString();

        const int seeders =
            tracker.value("seederCount").toInt(-1);

        const int leechers =
            tracker.value("leecherCount").toInt(-1);

        const QString lastAnnounceTime =
            tracker.value("lastAnnounceTime").toVariant().toLongLong() > 0
                ? QLocale().toString(
                      QDateTime::fromSecsSinceEpoch(
                          tracker.value("lastAnnounceTime").toVariant().toLongLong()
                          ),
                      QLocale::ShortFormat
                      )
                : QStringLiteral("Never");

        const QString lastAnnounceResult =
            tracker.value("lastAnnounceResult").toString();

        auto *hostItem = new QTableWidgetItem(host);
        auto *announceItem = new QTableWidgetItem(announce);

        auto *seedersItem = new QTableWidgetItem(
            seeders >= 0 ? QString::number(seeders) : QStringLiteral("Unknown")
            );

        auto *leechersItem = new QTableWidgetItem(
            leechers >= 0 ? QString::number(leechers) : QStringLiteral("Unknown")
            );

        auto *lastAnnounceItem = new QTableWidgetItem(lastAnnounceTime);
        auto *resultItem = new QTableWidgetItem(lastAnnounceResult);

        seedersItem->setData(Qt::UserRole, seeders);
        leechersItem->setData(Qt::UserRole, leechers);

        ui->trackerTableWidget->setItem(row, 0, hostItem);
        ui->trackerTableWidget->setItem(row, 1, announceItem);
        ui->trackerTableWidget->setItem(row, 2, seedersItem);
        ui->trackerTableWidget->setItem(row, 3, leechersItem);
        ui->trackerTableWidget->setItem(row, 4, lastAnnounceItem);
        ui->trackerTableWidget->setItem(row, 5, resultItem);

        ++row;
    }

    ui->trackerTableWidget->setSortingEnabled(true);
}