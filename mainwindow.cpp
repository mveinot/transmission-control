#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "piecemapwidget.h"
#include <QActionGroup>
#include <QAbstractItemView>
#include <QApplication>
#include <QAction>
#include <QCloseEvent>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>
#include <algorithm>

#include "rpc_client.h"
#include "dialogabout.h"
#include "serverconfig.h"
#include "appsettings.h"
#include "torrentsortproxymodel.h"
#include "percentfilldelegate.h"
#include "sessionsettingsdialog.h"
#include "settingskeys.h"
#include "torrentaddcontroller.h"
#include "watchfoldermanager.h"
#include "updatechecker.h"
#include "settingsimportexport.h"
#include "foldermapping.h"
#include "torrentpropertiesdialog.h"
#include "version.h"

namespace {
constexpr int DefaultUpdateIntervalSeconds = 10;
constexpr int MinimumUpdateIntervalSeconds = 1;
constexpr int MaximumUpdateIntervalSeconds = 3600;
constexpr int FilterTypeRole = Qt::UserRole;
constexpr int FilterValueRole = Qt::UserRole + 1;
constexpr int FilterTypeStatus = 0;
constexpr int FilterTypeTracker = 1;
constexpr int TrackerAnnounceRole = Qt::UserRole;
constexpr int TrackerIdRole = Qt::UserRole + 1;
}

// accept various representations of true and false
bool jsonValueToBool(const QJsonValue &value, bool defaultValue = false)
{
    if (value.isBool())
        return value.toBool();

    if (value.isDouble())
        return value.toInt(defaultValue ? 1 : 0) != 0;

    if (value.isString()) {
        const QString text = value.toString().trimmed().toLower();

        if (text == QStringLiteral("true")
            || text == QStringLiteral("yes")
            || text == QStringLiteral("1")) {
            return true;
        }

        if (text == QStringLiteral("false")
            || text == QStringLiteral("no")
            || text == QStringLiteral("0")) {
            return false;
        }
    }

    return defaultValue;
}


// determine if a string looks like a URL
static bool looksLikeUrl(const QString &text)
{
    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty())
        return false;

    const QUrl url = QUrl::fromUserInput(trimmed);

    return url.isValid() &&
           !url.scheme().isEmpty() &&
           !url.host().isEmpty();
}

QString displayVersion(QString version)
{
    version = version.trimmed();

    if (version.isEmpty())
        return QStringLiteral("Unknown");

    if (!version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        version.prepend(QLatin1Char('v'));

    return version;
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

    if (pieceMapWidget)
        pieceMapWidget->clear();

    if (pieceMapGroup)
        pieceMapGroup->setTitle(tr("Pieces"));

    clearTorrentDetailsTab();
    currentTorrentDetailsCache = QJsonObject();

    currentTorrentDownloadDir.clear();
    currentTorrentFilePaths.clear();
    currentDetailsTorrentId = -1;
    currentTorrentHashString.clear();
    currentTorrentMagnetLink.clear();
}

void MainWindow::clearTrackerTable()
{
    ui->trackerTableWidget->clearContents();
    ui->trackerTableWidget->setRowCount(0);
}

static QString priorityToString(int priority)
{
    switch (priority) {
    case 1:
        return QCoreApplication::translate("MainWindow", "High");
    case -1:
        return QCoreApplication::translate("MainWindow", "Low");
    case 0:
    default:
        return QCoreApplication::translate("MainWindow", "Normal");
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    timer = new QTimer(this);
    client = new rpc_client(this);
    torrentModel = new TorrentModel(this);

    // used as a intermediate model for sorting/filtering/etc
    proxy = new TorrentSortProxyModel(this);
    proxy->setSourceModel(torrentModel);

    // geo ip lookup
    geoIpService = new GeoIpService(this);

    // TODO: needs fixing for non-macos builds
    const QString geoIpPath =
        QCoreApplication::applicationDirPath()
        + "/../Resources/geoip/country.mmdb";

    if (!geoIpService->loadDatabase(geoIpPath)) {
        qWarning() << "GeoIP database could not be loaded; using dummy lookup";
    }

    torrentAddController = new TorrentAddController(client, this, this);

    // set up the main UI
    ui->setupUi(this);
    setupPieceMapWidget();
    setupTorrentDetailsTab();
    MainWindow::setWindowTitle(QCoreApplication::applicationName());
    setWindowIcon(QIcon(":/icons/planetary-512px.png"));

    // Initialization methods
    loadServerCombo();
    setupUpdateChecker();
    maybeCheckForUpdates();
    setupConnectionStatusIndicator();
    setupWatchFolderManager();
    loadWatchFolderSettings();
    updateTorrentActionState();
    setupTrayIcon();

    // UI setup
    this->mainMenu = new QMenu(0);
    this->menuBar()->addMenu(this->mainMenu);
    this->aboutAction = new QAction(0);
    this->aboutAction->setMenuRole(QAction::AboutRole);
    this->mainMenu->addAction(this->aboutAction);
    this->setMenuBar(this->menuBar());

    ui->fileTreeWidget->setColumnCount(FileColumnCount);
    ui->fileTreeWidget->setHeaderLabels({
        tr("Name"),
        tr("Priority"),
        tr("Size"),
        tr("Done"),
        tr("Completed")
    });
    ui->fileTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->fileTreeWidget->setAlternatingRowColors(true);
    ui->fileTreeWidget->setRootIsDecorated(true);
    ui->fileTreeWidget->setItemDelegateForColumn(
        FilePercentColumn,
        new PercentFillDelegate(FilePercentColumn, Qt::UserRole, ui->fileTreeWidget)
        );

    ui->statusbar->showMessage(client->getServer());

    ui->actionAll->setCheckable(true);
    ui->actionDownloading->setCheckable(true);
    ui->actionCompleted->setCheckable(true);
    ui->actionActive->setCheckable(true);
    ui->actionInactive->setCheckable(true);
    ui->actionStopped->setCheckable(true);
    ui->actionError->setCheckable(true);

    auto *stateGroup = new QActionGroup(this);
    stateGroup->setExclusive(true);
    stateGroup->addAction(ui->actionAll);
    stateGroup->addAction(ui->actionDownloading);
    stateGroup->addAction(ui->actionCompleted);
    stateGroup->addAction(ui->actionActive);
    stateGroup->addAction(ui->actionInactive);
    stateGroup->addAction(ui->actionStopped);
    stateGroup->addAction(ui->actionError);
    ui->actionAll->setChecked(true);

    ui->peerTableWidget->setColumnCount(9);
    ui->peerTableWidget->setHorizontalHeaderLabels({
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
    ui->peerTableWidget->setAlternatingRowColors(true);
    ui->peerTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->peerTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->peerTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->peerTableWidget->setSortingEnabled(true);

    ui->lineGeneralMagnet->setReadOnly(true);
    ui->lineGeneralMagnet->setFrame(false);
    ui->lineGeneralMagnet->setCursorPosition(0);
    ui->lineGeneralMagnet->setTextMargins(0, 0, 0, 0);
    ui->lineGeneralMagnet->setContextMenuPolicy(Qt::DefaultContextMenu);

    ui->lineGeneralMagnet->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0px;"
        "}"
        ));

    ui->tableView->setModel(proxy);
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

    ui->trackerTableWidget->setAlternatingRowColors(true);
    ui->trackerTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->trackerTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->trackerTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->trackerTableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->trackerTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->trackerTableWidget->setColumnCount(6);
    ui->trackerTableWidget->setHorizontalHeaderLabels({
        tr("Host"),
        tr("Announce"),
        tr("Seeds"),
        tr("Leechers"),
        tr("Last Announce"),
        tr("Result")
    });

    // signal connections
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

    connect(torrentAddController, &TorrentAddController::addStarted,
            this, [this]() {
                statusBar()->showMessage(
                    tr("Adding torrent..."),
                    3000
                    );
            });

    connect(torrentAddController, &TorrentAddController::addFailed,
            this, [this](const QString &message) {
                statusBar()->showMessage(message, 5000);
            });

    connect(torrentAddController, &TorrentAddController::addCancelled,
            this, [this]() {
                statusBar()->showMessage(tr("Torrent add cancelled."), 3000);
            });


    connect(ui->actionCheckForUpdates, &QAction::triggered,
            this, [this]() {
                if (updateChecker)
                    updateChecker->checkForUpdates(true);
            });

    connect(ui->fileTreeWidget, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::showFileContextMenu);

    connect(ui->trackerTableWidget, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::showTrackerContextMenu);

    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, [this](int) {
                refreshCurrentTorrentLiveDetailsIfNeeded();
            });

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

    connect(ui->actionExport_Settings, &QAction::triggered,
            this, &MainWindow::exportSettings);

    connect(ui->actionImport_Settings, &QAction::triggered,
            this, &MainWindow::importSettings);

    connect(ui->editTorrentFilter, &QLineEdit::textChanged,
            proxy, &TorrentSortProxyModel::setSearchText);

    connect(client, &rpc_client::sessionSettingsReceived,
            this, &MainWindow::handleSessionSettingsReceived);

    connect(ui->actionTransmission_Settings, &QAction::triggered,
            this, &MainWindow::showSessionSettings);

    connect(ui->actionServer_Setup, &QAction::triggered, this, &MainWindow::onServerSetupTriggered);

    connect(ui->listWidget, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current, QListWidgetItem *) {
                if (!current)
                    return;

                const int filterType = current->data(FilterTypeRole).toInt();

                if (filterType == FilterTypeStatus) {
                    const auto stateFilter =
                        static_cast<TorrentSortProxyModel::StateFilter>(
                            current->data(FilterValueRole).toInt()
                            );

                    proxy->setStateFilter(stateFilter);
                    proxy->setTrackerFilter(QString());
                    return;
                }

                if (filterType == FilterTypeTracker) {
                    proxy->setStateFilter(
                        TorrentSortProxyModel::StateFilter::All
                        );

                    proxy->setTrackerFilter(
                        current->data(FilterValueRole).toString()
                        );
                    return;
                }
            });

    connect(timer, &QTimer::timeout, this, &MainWindow::updateTorrentList);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    connect(client, &rpc_client::torrentDetailsReceived,
            this,
            [this](int torrentId, const QJsonObject &details) {
                if (torrentId != currentTorrentId())
                    return;

                currentTorrentDetailsCache = details;
                populateGeneralTab(details);
                updateTorrentDetailsTab(currentTorrentDetailsCache);
                populateTrackerTable(details);
                populateFileTree(
                    details.value("files").toArray(),
                    details.value("wanted").toArray(),
                    details.value("priorities").toArray()
                    );
                populatePeerTable(details.value("peers").toArray());
            });

    connect(client, &rpc_client::torrentPiecesReceived,
            this,
            [this](int torrentId, const QJsonObject &details) {
                if (torrentId != currentDetailsTorrentId)
                    return;

                for (auto it = details.constBegin(); it != details.constEnd(); ++it)
                    currentTorrentDetailsCache.insert(it.key(), it.value());

                updatePieceMap(currentTorrentDetailsCache);
                updateTorrentDetailsTab(currentTorrentDetailsCache);
            });

    connect(client, &rpc_client::updateStarted,
            this,
            [this]() {
                connectionStatusLabel->setStyleSheet("");
                connectionStatusLabel->setText(tr("Updating..."));
            });

    connect(client, &rpc_client::updateFailed,
            this,
            [this](const QString &message) {
                connectionStatusLabel->setStyleSheet("color: #ff6b6b;");
                connectionStatusLabel->setText(
                    tr("Connection error: %1").arg(message)
                    );
            });

    // Data path: required for the table to show anything
    connect(client, &rpc_client::torrentsReceived,
                torrentModel, &TorrentModel::applyUpdate);

    // Side effects: notifications/status only
    connect(client, &rpc_client::torrentsReceived,
            this, &MainWindow::handleTorrentsReceived);

    connect(ui->tableView, &QTableView::customContextMenuRequested,
            this, &MainWindow::showTorrentContextMenu);

    connect(ui->comboServers,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) {
                saveSelectedServerFromCombo();
            });

    connect(client, &rpc_client::freeSpaceReceived,
            this, [this](const QString &, qint64 sizeBytes) {
                remoteFreeSpaceBytes = sizeBytes;
                updateConnectionStatus(lastTorrentCount);
            });

    connect(client, &rpc_client::commandSucceeded,
            this, [this](const QString &method) {
                if (method == QStringLiteral("torrent-set-location")) {
                    statusBar()->showMessage(
                        tr("Torrent location updated."),
                        3000
                        );

                    client->getTorrentList();

                    const int torrentId = currentTorrentId();

                    if (torrentId >= 0)
                        client->getTorrentDetails(torrentId);
                } else if (method == QStringLiteral("torrent-rename-path")) {
                    statusBar()->showMessage(
                        tr("Torrent path renamed."),
                        3000
                        );

                    client->getTorrentList();

                    const int torrentId = currentTorrentId();

                    if (torrentId >= 0)
                        client->getTorrentDetails(torrentId);
                } else if (method == QStringLiteral("torrent-set")) {
                    const int torrentId = currentTorrentId();

                    if (torrentId >= 0)
                        client->getTorrentDetails(torrentId);
                }
            });

    connect(client, &rpc_client::commandFailed,
            this, [this](const QString &, const QString &message) {
                statusBar()->showMessage(message, 5000);
            });


    restoreTableViewState();

    client->init();
    applyAppSettings();
    client->getSessionSettings();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateTorrentList()
{
    client->getTorrentList();
}

void MainWindow::showAbout()
{
    DialogAbout dialog(this);
    dialog.exec();
}

// torent(s) selected
void MainWindow::on_tableView_clicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid())
        return;

    const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return;

    const int torrentId = sourceIndex.data(Qt::UserRole).toInt();

    // clear the informational widgets
    clearGeneralTab();
    ui->peerTableWidget->clearContents();
    ui->peerTableWidget->setRowCount(0);
    clearTrackerTable();
    ui->fileTreeWidget->clear();

    // get info for the selected torrent
    client->getTorrentDetails(torrentId);
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

void MainWindow::on_actionAdd_Torrent_from_Magnet_Link_triggered()
{
    addTorrentFromMagnet();
}

void MainWindow::on_actionReannounce_triggered()
{
    reannounceSelectedTorrent();
}

void MainWindow::on_actionVerify_Torrent_triggered()
{
    verifySelectedTorrent();
}

void MainWindow::on_actionAbout_triggered()
{
    showAbout();
}

void MainWindow::on_actionQuit_triggered()
{
    quitApplication();
}

void MainWindow::on_actionDelete_Torrent_triggered()
{
    const QList<int> ids = selectedTorrentIds();
    const QStringList names = selectedTorrentNames();

    if (ids.isEmpty()) {
        QMessageBox::information(
            this,
            tr("Delete Torrent"),
            tr("No torrent is selected.")
            );
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Delete Torrent"));
    msgBox.setIcon(QMessageBox::Warning);
    QPushButton *torrentOnlyButton = nullptr;
    QPushButton *torrentAndDataButton = nullptr;
    QPushButton *noButton = nullptr;

    if (ids.size() == 1) {
        msgBox.setText(tr("Delete the selected torrent?"));
        msgBox.setInformativeText(
            tr("Remove only the torrent from Transmission, or also delete the downloaded data?\n\n"
               "View details to see the affected torrent name.")
            );
        msgBox.setDetailedText(names.value(0));

        torrentOnlyButton = msgBox.addButton(tr("Torrent only"), QMessageBox::AcceptRole);
        torrentAndDataButton = msgBox.addButton(tr("Torrent and data"), QMessageBox::DestructiveRole);
    } else {
        QString preview = names.join("\n");

        msgBox.setText(tr("Delete %1 selected torrents?").arg(ids.size()));
        msgBox.setInformativeText(
            tr("Remove only the torrents from Transmission, or also delete the downloaded data?\n\n"
               "View details to see the affected torrent names.")
            );
        msgBox.setDetailedText(preview);

        torrentOnlyButton = msgBox.addButton(tr("Torrents only"), QMessageBox::AcceptRole);
        torrentAndDataButton = msgBox.addButton(tr("Torrents and data"), QMessageBox::DestructiveRole);
    }

    noButton = msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);

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

void MainWindow::on_actionSettings_triggered()
{
    AppSettings dialog(this);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    applyAppSettings();
    loadWatchFolderSettings();
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

// save header dimensions of all tables
void MainWindow::saveTableViewState()
{
    QSettings settings;

    settings.setValue(
        "ui/tableView/horizontalHeaderState/v3",
        ui->tableView->horizontalHeader()->saveState()
        );

    settings.setValue(
        "ui/tableView/verticalHeaderState/v2",
        ui->tableView->verticalHeader()->saveState()
        );

    settings.setValue(
        "ui/fileTreeWidget/headerState/v4",
        ui->fileTreeWidget->header()->saveState()
        );

    settings.setValue(
        "ui/peerTableWidget/horizontalHeaderState/v2",
        ui->peerTableWidget->horizontalHeader()->saveState()
        );

    settings.setValue(
        "ui/trackerTableWidget/headerState/v2",
        ui->trackerTableWidget->horizontalHeader()->saveState()
        );
}

// restore header dimensions
void MainWindow::restoreTableViewState()
{
    QSettings settings;

    const QByteArray horizontalState =
        settings.value("ui/tableView/horizontalHeaderState/v3").toByteArray();

    if (!horizontalState.isEmpty()) {
        ui->tableView->horizontalHeader()->restoreState(horizontalState);
    }

    const QByteArray verticalState =
        settings.value("ui/tableView/verticalHeaderState/v2").toByteArray();

    if (!verticalState.isEmpty()) {
        ui->tableView->verticalHeader()->restoreState(verticalState);
    }

    const QByteArray fileTreeHeaderState =
        settings.value("ui/fileTreeWidget/headerState/v4").toByteArray();

    if (!fileTreeHeaderState.isEmpty()) {
        ui->fileTreeWidget->header()->restoreState(fileTreeHeaderState);
    }

    const QByteArray peerTableHeaderState =
        settings.value("ui/peerTableWidget/horizontalHeaderState/v2").toByteArray();

    if (!peerTableHeaderState.isEmpty()) {
        ui->peerTableWidget->horizontalHeader()->restoreState(peerTableHeaderState);
    }

    const QByteArray headerState =
        settings.value("ui/trackerTableWidget/headerState/v2").toByteArray();

    if (!headerState.isEmpty()) {
        ui->trackerTableWidget->horizontalHeader()->restoreState(headerState);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveTableViewState();

    if (!reallyQuit && trayIcon && trayIcon->isVisible()) {
        event->ignore();
        hide();
        return;
    }

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
        ui->listWidget->setCurrentRow(1);
        break;

    case TorrentSortProxyModel::StateFilter::Downloading:
        ui->actionDownloading->setChecked(true);
        ui->listWidget->setCurrentRow(2);
        break;

    case TorrentSortProxyModel::StateFilter::Completed:
        ui->actionCompleted->setChecked(true);
        ui->listWidget->setCurrentRow(3);
        break;

    case TorrentSortProxyModel::StateFilter::Active:
        ui->actionActive->setChecked(true);
        ui->listWidget->setCurrentRow(4);
        break;

    case TorrentSortProxyModel::StateFilter::Inactive:
        ui->actionInactive->setChecked(true);
        ui->listWidget->setCurrentRow(5);
        break;

    case TorrentSortProxyModel::StateFilter::Stopped:
        ui->actionStopped->setChecked(true);
        ui->listWidget->setCurrentRow(6);
        break;

    case TorrentSortProxyModel::StateFilter::Error:
        ui->actionError->setChecked(true);
        ui->listWidget->setCurrentRow(7);
        break;
    }
}

// Create file list folder heirarchy
QTreeWidgetItem *MainWindow::findOrCreateTopLevelItem(const QString &name)
{
    for (int i = 0; i < ui->fileTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = ui->fileTreeWidget->topLevelItem(i);

        if (item->text(0) == name)
            return item;
    }

    auto *item = new QTreeWidgetItem(ui->fileTreeWidget);
    item->setText(0, name);
    item->setData(FileNameColumn, FileKindRole, QStringLiteral("folder"));

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
    child->setData(FileNameColumn,
                   FileKindRole,
                   isFolder ? QStringLiteral("folder") : QStringLiteral("file"));

    return child;
}

void MainWindow::populateFileTree(const QJsonArray &files,
                                  const QJsonArray &wanted,
                                  const QJsonArray &priorities)
{
    ui->fileTreeWidget->clear();
    currentTorrentFilePaths.clear();

    for (int fileIndex = 0; fileIndex < files.size(); ++fileIndex) {
        const QJsonObject file = files.at(fileIndex).toObject();

        const QString path = file.value("name").toString();
        currentTorrentFilePaths.insert(fileIndex, path);

        const qint64 length = file.value("length").toVariant().toLongLong();
        const qint64 bytesCompleted = file.value("bytesCompleted").toVariant().toLongLong();

        const bool isWanted =
            fileIndex < wanted.size()
                ? jsonValueToBool(wanted.at(fileIndex), true)
                : true;

        const int priority =
            fileIndex < priorities.size()
                ? priorities.at(fileIndex).toInt(0)
                : 0;

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

        current->setData(FileNameColumn, FileKindRole, QStringLiteral("file"));
        current->setData(FileNameColumn, FileIndexRole, fileIndex);
        current->setData(FileNameColumn, FileWantedRole, isWanted);
        current->setData(FileNameColumn, FilePriorityRole, priority);

        current->setText(FilePriorityColumn, isWanted ? priorityToString(priority) : tr("Skip"));

        current->setText(FileSizeColumn, QLocale().formattedDataSize(
                                             length, 1, QLocale::DataSizeIecFormat));

        current->setText(FileDoneColumn, QLocale().formattedDataSize(
                                             bytesCompleted, 1, QLocale::DataSizeIecFormat));

        current->setText(FilePercentColumn, QString("%1%").arg(percentDone, 0, 'f', 1));

        current->setData(FileSizeColumn, Qt::UserRole, length);
        current->setData(FileDoneColumn, Qt::UserRole, bytesCompleted);
        current->setData(FilePercentColumn, Qt::UserRole, percentDone);
    }

    updateFolderPriorityStates();

    ui->fileTreeWidget->expandToDepth(0);
    ui->fileTreeWidget->resizeColumnToContents(FileNameColumn);
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

        ui->peerTableWidget->setItem(row, 0, countryItem);
        ui->peerTableWidget->setItem(row, 1, addressItem);
        ui->peerTableWidget->setItem(row, 2, portItem);
        ui->peerTableWidget->setItem(row, 3, clientItem);
        ui->peerTableWidget->setItem(row, 4, progressItem);
        ui->peerTableWidget->setItem(row, 5, downloadItem);
        ui->peerTableWidget->setItem(row, 6, uploadItem);
        ui->peerTableWidget->setItem(row, 7, encryptedItem);
        ui->peerTableWidget->setItem(row, 8, incomingItem);

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
    menu.addAction(ui->actionForce_Start_Torrent);
    menu.addAction(ui->actionStop_Torrent);
    menu.addSeparator();
    menu.addAction(ui->actionVerify_Torrent);
    menu.addAction(ui->actionReannounce);

    QAction *propertiesAction =
        menu.addAction(tr("Properties…"));

    propertiesAction->setEnabled(selectedTorrentIds().size() == 1);

    menu.addSeparator();

    QAction *copyMagnetAction =
        menu.addAction(tr("Copy Magnet Link"));

    QAction *copyHashAction =
        menu.addAction(tr("Copy Hash"));

    const QList<int> contextTorrentIds = selectedTorrentIds();
    const bool canCopyCurrentTorrentDetails =
        contextTorrentIds.size() == 1
        && currentDetailsTorrentId == contextTorrentIds.first();

    copyMagnetAction->setEnabled(
        canCopyCurrentTorrentDetails
        && !currentTorrentMagnetLink.trimmed().isEmpty()
        );

    copyHashAction->setEnabled(
        canCopyCurrentTorrentDetails
        && !currentTorrentHashString.trimmed().isEmpty()
        );

    menu.addSeparator();

    connect(ui->actionForce_Start_Torrent, &QAction::triggered,
            this, &MainWindow::forceStartSelectedTorrents);

    QMenu *queueMenu = menu.addMenu(tr("Queue"));

    QAction *moveTopAction =
        queueMenu->addAction(tr("Move to Top"));

    QAction *moveUpAction =
        queueMenu->addAction(tr("Move Up"));

    QAction *moveDownAction =
        queueMenu->addAction(tr("Move Down"));

    QAction *moveBottomAction =
        queueMenu->addAction(tr("Move to Bottom"));

    menu.addSeparator();

    QAction *setLocationAction =
        menu.addAction(tr("Set Location…"));

    menu.addSeparator();

    menu.addAction(ui->actionDelete_Torrent);

    connect(moveTopAction, &QAction::triggered,
            this, &MainWindow::queueMoveSelectedTop);

    connect(moveUpAction, &QAction::triggered,
            this, &MainWindow::queueMoveSelectedUp);

    connect(moveDownAction, &QAction::triggered,
            this, &MainWindow::queueMoveSelectedDown);

    connect(moveBottomAction, &QAction::triggered,
            this, &MainWindow::queueMoveSelectedBottom);

    connect(setLocationAction, &QAction::triggered,
            this, &MainWindow::setSelectedTorrentsLocation);

    connect(propertiesAction, &QAction::triggered,
            this, &MainWindow::showSelectedTorrentProperties);

    connect(copyMagnetAction, &QAction::triggered,
            this, &MainWindow::copySelectedTorrentMagnetLink);

    connect(copyHashAction, &QAction::triggered,
            this, &MainWindow::copySelectedTorrentHash);

    menu.exec(ui->tableView->viewport()->mapToGlobal(pos));
}

void MainWindow::showSelectedTorrentProperties()
{
    const QList<int> ids = selectedTorrentIds();

    if (ids.size() != 1) {
        statusBar()->showMessage(tr("Select one torrent to edit properties."), 3000);
        return;
    }

    TorrentPropertiesDialog dialog(client, ids.first(), this);
    dialog.exec();

    client->getTorrentList();

    const int torrentId = currentTorrentId();

    if (torrentId >= 0)
        client->getTorrentDetails(torrentId);
}

void MainWindow::startSelectedTorrent()
{
    invokeSelectedTorrentCommand(&rpc_client::startTorrents,
                                 tr("Starting %1 torrent(s)..."));
}

void MainWindow::stopSelectedTorrent()
{
    invokeSelectedTorrentCommand(&rpc_client::stopTorrents,
                                 tr("Stopping %1 torrent(s)..."));
}

void MainWindow::handleLaunchArguments(const QStringList &arguments)
{
    if (arguments.isEmpty()) {
        bringToFront();
        return;
    }

    if (!torrentAddController)
        return;

    bool handledAny = false;

    for (const QString &argument : arguments) {
        const QString trimmed = argument.trimmed();

        if (trimmed.isEmpty())
            continue;

        if (trimmed.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive)) {
            torrentAddController->addMagnetLink(trimmed);
            handledAny = true;
            continue;
        }

        const QUrl url(trimmed);

        if (url.isValid()
            && url.scheme().compare(QStringLiteral("magnet"), Qt::CaseInsensitive) == 0) {
            torrentAddController->addMagnetLink(url.toString());
            handledAny = true;
            continue;
        }

        const QFileInfo fileInfo(trimmed);

        if (fileInfo.exists()
            && fileInfo.isFile()
            && fileInfo.suffix().compare(QStringLiteral("torrent"),
                                         Qt::CaseInsensitive) == 0) {
            torrentAddController->addTorrentFile(fileInfo.absoluteFilePath());
            handledAny = true;
            continue;
        }
    }

    if (handledAny)
        bringToFront();
}

void MainWindow::addTorrentFromFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Add Torrent File"),
        QString(),
        tr("Torrent Files (*.torrent);;All Files (*)")
        );

    if (fileName.isEmpty())
        return;

    torrentAddController->addTorrentFile(fileName);
}

void MainWindow::addTorrentFromMagnet()
{
    bool ok = false;

    const QString magnetLink = QInputDialog::getText(
        this,
        tr("Add Magnet Link"),
        tr("Magnet link:"),
        QLineEdit::Normal,
        QString(),
        &ok
        );

    if (!ok || magnetLink.trimmed().isEmpty())
        return;

    torrentAddController->addMagnetLink(magnetLink);
}

void MainWindow::reannounceSelectedTorrent()
{
    invokeSelectedTorrentCommand(&rpc_client::reannounceTorrents,
                                 tr("Reannouncing %1 torrent(s)..."));
}

void MainWindow::verifySelectedTorrent()
{
    invokeSelectedTorrentCommand(&rpc_client::verifyTorrents,
                                 tr("Verifying %1 torrent(s)..."));
}

void MainWindow::setSelectedTorrentsLocation()
{
    const QList<int> ids = selectedTorrentIds();

    if (ids.isEmpty()) {
        statusBar()->showMessage(tr("No torrent selected."), 3000);
        return;
    }

    QString initialLocation = remoteDownloadDir.trimmed();

    if (ids.size() == 1 && currentTorrentId() == ids.first()) {
        const QString currentDownloadDir =
            ui->labelGeneralDownloadDir->text().trimmed();

        if (!currentDownloadDir.isEmpty() &&
            currentDownloadDir != tr("Unknown")) {
            initialLocation = currentDownloadDir;
        }
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Set Location"));

    auto *layout = new QVBoxLayout(&dialog);

    auto *descriptionLabel = new QLabel(
        tr("Set the download location on the Transmission server."),
        &dialog
        );
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);

    auto *formLayout = new QFormLayout;
    auto *locationEdit = new QLineEdit(initialLocation, &dialog);
    locationEdit->setPlaceholderText(tr("Remote download location"));
    locationEdit->selectAll();

    formLayout->addRow(tr("Location:"), locationEdit);
    layout->addLayout(formLayout);

    auto *moveDataCheckBox = new QCheckBox(
        tr("Move existing data to the new location"),
        &dialog
        );
    moveDataCheckBox->setChecked(false);
    layout->addWidget(moveDataCheckBox);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog
        );
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);

    connect(buttonBox, &QDialogButtonBox::rejected,
            &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString location = locationEdit->text().trimmed();

    if (location.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Set Location"),
            tr("The download location cannot be empty.")
            );
        return;
    }

    const bool moveData = moveDataCheckBox->isChecked();

    client->setTorrentLocation(ids, location, moveData);

    statusBar()->showMessage(
        moveData
            ? tr("Moving %1 torrent(s) to %2...").arg(ids.size()).arg(location)
            : tr("Setting location for %1 torrent(s) to %2...").arg(ids.size()).arg(location),
        5000
        );

    QTimer::singleShot(1500, this, [this]() {
        client->getTorrentList();

        const int torrentId = currentTorrentId();

        if (torrentId >= 0)
            client->getTorrentDetails(torrentId);
    });
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
            ? tr("(unnamed server)")
            : rpcUrl;
        }

        if (i == defaultIndex)
            name += tr(" (default)");

        ui->comboServers->addItem(name, i);
    }

    settings.endArray();

    if (ui->comboServers->count() == 0) {
        ui->comboServers->addItem(tr("No servers configured"), -1);
        ui->comboServers->setEnabled(false);
        return;
    }

    ui->comboServers->setEnabled(true);

    int comboIndex = -1;

    // First preference: keep whatever the combo was already showing.
    if (hadPreviousSelection)
        comboIndex = ui->comboServers->findData(previouslySelectedServerIndex);

    // Next fallback: default server.
    if (comboIndex < 0)
        comboIndex = ui->comboServers->findData(defaultIndex);

    // First-load fallback: saved current server.
    if (comboIndex < 0)
        comboIndex = ui->comboServers->findData(savedCurrentIndex);

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
        statusBar()->showMessage(tr("Could not switch server."), 5000);
        return;
    }

    ui->fileTreeWidget->clear();
    ui->peerTableWidget->clearContents();
    ui->peerTableWidget->setRowCount(0);
    updateTorrentActionState();

    statusBar()->showMessage(
        tr("Selected server: %1").arg(client->getServer()),
        3000
        );

    client->getTorrentList();

    remoteDownloadDir.clear();
    remoteFreeSpaceBytes = -1;
    lastTorrentCount = 0;
    updateConnectionStatus(0);

    client->getSessionSettings();
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
    ui->actionForce_Start_Torrent->setEnabled(hasSelection);
}

void MainWindow::setupTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    trayIcon = new QSystemTrayIcon(this);

    QIcon menuBarIcon(":/icons/planetary_menu.png");
    menuBarIcon.setIsMask(true);

    trayIcon->setIcon(menuBarIcon);
    trayIcon->setToolTip("Planetary");

    trayMenu = new QMenu(this);

    QAction *showAction = trayMenu->addAction(tr("Show Planetary"));
    QAction *quitAction = trayMenu->addAction(tr("Quit"));

    connect(showAction, &QAction::triggered,
            this, &MainWindow::showMainWindow);

    connect(quitAction, &QAction::triggered,
            this, [this]() {
                reallyQuit = true;
                close();
            });

    trayIcon->setContextMenu(trayMenu);

    connect(trayIcon, &QSystemTrayIcon::activated,
            this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick) {
                    showMainWindow();
                }
            });

    updateTrayIconVisibility();
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
}

int MainWindow::updateIntervalMs() const
{
    QSettings settings;

    const int seconds =
        settings.value(SettingsKeys::UpdateInterval, DefaultUpdateIntervalSeconds).toInt();

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
        tr("Update interval: %1 seconds").arg(timer->interval() / 1000),
        3000
        );
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

void MainWindow::invokeSelectedTorrentCommand(void (rpc_client::*command)(const QList<int> &),
                                              const QString &message)
{
    const QList<int> ids = selectedTorrentIds();

    if (ids.isEmpty()) {
        statusBar()->showMessage(tr("No torrent selected."), 3000);
        return;
    }

    (client->*command)(ids);

    if (!message.isEmpty())
        statusBar()->showMessage(message.arg(ids.size()), 3000);
}

bool MainWindow::currentTabWantsLiveTorrentDetails() const
{
    QWidget *current = ui->tabWidget->currentWidget();
    return current == ui->general || current == torrentDetailsTab;
}

void MainWindow::refreshCurrentTorrentLiveDetailsIfNeeded()
{
    if (currentDetailsTorrentId < 0 || !currentTabWantsLiveTorrentDetails())
        return;

    client->getTorrentPieces(currentDetailsTorrentId);
}

void MainWindow::setupConnectionStatusIndicator()
{
    connectionStatusLabel = new QLabel(this);
    connectionStatusLabel->setText(tr("Not connected"));

    connectionStatusLabel->setMinimumWidth(260);
    connectionStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    ui->statusbar->addPermanentWidget(connectionStatusLabel);

    connect(client, &rpc_client::updateStarted,
            this,
            [this]() {
                connectionStatusLabel->setText(tr("Updating..."));
            });

    connect(client, &rpc_client::updateFailed,
            this,
            [this](const QString &message) {
                QString displayMessage = message;

                if (message.contains("timed out", Qt::CaseInsensitive)) {
                    displayMessage = tr("Timed out contacting Transmission");
                } else if (message.contains("connection refused", Qt::CaseInsensitive)) {
                    displayMessage = tr("Connection refused by Transmission");
                } else if (message.contains("host not found", Qt::CaseInsensitive)) {
                    displayMessage = tr("Host not found");
                } else if (message.contains("network", Qt::CaseInsensitive)) {
                    displayMessage = message;
                }

                connectionStatusLabel->setText(
                    tr("Connection error: %1").arg(displayMessage)
                    );
            });

    connect(client, &rpc_client::serverChanged,
            this,
            [this]() {
                connectionStatusLabel->setText(
                    tr("Server changed: %1").arg(client->getServer())
                    );
            });
}

void MainWindow::setupPieceMapWidget()
{
    pieceMapGroup = new QGroupBox(tr("Pieces"), this);
    pieceMapGroup->setMinimumSize(220, 120);
    pieceMapGroup->setMaximumSize(320, 170);
    pieceMapGroup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    auto *pieceMapLayout = new QVBoxLayout(pieceMapGroup);
    pieceMapLayout->setContentsMargins(6, 6, 6, 6);

    pieceMapWidget = new PieceMapWidget(pieceMapGroup);
    pieceMapLayout->addWidget(pieceMapWidget);

    auto *topGeneralRow = new QWidget(ui->general);
    auto *topGeneralLayout = new QHBoxLayout(topGeneralRow);
    topGeneralLayout->setContentsMargins(0, 0, 0, 0);
    topGeneralLayout->setSpacing(6);

    ui->verticalLayoutGeneral->removeWidget(ui->groupGeneralInfo);
    topGeneralLayout->addWidget(ui->groupGeneralInfo, 1);
    topGeneralLayout->addWidget(pieceMapGroup, 0, Qt::AlignTop);

    ui->verticalLayoutGeneral->insertWidget(0, topGeneralRow, 0);
}


void MainWindow::setupTorrentDetailsTab()
{
    torrentDetailsTab = new QWidget(ui->tabWidget);

    auto *layout = new QVBoxLayout(torrentDetailsTab);
    layout->setContentsMargins(4, 4, 4, 4);

    torrentDetailsTable = new QTableWidget(torrentDetailsTab);
    torrentDetailsTable->setColumnCount(2);
    torrentDetailsTable->setHorizontalHeaderLabels({ tr("Property"), tr("Value") });
    torrentDetailsTable->verticalHeader()->setVisible(false);
    torrentDetailsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    torrentDetailsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    torrentDetailsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    torrentDetailsTable->setAlternatingRowColors(true);
    torrentDetailsTable->setWordWrap(false);
    torrentDetailsTable->horizontalHeader()->setStretchLastSection(true);
    torrentDetailsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    torrentDetailsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    layout->addWidget(torrentDetailsTable);

    const int generalIndex = ui->tabWidget->indexOf(ui->general);
    ui->tabWidget->insertTab(generalIndex + 1, torrentDetailsTab, tr("Details"));

    clearTorrentDetailsTab();
}

void MainWindow::clearTorrentDetailsTab()
{
    if (!torrentDetailsTable)
        return;

    torrentDetailsTable->setSortingEnabled(false);
    torrentDetailsTable->clearContents();
    torrentDetailsTable->setRowCount(0);
}

void MainWindow::updateTorrentDetailsTab(const QJsonObject &details)
{
    if (!torrentDetailsTable)
        return;

    torrentDetailsTable->setSortingEnabled(false);
    torrentDetailsTable->clearContents();
    torrentDetailsTable->setRowCount(0);

    auto jsonInt64 = [&details](const QString &key, qint64 defaultValue = 0) -> qint64 {
        const QJsonValue value = details.value(key);

        if (value.isUndefined() || value.isNull())
            return defaultValue;

        return value.toVariant().toLongLong();
    };

    auto jsonDouble = [&details](const QString &key, double defaultValue = 0.0) -> double {
        const QJsonValue value = details.value(key);

        if (value.isUndefined() || value.isNull())
            return defaultValue;

        return value.toDouble(defaultValue);
    };

    auto jsonBool = [&details](const QString &key, bool defaultValue = false) -> bool {
        return jsonValueToBool(details.value(key), defaultValue);
    };

    auto formatBytesText = [](qint64 bytes) -> QString {
        return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeIecFormat);
    };

    auto formatRate = [this, &formatBytesText](qint64 bytesPerSecond) -> QString {
        return tr("%1/s").arg(formatBytesText(bytesPerSecond));
    };

    auto formatPercent = [](double fraction) -> QString {
        return QStringLiteral("%1%").arg(QLocale().toString(fraction * 100.0, 'f', 1));
    };

    auto formatRatio = [this](double ratio) -> QString {
        if (ratio < 0.0)
            return tr("None");

        return QLocale().toString(ratio, 'f', 2);
    };

    auto formatDate = [this](qint64 seconds) -> QString {
        if (seconds <= 0)
            return tr("Unknown");

        return QLocale().toString(QDateTime::fromSecsSinceEpoch(seconds), QLocale::ShortFormat);
    };

    auto formatDuration = [this](qint64 seconds) -> QString {
        if (seconds < 0)
            return tr("Unknown");

        const qint64 days = seconds / 86400;
        seconds %= 86400;
        const qint64 hours = seconds / 3600;
        seconds %= 3600;
        const qint64 minutes = seconds / 60;
        const qint64 secs = seconds % 60;

        if (days > 0)
            return tr("%1 d %2 h").arg(days).arg(hours);

        if (hours > 0)
            return tr("%1 h %2 m").arg(hours).arg(minutes);

        if (minutes > 0)
            return tr("%1 m %2 s").arg(minutes).arg(secs);

        return tr("%1 s").arg(secs);
    };

    auto formatEta = [this, &formatDuration](qint64 seconds) -> QString {
        if (seconds < 0)
            return tr("Unknown");

        return formatDuration(seconds);
    };

    auto yesNo = [this](bool value) -> QString {
        return value ? tr("Yes") : tr("No");
    };

    auto statusText = [this](int status) -> QString {
        switch (status) {
        case 0:
            return tr("Stopped");
        case 1:
            return tr("Queued for check");
        case 2:
            return tr("Checking");
        case 3:
            return tr("Queued for download");
        case 4:
            return tr("Downloading");
        case 5:
            return tr("Queued for seeding");
        case 6:
            return tr("Seeding");
        default:
            return tr("Unknown (%1)").arg(status);
        }
    };

    auto seedRatioModeText = [this](int mode) -> QString {
        switch (mode) {
        case 0:
            return tr("Use global setting");
        case 1:
            return tr("Use torrent ratio limit");
        case 2:
            return tr("Seed regardless of ratio");
        default:
            return tr("Unknown (%1)").arg(mode);
        }
    };

    auto seedIdleModeText = [this](int mode) -> QString {
        switch (mode) {
        case 0:
            return tr("Use global setting");
        case 1:
            return tr("Use torrent idle limit");
        case 2:
            return tr("Seed regardless of idle time");
        default:
            return tr("Unknown (%1)").arg(mode);
        }
    };

    auto priorityText = [this](int priority) -> QString {
        switch (priority) {
        case 1:
            return tr("High");
        case -1:
            return tr("Low");
        case 0:
            return tr("Normal");
        default:
            return tr("Unknown (%1)").arg(priority);
        }
    };

    auto addSection = [this](const QString &title) {
        const int row = torrentDetailsTable->rowCount();
        torrentDetailsTable->insertRow(row);
        torrentDetailsTable->setSpan(row, 0, 1, 2);

        auto *item = new QTableWidgetItem(title);
        QFont font = item->font();
        font.setBold(true);
        item->setFont(font);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setBackground(palette().alternateBase());

        torrentDetailsTable->setItem(row, 0, item);
    };

    auto addRow = [this](const QString &label, const QString &value) {
        const int row = torrentDetailsTable->rowCount();
        torrentDetailsTable->insertRow(row);

        auto *labelItem = new QTableWidgetItem(label);
        labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);

        auto *valueItem = new QTableWidgetItem(value);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setToolTip(value);

        torrentDetailsTable->setItem(row, 0, labelItem);
        torrentDetailsTable->setItem(row, 1, valueItem);
    };

    auto addRowIfPresent = [&details, &addRow](const QString &key, const QString &label, const QString &value) {
        if (!details.contains(key))
            return;

        addRow(label, value);
    };

    const qint64 pieceSize = jsonInt64(QStringLiteral("pieceSize"));
    const int pieceCount = details.value(QStringLiteral("pieceCount")).toInt(0);
    const QByteArray pieces = QByteArray::fromBase64(
        details.value(QStringLiteral("pieces")).toString().toLatin1()
    );

    int completedPieces = 0;
    if (pieceCount > 0 && !pieces.isEmpty()) {
        for (int index = 0; index < pieceCount; ++index) {
            const int byteIndex = index / 8;
            const int bitIndex = 7 - (index % 8);

            if (byteIndex >= 0 && byteIndex < pieces.size()) {
                const uchar byte = static_cast<uchar>(pieces.at(byteIndex));
                if ((byte & (1u << bitIndex)) != 0)
                    ++completedPieces;
            }
        }
    }

    addSection(tr("State"));
    addRowIfPresent(QStringLiteral("status"), tr("Status"), statusText(details.value(QStringLiteral("status")).toInt(-1)));
    addRowIfPresent(QStringLiteral("isPrivate"), tr("Private torrent"), yesNo(jsonBool(QStringLiteral("isPrivate"))));
    addRowIfPresent(QStringLiteral("isStalled"), tr("Stalled"), yesNo(jsonBool(QStringLiteral("isStalled"))));
    addRowIfPresent(QStringLiteral("isFinished"), tr("Finished"), yesNo(jsonBool(QStringLiteral("isFinished"))));
    if (details.value(QStringLiteral("error")).toInt(0) != 0 || !details.value(QStringLiteral("errorString")).toString().isEmpty()) {
        addRow(tr("Error"), details.value(QStringLiteral("errorString")).toString(tr("Unknown error")));
    }

    addSection(tr("Progress"));
    addRowIfPresent(QStringLiteral("percentDone"), tr("Done"), formatPercent(jsonDouble(QStringLiteral("percentDone"))));
    addRowIfPresent(QStringLiteral("metadataPercentComplete"), tr("Metadata"), formatPercent(jsonDouble(QStringLiteral("metadataPercentComplete"))));
    addRowIfPresent(QStringLiteral("recheckProgress"), tr("Recheck"), formatPercent(jsonDouble(QStringLiteral("recheckProgress"))));
    if (pieceCount > 0)
        addRow(tr("Pieces complete"), tr("%1 / %2 (%3%)")
                                  .arg(completedPieces)
                                  .arg(pieceCount)
                                  .arg(QLocale().toString(100.0 * completedPieces / pieceCount, 'f', 1)));

    addSection(tr("Transfer"));
    addRowIfPresent(QStringLiteral("rateDownload"), tr("Download rate"), formatRate(jsonInt64(QStringLiteral("rateDownload"))));
    addRowIfPresent(QStringLiteral("rateUpload"), tr("Upload rate"), formatRate(jsonInt64(QStringLiteral("rateUpload"))));
    addRowIfPresent(QStringLiteral("uploadRatio"), tr("Ratio"), formatRatio(jsonDouble(QStringLiteral("uploadRatio"), -1.0)));
    addRowIfPresent(QStringLiteral("downloadedEver"), tr("Downloaded"), formatBytesText(jsonInt64(QStringLiteral("downloadedEver"))));
    addRowIfPresent(QStringLiteral("uploadedEver"), tr("Uploaded"), formatBytesText(jsonInt64(QStringLiteral("uploadedEver"))));
    addRowIfPresent(QStringLiteral("corruptEver"), tr("Wasted/corrupt"), formatBytesText(jsonInt64(QStringLiteral("corruptEver"))));
    addRowIfPresent(QStringLiteral("haveValid"), tr("Have valid"), formatBytesText(jsonInt64(QStringLiteral("haveValid"))));
    addRowIfPresent(QStringLiteral("haveUnchecked"), tr("Have unchecked"), formatBytesText(jsonInt64(QStringLiteral("haveUnchecked"))));
    addRowIfPresent(QStringLiteral("desiredAvailable"), tr("Desired available"), formatBytesText(jsonInt64(QStringLiteral("desiredAvailable"))));
    addRowIfPresent(QStringLiteral("leftUntilDone"), tr("Left until done"), formatBytesText(jsonInt64(QStringLiteral("leftUntilDone"))));
    addRowIfPresent(QStringLiteral("sizeWhenDone"), tr("Size when done"), formatBytesText(jsonInt64(QStringLiteral("sizeWhenDone"))));
    addRowIfPresent(QStringLiteral("totalSize"), tr("Total size"), formatBytesText(jsonInt64(QStringLiteral("totalSize"))));

    addSection(tr("Time"));
    addRowIfPresent(QStringLiteral("eta"), tr("ETA"), formatEta(jsonInt64(QStringLiteral("eta"), -1)));
    addRowIfPresent(QStringLiteral("etaIdle"), tr("Idle ETA"), formatEta(jsonInt64(QStringLiteral("etaIdle"), -1)));
    addRowIfPresent(QStringLiteral("secondsDownloading"), tr("Downloading time"), formatDuration(jsonInt64(QStringLiteral("secondsDownloading"))));
    addRowIfPresent(QStringLiteral("secondsSeeding"), tr("Seeding time"), formatDuration(jsonInt64(QStringLiteral("secondsSeeding"))));
    addRowIfPresent(QStringLiteral("dateCreated"), tr("Created"), formatDate(jsonInt64(QStringLiteral("dateCreated"))));
    addRowIfPresent(QStringLiteral("addedDate"), tr("Added"), formatDate(jsonInt64(QStringLiteral("addedDate"))));
    addRowIfPresent(QStringLiteral("startDate"), tr("Started"), formatDate(jsonInt64(QStringLiteral("startDate"))));
    addRowIfPresent(QStringLiteral("doneDate"), tr("Completed"), formatDate(jsonInt64(QStringLiteral("doneDate"))));
    addRowIfPresent(QStringLiteral("activityDate"), tr("Last activity"), formatDate(jsonInt64(QStringLiteral("activityDate"))));
    addRowIfPresent(QStringLiteral("editDate"), tr("Edited"), formatDate(jsonInt64(QStringLiteral("editDate"))));
    addRowIfPresent(QStringLiteral("manualAnnounceTime"), tr("Manual announce available"), formatDate(jsonInt64(QStringLiteral("manualAnnounceTime"))));

    addSection(tr("Peers"));
    addRowIfPresent(QStringLiteral("peersConnected"), tr("Connected peers"), QString::number(jsonInt64(QStringLiteral("peersConnected"))));
    addRowIfPresent(QStringLiteral("peersSendingToUs"), tr("Peers sending to us"), QString::number(jsonInt64(QStringLiteral("peersSendingToUs"))));
    addRowIfPresent(QStringLiteral("peersGettingFromUs"), tr("Peers getting from us"), QString::number(jsonInt64(QStringLiteral("peersGettingFromUs"))));
    addRowIfPresent(QStringLiteral("webseedsSendingToUs"), tr("Web seeds sending to us"), QString::number(jsonInt64(QStringLiteral("webseedsSendingToUs"))));
    addRowIfPresent(QStringLiteral("maxConnectedPeers"), tr("Max connected peers"), QString::number(jsonInt64(QStringLiteral("maxConnectedPeers"))));
    if (details.contains(QStringLiteral("peersFrom"))) {
        const QJsonObject peersFrom = details.value(QStringLiteral("peersFrom")).toObject();
        QStringList peerSources;
        for (auto it = peersFrom.constBegin(); it != peersFrom.constEnd(); ++it)
            peerSources << QStringLiteral("%1: %2").arg(it.key()).arg(it.value().toInt());
        peerSources.sort(Qt::CaseInsensitive);
        addRow(tr("Peer sources"), peerSources.join(QStringLiteral(", ")));
    }

    addSection(tr("Limits and seeding"));
    addRowIfPresent(QStringLiteral("bandwidthPriority"), tr("Bandwidth priority"), priorityText(details.value(QStringLiteral("bandwidthPriority")).toInt(0)));
    addRowIfPresent(QStringLiteral("honorsSessionLimits"), tr("Honor session limits"), yesNo(jsonBool(QStringLiteral("honorsSessionLimits"), true)));
    if (details.contains(QStringLiteral("downloadLimited")) || details.contains(QStringLiteral("downloadLimit"))) {
        const bool limited = jsonBool(QStringLiteral("downloadLimited"));
        addRow(tr("Download limit"), limited
                                      ? tr("%1/s").arg(formatBytesText(jsonInt64(QStringLiteral("downloadLimit")) * 1000))
                                      : tr("Unlimited"));
    }
    if (details.contains(QStringLiteral("uploadLimited")) || details.contains(QStringLiteral("uploadLimit"))) {
        const bool limited = jsonBool(QStringLiteral("uploadLimited"));
        addRow(tr("Upload limit"), limited
                                    ? tr("%1/s").arg(formatBytesText(jsonInt64(QStringLiteral("uploadLimit")) * 1000))
                                    : tr("Unlimited"));
    }
    addRowIfPresent(QStringLiteral("seedRatioMode"), tr("Seed ratio mode"), seedRatioModeText(details.value(QStringLiteral("seedRatioMode")).toInt(0)));
    addRowIfPresent(QStringLiteral("seedRatioLimit"), tr("Seed ratio limit"), QLocale().toString(jsonDouble(QStringLiteral("seedRatioLimit")), 'f', 2));
    addRowIfPresent(QStringLiteral("seedIdleMode"), tr("Seed idle mode"), seedIdleModeText(details.value(QStringLiteral("seedIdleMode")).toInt(0)));
    addRowIfPresent(QStringLiteral("seedIdleLimit"), tr("Seed idle limit"), tr("%1 minute(s)").arg(jsonInt64(QStringLiteral("seedIdleLimit"))));
    addRowIfPresent(QStringLiteral("queuePosition"), tr("Queue position"), QString::number(jsonInt64(QStringLiteral("queuePosition"))));

    addSection(tr("Torrent"));
    addRowIfPresent(QStringLiteral("pieceSize"), tr("Piece size"), formatBytesText(pieceSize));
    addRowIfPresent(QStringLiteral("pieceCount"), tr("Piece count"), QString::number(pieceCount));
    addRowIfPresent(QStringLiteral("file-count"), tr("File count"), QString::number(jsonInt64(QStringLiteral("file-count"))));
    addRowIfPresent(QStringLiteral("group"), tr("Group"), details.value(QStringLiteral("group")).toString().isEmpty()
                                                    ? tr("None")
                                                    : details.value(QStringLiteral("group")).toString());
    if (details.contains(QStringLiteral("labels"))) {
        const QJsonArray labels = details.value(QStringLiteral("labels")).toArray();
        QStringList labelTexts;
        for (const QJsonValue &label : labels)
            labelTexts << label.toString();
        addRow(tr("Labels"), labelTexts.isEmpty() ? tr("None") : labelTexts.join(QStringLiteral(", ")));
    }
    addRowIfPresent(QStringLiteral("downloadDir"), tr("Download directory"), details.value(QStringLiteral("downloadDir")).toString());
    addRowIfPresent(QStringLiteral("hashString"), tr("Hash"), details.value(QStringLiteral("hashString")).toString());

    torrentDetailsTable->resizeRowsToContents();
}

void MainWindow::updatePieceMap(const QJsonObject &details)
{
    if (!pieceMapWidget || !pieceMapGroup)
        return;

    const int pieceCount = details.value(QStringLiteral("pieceCount")).toInt(0);
    const QByteArray pieces =
        QByteArray::fromBase64(
            details.value(QStringLiteral("pieces")).toString().toLatin1()
        );

    pieceMapWidget->setPieces(pieceCount, pieces);

    if (pieceCount <= 0) {
        pieceMapGroup->setTitle(tr("Pieces"));
        return;
    }

    const int completed = pieceMapWidget->completedPieceCount();
    const double percent =
        100.0 * static_cast<double>(completed) / static_cast<double>(pieceCount);

    pieceMapGroup->setTitle(
        tr("Pieces (%1 / %2, %3%)")
            .arg(completed)
            .arg(pieceCount)
            .arg(QLocale().toString(percent, 'f', 1))
    );
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

    currentTorrentDownloadDir = downloadDir;

    const QString hashString =
        details.value("hashString").toString();

    const QString magnetLink =
        details.value("magnetLink").toString();

    currentDetailsTorrentId = details.value("id").toInt(-1);
    currentTorrentHashString = hashString;
    currentTorrentMagnetLink = magnetLink;

    const qint64 totalSize =
        details.value("totalSize").toVariant().toLongLong();

    const qint64 dateCreated =
        details.value("dateCreated").toVariant().toLongLong();

    ui->labelGeneralName->setText(name);
    ui->labelGeneralCreator->setText(creator);
    ui->labelGeneralDownloadDir->setText(downloadDir);
    ui->labelGeneralHash->setText(hashString);
    ui->lineGeneralMagnet->setText(magnetLink);

    const QString trimmedComment = comment.trimmed();

    if (trimmedComment.isEmpty()) {
        ui->labelGeneralComment->setText(tr("None"));
    } else if (looksLikeUrl(trimmedComment)) {
        const QUrl url = QUrl::fromUserInput(trimmedComment);

        ui->labelGeneralComment->setText(
            QString("<a href=\"%1\">%2</a>")
                .arg(url.toString().toHtmlEscaped(),
                     trimmedComment.toHtmlEscaped())
            );
    } else {
        ui->labelGeneralComment->setText(trimmedComment.toHtmlEscaped());
    }

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
        ui->labelGeneralCreated->setText(tr("Unknown"));
    }

    updatePieceMap(details);
}

void MainWindow::populateTrackerTable(const QJsonObject &details)
{
    const QJsonArray trackerStats =
        details.value("trackerStats").toArray();

    const QJsonArray trackers =
        details.value("trackers").toArray();

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

        int trackerId =
            tracker.value("id").toInt(-1);

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

void MainWindow::bringToFront()
{
    showNormal();
    raise();
    activateWindow();
}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::FileOpen) {
        auto *fileOpenEvent = static_cast<QFileOpenEvent *>(event);

        const QUrl url = fileOpenEvent->url();

        if (url.isValid()
            && url.scheme().compare(QStringLiteral("magnet"), Qt::CaseInsensitive) == 0) {
            handleLaunchArguments(QStringList { url.toString() });
            return true;
        }

        const QString filePath = fileOpenEvent->file();

        if (!filePath.isEmpty()) {
            handleLaunchArguments(QStringList { filePath });
            return true;
        }
    }

    return QMainWindow::event(event);
}

bool MainWindow::trayIconEnabled() const
{
    QSettings settings;
    return settings.value(SettingsKeys::ShowTrayIcon, true).toBool();
}

bool MainWindow::trayNotificationsEnabled() const
{
    QSettings settings;

    return trayIconEnabled() &&
           settings.value(SettingsKeys::ShowTrayNotifications, true).toBool();
}

void MainWindow::updateTrayIconVisibility()
{
    if (!trayIcon)
        return;

    if (trayIconEnabled()) {
        trayIcon->show();
    } else {
        trayIcon->hide();
    }
}

void MainWindow::showTrayNotification(const QString &title,
                                      const QString &message,
                                      QSystemTrayIcon::MessageIcon icon,
                                      int millisecondsTimeoutHint)
{
    if (!trayIcon || !trayIcon->isVisible())
        return;

    if (!trayNotificationsEnabled())
        return;

    trayIcon->showMessage(title, message, icon, millisecondsTimeoutHint);
}

void MainWindow::applyAppSettings()
{
    applyUpdateInterval();
    updateTrayIconVisibility();
}

bool MainWindow::isTorrentCompleteForNotification(const torrent &torrentItem)
{
    const QString status = torrentItem.getStatus();

    return torrentItem.getPercentDone() >= 99.9 ||
           status == tr("Seeding") ||
           status == tr("Waiting to Seed");
}

void MainWindow::processFinishedTorrentNotifications(const QVector<torrent> &torrents)
{
    QSet<int> currentlyCompleted;

    for (const torrent &torrentItem : torrents) {
        if (isTorrentCompleteForNotification(torrentItem)) {
            currentlyCompleted.insert(torrentItem.getId());
        }
    }

    if (!completedTorrentNotificationBaselineLoaded) {
        knownCompletedTorrentIds = currentlyCompleted;
        completedTorrentNotificationBaselineLoaded = true;
        return;
    }

    for (const torrent &torrentItem : torrents) {
        const int id = torrentItem.getId();

        if (!currentlyCompleted.contains(id))
            continue;

        if (knownCompletedTorrentIds.contains(id))
            continue;

        showTrayNotification(
            tr("Torrent finished"),
            torrentItem.getName(),
            QSystemTrayIcon::Information,
            5000
            );
    }

    knownCompletedTorrentIds = currentlyCompleted;
}

void MainWindow::handleTorrentsReceived(const QVector<torrent> &torrents)
{
    processFinishedTorrentNotifications(torrents);

    rebuildTorrentFilterList(torrents);
    updateConnectionStatus(torrents.size());

    if (!remoteDownloadDir.isEmpty())
        client->getFreeSpace(remoteDownloadDir);

    refreshCurrentTorrentLiveDetailsIfNeeded();
}

void MainWindow::updateFolderPriorityStates()
{
    for (int i = 0; i < ui->fileTreeWidget->topLevelItemCount(); ++i) {
        updateFolderPriorityState(ui->fileTreeWidget->topLevelItem(i));
    }
}

void MainWindow::updateFolderPriorityState(QTreeWidgetItem *item)
{
    if (!item)
        return;

    const QString kind =
        item->data(FileNameColumn, FileKindRole).toString();

    if (kind == QStringLiteral("file"))
        return;

    QSet<QString> effectivePriorities;

    std::function<void(QTreeWidgetItem *)> scan = [&](QTreeWidgetItem *node) {
        if (!node)
            return;

        const QString nodeKind =
            node->data(FileNameColumn, FileKindRole).toString();

        if (nodeKind == QStringLiteral("file")) {
            const QVariant wantedValue =
                node->data(FileNameColumn, FileWantedRole);

            const bool wanted =
                wantedValue.isValid() ? wantedValue.toBool() : true;

            if (!wanted) {
                effectivePriorities.insert(tr("Skip"));
                return;
            }

            const int priority =
                node->data(FileNameColumn, FilePriorityRole).toInt();

            effectivePriorities.insert(priorityToString(priority));
            return;
        }

        for (int i = 0; i < node->childCount(); ++i)
            scan(node->child(i));
    };

    scan(item);

    if (effectivePriorities.size() == 1) {
        item->setText(FilePriorityColumn, *effectivePriorities.constBegin());
    } else if (effectivePriorities.size() > 1) {
        item->setText(FilePriorityColumn, tr("Mixed"));
    } else {
        item->setText(FilePriorityColumn, QString());
    }
}

QList<int> MainWindow::fileIndicesForItem(QTreeWidgetItem *item) const
{
    QList<int> indices;

    if (!item)
        return indices;

    const QString kind =
        item->data(FileNameColumn, FileKindRole).toString();

    if (kind == QStringLiteral("file")) {
        const QVariant fileIndexValue =
            item->data(FileNameColumn, FileIndexRole);

        const int fileIndex =
            fileIndexValue.isValid() ? fileIndexValue.toInt() : -1;

        if (fileIndex >= 0)
            indices.append(fileIndex);

        return indices;
    }

    for (int i = 0; i < item->childCount(); ++i) {
        indices.append(fileIndicesForItem(item->child(i)));
    }

    return indices;
}

QList<int> MainWindow::selectedFileIndicesForContextItem(QTreeWidgetItem *item) const
{
    if (!item)
        return {};

    /*
     * If the user right-clicks one of several selected items, operate on
     * all selected items. Otherwise operate on the clicked item only.
     */
    QList<QTreeWidgetItem *> items;

    if (item->isSelected()) {
        items = ui->fileTreeWidget->selectedItems();
    } else {
        items.append(item);
    }

    QSet<int> uniqueIndices;

    for (QTreeWidgetItem *selectedItem : std::as_const(items)) {
        const QList<int> indices = fileIndicesForItem(selectedItem);

        for (int index : indices)
            uniqueIndices.insert(index);
    }

    QList<int> result = uniqueIndices.values();
    std::sort(result.begin(), result.end());

    return result;
}

void MainWindow::copyTextToClipboard(const QString &text,
                                      const QString &statusMessage)
{
    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty())
        return;

    QApplication::clipboard()->setText(trimmed);

    if (!statusMessage.isEmpty())
        statusBar()->showMessage(statusMessage, 3000);
}

void MainWindow::copySelectedTorrentMagnetLink()
{
    copyTextToClipboard(
        currentTorrentMagnetLink,
        tr("Magnet link copied to clipboard.")
        );
}

void MainWindow::copySelectedTorrentHash()
{
    copyTextToClipboard(
        currentTorrentHashString,
        tr("Torrent hash copied to clipboard.")
        );
}


QString MainWindow::torrentPathForFileTreeItem(QTreeWidgetItem *item) const
{
    if (!item)
        return QString();

    const QString kind =
        item->data(FileNameColumn, FileKindRole).toString();

    if (kind == QStringLiteral("file")) {
        bool ok = false;
        const int fileIndex =
            item->data(FileNameColumn, FileIndexRole).toInt(&ok);

        if (ok && fileIndex >= 0)
            return currentTorrentFilePaths.value(fileIndex).trimmed();
    }

    QStringList parts;

    for (QTreeWidgetItem *current = item;
         current;
         current = current->parent()) {
        parts.prepend(current->text(FileNameColumn));
    }

    return parts.join(QLatin1Char('/')).trimmed();
}

void MainWindow::renameFileTreeItem(QTreeWidgetItem *item)
{
    const int torrentId = currentTorrentId();

    if (torrentId < 0 || !item)
        return;

    const QString oldPath = torrentPathForFileTreeItem(item);

    if (oldPath.isEmpty())
        return;

    const QString oldName = item->text(FileNameColumn).trimmed();

    bool ok = false;
    const QString newName = QInputDialog::getText(
        this,
        tr("Rename Path"),
        tr("New name:"),
        QLineEdit::Normal,
        oldName,
        &ok
        ).trimmed();

    if (!ok)
        return;

    if (newName.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Rename Path"),
            tr("The new name cannot be empty.")
            );
        return;
    }

    if (newName.contains(QLatin1Char('/'))
        || newName.contains(QLatin1Char('\\'))) {
        QMessageBox::warning(
            this,
            tr("Rename Path"),
            tr("Enter a file or folder name, not a path.")
            );
        return;
    }

    if (newName == oldName)
        return;

    client->renameTorrentPath(torrentId, oldPath, newName);

    statusBar()->showMessage(
        tr("Renaming %1...").arg(oldName),
        3000
        );
}

int MainWindow::trackerIdForRow(int row) const
{
    if (row < 0)
        return -1;

    QTableWidgetItem *announceItem =
        ui->trackerTableWidget->item(row, 1);

    if (!announceItem)
        return -1;

    bool ok = false;
    const int trackerId = announceItem->data(TrackerIdRole).toInt(&ok);
    return ok ? trackerId : -1;
}

QString MainWindow::trackerAnnounceUrlForRow(int row) const
{
    if (row < 0)
        return QString();

    QTableWidgetItem *announceItem =
        ui->trackerTableWidget->item(row, 1);

    if (!announceItem)
        return QString();

    QString trackerUrl =
        announceItem->data(TrackerAnnounceRole).toString().trimmed();

    if (trackerUrl.isEmpty())
        trackerUrl = announceItem->text().trimmed();

    return trackerUrl;
}

void MainWindow::addTrackerFromContextMenu()
{
    const int torrentId = currentTorrentId();

    if (torrentId < 0)
        return;

    bool ok = false;
    const QString trackerUrl = QInputDialog::getText(
        this,
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
            this,
            tr("Add Tracker"),
            tr("The tracker announce URL cannot be empty.")
            );
        return;
    }

    client->addTorrentTracker(torrentId, trackerUrl);

    statusBar()->showMessage(
        tr("Adding tracker..."),
        3000
        );
}

void MainWindow::editTrackerFromContextMenu(int row)
{
    const int torrentId = currentTorrentId();
    const int trackerId = trackerIdForRow(row);
    const QString oldTrackerUrl = trackerAnnounceUrlForRow(row);

    if (torrentId < 0 || trackerId < 0 || oldTrackerUrl.isEmpty())
        return;

    bool ok = false;
    const QString trackerUrl = QInputDialog::getText(
        this,
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
            this,
            tr("Edit Tracker"),
            tr("The tracker announce URL cannot be empty.")
            );
        return;
    }

    if (trackerUrl == oldTrackerUrl)
        return;

    client->editTorrentTracker(torrentId, trackerId, trackerUrl);

    statusBar()->showMessage(
        tr("Updating tracker..."),
        3000
        );
}

void MainWindow::removeTrackerFromContextMenu(int row)
{
    const int torrentId = currentTorrentId();
    const int trackerId = trackerIdForRow(row);
    const QString trackerUrl = trackerAnnounceUrlForRow(row);

    if (torrentId < 0 || trackerId < 0 || trackerUrl.isEmpty())
        return;

    const QMessageBox::StandardButton result = QMessageBox::question(
        this,
        tr("Remove Tracker"),
        tr("Remove this tracker from the torrent?\n\n%1").arg(trackerUrl),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
        );

    if (result != QMessageBox::Yes)
        return;

    client->removeTorrentTracker(torrentId, trackerId);

    statusBar()->showMessage(
        tr("Removing tracker..."),
        3000
        );
}

void MainWindow::showTrackerContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = ui->trackerTableWidget->itemAt(pos);
    const int row = item ? item->row() : -1;

    if (item)
        ui->trackerTableWidget->selectRow(row);

    const QString trackerUrl = trackerAnnounceUrlForRow(row);
    const int trackerId = trackerIdForRow(row);

    QMenu menu(this);

    QAction *addTrackerAction =
        menu.addAction(tr("Add Tracker…"));

    QAction *editTrackerAction =
        menu.addAction(tr("Edit Tracker…"));
    editTrackerAction->setEnabled(row >= 0 && trackerId >= 0);

    QAction *removeTrackerAction =
        menu.addAction(tr("Remove Tracker"));
    removeTrackerAction->setEnabled(row >= 0 && trackerId >= 0);

    menu.addSeparator();

    QAction *copyTrackerUrlAction =
        menu.addAction(tr("Copy Tracker URL"));
    copyTrackerUrlAction->setEnabled(!trackerUrl.isEmpty());

    connect(addTrackerAction, &QAction::triggered,
            this, &MainWindow::addTrackerFromContextMenu);

    connect(editTrackerAction, &QAction::triggered,
            this, [this, row]() { editTrackerFromContextMenu(row); });

    connect(removeTrackerAction, &QAction::triggered,
            this, [this, row]() { removeTrackerFromContextMenu(row); });

    connect(copyTrackerUrlAction, &QAction::triggered,
            this, [this, trackerUrl]() {
                copyTextToClipboard(
                    trackerUrl,
                    tr("Tracker URL copied to clipboard.")
                    );
            });

    menu.exec(ui->trackerTableWidget->viewport()->mapToGlobal(pos));
}

void MainWindow::showFileContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = ui->fileTreeWidget->itemAt(pos);

    if (!item)
        return;

    if (!item->isSelected()) {
        ui->fileTreeWidget->clearSelection();
        item->setSelected(true);
        ui->fileTreeWidget->setCurrentItem(item);
    }

    const QList<int> fileIndices =
        selectedFileIndicesForContextItem(item);

    if (fileIndices.isEmpty())
        return;

    QMenu menu(this);

    QAction *openAction = menu.addAction(tr("Open"));
    openAction->setEnabled(fileIndices.size() == 1);

    connect(openAction, &QAction::triggered,
            this, [this, fileIndices]() {
                openFileFromContextMenu(fileIndices);
            });

    QAction *openContainingFolderAction =
        menu.addAction(tr("Open Containing Folder"));
    openContainingFolderAction->setEnabled(fileIndices.size() == 1);

    connect(openContainingFolderAction, &QAction::triggered,
            this, [this, fileIndices]() {
                openContainingFolderFromContextMenu(fileIndices);
            });

    QAction *renameAction = menu.addAction(tr("Rename…"));
    renameAction->setEnabled(ui->fileTreeWidget->selectedItems().size() == 1);

    connect(renameAction, &QAction::triggered,
            this, [this, item]() { renameFileTreeItem(item); });

    menu.addSeparator();

    QMenu *priorityMenu = menu.addMenu(tr("Priority"));

    QAction *skipPriorityAction =
        priorityMenu->addAction(tr("Skip"));
    QAction *lowPriorityAction =
        priorityMenu->addAction(tr("Low"));
    QAction *normalPriorityAction =
        priorityMenu->addAction(tr("Normal"));
    QAction *highPriorityAction =
        priorityMenu->addAction(tr("High"));

    connect(skipPriorityAction, &QAction::triggered,
            this, [this]() { setSelectedFilesPriorityState(0, false); });

    connect(lowPriorityAction, &QAction::triggered,
            this, [this]() { setSelectedFilesPriorityState(-1, true); });

    connect(normalPriorityAction, &QAction::triggered,
            this, [this]() { setSelectedFilesPriorityState(0, true); });

    connect(highPriorityAction, &QAction::triggered,
            this, [this]() { setSelectedFilesPriorityState(1, true); });

    menu.exec(ui->fileTreeWidget->viewport()->mapToGlobal(pos));
}

QList<FolderMapping> MainWindow::currentServerFolderMappings() const
{
    QList<FolderMapping> mappings;

    const int serverIndex = ui->comboServers->currentData().toInt();

    if (serverIndex < 0)
        return mappings;

    QSettings settings;
    const int serverCount = settings.beginReadArray(QStringLiteral("servers"));

    if (serverIndex >= serverCount) {
        settings.endArray();
        return mappings;
    }

    settings.setArrayIndex(serverIndex);

    const int mappingCount =
        settings.beginReadArray(QStringLiteral("folderMappings"));

    for (int i = 0; i < mappingCount; ++i) {
        settings.setArrayIndex(i);

        FolderMapping mapping;
        mapping.remotePath =
            settings.value(QStringLiteral("remotePath")).toString().trimmed();
        mapping.localPath =
            settings.value(QStringLiteral("localPath")).toString().trimmed();

        if (!mapping.remotePath.isEmpty() && !mapping.localPath.isEmpty())
            mappings.append(mapping);
    }

    settings.endArray(); // folderMappings
    settings.endArray(); // servers

    return mappings;
}

QString MainWindow::mapRemotePathToLocalPath(
    const QString &remotePath,
    const QList<FolderMapping> &mappings) const
{
    const QString cleanRemotePath =
        QDir::cleanPath(remotePath.trimmed());

    if (cleanRemotePath.isEmpty())
        return {};

    const FolderMapping *bestMatch = nullptr;
    QString bestRemotePrefix;

    for (const FolderMapping &mapping : mappings) {
        const QString remotePrefix =
            QDir::cleanPath(mapping.remotePath.trimmed());

        if (remotePrefix.isEmpty())
            continue;

        const bool exactMatch = cleanRemotePath == remotePrefix;
        const bool childMatch =
            cleanRemotePath.startsWith(remotePrefix + QLatin1Char('/'));

        if (!exactMatch && !childMatch)
            continue;

        if (!bestMatch || remotePrefix.length() > bestRemotePrefix.length()) {
            bestMatch = &mapping;
            bestRemotePrefix = remotePrefix;
        }
    }

    if (!bestMatch)
        return {};

    const QString localPrefix =
        QDir::cleanPath(bestMatch->localPath.trimmed());

    QString suffix = cleanRemotePath.mid(bestRemotePrefix.length());

    if (suffix.startsWith(QLatin1Char('/')))
        suffix.remove(0, 1);

    if (suffix.isEmpty())
        return localPrefix;

    return QDir(localPrefix).filePath(suffix);
}

bool MainWindow::resolveMappedLocalPathForSingleFile(
    const QList<int> &fileIndices,
    const QString &dialogTitle,
    QString *localPath,
    QString *remotePath,
    bool requireFileExists)
{
    if (localPath)
        localPath->clear();

    if (remotePath)
        remotePath->clear();

    if (fileIndices.isEmpty())
        return false;

    if (fileIndices.size() != 1) {
        QMessageBox::information(
            this,
            dialogTitle,
            tr("Please select a single file.")
            );
        return false;
    }

    const int fileIndex = fileIndices.first();
    const QString relativeFilePath =
        currentTorrentFilePaths.value(fileIndex).trimmed();

    if (relativeFilePath.isEmpty()) {
        QMessageBox::warning(
            this,
            dialogTitle,
            tr("Planetary could not determine the selected file path.")
            );
        return false;
    }

    if (currentTorrentDownloadDir.trimmed().isEmpty()) {
        QMessageBox::warning(
            this,
            dialogTitle,
            tr("Planetary could not determine the torrent download directory.")
            );
        return false;
    }

    const QString resolvedRemotePath =
        QDir::cleanPath(
            currentTorrentDownloadDir + QLatin1Char('/') + relativeFilePath
            );

    const QString resolvedLocalPath =
        mapRemotePathToLocalPath(resolvedRemotePath, currentServerFolderMappings());

    if (remotePath)
        *remotePath = resolvedRemotePath;

    if (resolvedLocalPath.isEmpty()) {
        QMessageBox::information(
            this,
            tr("No Folder Mapping"),
            tr("Planetary could not map this remote file path to a local file path.\n\n"
               "Remote path:\n%1")
                .arg(resolvedRemotePath)
            );
        return false;
    }

    if (requireFileExists && !QFileInfo::exists(resolvedLocalPath)) {
        QMessageBox::warning(
            this,
            tr("File Not Found"),
            tr("The mapped local file does not exist.\n\n"
               "Remote path:\n%1\n\n"
               "Local path:\n%2")
                .arg(resolvedRemotePath, resolvedLocalPath)
            );
        return false;
    }

    if (localPath)
        *localPath = resolvedLocalPath;

    return true;
}

void MainWindow::openFileFromContextMenu(const QList<int> &fileIndices)
{
    QString localPath;

    if (!resolveMappedLocalPathForSingleFile(
            fileIndices,
            tr("Open File"),
            &localPath,
            nullptr,
            true)) {
        return;
    }

    const bool opened =
        QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));

    if (!opened) {
        QMessageBox::warning(
            this,
            tr("Open File Failed"),
            tr("The operating system could not open this file:\n\n%1")
                .arg(localPath)
            );
        return;
    }

    statusBar()->showMessage(
        tr("Opening %1").arg(QFileInfo(localPath).fileName()),
        3000
        );
}

void MainWindow::openContainingFolderFromContextMenu(const QList<int> &fileIndices)
{
    QString localPath;
    QString remotePath;

    if (!resolveMappedLocalPathForSingleFile(
            fileIndices,
            tr("Open Containing Folder"),
            &localPath,
            &remotePath,
            false)) {
        return;
    }

    const QFileInfo localInfo(localPath);
    const QString folderPath = localInfo.isDir()
        ? localInfo.absoluteFilePath()
        : localInfo.absolutePath();

    if (folderPath.isEmpty() || !QFileInfo::exists(folderPath)) {
        QMessageBox::warning(
            this,
            tr("Folder Not Found"),
            tr("The mapped containing folder does not exist.\n\n"
               "Remote path:\n%1\n\n"
               "Local folder:\n%2")
                .arg(remotePath, folderPath)
            );
        return;
    }

    const bool opened =
        QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));

    if (!opened) {
        QMessageBox::warning(
            this,
            tr("Open Folder Failed"),
            tr("The operating system could not open this folder:\n\n%1")
                .arg(folderPath)
            );
        return;
    }

    statusBar()->showMessage(
        tr("Opening folder %1").arg(QDir::toNativeSeparators(folderPath)),
        3000
        );
}

void MainWindow::setSelectedFilesPriorityState(int priority, bool wanted)
{
    const int torrentId = currentTorrentId();

    if (torrentId < 0)
        return;

    QTreeWidgetItem *item = ui->fileTreeWidget->currentItem();

    if (!item)
        return;

    const QList<int> fileIndices =
        selectedFileIndicesForContextItem(item);

    if (fileIndices.isEmpty())
        return;

    client->setTorrentFilesWantedAndPriority(torrentId, fileIndices, wanted, priority);

    const QString priorityText = wanted ? priorityToString(priority) : tr("Skip");

    statusBar()->showMessage(
        tr("Setting %1 file(s) to %2...")
            .arg(fileIndices.size())
            .arg(priorityText),
        3000
        );

    client->getTorrentDetails(torrentId);
}

void MainWindow::queueMoveSelectedTop()
{
    invokeSelectedTorrentCommand(&rpc_client::queueMoveTop, QString());
}

void MainWindow::queueMoveSelectedUp()
{
    invokeSelectedTorrentCommand(&rpc_client::queueMoveUp, QString());
}

void MainWindow::queueMoveSelectedDown()
{
    invokeSelectedTorrentCommand(&rpc_client::queueMoveDown, QString());
}

void MainWindow::queueMoveSelectedBottom()
{
    invokeSelectedTorrentCommand(&rpc_client::queueMoveBottom, QString());
}

void MainWindow::showSessionSettings()
{
    openSessionSettingsWhenReceived = true;

    statusBar()->showMessage(
        tr("Loading Transmission session settings..."),
        3000
        );

    client->getSessionSettings();
}

void MainWindow::handleSessionSettingsReceived(const QJsonObject &sessionSettings)
{
    remoteDownloadDir =
        sessionSettings.value(QStringLiteral("download-dir")).toString();

    if (torrentAddController)
        torrentAddController->setDefaultDownloadDir(remoteDownloadDir);

    if (!remoteDownloadDir.isEmpty())
        client->getFreeSpace(remoteDownloadDir);

    if (!openSessionSettingsWhenReceived)
        return;

    openSessionSettingsWhenReceived = false;

    SessionSettingsDialog dialog(this);
    dialog.setSessionSettings(sessionSettings);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QJsonObject changes = dialog.changedSettings();

    if (changes.isEmpty()) {
        statusBar()->showMessage(
            tr("No Transmission session settings changed"),
            3000
            );
        return;
    }

    client->setSessionSettings(changes);

    if (changes.contains(QStringLiteral("download-dir"))) {
        remoteDownloadDir =
            changes.value(QStringLiteral("download-dir")).toString();

        if (torrentAddController)
            torrentAddController->setDefaultDownloadDir(remoteDownloadDir);

        if (!remoteDownloadDir.isEmpty())
            client->getFreeSpace(remoteDownloadDir);
    }

    statusBar()->showMessage(
        tr("Saving Transmission session settings..."),
        3000
        );
}

void MainWindow::updateConnectionStatus(int torrentCount)
{
    lastTorrentCount = torrentCount;

    QString text = tr("Connected: %1 · %2 torrent(s)")
                       .arg(client->getServer(),
                            QString::number(torrentCount));

    if (remoteFreeSpaceBytes >= 0) {
        text += tr(" · Free: %1").arg(
            QLocale().formattedDataSize(
                remoteFreeSpaceBytes,
                1,
                QLocale::DataSizeTraditionalFormat
                )
            );
    }

    if (connectionStatusLabel) {
        connectionStatusLabel->setStyleSheet(QString());
        connectionStatusLabel->setText(text);
    }
}

void MainWindow::forceStartSelectedTorrents()
{
    invokeSelectedTorrentCommand(&rpc_client::startTorrentsNow,
                                 tr("Force starting %1 torrent(s)..."));
}

void MainWindow::rebuildTorrentFilterList(const QVector<torrent> &torrents)
{
    if (!ui->listWidget)
        return;

    const QString currentValue =
        ui->listWidget->currentItem()
            ? ui->listWidget->currentItem()->data(FilterValueRole).toString()
            : QString();

    const int currentType =
        ui->listWidget->currentItem()
            ? ui->listWidget->currentItem()->data(FilterTypeRole).toInt()
            : FilterTypeStatus;

    QSet<QString> uniqueTrackers;

    for (const torrent &torrentItem : torrents) {
        for (const QString &trackerHost : torrentItem.getTrackerHosts()) {
            if (!trackerHost.trimmed().isEmpty())
                uniqueTrackers.insert(trackerHost.trimmed().toLower());
        }
    }

    QStringList trackerHosts = uniqueTrackers.values();
    std::sort(trackerHosts.begin(), trackerHosts.end());

    if (trackerHosts == lastTrackerFilterHosts && ui->listWidget->count() > 0)
        return;

    lastTrackerFilterHosts = trackerHosts;

    QSignalBlocker blocker(ui->listWidget);

    ui->listWidget->clear();

    addStatusFilterItems();
    addTrackerFilterItems(trackerHosts);

    for (int row = 0; row < ui->listWidget->count(); ++row) {
        QListWidgetItem *item = ui->listWidget->item(row);

        if (!item)
            continue;

        const int type = item->data(FilterTypeRole).toInt();
        const QString value = item->data(FilterValueRole).toString();

        if (type == currentType && value == currentValue) {
            ui->listWidget->setCurrentRow(row);
            return;
        }
    }

    ui->listWidget->setCurrentRow(1);
}

void MainWindow::addStatusFilterItems()
{
    auto addStatusItem =
        [this](const QString &label, TorrentSortProxyModel::StateFilter filter) {
            auto *item = new QListWidgetItem(label);
            item->setData(FilterTypeRole, FilterTypeStatus);
            item->setData(FilterValueRole, static_cast<int>(filter));
            ui->listWidget->addItem(item);
        };

    auto *statusHeader = new QListWidgetItem(QStringLiteral("Status"));
    statusHeader->setFlags(Qt::NoItemFlags);
    QFont headerFont = statusHeader->font();
    headerFont.setBold(true);
    statusHeader->setFont(headerFont);
    ui->listWidget->addItem(statusHeader);

    addStatusItem(tr("All"), TorrentSortProxyModel::StateFilter::All);
    addStatusItem(tr("Downloading"), TorrentSortProxyModel::StateFilter::Downloading);
    addStatusItem(tr("Complete"), TorrentSortProxyModel::StateFilter::Completed);
    addStatusItem(tr("Active"), TorrentSortProxyModel::StateFilter::Active);
    addStatusItem(tr("Inactive"), TorrentSortProxyModel::StateFilter::Inactive);
    addStatusItem(tr("Stopped"), TorrentSortProxyModel::StateFilter::Stopped);
    addStatusItem(tr("Error"), TorrentSortProxyModel::StateFilter::Error);
}

void MainWindow::addTrackerFilterItems(const QStringList &trackerHosts)
{
    if (trackerHosts.isEmpty())
        return;

    auto *trackerHeader = new QListWidgetItem(tr("Trackers"));
    trackerHeader->setFlags(Qt::NoItemFlags);
    QFont headerFont = trackerHeader->font();
    headerFont.setBold(true);
    trackerHeader->setFont(headerFont);
    ui->listWidget->addItem(trackerHeader);

    auto *allTrackersItem = new QListWidgetItem(tr("All Trackers"));
    allTrackersItem->setData(FilterTypeRole, FilterTypeTracker);
    allTrackersItem->setData(FilterValueRole, QString());
    ui->listWidget->addItem(allTrackersItem);

    for (const QString &trackerHost : trackerHosts) {
        auto *item = new QListWidgetItem(trackerHost);
        item->setData(FilterTypeRole, FilterTypeTracker);
        item->setData(FilterValueRole, trackerHost);
        ui->listWidget->addItem(item);
    }
}

void MainWindow::setupWatchFolderManager()
{
    watchFolderManager = new WatchFolderManager(this);

    connect(watchFolderManager, &WatchFolderManager::torrentFileReady,
            torrentAddController, &TorrentAddController::addTorrentFileUsingDefaults);

    connect(watchFolderManager, &WatchFolderManager::statusMessage,
            this, [this](const QString &message) {
                statusBar()->showMessage(message, 3000);
            });

    connect(watchFolderManager, &WatchFolderManager::warningMessage,
            this, [this](const QString &message) {
                statusBar()->showMessage(message, 5000);
            });

    connect(watchFolderManager, &WatchFolderManager::torrentFileReady,
            this, [this](const QString &) {
                /*
                 * The controller emits addStarted too, but keeping this here
                 * makes the watch folder behavior obvious and resilient.
                 */
                QTimer::singleShot(1000, this, [this]() {
                    client->getTorrentList();
                });
            });
}

void MainWindow::loadWatchFolderSettings()
{
    if (!watchFolderManager)
        return;

    QSettings settings;

    const bool enabled =
        settings.value(QString::fromLatin1(SettingsKeys::WatchFolderEnabled),
                       false).toBool();

    const QString folderPath =
        settings.value(QString::fromLatin1(SettingsKeys::WatchFolderPath))
            .toString();

    const int scanIntervalMs =
        settings.value(QString::fromLatin1(SettingsKeys::WatchFolderScanIntervalMs),
                       1000).toInt();

    const int stableChecks =
        settings.value(QString::fromLatin1(SettingsKeys::WatchFolderStableChecks),
                       2).toInt();

    watchFolderManager->setScanIntervalMs(scanIntervalMs);
    watchFolderManager->setRequiredStableChecks(stableChecks);
    watchFolderManager->setWatchFolder(folderPath);
    watchFolderManager->setEnabled(enabled);

    qDebug() << "Watch folder settings:"
             << "enabled=" << enabled
             << "path=" << folderPath
             << "stableChecks=" << stableChecks;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)

    if (event->type() == QEvent::FileOpen) {
        auto *fileOpenEvent = static_cast<QFileOpenEvent *>(event);

        QStringList arguments;

        const QUrl url = fileOpenEvent->url();

        if (url.isValid() && !url.isEmpty()) {
            if (url.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) == 0) {
                const QString localFile = url.toLocalFile();

                if (!localFile.isEmpty())
                    arguments.append(localFile);
            } else {
                arguments.append(url.toString());
            }
        }

        if (arguments.isEmpty()) {
            const QString filePath = fileOpenEvent->file();

            if (!filePath.isEmpty())
                arguments.append(filePath);
        }

        if (!arguments.isEmpty()) {
            /*
             * Defer opening the add dialog until after the current macOS
             * open-file event unwinds. Qt modal dialogs during platform event
             * delivery are how we get tiny crash goblins.
             */
            QTimer::singleShot(0, this, [this, arguments]() {
                handleLaunchArguments(arguments);
            });

            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupUpdateChecker()
{
    updateChecker = new UpdateChecker(this);
    updateChecker->setCurrentVersion(QStringLiteral(PLANETARY_VERSION_STRING));
    updateChecker->setRepository(QStringLiteral("mveinot"),
                                 QStringLiteral("transmission-control"));

    connect(updateChecker, &UpdateChecker::updateAvailable,
            this,
            [this](const QString &currentVersion,
                   const QString &latestVersion,
                   const QUrl &releaseUrl,
                   bool userInitiated) {
                Q_UNUSED(userInitiated)

                QMessageBox messageBox(this);
                messageBox.setIcon(QMessageBox::Information);
                messageBox.setWindowTitle(tr("Update Available"));
                messageBox.setText(tr("A newer version of Planetary is available."));
                messageBox.setInformativeText(
                    tr("Installed version: %1\nLatest version: %2")
                        .arg(displayVersion(currentVersion),
                             displayVersion(latestVersion))
                    );

                QPushButton *openButton =
                    messageBox.addButton(tr("Open Release Page"),
                                         QMessageBox::AcceptRole);

                messageBox.addButton(QMessageBox::Cancel);

                messageBox.exec();

                if (messageBox.clickedButton() == openButton)
                    QDesktopServices::openUrl(releaseUrl);
            });

    connect(updateChecker, &UpdateChecker::noUpdateAvailable,
            this,
            [this](const QString &currentVersion,
                   const QString &latestVersion,
                   const QUrl &releaseUrl,
                   bool userInitiated) {
                if (!userInitiated)
                    return;

                QMessageBox messageBox(this);
                messageBox.setIcon(QMessageBox::Information);
                messageBox.setWindowTitle(tr("Planetary Is Up to Date"));
                messageBox.setText(tr("You are running the latest available version of Planetary."));
                messageBox.setInformativeText(
                    tr("Installed version: %1\nLatest version: %2")
                        .arg(displayVersion(currentVersion),
                             displayVersion(latestVersion))
                    );

                QPushButton *openButton =
                    messageBox.addButton(tr("Open Release Page"),
                                         QMessageBox::ActionRole);

                messageBox.addButton(QMessageBox::Ok);

                messageBox.exec();

                if (messageBox.clickedButton() == openButton)
                    QDesktopServices::openUrl(releaseUrl);
            });

    connect(updateChecker, &UpdateChecker::updateCheckFailed,
            this,
            [this](const QString &message, bool userInitiated) {
                if (userInitiated) {
                    QMessageBox::warning(
                        this,
                        tr("Update Check Failed"),
                        tr("Planetary could not check for updates.\n\n%1").arg(message)
                        );
                    return;
                }

                statusBar()->showMessage(
                    tr("Update check failed: %1").arg(message),
                    5000
                    );
            });
}

void MainWindow::maybeCheckForUpdates()
{
    QSettings settings;

    const bool enabled =
        settings.value(QStringLiteral("updates/checkAutomatically"), true).toBool();

    if (!enabled)
        return;

    const QDateTime lastCheck =
        settings.value(QStringLiteral("updates/lastCheck")).toDateTime();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (lastCheck.isValid() && lastCheck.secsTo(now) < 24 * 60 * 60)
        return;

    settings.setValue(QStringLiteral("updates/lastCheck"), now);

    updateChecker->checkForUpdates(false);
}

void MainWindow::exportSettings()
{
    const QString filePath =
        QFileDialog::getSaveFileName(
            this,
            tr("Export Settings"),
            QStringLiteral("planetary-settings.json"),
            tr("JSON Files (*.json);;All Files (*)")
            );

    if (filePath.isEmpty())
        return;

    if (SettingsImportExport::exportSettings(this, filePath)) {
        QMessageBox::information(
            this,
            tr("Settings Exported"),
            tr("Planetary settings were exported successfully.")
            );
    }
}

void MainWindow::importSettings()
{
    const QString filePath =
        QFileDialog::getOpenFileName(
            this,
            tr("Import Settings"),
            QString(),
            tr("JSON Files (*.json);;All Files (*)")
            );

    if (filePath.isEmpty())
        return;

    if (SettingsImportExport::importSettings(this, filePath)) {
        QMessageBox::information(
            this,
            tr("Settings Imported"),
            tr("Planetary settings were imported successfully.\n\nRestart Planetary for all changes to take effect.")
            );
    }
}