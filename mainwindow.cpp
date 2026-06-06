#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "rpc_client.h"
#include "dialogabout.h"
#include "serverconfig.h"
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
#include "torrentsortproxymodel.h"
#include "percentfilldelegate.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    timer = new QTimer(this);
    client = new rpc_client(this);
    proxy = new TorrentSortProxyModel(this);
    proxy->setSourceModel(client);

    this->aboutAction = new QAction(0);
    this->aboutAction->setMenuRole(QAction::AboutRole);

    ui->setupUi(this);

    auto *stateGroup = new QActionGroup(this);
    stateGroup->setExclusive(true);

    MainWindow::setWindowTitle(QCoreApplication::applicationName());

    ui->tableView->setModel(proxy);

    updateTorrentActionState();

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

    /*
    ui->actionStart_Torrent->setEnabled(false);
    ui->actionStop_Torrent->setEnabled(false);
    ui->actionDelete_Torrent->setEnabled(false);
    ui->actionVerify_Torrent->setEnabled(false);
    ui->actionReannounce->setEnabled(false);
    */

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

    ui->tableView->hideColumn(rpc_client::IdColumn);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->sortByColumn(rpc_client::NameColumn, Qt::AscendingOrder);
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableView->setItemDelegateForColumn(
        rpc_client::PercentDoneColumn,
        new PercentFillDelegate(
            rpc_client::PercentDoneColumn,
            Qt::UserRole + 1,
            ui->tableView
            )
        );

    ui->fileTreeWidget->setItemDelegateForColumn(
        FilePercentColumn,
        new PercentFillDelegate(FilePercentColumn, Qt::UserRole, ui->fileTreeWidget)
        );

    ui->statusbar->showMessage(client->getServer());

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
    connect(client, &rpc_client::updateStarted, this, [this]() {
        ui->statusbar->showMessage(client->getServer() + " (updating)");
    });

    connect(client, &rpc_client::updateFinished, this, [this]() {
        ui->statusbar->showMessage(client->getServer());
    });

    /*
    connect(ui->tableView->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            [this]() {
                const bool hasSelection = ui->tableView->currentIndex().isValid();

                ui->actionStart_Torrent->setEnabled(hasSelection);
                ui->actionStop_Torrent->setEnabled(hasSelection);
                ui->actionReannounce->setEnabled(hasSelection);
                ui->actionVerify_Torrent->setEnabled(hasSelection);
                ui->actionDelete_Torrent->setEnabled(hasSelection);
            });
*/

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

    client->init();
    timer->start(10000);
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

    const int sourceRow = sourceIndex.row();
    const torrent t = client->getTorrent(sourceRow);

    populateFileTree(t.getFiles());
    populatePeerTable(t.getPeers());
}

/*
int MainWindow::currentSourceRow() const
{
    const QModelIndex proxyIndex = ui->tableView->currentIndex();

    if (!proxyIndex.isValid())
        return -1;

    const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return -1;

    return sourceIndex.row();
}
*/
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
    const int torrentId = currentTorrentId();
    const QString torrentName = currentTorrentName();

    if (torrentId < 0) {
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

    msgBox.setText("Delete the selected torrent?");

    msgBox.setInformativeText(
        QString(
            "Torrent:\n%1\n\n"
            "Choose whether to remove only the torrent from Transmission, "
            "or also delete the downloaded data."
            ).arg(torrentName)
        );

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
        client->removeTorrent(torrentId, false);
        QTimer::singleShot(500, client, &rpc_client::getTorrentList);
        return;
    }

    if (msgBox.clickedButton() == torrentAndDataButton) {
        client->removeTorrent(torrentId, true);
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

    QMainWindow::closeEvent(event);
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

    ui->tableView->selectionModel()->select(
        index,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
        );

    ui->tableView->setCurrentIndex(index);

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
    const int torrentId = currentTorrentId();

    if (torrentId < 0) {
        statusBar()->showMessage("No torrent selected.", 3000);
        return;
    }

    client->startTorrent(torrentId);

    statusBar()->showMessage("Starting torrent...", 3000);

    // Simple refresh approach, same spirit as your delete action.
    //QTimer::singleShot(500, client, &rpc_client::getTorrentList);
}

void MainWindow::stopSelectedTorrent()
{
    const int torrentId = currentTorrentId();

    if (torrentId < 0) {
        statusBar()->showMessage("No torrent selected.", 3000);
        return;
    }

    client->stopTorrent(torrentId);

    statusBar()->showMessage("Stopping torrent...", 3000);

    //QTimer::singleShot(500, client, &rpc_client::getTorrentList);
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

    client->addTorrentFromFile(filePath);

    statusBar()->showMessage("Adding torrent...", 3000);

    //QTimer::singleShot(750, client, &rpc_client::getTorrentList);
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
        .siblingAtColumn(rpc_client::NameColumn)
        .data(Qt::DisplayRole)
        .toString();
}

void MainWindow::on_actionAdd_Torrent_from_Magnet_Link_triggered()
{
    addTorrentFromMagnet();
}

void MainWindow::reannounceSelectedTorrent()
{
    const int torrentId = currentTorrentId();

    if (torrentId < 0) {
        statusBar()->showMessage("No torrent selected.", 3000);
        return;
    }

    client->reannounceTorrent(torrentId);
    statusBar()->showMessage("Reannouncing torrent...", 3000);

    //QTimer::singleShot(750, client, &rpc_client::getTorrentList);
}

void MainWindow::verifySelectedTorrent()
{
    const int torrentId = currentTorrentId();

    if (torrentId < 0) {
        statusBar()->showMessage("No torrent selected.", 3000);
        return;
    }

    client->verifyTorrent(torrentId);

    statusBar()->showMessage("Verifying torrent...", 3000);
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
        ui->tableView->currentIndex().isValid();

    ui->actionStart_Torrent->setEnabled(hasSelection);
    ui->actionStop_Torrent->setEnabled(hasSelection);
    ui->actionDelete_Torrent->setEnabled(hasSelection);
    ui->actionVerify_Torrent->setEnabled(hasSelection);
    ui->actionReannounce->setEnabled(hasSelection);
}