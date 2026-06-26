#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "piecemapcontroller.h"
#include "torrentdetailstabcontroller.h"
#include "torrentfilescontroller.h"
#include "torrentpeerscontroller.h"
#include "torrenttrackerscontroller.h"
#include "torrentlistcontroller.h"
#include "watchfoldercontroller.h"
#include "watchfoldermanager.h"
#include <QActionGroup>
#include <QApplication>
#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QFont>
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
#include "updatechecker.h"
#include "settingsimportexport.h"
#include "foldermapping.h"
#include "version.h"

namespace {
constexpr int DefaultUpdateIntervalSeconds = 10;
constexpr int MinimumUpdateIntervalSeconds = 1;
constexpr int MaximumUpdateIntervalSeconds = 3600;
constexpr int FilterTypeRole = Qt::UserRole;
constexpr int FilterValueRole = Qt::UserRole + 1;
constexpr int FilterTypeStatus = 0;
constexpr int FilterTypeTracker = 1;
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

    if (pieceMapController)
        pieceMapController->clear();

    if (torrentDetailsController)
        torrentDetailsController->clear();

    if (torrentFilesController) {
        torrentFilesController->setTorrentContext(-1, QString());
        torrentFilesController->clear();
    }

    if (torrentTrackersController) {
        torrentTrackersController->setTorrentId(-1);
        torrentTrackersController->clear();
    }

    if (torrentPeersController)
        torrentPeersController->clear();

    currentTorrentDetailsCache = QJsonObject();
    currentDetailsTorrentId = -1;
    currentTorrentHashString.clear();
    currentTorrentMagnetLink.clear();

    if (torrentListController)
        torrentListController->clearCurrentTorrentDetails();
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
    pieceMapController = new PieceMapController(ui->general,
                                                ui->verticalLayoutGeneral,
                                                ui->groupGeneralInfo,
                                                this);
    torrentDetailsController = new TorrentDetailsTabController(ui->tabWidget,
                                                              ui->general,
                                                              this);
    MainWindow::setWindowTitle(QCoreApplication::applicationName());
    setWindowIcon(QIcon(":/icons/planetary-512px.png"));

    // Initialization methods
    loadServerCombo();
    setupUpdateChecker();
    maybeCheckForUpdates();
    setupConnectionStatusIndicator();
    watchFolderManager = new WatchFolderManager(this);
    watchFolderController = new WatchFolderController(
        watchFolderManager,
        torrentAddController,
        client,
        this
        );
    watchFolderController->setup();

    connect(watchFolderController, &WatchFolderController::statusMessageRequested,
            this, [this](const QString &message, int timeoutMs) {
                statusBar()->showMessage(message, timeoutMs);
            });

    connect(watchFolderController, &WatchFolderController::torrentListRefreshRequested,
            client, &rpc_client::getTorrentList);

    watchFolderController->loadSettings();
    setupTrayIcon();

    // UI setup
    this->mainMenu = new QMenu(0);
    this->menuBar()->addMenu(this->mainMenu);
    this->aboutAction = new QAction(0);
    this->aboutAction->setMenuRole(QAction::AboutRole);
    this->mainMenu->addAction(this->aboutAction);
    this->setMenuBar(this->menuBar());

    torrentFilesController = new TorrentFilesController(
        ui->fileTreeWidget,
        client,
        this,
        this
        );
    torrentFilesController->setFolderMappingsProvider(
        [this]() { return currentServerFolderMappings(); }
        );
    torrentFilesController->setup();

    connect(torrentFilesController, &TorrentFilesController::statusMessageRequested,
            this, [this](const QString &message, int timeoutMs) {
                statusBar()->showMessage(message, timeoutMs);
            });

    connect(torrentFilesController,
            &TorrentFilesController::torrentDetailsRefreshRequested,
            client,
            &rpc_client::getTorrentDetails);

    torrentTrackersController = new TorrentTrackersController(
        ui->trackerTableWidget,
        client,
        this,
        this
        );
    torrentTrackersController->setup();

    connect(torrentTrackersController,
            &TorrentTrackersController::statusMessageRequested,
            this, [this](const QString &message, int timeoutMs) {
                statusBar()->showMessage(message, timeoutMs);
            });

    torrentPeersController = new TorrentPeersController(
        ui->peerTableWidget,
        geoIpService,
        this
        );
    torrentPeersController->setup();

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

    torrentListController = new TorrentListController(
        ui->tableView,
        proxy,
        client,
        this,
        this
        );

    torrentListController->setup({
        ui->actionStart_Torrent,
        ui->actionStop_Torrent,
        ui->actionDelete_Torrent,
        ui->actionVerify_Torrent,
        ui->actionReannounce,
        ui->actionForce_Start_Torrent
        });

    torrentListController->setCurrentDetailsDownloadDirProvider(
        [this]() { return ui->labelGeneralDownloadDir->text(); }
        );

    ui->tableView->setItemDelegateForColumn(
        TorrentModel::PercentDoneColumn,
        new PercentFillDelegate(
            TorrentModel::PercentDoneColumn,
            Qt::UserRole + 1,
            ui->tableView
            )
        );

    connect(torrentListController, &TorrentListController::statusMessageRequested,
            this, [this](const QString &message, int timeoutMs) {
                statusBar()->showMessage(message, timeoutMs);
            });

    connect(torrentListController, &TorrentListController::torrentSelected,
            this, [this](int torrentId) {
                clearGeneralTab();
                torrentPeersController->clear();
                torrentTrackersController->clear();
                torrentTrackersController->setTorrentId(torrentId);
                torrentFilesController->setTorrentContext(-1, QString());
                torrentFilesController->clear();
                client->getTorrentDetails(torrentId);
            });

    connect(torrentListController, &TorrentListController::torrentListRefreshRequested,
            client, &rpc_client::getTorrentList);

    connect(torrentListController, &TorrentListController::torrentDetailsRefreshRequested,
            client, &rpc_client::getTorrentDetails);

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
                torrentDetailsController->update(currentTorrentDetailsCache);
                torrentTrackersController->setTorrentId(torrentId);
                torrentTrackersController->populate(details);
                torrentFilesController->setTorrentContext(
                    torrentId,
                    details.value("downloadDir").toString()
                    );
                torrentFilesController->populate(
                    details.value("files").toArray(),
                    details.value("wanted").toArray(),
                    details.value("priorities").toArray()
                    );
                torrentPeersController->populate(details.value("peers").toArray());
            });

    connect(client, &rpc_client::torrentPiecesReceived,
            this,
            [this](int torrentId, const QJsonObject &details) {
                if (torrentId != currentDetailsTorrentId)
                    return;

                for (auto it = details.constBegin(); it != details.constEnd(); ++it)
                    currentTorrentDetailsCache.insert(it.key(), it.value());

                pieceMapController->update(currentTorrentDetailsCache);
                torrentDetailsController->update(currentTorrentDetailsCache);
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
    if (torrentListController)
        torrentListController->handleTableClicked(proxyIndex);
}

void MainWindow::on_actionStart_Torrent_triggered()
{
    if (torrentListController)
        torrentListController->startSelectedTorrents();
}

void MainWindow::on_actionStop_Torrent_triggered()
{
    if (torrentListController)
        torrentListController->stopSelectedTorrents();
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
    if (torrentListController)
        torrentListController->reannounceSelectedTorrents();
}

void MainWindow::on_actionVerify_Torrent_triggered()
{
    if (torrentListController)
        torrentListController->verifySelectedTorrents();
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
    if (torrentListController)
        torrentListController->deleteSelectedTorrents();
}

void MainWindow::on_actionSettings_triggered()
{
    AppSettings dialog(this);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    applyAppSettings();
    if (watchFolderController)
        watchFolderController->loadSettings();
}

int MainWindow::currentTorrentId() const
{
    return torrentListController ? torrentListController->currentTorrentId() : -1;
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
    const QStringList fileNames = QFileDialog::getOpenFileNames(
        this,
        tr("Add Torrent Files"),
        QString(),
        tr("Torrent Files (*.torrent);;All Files (*)")
        );

    if (fileNames.isEmpty())
        return;

    torrentAddController->addTorrentFiles(fileNames);
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

    torrentFilesController->setTorrentContext(-1, QString());
    torrentFilesController->clear();
    torrentTrackersController->setTorrentId(-1);
    torrentTrackersController->clear();
    torrentPeersController->clear();
    if (torrentListController)
        torrentListController->updateActionState();

    statusBar()->showMessage(
        tr("Selected server: %1").arg(client->getServer()),
        3000
        );

    client->getTorrentList();

    remoteDownloadDir.clear();

    if (torrentListController) {
        torrentListController->clearCurrentTorrentDetails();
        torrentListController->setDefaultDownloadDir(remoteDownloadDir);
    }

    remoteFreeSpaceBytes = -1;
    lastTorrentCount = 0;
    updateConnectionStatus(0);

    client->getSessionSettings();
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

bool MainWindow::currentTabWantsLiveTorrentDetails() const
{
    QWidget *current = ui->tabWidget->currentWidget();
    return current == ui->general
           || (torrentDetailsController && current == torrentDetailsController->widget());
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

    currentDetailsTorrentId = details.value("id").toInt(-1);
    currentTorrentHashString = hashString;
    currentTorrentMagnetLink = magnetLink;

    if (torrentListController) {
        torrentListController->setCurrentTorrentDetails(
            currentDetailsTorrentId,
            currentTorrentHashString,
            currentTorrentMagnetLink
            );
    }

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

    if (pieceMapController)
        pieceMapController->update(details);
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

    if (torrentListController)
        torrentListController->setDefaultDownloadDir(remoteDownloadDir);

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

        if (torrentListController)
            torrentListController->setDefaultDownloadDir(remoteDownloadDir);

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
