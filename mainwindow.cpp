#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "torrentgeneralcontroller.h"
#include "torrentfilescontroller.h"
#include "torrentpeerscontroller.h"
#include "torrenttrackerscontroller.h"
#include "torrentlistcontroller.h"
#include "torrentfiltercontroller.h"
#include "watchfoldercontroller.h"
#include "watchfoldermanager.h"
#include "updatecheckcontroller.h"
#include "traycontroller.h"
#include "statusbarcontroller.h"
#include "notificationcontroller.h"
#include <QActionGroup>
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QComboBox>
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
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>
#include <initializer_list>

#include "rpc_client.h"
#include "dialogabout.h"
#include "serverconfig.h"
#include "appsettings.h"
#include "torrentsortproxymodel.h"
#include "percentfilldelegate.h"
#include "sessionsettingsdialog.h"
#include "settingskeys.h"
#include "torrentaddcontroller.h"
#include "settingsimportexport.h"
#include "foldermapping.h"

namespace {
constexpr int DefaultUpdateIntervalSeconds = 10;
constexpr int MinimumUpdateIntervalSeconds = 1;
constexpr int MaximumUpdateIntervalSeconds = 3600;

bool semverAtLeast(const QString &version, int requiredMajor, int requiredMinor, int requiredPatch)
{
    const QStringList parts = version.split(QLatin1Char('.'));

    const int major = parts.value(0).toInt();
    const int minor = parts.value(1).toInt();
    const int patch = parts.value(2).toInt();

    if (major != requiredMajor)
        return major > requiredMajor;

    if (minor != requiredMinor)
        return minor > requiredMinor;

    return patch >= requiredPatch;
}

bool sessionSupportsSequentialDownload(const QJsonObject &settings)
{
    const int rpcVersion = settings.value(QStringLiteral("rpc-version")).toInt(
        settings.value(QStringLiteral("rpc_version")).toInt()
        );

    if (rpcVersion >= 18)
        return true;

    const QString rpcVersionSemver = settings.value(QStringLiteral("rpc-version-semver")).toString(
        settings.value(QStringLiteral("rpc_version_semver")).toString()
        );

    return semverAtLeast(rpcVersionSemver, 6, 0, 0);
}

bool jsonBoolAny(const QJsonObject &object,
                 std::initializer_list<const char *> keys,
                 bool *found = nullptr)
{
    for (const char *key : keys) {
        const QJsonValue value = object.value(QString::fromLatin1(key));

        if (!value.isUndefined()) {
            if (found)
                *found = true;

            return value.toBool(false);
        }
    }

    if (found)
        *found = false;

    return false;
}
}

void MainWindow::clearGeneralTab()
{
    if (torrentGeneralController)
        torrentGeneralController->clear();

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

    if (!geoIpService->loadDefaultDatabase()) {
        qWarning() << "GeoIP database could not be loaded; using fallback lookup";
    }

    torrentAddController = new TorrentAddController(client, this, this);

    // set up the main UI
    ui->setupUi(this);
    TorrentGeneralController::Widgets generalWidgets;
    generalWidgets.generalTab = ui->general;
    generalWidgets.generalLayout = ui->verticalLayoutGeneral;
    generalWidgets.generalInfoGroup = ui->groupGeneralInfo;
    generalWidgets.tabWidget = ui->tabWidget;
    generalWidgets.nameLabel = ui->labelGeneralName;
    generalWidgets.totalSizeLabel = ui->labelGeneralTotalSize;
    generalWidgets.creatorLabel = ui->labelGeneralCreator;
    generalWidgets.createdLabel = ui->labelGeneralCreated;
    generalWidgets.downloadDirLabel = ui->labelGeneralDownloadDir;
    generalWidgets.hashLabel = ui->labelGeneralHash;
    generalWidgets.commentLabel = ui->labelGeneralComment;
    generalWidgets.magnetLineEdit = ui->lineGeneralMagnet;

    torrentGeneralController = new TorrentGeneralController(generalWidgets, this);
    torrentGeneralController->setup();

    connect(torrentGeneralController,
            &TorrentGeneralController::currentTorrentDetailsChanged,
            this,
            [this](int torrentId, const QString &hashString, const QString &magnetLink) {
                if (torrentListController) {
                    torrentListController->setCurrentTorrentDetails(
                        torrentId,
                        hashString,
                        magnetLink
                        );
                }
            });

    connect(torrentGeneralController,
            &TorrentGeneralController::currentTorrentDetailsCleared,
            this,
            [this]() {
                if (torrentListController)
                    torrentListController->clearCurrentTorrentDetails();
            });
    MainWindow::setWindowTitle(QCoreApplication::applicationName());
    setWindowIcon(QIcon(":/icons/planetary-512px.png"));

    // Initialization methods
    loadServerCombo();
    setupConnectionStatusIndicator();
    updateCheckController = new UpdateCheckController(this, this);
    updateCheckController->setup();

    connect(updateCheckController, &UpdateCheckController::statusMessageRequested,
            this, [this](const QString &message, int timeoutMs) {
                if (statusBarController)
                    statusBarController->showMessage(message, timeoutMs);
            });

    updateCheckController->maybeCheckAutomatically();
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
                if (statusBarController)
                    statusBarController->showMessage(message, timeoutMs);
            });

    connect(watchFolderController, &WatchFolderController::torrentListRefreshRequested,
            client, &rpc_client::getTorrentList);

    watchFolderController->loadSettings();
    trayController = new TrayController(this, this);
    trayController->setup();
    trayController->setTorrentGlobalActions(ui->actionStart_All_Torrents,
                                            ui->actionStop_All_Torrents);

    notificationController = new NotificationController(this);

    connect(notificationController, &NotificationController::statusMessageRequested,
            this, [this](const QString &message, int timeoutMs) {
                if (statusBarController)
                    statusBarController->showMessage(message, timeoutMs);
            });

    // UI setup
    mainMenu = new QMenu(this);
    this->menuBar()->addMenu(this->mainMenu);
    aboutAction = new QAction(this);
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
                if (statusBarController)
                    statusBarController->showMessage(message, timeoutMs);
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
                if (statusBarController)
                    statusBarController->showMessage(message, timeoutMs);
            });

    torrentPeersController = new TorrentPeersController(
        ui->peerTableWidget,
        geoIpService,
        this
        );
    torrentPeersController->setup();

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

    TorrentFilterController::Actions filterActions;
    filterActions.all = ui->actionAll;
    filterActions.downloading = ui->actionDownloading;
    filterActions.completed = ui->actionCompleted;
    filterActions.active = ui->actionActive;
    filterActions.inactive = ui->actionInactive;
    filterActions.stopped = ui->actionStopped;
    filterActions.error = ui->actionError;

    torrentFilterController = new TorrentFilterController(
        ui->listWidget,
        ui->editTorrentFilter,
        proxy,
        filterActions,
        this
        );
    torrentFilterController->setup();

    connect(torrentFilterController,
            &TorrentFilterController::resultCountChanged,
            this,
            [this](int visibleCount, int totalCount) {
                if (statusBarController)
                    statusBarController->setTorrentResultCount(visibleCount, totalCount);
            });

    connect(torrentFilterController,
            &TorrentFilterController::filterSummaryChanged,
            this,
            [this](const QString &summary) {
                if (statusBarController)
                    statusBarController->setFilterSummary(summary);
            });

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
        [this]() { return torrentGeneralController->currentDownloadDir(); }
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
                if (statusBarController)
                    statusBarController->showMessage(message, timeoutMs);
            });

    connect(torrentListController, &TorrentListController::torrentSelected,
            this, [this](int torrentId) {
                clearGeneralTab();
                torrentPeersController->clear();
                torrentTrackersController->clear();
                torrentTrackersController->setTorrentId(torrentId);
                torrentFilesController->setTorrentContext(-1, QString());
                torrentFilesController->clear();
                torrentPeersController->setLoading();
                torrentTrackersController->setLoading();
                torrentFilesController->setLoading();
                client->getTorrentDetails(torrentId);
            });

    connect(torrentListController, &TorrentListController::torrentListRefreshRequested,
            client, &rpc_client::getTorrentList);

    connect(torrentListController, &TorrentListController::torrentDetailsRefreshRequested,
            client, &rpc_client::getTorrentDetails);

    connect(torrentAddController, &TorrentAddController::addStarted,
            this, [this]() {
                if (statusBarController) {
                    statusBarController->showMessage(
                        tr("Adding torrent..."),
                        3000
                        );
                }
            });

    connect(torrentAddController, &TorrentAddController::addFailed,
            this, [this](const QString &message) {
                if (statusBarController)
                    statusBarController->showMessage(message, 5000);
            });

    connect(torrentAddController, &TorrentAddController::addCancelled,
            this, [this]() {
                if (statusBarController)
                    statusBarController->showMessage(tr("Torrent add cancelled."), 3000);
            });


    connect(ui->actionCheckForUpdates, &QAction::triggered,
            this, [this]() {
                if (updateCheckController)
                    updateCheckController->checkNow();
            });

    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, [this](int) {
                refreshCurrentTorrentLiveDetailsIfNeeded();
            });

    connect(client, &rpc_client::serverChanged,
            torrentModel, &TorrentModel::clear);

    connect(client, &rpc_client::sessionSettingsReceived,
            this, &MainWindow::handleSessionSettingsReceived);

    connect(ui->actionTransmission_Settings, &QAction::triggered,
            this, &MainWindow::showSessionSettings);

    updateAlternativeSpeedAction(false, false);

    connect(ui->actionAlternative_Speed_Mode, &QAction::triggered,
            this, &MainWindow::toggleAlternativeSpeedMode);

    connect(ui->actionServer_Setup, &QAction::triggered, this, &MainWindow::onServerSetupTriggered);

    connect(timer, &QTimer::timeout, this, &MainWindow::updateTorrentList);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    connect(client, &rpc_client::torrentDetailsReceived,
            this,
            [this](int torrentId, const QJsonObject &details) {
                if (torrentId != currentTorrentId())
                    return;

                torrentGeneralController->update(details);

                if (torrentListController) {
                    bool hasSequentialDownload = false;
                    const bool sequentialDownloadEnabled = jsonBoolAny(
                        details,
                        { "sequential_download", "sequentialDownload" },
                        &hasSequentialDownload
                        );

                    torrentListController->setCurrentTorrentSequentialDownload(
                        torrentId,
                        sequentialDownloadEnabled,
                        hasSequentialDownload
                        );

                    const bool hasBandwidthPriority =
                        details.contains(QStringLiteral("bandwidthPriority"));

                    torrentListController->setCurrentTorrentBandwidthPriority(
                        torrentId,
                        details.value(QStringLiteral("bandwidthPriority")).toInt(0),
                        hasBandwidthPriority
                        );
                }

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
                torrentGeneralController->updatePieces(torrentId, details);
            });

    // Data path: required for the table to show anything
    connect(client, &rpc_client::torrentsReceived,
                torrentModel, &TorrentModel::applyUpdate);

    // Side effects: notifications/status only
    connect(client, &rpc_client::torrentsReceived,
            this, &MainWindow::handleTorrentsReceived);

    connect(client, &rpc_client::updateFailed,
            this, [this](const QString &message) {
                if (torrentListController)
                    torrentListController->markTorrentListLoadFailed(message);
            });

    connect(ui->comboServers,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) {
                saveSelectedServerFromCombo();
            });

    connect(client, &rpc_client::freeSpaceReceived,
            this, [this](const QString &, qint64 sizeBytes) {
                if (statusBarController)
                    statusBarController->setFreeSpace(sizeBytes);
            });

    connect(client, &rpc_client::commandSucceeded,
            this, [this](const QString &method) {
                if (method == QStringLiteral("session-set")) {
                    client->getSessionSettings();
                    return;
                }

                const auto refreshTorrentState = [this](bool refreshDetails) {
                    client->getTorrentList();

                    if (!refreshDetails)
                        return;

                    const int torrentId = currentTorrentId();

                    if (torrentId >= 0)
                        client->getTorrentDetails(torrentId);
                };

                if (method == QStringLiteral("torrent-set-location")) {
                    if (statusBarController) {
                        statusBarController->showMessage(
                            tr("Torrent location updated."),
                            3000
                            );
                    }

                    refreshTorrentState(true);
                    return;
                }

                if (method == QStringLiteral("torrent-rename-path")) {
                    if (statusBarController) {
                        statusBarController->showMessage(
                            tr("Torrent path renamed."),
                            3000
                            );
                    }

                    refreshTorrentState(true);
                    return;
                }

                if (method == QStringLiteral("torrent-set")) {
                    refreshTorrentState(true);
                    return;
                }

                if (method == QStringLiteral("torrent-add")
                    || method == QStringLiteral("torrent-remove")) {
                    refreshTorrentState(false);
                    return;
                }

                if (method == QStringLiteral("torrent-start")
                    || method == QStringLiteral("torrent-start-now")
                    || method == QStringLiteral("torrent-stop")
                    || method == QStringLiteral("torrent-verify")
                    || method == QStringLiteral("torrent-reannounce")
                    || method == QStringLiteral("queue-move-top")
                    || method == QStringLiteral("queue-move-up")
                    || method == QStringLiteral("queue-move-down")
                    || method == QStringLiteral("queue-move-bottom")) {
                    refreshTorrentState(true);
                    return;
                }
            });

    connect(client, &rpc_client::commandFailed,
            this, [this](const QString &method, const QString &message) {
                if (method == QStringLiteral("session-set"))
                    updateAlternativeSpeedAction(confirmedAlternativeSpeedEnabled,
                                                 alternativeSpeedSettingsAvailable);

                if (statusBarController)
                    statusBarController->showMessage(message, 5000);
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
    if (torrentListController)
        torrentListController->beginTorrentListRefresh();

    client->getTorrentList();
}

void MainWindow::showAbout()
{
    DialogAbout dialog(geoIpService, this);
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

void MainWindow::on_actionStart_All_Torrents_triggered()
{
    if (statusBarController)
        statusBarController->showMessage(tr("Starting all torrents..."), 3000);

    client->startAllTorrents();
}

void MainWindow::on_actionStop_All_Torrents_triggered()
{
    if (statusBarController)
        statusBarController->showMessage(tr("Stopping all torrents..."), 3000);

    client->stopAllTorrents();
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
    if (torrentListController)
        torrentListController->saveViewState();

    if (torrentFilesController)
        torrentFilesController->saveViewState();

    if (torrentPeersController)
        torrentPeersController->saveViewState();

    if (torrentTrackersController)
        torrentTrackersController->saveViewState();
}

// restore header dimensions
void MainWindow::restoreTableViewState()
{
    if (torrentListController)
        torrentListController->restoreViewState();

    if (torrentFilesController)
        torrentFilesController->restoreViewState();

    if (torrentPeersController)
        torrentPeersController->restoreViewState();

    if (torrentTrackersController)
        torrentTrackersController->restoreViewState();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveTableViewState();

    if (trayController && trayController->handleCloseEvent(event))
        return;

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
            if (statusBarController)
                statusBarController->showMessage(client->getServer());
            if (torrentListController)
                torrentListController->beginTorrentListRefresh();
            client->getTorrentList();
        }
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
        if (statusBarController)
            statusBarController->showMessage(tr("Could not switch server."), 5000);
        return;
    }

    torrentFilesController->setTorrentContext(-1, QString());
    torrentFilesController->clear();
    torrentTrackersController->setTorrentId(-1);
    torrentTrackersController->clear();
    torrentPeersController->clear();
    if (torrentListController)
        torrentListController->updateActionState();

    if (statusBarController) {
        statusBarController->showMessage(
            tr("Selected server: %1").arg(client->getServer()),
            3000
            );
    }

    if (torrentListController)
        torrentListController->beginTorrentListRefresh();
    client->getTorrentList();

    remoteDownloadDir.clear();

    if (torrentListController) {
        torrentListController->clearCurrentTorrentDetails();
        torrentListController->setDefaultDownloadDir(remoteDownloadDir);
    }

    if (statusBarController) {
        statusBarController->clearFreeSpace();
        statusBarController->updateTorrents({});
    }

    client->getSessionSettings();
}

void MainWindow::showMainWindow()
{
    if (trayController) {
        trayController->showMainWindow();
        return;
    }

    show();
    setWindowState(windowState() & ~Qt::WindowMinimized);
    raise();
    activateWindow();
}

void MainWindow::quitApplication()
{
    if (trayController) {
        trayController->quitApplication();
        return;
    }

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

    if (statusBarController) {
        statusBarController->setUpdateIntervalSeconds(timer->interval() / 1000);
        statusBarController->showMessage(
            tr("Update interval: %1 seconds").arg(timer->interval() / 1000),
            3000
            );
    }
}

bool MainWindow::currentTabWantsLiveTorrentDetails() const
{
    return torrentGeneralController
           && torrentGeneralController->wantsLiveTorrentDetails(ui->tabWidget->currentWidget());
}

void MainWindow::refreshCurrentTorrentLiveDetailsIfNeeded()
{
    if (!torrentGeneralController || !currentTabWantsLiveTorrentDetails())
        return;

    const int torrentId = torrentGeneralController->currentTorrentId();

    if (torrentId < 0)
        return;

    client->getTorrentPieces(torrentId);
}

void MainWindow::setupConnectionStatusIndicator()
{
    statusBarController = new StatusBarController(ui->statusbar, client, this);
    statusBarController->setup();
    statusBarController->setServerName(client->getServer());

    connect(statusBarController, &StatusBarController::alternativeSpeedToggleRequested,
            this, [this]() {
                toggleAlternativeSpeedMode(!confirmedAlternativeSpeedEnabled);
            });

    connect(statusBarController, &StatusBarController::freeSpaceRefreshRequested,
            this, &MainWindow::refreshRemoteFreeSpace);

    connect(statusBarController, &StatusBarController::serverSetupRequested,
            this, &MainWindow::onServerSetupTriggered);

    connect(statusBarController, &StatusBarController::speedLimitsDialogRequested,
            this, &MainWindow::showQuickSpeedLimitsDialog);

    connect(statusBarController, &StatusBarController::appSettingsRequested,
            this, &MainWindow::on_actionSettings_triggered);
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

void MainWindow::applyAppSettings()
{
    applyUpdateInterval();

    if (trayController)
        trayController->applySettings();
}

void MainWindow::handleTorrentsReceived(const QVector<torrent> &torrents)
{
    if (torrentListController)
        torrentListController->markTorrentListLoaded();

    if (notificationController)
        notificationController->processTorrentList(torrents);

    if (torrentFilterController)
        torrentFilterController->rebuild(torrents);

    if (statusBarController)
        statusBarController->updateTorrents(torrents);

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

void MainWindow::updateAlternativeSpeedAction(bool enabled, bool available)
{
    if (!ui || !ui->actionAlternative_Speed_Mode)
        return;

    const QSignalBlocker blocker(ui->actionAlternative_Speed_Mode);
    ui->actionAlternative_Speed_Mode->setEnabled(available);
    ui->actionAlternative_Speed_Mode->setChecked(enabled);
}


void MainWindow::refreshRemoteFreeSpace()
{
    if (remoteDownloadDir.trimmed().isEmpty()) {
        if (statusBarController) {
            statusBarController->showMessage(
                tr("No remote download directory is available yet."),
                3000
                );
        }

        client->getSessionSettings();
        return;
    }

    if (statusBarController) {
        statusBarController->showMessage(
            tr("Refreshing free-space information..."),
            2000
            );
    }

    client->getFreeSpace(remoteDownloadDir);
}

void MainWindow::showQuickSpeedLimitsDialog()
{
    if (cachedSessionSettings.isEmpty()) {
        openQuickSpeedLimitsWhenReceived = true;

        if (statusBarController) {
            statusBarController->showMessage(
                tr("Loading Transmission speed settings..."),
                3000
                );
        }

        client->getSessionSettings();
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Speed Limits"));

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    layout->addLayout(form);

    auto *downloadLimitEnabled = new QCheckBox(tr("Limit download speed"), &dialog);
    auto *downloadLimit = new QSpinBox(&dialog);
    downloadLimit->setRange(0, 1000000);
    downloadLimit->setSuffix(tr(" KB/s"));
    downloadLimit->setValue(cachedSessionSettings.value(QStringLiteral("speed-limit-down")).toInt(100));
    downloadLimitEnabled->setChecked(
        cachedSessionSettings.value(QStringLiteral("speed-limit-down-enabled")).toBool(false)
        );
    downloadLimit->setEnabled(downloadLimitEnabled->isChecked());
    form->addRow(downloadLimitEnabled, downloadLimit);

    auto *uploadLimitEnabled = new QCheckBox(tr("Limit upload speed"), &dialog);
    auto *uploadLimit = new QSpinBox(&dialog);
    uploadLimit->setRange(0, 1000000);
    uploadLimit->setSuffix(tr(" KB/s"));
    uploadLimit->setValue(cachedSessionSettings.value(QStringLiteral("speed-limit-up")).toInt(100));
    uploadLimitEnabled->setChecked(
        cachedSessionSettings.value(QStringLiteral("speed-limit-up-enabled")).toBool(false)
        );
    uploadLimit->setEnabled(uploadLimitEnabled->isChecked());
    form->addRow(uploadLimitEnabled, uploadLimit);

    auto *altSpeedEnabled = new QCheckBox(tr("Alternative speed mode"), &dialog);
    altSpeedEnabled->setChecked(
        cachedSessionSettings.value(QStringLiteral("alt-speed-enabled")).toBool(false)
        );
    layout->addWidget(altSpeedEnabled);

    auto *altDownloadLimit = new QSpinBox(&dialog);
    altDownloadLimit->setRange(0, 1000000);
    altDownloadLimit->setSuffix(tr(" KB/s"));
    altDownloadLimit->setValue(cachedSessionSettings.value(QStringLiteral("alt-speed-down")).toInt(50));
    form->addRow(tr("Alternative download limit"), altDownloadLimit);

    auto *altUploadLimit = new QSpinBox(&dialog);
    altUploadLimit->setRange(0, 1000000);
    altUploadLimit->setSuffix(tr(" KB/s"));
    altUploadLimit->setValue(cachedSessionSettings.value(QStringLiteral("alt-speed-up")).toInt(50));
    form->addRow(tr("Alternative upload limit"), altUploadLimit);

    connect(downloadLimitEnabled, &QCheckBox::toggled,
            downloadLimit, &QSpinBox::setEnabled);
    connect(uploadLimitEnabled, &QCheckBox::toggled,
            uploadLimit, &QSpinBox::setEnabled);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog
        );
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QJsonObject changes;
    changes[QStringLiteral("speed-limit-down-enabled")] = downloadLimitEnabled->isChecked();
    changes[QStringLiteral("speed-limit-down")] = downloadLimit->value();
    changes[QStringLiteral("speed-limit-up-enabled")] = uploadLimitEnabled->isChecked();
    changes[QStringLiteral("speed-limit-up")] = uploadLimit->value();
    changes[QStringLiteral("alt-speed-enabled")] = altSpeedEnabled->isChecked();
    changes[QStringLiteral("alt-speed-down")] = altDownloadLimit->value();
    changes[QStringLiteral("alt-speed-up")] = altUploadLimit->value();

    client->setSessionSettings(changes);

    if (statusBarController) {
        statusBarController->showMessage(
            tr("Updating speed limits..."),
            3000
            );
    }
}

void MainWindow::toggleAlternativeSpeedMode(bool enabled)
{
    if (!alternativeSpeedSettingsAvailable) {
        updateAlternativeSpeedAction(confirmedAlternativeSpeedEnabled, false);
        return;
    }

    updateAlternativeSpeedAction(enabled, true);

    QJsonObject changes;
    changes[QStringLiteral("alt-speed-enabled")] = enabled;

    client->setSessionSettings(changes);

    if (statusBarController) {
        statusBarController->showMessage(
            enabled
                ? tr("Enabling alternative speed mode...")
                : tr("Disabling alternative speed mode..."),
            3000
            );
    }
}

void MainWindow::showSessionSettings()
{
    openSessionSettingsWhenReceived = true;

    if (statusBarController) {
        statusBarController->showMessage(
            tr("Loading Transmission session settings..."),
            3000
            );
    }

    client->getSessionSettings();
}

void MainWindow::handleSessionSettingsReceived(const QJsonObject &sessionSettings)
{
    cachedSessionSettings = sessionSettings;

    remoteDownloadDir =
        sessionSettings.value(QStringLiteral("download-dir")).toString();

    confirmedAlternativeSpeedEnabled =
        sessionSettings.value(QStringLiteral("alt-speed-enabled")).toBool(false);
    alternativeSpeedSettingsAvailable =
        sessionSettings.contains(QStringLiteral("alt-speed-enabled"));
    updateAlternativeSpeedAction(confirmedAlternativeSpeedEnabled,
                                 alternativeSpeedSettingsAvailable);

    if (torrentListController)
        torrentListController->setSequentialDownloadSupported(
            sessionSupportsSequentialDownload(sessionSettings)
            );

    if (torrentAddController)
        torrentAddController->setDefaultDownloadDir(remoteDownloadDir);

    if (torrentListController)
        torrentListController->setDefaultDownloadDir(remoteDownloadDir);

    if (statusBarController)
        statusBarController->setSessionSettings(sessionSettings);

    if (!remoteDownloadDir.isEmpty()) {
        client->getFreeSpace(remoteDownloadDir);
    } else if (statusBarController) {
        statusBarController->clearFreeSpace();
    }

    if (openQuickSpeedLimitsWhenReceived) {
        openQuickSpeedLimitsWhenReceived = false;
        showQuickSpeedLimitsDialog();
    }

    if (!openSessionSettingsWhenReceived)
        return;

    openSessionSettingsWhenReceived = false;

    SessionSettingsDialog dialog(this);
    dialog.setSessionSettings(sessionSettings);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QJsonObject changes = dialog.changedSettings();

    if (changes.isEmpty()) {
        if (statusBarController) {
            statusBarController->showMessage(
                tr("No Transmission session settings changed"),
                3000
                );
        }
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

    if (statusBarController) {
        statusBarController->showMessage(
            tr("Saving Transmission session settings..."),
            3000
            );
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