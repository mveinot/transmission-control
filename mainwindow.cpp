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
#include "statisticsdialog.h"
#include "sessionoverviewwidget.h"
#include "notificationcontroller.h"
#include "activitylogmodel.h"
#include "applicationcommandcontroller.h"
#include <QActionGroup>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDockWidget>
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
#include <QIcon>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QSize>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QScrollBar>
#include <QStringList>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTableView>
#include <QClipboard>
#include <QTableWidgetItem>
#include <QTreeView>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolBar>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <functional>
#include <algorithm>
#include <utility>
#include <initializer_list>

#include "torrentbackendfactory.h"
#include "dialogabout.h"
#include "diagnosticsdialog.h"
#include "serverconfig.h"
#include "serverselectioncontroller.h"
#include "pollingcoordinator.h"
#include "windowlayoutcontroller.h"
#include "appsettings.h"
#include "serversetupwizard.h"
#include "torrentsortproxymodel.h"
#include "percentfilldelegate.h"
#include "elidedtexttooltipdelegate.h"
#include "sessionsettingsdialog.h"
#include "settingskeys.h"
#include "torrentaddcontroller.h"
#include "settingsimportexport.h"
#include "foldermapping.h"

namespace {
constexpr int DefaultUpdateIntervalSeconds = 10;
constexpr int MinimumUpdateIntervalSeconds = 1;
constexpr int MaximumUpdateIntervalSeconds = 3600;
constexpr qsizetype MaximumClipboardMagnetLength = 64 * 1024;

class MagnetLinkEditor final : public QPlainTextEdit
{
public:
    explicit MagnetLinkEditor(QDialog *dialog)
        : QPlainTextEdit(dialog)
        , m_dialog(dialog)
    {
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            m_dialog->accept();
            event->accept();
            return;
        }

        QPlainTextEdit::keyPressEvent(event);
    }

private:
    QDialog *m_dialog;
};

bool isEditablePasteTarget(QWidget *widget)
{
    if (!widget)
        return false;

    if (const auto *lineEdit = qobject_cast<QLineEdit *>(widget))
        return !lineEdit->isReadOnly();
    if (const auto *textEdit = qobject_cast<QTextEdit *>(widget))
        return !textEdit->isReadOnly();
    if (const auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(widget))
        return !plainTextEdit->isReadOnly();
    if (const auto *comboBox = qobject_cast<QComboBox *>(widget))
        return comboBox->isEditable();

    return qobject_cast<QAbstractSpinBox *>(widget) != nullptr;
}

QString clipboardMagnetCandidate()
{
    const QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard)
        return {};

    const QString candidate = clipboard->text(QClipboard::Clipboard).trimmed();
    if (candidate.isEmpty() || candidate.size() > MaximumClipboardMagnetLength)
        return {};

    const QUrl url(candidate, QUrl::StrictMode);
    if (!url.isValid() || url.scheme().compare(QStringLiteral("magnet"), Qt::CaseInsensitive) != 0)
        return {};

    // Require a recognized BitTorrent exact-topic identifier. This avoids
    // treating arbitrary magnet-style URIs as torrents while accepting both
    // v1 (btih) and v2/hybrid (btmh) links.
    const auto queryItems = QUrlQuery(url).queryItems();
    for (const auto &[key, value] : queryItems) {
        if (key.compare(QStringLiteral("xt"), Qt::CaseInsensitive) != 0)
            continue;

        const QString topic = value.trimmed();
        constexpr auto BtihPrefix = "urn:btih:";
        constexpr auto BtmhPrefix = "urn:btmh:";

        if ((topic.startsWith(QLatin1StringView(BtihPrefix), Qt::CaseInsensitive)
             && topic.size() > QLatin1StringView(BtihPrefix).size())
            || (topic.startsWith(QLatin1StringView(BtmhPrefix), Qt::CaseInsensitive)
                && topic.size() > QLatin1StringView(BtmhPrefix).size())) {
            return candidate;
        }
    }

    return {};
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
        torrentFilesController->setTorrentContext({}, QString());
        torrentFilesController->clear();
    }

    if (torrentTrackersController) {
        torrentTrackersController->setTorrentKey({});
        torrentTrackersController->clear();
    }

    if (torrentPeersController)
        torrentPeersController->clear();
}

void MainWindow::copyFromFocusedWidget()
{
    QWidget *focused = QApplication::focusWidget();
    if (!focused)
        return;

    if (auto *lineEdit = qobject_cast<QLineEdit *>(focused)) {
        lineEdit->copy();
        return;
    }

    if (auto *textEdit = qobject_cast<QTextEdit *>(focused)) {
        textEdit->copy();
        return;
    }

    if (auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(focused)) {
        plainTextEdit->copy();
        return;
    }

    auto *view = qobject_cast<QAbstractItemView *>(focused);
    if (!view)
        view = qobject_cast<QAbstractItemView *>(focused->parentWidget());
    if (!view || !view->selectionModel() || !view->model())
        return;

    QModelIndexList rows = view->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        const QModelIndexList indexes = view->selectionModel()->selectedIndexes();
        if (indexes.isEmpty())
            return;
        rows = indexes;
    }

    std::sort(rows.begin(), rows.end(), [](const QModelIndex &left, const QModelIndex &right) {
        if (left.row() != right.row())
            return left.row() < right.row();
        return left.column() < right.column();
    });

    QStringList output;
    int previousRow = -1;
    for (const QModelIndex &rowIndex : std::as_const(rows)) {
        if (rowIndex.row() == previousRow)
            continue;
        previousRow = rowIndex.row();

        QStringList columns;
        for (int column = 0; column < view->model()->columnCount(rowIndex.parent()); ++column) {
            const bool hidden =
                (qobject_cast<QTableView *>(view) &&
                 qobject_cast<QTableView *>(view)->isColumnHidden(column)) ||
                (qobject_cast<QTreeView *>(view) &&
                 qobject_cast<QTreeView *>(view)->isColumnHidden(column));
            if (hidden)
                continue;
            columns << view->model()->index(rowIndex.row(), column, rowIndex.parent())
                           .data(Qt::DisplayRole).toString();
        }
        output << columns.join(QLatin1Char('\t'));
    }

    if (!output.isEmpty())
        QApplication::clipboard()->setText(output.join(QLatin1Char('\n')));
}

void MainWindow::selectAllInFocusedWidget()
{
    QWidget *focused = QApplication::focusWidget();
    if (!focused)
        return;

    if (auto *lineEdit = qobject_cast<QLineEdit *>(focused)) {
        lineEdit->selectAll();
        return;
    }
    if (auto *textEdit = qobject_cast<QTextEdit *>(focused)) {
        textEdit->selectAll();
        return;
    }
    if (auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(focused)) {
        plainTextEdit->selectAll();
        return;
    }

    auto *view = qobject_cast<QAbstractItemView *>(focused);
    if (!view)
        view = qobject_cast<QAbstractItemView *>(focused->parentWidget());
    if (view)
        view->selectAll();
}

void MainWindow::focusTorrentSearch()
{
    ui->editTorrentFilter->setFocus(Qt::ShortcutFocusReason);
    ui->editTorrentFilter->selectAll();
}

void MainWindow::focusFileSearch()
{
    ui->tabWidget->setCurrentWidget(ui->fileTreeWidget);
    ui->editFileFilter->setFocus(Qt::ShortcutFocusReason);
    ui->editFileFilter->selectAll();
}

void MainWindow::setupApplicationCommands()
{
    ApplicationCommandController::Actions actions;
    actions.openTorrent = ui->action_Open_Torrent;
    actions.addMagnet = ui->actionAdd_Torrent_from_Magnet_Link;
    actions.startSelected = ui->actionStart_Torrent;
    actions.stopSelected = ui->actionStop_Torrent;
    actions.startAll = ui->actionStart_All_Torrents;
    actions.stopAll = ui->actionStop_All_Torrents;
    actions.forceStart = ui->actionForce_Start_Torrent;
    actions.verify = ui->actionVerify_Torrent;
    actions.reannounce = ui->actionReannounce;
    actions.deleteTorrent = ui->actionDelete_Torrent;
    actions.queueTop = ui->moveTopAction;
    actions.queueUp = ui->moveUpAction;
    actions.queueDown = ui->moveDownAction;
    actions.queueBottom = ui->moveBottomAction;
    actions.closeWindow = ui->actionClose_Window;
    actions.applicationSettings = ui->actionSettings;
    actions.manageServers = ui->actionServer_Setup;
    actions.serverSettings = ui->actionTransmission_Settings;
    actions.alternativeSpeed = ui->actionAlternative_Speed_Mode;
    actions.statistics = statisticsAction;
    actions.about = ui->actionAbout;
    actions.quit = ui->actionQuit;
    actions.checkForUpdates = ui->actionCheckForUpdates;
    actions.exportSettings = ui->actionExport_Settings;
    actions.importSettings = ui->actionImport_Settings;

    ApplicationCommandController::Menus menus;
    menus.menuBar = menuBar();
    menus.transfers = ui->menuTransfers;
    menus.help = ui->menuHelp;

    ApplicationCommandController::Handlers handlers;
    handlers.copy = [this]() { copyFromFocusedWidget(); };
    handlers.selectAll = [this]() { selectAllInFocusedWidget(); };
    handlers.findTorrents = [this]() { focusTorrentSearch(); };
    handlers.findFiles = [this]() { focusFileSearch(); };
    handlers.openTorrent = [this]() { addTorrentFromFile(); };
    handlers.addMagnet = [this]() { addTorrentFromMagnet(); };
    handlers.startSelected = [this]() {
        if (torrentListController)
            torrentListController->startSelectedTorrents();
    };
    handlers.stopSelected = [this]() {
        if (torrentListController)
            torrentListController->stopSelectedTorrents();
    };
    handlers.startAll = [this]() {
        if (statusBarController) {
            statusBarController->showMessage(
                tr("Starting all torrents..."), 3000);
        }
        client->startAllTorrents();
    };
    handlers.stopAll = [this]() {
        if (statusBarController) {
            statusBarController->showMessage(
                tr("Stopping all torrents..."), 3000);
        }
        client->stopAllTorrents();
    };
    handlers.verify = [this]() {
        if (torrentListController)
            torrentListController->verifySelectedTorrents();
    };
    handlers.reannounce = [this]() {
        if (torrentListController)
            torrentListController->reannounceSelectedTorrents();
    };
    handlers.deleteTorrent = [this]() {
        if (torrentListController)
            torrentListController->deleteSelectedTorrents();
    };
    handlers.applicationSettings =
        [this]() { showApplicationSettings(); };
    handlers.manageServers = [this]() { onServerSetupTriggered(); };
    handlers.serverSettings = [this]() { showSessionSettings(); };
    handlers.alternativeSpeed =
        [this](bool enabled) { toggleAlternativeSpeedMode(enabled); };
    handlers.statistics = [this]() { showStatistics(); };
    handlers.closeWindow = [this]() { close(); };
    handlers.about = [this]() { showAbout(); };
    handlers.quit = [this]() { quitApplication(); };
    handlers.diagnostics = [this]() { showDiagnostics(); };
    handlers.checkForUpdates = [this]() {
        if (updateCheckController)
            updateCheckController->checkNow();
    };
    handlers.exportSettings = [this]() { exportSettings(); };
    handlers.importSettings = [this]() { importSettings(); };
    handlers.statusMessage =
        [this](const QString &message, int timeoutMs) {
            if (statusBarController)
                statusBarController->showMessage(message, timeoutMs);
        };

    applicationCommandController = new ApplicationCommandController(
        this, actions, menus, std::move(handlers), this);
    applicationCommandController->setup();
}

void MainWindow::setupViewMenu()
{
    WindowLayoutController::Widgets widgets;
    widgets.toolBar = ui->toolBar;
    widgets.statusBar = ui->statusbar;
    widgets.contentSplitter = ui->splitter_2;
    widgets.mainSplitter = ui->splitter;
    widgets.detailsPane = detailsPaneStack;
    widgets.filterSidebar = ui->filterPanel;
    windowLayoutController =
        new WindowLayoutController(this, widgets, this);
    windowLayoutController->setupViewMenu(
        menuBar(), ui->menuHelp->menuAction());
    viewMenu = windowLayoutController->viewMenu();

    connect(windowLayoutController,
            &WindowLayoutController::detailsPaneVisibilityChanged,
            this,
            [this](bool visible) {
                if (!visible) {
                    pollingCoordinator->setDetailsPaneVisible(false);
                    return;
                }

                // Hidden panes suspend detail RPCs. Rehydrate the selected
                // torrent once when the layout controller exposes the pane.
                pollingCoordinator->setDetailsPaneVisible(true);
                updatePollingDetailView();
                pollingCoordinator->requestSelectedTorrent(true);
            });
    pollingCoordinator->setDetailsPaneVisible(
        windowLayoutController->detailsPaneVisible());

    viewMenu->addSeparator();
    showBandwidthGraphAction = viewMenu->addAction(tr("Bandwidth Graph"));
    showBandwidthGraphAction->setCheckable(true);
    showBandwidthGraphAction->setChecked(
        QSettings().value(SettingsKeys::ShowSessionOverview, false).toBool());
    showBandwidthGraphAction->setToolTip(
        tr("Show bandwidth activity when no torrent is selected"));
    connect(showBandwidthGraphAction, &QAction::toggled,
            this, [this](bool enabled) {
                QSettings().setValue(SettingsKeys::ShowSessionOverview,
                                     enabled);
                sessionOverviewEnabled = enabled;
                showTorrentDetails(isValidTorrentKey(currentTorrentKey()));
            });

    statisticsAction = viewMenu->addAction(tr("Statistics…"));
    statisticsAction->setToolTip(tr("Show session statistics"));
}

void MainWindow::setupActivityDock()
{
    activityDock = new QDockWidget(tr("Activity"), this);
    activityDock->setObjectName(QStringLiteral("activityDock"));
    activityDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    activityDock->setFeatures(QDockWidget::DockWidgetClosable |
                              QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetFloatable);

    activityLogModel = new ActivityLogModel(activityDock);
    activityTable = new QTableView(activityDock);
    activityTable->setModel(activityLogModel);
    activityTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    activityTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    activityTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    activityTable->setAlternatingRowColors(true);
    activityTable->setShowGrid(false);
    activityTable->verticalHeader()->hide();
    activityTable->horizontalHeader()->setStretchLastSection(false);
    activityTable->horizontalHeader()->setSectionResizeMode(ActivityLogModel::TimeColumn,
                                                             QHeaderView::ResizeToContents);
    activityTable->horizontalHeader()->setSectionResizeMode(ActivityLogModel::EventColumn,
                                                             QHeaderView::ResizeToContents);
    activityTable->horizontalHeader()->setSectionResizeMode(ActivityLogModel::DetailsColumn,
                                                             QHeaderView::Stretch);
    activityTable->horizontalHeader()->setSectionResizeMode(ActivityLogModel::ServerColumn,
                                                             QHeaderView::ResizeToContents);
    activityTable->setContextMenuPolicy(Qt::CustomContextMenu);
    activityDock->setWidget(activityTable);
    addDockWidget(Qt::BottomDockWidgetArea, activityDock);

    QAction *toggleAction = activityDock->toggleViewAction();
    toggleAction->setText(tr("Activity"));
    toggleAction->setToolTip(tr("Show or hide activity observed while Planetary is connected"));
    if (viewMenu) {
        viewMenu->insertAction(viewMenu->actions().value(2), toggleAction);
    }

    connect(activityTable, &QTableView::customContextMenuRequested,
            this, [this](const QPoint &position) {
                QMenu menu(activityTable);
                QAction *copyAction = menu.addAction(tr("Copy"));
                copyAction->setEnabled(activityTable->selectionModel()->hasSelection());
                QAction *clearAction = menu.addAction(tr("Clear Activity"));
                clearAction->setEnabled(activityLogModel->rowCount() > 0);

                QAction *chosen = menu.exec(activityTable->viewport()->mapToGlobal(position));
                if (chosen == copyAction) {
                    QStringList lines;
                    const QModelIndexList rows =
                        activityTable->selectionModel()->selectedRows();
                    for (const QModelIndex &rowIndex : rows) {
                        QStringList columns;
                        for (int column = 0; column < ActivityLogModel::ColumnCount; ++column)
                            columns << activityLogModel->index(rowIndex.row(), column).data().toString();
                        lines << columns.join(QLatin1Char('\t'));
                    }
                    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
                } else if (chosen == clearAction) {
                    activityLogModel->clear();
                }
            });

    if (!windowLayoutController
        || !windowLayoutController->restoreWindowState()) {
        activityDock->hide();
    }

    connect(activityDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible && !activityDock->isFloating())
            resizeDocks({activityDock}, {150}, Qt::Vertical);
    });
}

void MainWindow::recordActivity(const QString &event, const QString &details,
                                const QString &server)
{
    if (!activityLogModel)
        return;

    const bool wasAtBottom = activityTable &&
        activityTable->verticalScrollBar()->value() >=
            activityTable->verticalScrollBar()->maximum();
    activityLogModel->addEvent(event, details, server);
    if (wasAtBottom && activityTable)
        activityTable->scrollToBottom();
}

QMenu *MainWindow::createPopupMenu()
{
    return windowLayoutController
               ? windowLayoutController->createToolBarPopupMenu()
               : QMainWindow::createPopupMenu();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    client = createConfiguredTorrentBackend(this);
    PollingCoordinator::Requests pollingRequests;
    pollingRequests.torrentList = [this]() { client->getTorrentList(); };
    pollingRequests.trackerMetadata =
        [this]() { client->getTorrentTrackerMetadata(); };
    pollingRequests.torrentDetails =
        [this](const TorrentKey &key) { client->getTorrentDetails(key); };
    pollingRequests.torrentFiles =
        [this](const TorrentKey &key) { client->getTorrentFiles(key); };
    pollingRequests.torrentPeers =
        [this](const TorrentKey &key) { client->getTorrentPeers(key); };
    pollingRequests.torrentTrackers =
        [this](const TorrentKey &key) { client->getTorrentTrackers(key); };
    pollingRequests.torrentPieces =
        [this](const TorrentKey &key) { client->getTorrentPieces(key); };
    pollingRequests.cancelTorrentDetails =
        [this]() { client->cancelTorrentDetailRequests(); };
    pollingRequests.freeSpace =
        [this](const QString &path) { client->getFreeSpace(path); };
    pollingRequests.freeSpaceSupported =
        [this]() { return client->capabilities().freeSpaceQuery; };
    pollingCoordinator =
        new PollingCoordinator(std::move(pollingRequests), this);
    torrentModel = new TorrentModel(this);

    connect(pollingCoordinator,
            &PollingCoordinator::torrentListRefreshStarted,
            this,
            [this](bool serverChanged) {
                if (torrentListController)
                    torrentListController->beginTorrentListRefresh(
                        serverChanged);
            });

    // Keep the canonical torrent collection independent of view ordering and
    // filtering; controllers translate proxy selections back to torrent IDs.
    proxy = new TorrentSortProxyModel(this);
    proxy->setSourceModel(torrentModel);

    // GeoIP failure is non-fatal: GeoIpService retains its compiled fallback.
    geoIpService = new GeoIpService(this);

    if (!geoIpService->loadDefaultDatabase()) {
        qWarning() << "GeoIP database could not be loaded; using fallback lookup";
    }

    torrentAddController = new TorrentAddController(client, this, this);

    // setupUi() must precede controller construction because controllers retain
    // pointers to widgets owned by the generated UI tree.
    ui->setupUi(this);
    setupDetailsPane();

    // Native controls can exceed their nominal 28 px minimum (notably the
    // macOS combo box). Match the filter header to the actual control-row
    // height so the filter list and torrent table begin on the same baseline.
    const int torrentControlsHeight = qMax(
        28,
        qMax(ui->comboServers->sizeHint().height(),
             ui->editTorrentFilter->sizeHint().height()));
    ui->labelTorrentStatus->setFixedHeight(torrentControlsHeight);

    setupViewMenu();
    setupActivityDock();
    setupApplicationCommands();
    TorrentGeneralController::Widgets generalWidgets;
    generalWidgets.generalTab = ui->general;
    generalWidgets.generalLayout = ui->verticalLayoutGeneral;
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
            [this](TorrentKey torrentKey, const QString &hashString, const QString &magnetLink) {
                if (torrentListController) {
                    torrentListController->setCurrentTorrentDetails(
                        torrentKey,
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

    // Platform chrome and passive services can be initialized before the RPC
    // client starts producing responses.
    serverSelectionController =
        new ServerSelectionController(ui->comboServers, client, this);
    serverSelectionController->reloadProfiles();
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
            this, [this]() { pollingCoordinator->requestTorrentList(); });

    connect(watchFolderController, &WatchFolderController::activityEventRequested,
            this, [this](const QString &event, const QString &details) {
                recordActivity(
                    event,
                    details,
                    serverSelectionController->currentDisplayText());
            });

    watchFolderController->loadSettings();
    trayController = new TrayController(this, this);
    trayController->setup();
    trayController->setTorrentGlobalActions(ui->actionStart_All_Torrents,
                                            ui->actionStop_All_Torrents);

#if defined(Q_OS_WIN)
    notificationController = new NotificationController(
        this,
        [this](const QString &title, const QString &message, int timeoutMs) {
            return trayController &&
                   trayController->showNotification(title, message, timeoutMs);
        });
#else
    notificationController = new NotificationController(this);
#endif
    notificationController->setServerName(
        serverSelectionController->currentDisplayText());

    connect(notificationController, &NotificationController::statusMessageRequested,
            this, [this](const QString &message, int timeoutMs) {
                if (statusBarController)
                    statusBarController->showMessage(message, timeoutMs);
            });

    connect(notificationController, &NotificationController::activityEventObserved,
            this, &MainWindow::recordActivity);

    connect(client, &TorrentBackend::torrentAdded,
            notificationController, &NotificationController::handleTorrentAdded);

    torrentFilesController = new TorrentFilesController(
        ui->fileTreeWidget,
        ui->editFileFilter,
        client,
        this,
        this
        );
    torrentFilesController->setFolderMappingsProvider(
        [this]() {
            return serverSelectionController
                       ? serverSelectionController->currentFolderMappings()
                       : QList<FolderMapping>{};
        }
        );
    torrentFilesController->setup();

    connect(torrentFilesController,
            &TorrentFilesController::selectionActiveChanged,
            pollingCoordinator,
            &PollingCoordinator::setFileRefreshSuppressed);

    connect(torrentFilesController, &TorrentFilesController::statusMessageRequested,
            this, [this](const QString &message, int timeoutMs) {
                if (statusBarController)
                    statusBarController->showMessage(message, timeoutMs);
            });

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
    ui->actionWaiting->setCheckable(true);
    ui->actionCompleted->setCheckable(true);
    ui->actionActive->setCheckable(true);
    ui->actionInactive->setCheckable(true);
    ui->actionStopped->setCheckable(true);
    ui->actionError->setCheckable(true);

    auto *stateGroup = new QActionGroup(this);
    stateGroup->setExclusive(true);
    stateGroup->addAction(ui->actionAll);
    stateGroup->addAction(ui->actionDownloading);
    stateGroup->addAction(ui->actionWaiting);
    stateGroup->addAction(ui->actionCompleted);
    stateGroup->addAction(ui->actionActive);
    stateGroup->addAction(ui->actionInactive);
    stateGroup->addAction(ui->actionStopped);
    stateGroup->addAction(ui->actionError);
    ui->actionAll->setChecked(true);

    TorrentFilterController::Actions filterActions;
    filterActions.all = ui->actionAll;
    filterActions.downloading = ui->actionDownloading;
    filterActions.waiting = ui->actionWaiting;
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

    ui->tableView->setItemDelegateForColumn(
        TorrentModel::NameColumn,
        new ElidedTextTooltipDelegate(ui->tableView)
        );

    connect(torrentListController, &TorrentListController::statusMessageRequested,
            this, [this](const QString &message, int timeoutMs) {
                if (statusBarController)
                    statusBarController->showMessage(message, timeoutMs);
            });

    connect(torrentListController, &TorrentListController::torrentSelected,
            this, [this](TorrentKey torrentKey) {
                showTorrentDetails(true);
                // Selection defines a new detail generation. Cancel every
                // category still associated with the previous torrent.
                pollingCoordinator->setSelectedTorrent(torrentKey);
                clearGeneralTab();
                torrentPeersController->clear();
                torrentTrackersController->clear();
                torrentTrackersController->setTorrentKey(torrentKey);
                torrentFilesController->setTorrentContext({}, QString());
                torrentFilesController->clear();
                torrentPeersController->setLoading();
                torrentTrackersController->setLoading();
                torrentFilesController->setLoading();
                updatePollingDetailView();
                pollingCoordinator->requestSelectedTorrent(true);
            });

    connect(torrentListController, &TorrentListController::torrentSelectionCleared,
            this, [this]() {
                showTorrentDetails(false);
                pollingCoordinator->setSelectedTorrent({});
                clearGeneralTab();
            });

    connect(torrentListController, &TorrentListController::torrentListRefreshRequested,
            this, [this]() { pollingCoordinator->requestTorrentList(); });

    connect(torrentListController, &TorrentListController::torrentDetailsRefreshRequested,
            client, &TorrentBackend::getTorrentDetails);

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

    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, [this](int) {
                updatePollingDetailView();
                refreshCurrentTorrentTabData();
            });

    connect(client, &TorrentBackend::serverChanged,
            torrentModel, &TorrentModel::clear);

    connect(client, &TorrentBackend::serverChanged, this, [this]() {
        pollingCoordinator->resetForServerChange();
        activityConnectionEstablished = false;
        activityConnectionFailed = false;
        showTorrentDetails(false);
        if (sessionOverviewWidget)
            sessionOverviewWidget->clearHistory();
        applicationCommandController->setBackendState(
            client->backendName(), client->capabilities());
        recordActivity(tr("Server changed"),
                       tr("Switched active %1 server.").arg(client->backendName()),
                       ui->comboServers->currentText());
    });

    connect(client, &TorrentBackend::capabilitiesChanged,
            this, [this](const TorrentBackendCapabilities &capabilities) {
                applicationCommandController->setBackendState(
                    client->backendName(), capabilities);
            });

    connect(client, &TorrentBackend::updateFailed, this, [this](const QString &message) {
        if (sessionOverviewWidget)
            sessionOverviewWidget->markDisconnected();
        if (!activityConnectionFailed) {
            recordActivity(tr("Connection lost"), message, ui->comboServers->currentText());
            activityConnectionFailed = true;
        }
    });

    connect(client, &TorrentBackend::updateFinished, this, [this]() {
        const QString server = ui->comboServers->currentText();
        if (activityConnectionFailed) {
            recordActivity(tr("Reconnected"),
                           tr("Connection to %1 was restored.").arg(client->backendName()),
                           server);
        } else if (!activityConnectionEstablished) {
            recordActivity(tr("Connected"),
                           tr("Connected to %1.").arg(client->backendName()),
                           server);
        }
        activityConnectionEstablished = true;
        activityConnectionFailed = false;
    });

    connect(client, &TorrentBackend::sessionSettingsReceived,
            this, &MainWindow::handleSessionSettingsReceived);

    applicationCommandController->setBackendState(
        client->backendName(), client->capabilities());
    applicationCommandController->setAlternativeSpeedState(false, false);

    connect(client, &TorrentBackend::torrentDetailsReceived,
            this,
            [this](const TorrentDetails &details) {
                // Detail requests can overlap selection changes. Never apply a
                // late response to widgets representing another torrent.
                if (details.key != currentTorrentKey())
                    return;

                torrentGeneralController->update(details);
                // Piece/live data merges into the summary cache, so start that
                // category only after the summary establishes its torrent ID.
                refreshCurrentTorrentTabData();

                if (torrentListController) {
                    torrentListController->setCurrentTorrentSequentialDownload(
                        details.key,
                        details.sequentialDownload,
                        details.hasSequentialDownload
                        );

                    torrentListController->setCurrentTorrentBandwidthPriority(
                        details.key,
                        details.bandwidthPriority,
                        details.hasBandwidthPriority
                        );
                }

            });

    connect(client, &TorrentBackend::torrentFilesReceived,
            this, [this](const TorrentFiles &files) {
                if (files.key != currentTorrentKey())
                    return;

                // A reply may already be in flight when file selection
                // begins. Rebuilding now could retarget a pending context-menu
                // action, so the coordinator requests a fresh snapshot later.
                if (torrentFilesController->hasSelection())
                    return;

                torrentFilesController->setTorrentContext(
                    files.key,
                    files.downloadDirectory);
                torrentFilesController->populate(files);
            });

    connect(client, &TorrentBackend::torrentPeersReceived,
            this, [this](const TorrentPeers &peers) {
                if (peers.key == currentTorrentKey())
                    torrentPeersController->populate(peers);
            });

    connect(client, &TorrentBackend::torrentTrackersReceived,
            this, [this](const TorrentTrackers &trackers) {
                if (trackers.key != currentTorrentKey())
                    return;

                torrentTrackersController->setTorrentKey(trackers.key);
                torrentTrackersController->populate(trackers);
            });

    connect(client, &TorrentBackend::torrentPiecesReceived,
            this,
            [this](const TorrentPieces &pieces) {
                torrentGeneralController->updatePieces(pieces);
            });

    // Primary data path: the source model must update before observers rebuild
    // filters, counts, notifications, and status summaries.
    connect(client, &TorrentBackend::torrentsReceived,
                torrentModel, &TorrentModel::applyUpdate);

    // Secondary consumers do not own torrent data and may safely derive state
    // from each completed snapshot.
    connect(client, &TorrentBackend::torrentsReceived,
            this, &MainWindow::handleTorrentsReceived);

    connect(client, &TorrentBackend::updateFailed,
            this, [this](const QString &message) {
                if (torrentListController)
                    torrentListController->markTorrentListLoadFailed(message);
            });

    connect(serverSelectionController,
            &ServerSelectionController::serverActivated,
            this,
            [this](const ServerProfile &) {
                handleServerActivated();
            });
    connect(serverSelectionController,
            &ServerSelectionController::activationFailed,
            this,
            [this](const QString &message) {
                if (statusBarController)
                    statusBarController->showMessage(message, 5000);
            });

    connect(client, &TorrentBackend::freeSpaceReceived,
            this, [this](const QString &, qint64 sizeBytes) {
                if (statusBarController)
                    statusBarController->setFreeSpace(sizeBytes);
            });

    connect(client, &TorrentBackend::torrentTrackerMetadataUpdated,
            this, [this]() { pollingCoordinator->requestTorrentList(); });

    connect(client, &TorrentBackend::commandSucceeded,
            this, [this](const QString &method) {
                // Mutating RPC responses contain little or no updated torrent
                // state, so refresh the affected projections explicitly.
                if (method == QStringLiteral("session-set")) {
                    client->getSessionSettings();
                    return;
                }

                const auto refreshTorrentState = [this](bool refreshDetails) {
                    pollingCoordinator->scheduleCommandRefresh(refreshDetails);
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

    connect(client, &TorrentBackend::commandFailed,
            this, [this](const QString &method, const QString &message) {
                if (method == QStringLiteral("session-set"))
                    applicationCommandController->setAlternativeSpeedState(
                        alternativeSpeedSettingsAvailable,
                        confirmedAlternativeSpeedEnabled);

                if (statusBarController)
                    statusBarController->showMessage(message, 5000);
            });


    restoreTableViewState();

    applyAppSettings();

    // A first-run window has no backend to initialize; the setup assistant
    // activates the saved definition before requesting list/session data.
    if (ServerSetupWizard::hasConfiguredServer()) {
        if (serverSelectionController->activateCurrent(false)) {
            client->init();
            client->getSessionSettings();
        }
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showDiagnostics()
{
    DiagnosticsDialog dialog(cachedSessionSettings,
                             client ? client->serverDisplayName() : QString(),
                             client ? client->endpointUrl() : QString(),
                             client ? client->backendName() : QString(),
                             client ? client->protocolDescription() : QString(),
                             geoIpService,
                             updateIntervalMs(),
                             this);
    dialog.exec();
}

void MainWindow::showAbout()
{
    DialogAbout dialog(this);
    dialog.exec();
}

void MainWindow::on_tableView_clicked(const QModelIndex &proxyIndex)
{
    if (torrentListController)
        torrentListController->handleTableClicked(proxyIndex);
}

void MainWindow::showApplicationSettings()
{
    AppSettings dialog(this);

    connect(&dialog, &AppSettings::testNotificationRequested,
            this, [this]() {
                if (notificationController)
                    notificationController->showTestNotification();
            });
    connect(&dialog, &AppSettings::testExternalCommandRequested,
            this, [this](const QString &executable, const QString &arguments) {
                if (notificationController)
                    notificationController->showTestExternalCommand(executable, arguments);
            });

    connect(&dialog, &AppSettings::clearWatchFolderHistoryRequested,
            this, [this]() {
                if (!watchFolderManager)
                    return;

                watchFolderManager->clearProcessedHistory();
                statusBar()->showMessage(
                    tr("Watch folder import history cleared."),
                    5000
                    );
            });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    applyAppSettings();
    if (watchFolderController)
        watchFolderController->loadSettings();
}

TorrentKey MainWindow::currentTorrentKey() const
{
    return torrentListController ? torrentListController->currentTorrentKey() : TorrentKey();
}

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
    // Persist layout before the tray controller potentially consumes the close
    // and converts it into a hide operation.
    saveTableViewState();
    if (windowLayoutController)
        windowLayoutController->saveState();

    if (trayController && trayController->handleCloseEvent(event))
        return;

    QMainWindow::closeEvent(event);
}

void MainWindow::onServerSetupTriggered()
{
    ServerConfig sc(this);

    if (sc.exec() == QDialog::Accepted) {
        serverSelectionController->reloadProfiles();
        if (serverSelectionController->activateCurrent()) {
            if (!pendingLaunchArguments.isEmpty()) {
                const QStringList arguments = pendingLaunchArguments;
                pendingLaunchArguments.clear();
                QTimer::singleShot(0, this, [this, arguments]() {
                    handleLaunchArguments(arguments);
                });
            }
        }
    }
}

void MainWindow::runFirstTimeServerSetup(const QStringList &launchArguments,
                                         bool forceWizard)
{
    pendingLaunchArguments.append(launchArguments);

    const bool alreadyConfigured = ServerSetupWizard::hasConfiguredServer();
    if (!alreadyConfigured || forceWizard) {
        ServerSetupWizard wizard(alreadyConfigured, this);
        if (wizard.exec() != QDialog::Accepted)
            return;

        // Rebuild the selector and route the stable backend facade to the new
        // definition before consuming any launch-time torrent arguments.
        serverSelectionController->reloadProfiles();
        serverSelectionController->selectSettingsIndex(
            wizard.savedServerIndex());
    }

    if (pendingLaunchArguments.isEmpty())
        return;

    const QStringList arguments = pendingLaunchArguments;
    pendingLaunchArguments.clear();
    handleLaunchArguments(arguments);
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
    QSettings settings;
    QString initialDirectory =
        settings.value(SettingsKeys::TorrentOpenDirectory).toString();

    if (!QFileInfo(initialDirectory).isDir()) {
        initialDirectory = QStandardPaths::writableLocation(
            QStandardPaths::DownloadLocation
            );
    }

    if (!QFileInfo(initialDirectory).isDir())
        initialDirectory = QDir::homePath();

    const QStringList fileNames = QFileDialog::getOpenFileNames(
        this,
        tr("Add Torrent Files"),
        initialDirectory,
        tr("Torrent Files (*.torrent);;All Files (*)")
        );

    if (fileNames.isEmpty())
        return;

    // Native multi-file dialogs normally constrain selection to one directory;
    // the first selected file remains a deterministic fallback if they do not.
    settings.setValue(
        SettingsKeys::TorrentOpenDirectory,
        QFileInfo(fileNames.constFirst()).absolutePath()
        );

    torrentAddController->addTorrentFiles(fileNames);
}

void MainWindow::addTorrentFromMagnet()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add Magnet Link"));

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Magnet link:"), &dialog));

    auto *editor = new MagnetLinkEditor(&dialog);
    editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    editor->setTabChangesFocus(true);
    editor->setFixedHeight(editor->fontMetrics().lineSpacing() * 4
                           + editor->frameWidth() * 2 + 12);
    editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(editor);

    // Clipboard access occurs only in response to explicitly opening this
    // dialog. Invalid or unrelated clipboard text leaves the field blank.
    const QString clipboardMagnet = clipboardMagnetCandidate();
    if (!clipboardMagnet.isEmpty()) {
        editor->setPlainText(clipboardMagnet);
        editor->moveCursor(QTextCursor::End);
    }

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog
        );
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Add"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Wrapping keeps long links readable without requiring an unusually wide
    // dialog; users can still enlarge it when comparing or editing a URI.
    dialog.resize(600, dialog.sizeHint().height());

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString magnetLink = editor->toPlainText().trimmed();

    if (magnetLink.isEmpty())
        return;

    torrentAddController->addMagnetLink(magnetLink);
}

void MainWindow::handleServerActivated()
{
    // A server switch defines a new notification baseline; otherwise torrents
    // on the new server would be reported as newly added or completed.
    if (notificationController) {
        notificationController->setServerName(
            serverSelectionController->currentDisplayText());
        notificationController->resetBaseline();
    }

    torrentFilesController->setTorrentContext({}, QString());
    torrentFilesController->clear();
    torrentTrackersController->setTorrentKey({});
    torrentTrackersController->clear();
    torrentPeersController->clear();
    if (torrentListController)
        torrentListController->updateActionState();

    if (statusBarController) {
        statusBarController->showMessage(
            tr("Selected server: %1").arg(client->serverDisplayName()),
            3000
            );
    }

    pollingCoordinator->requestTorrentList(
        PollingCoordinator::ListRefreshMode::ServerChanged);

    // Session-derived state is server-specific and remains unknown until the
    // new session-get response arrives.
    pollingCoordinator->setRemoteDownloadDirectory({}, false);

    if (torrentListController) {
        torrentListController->clearCurrentTorrentDetails();
        torrentListController->setDefaultDownloadDir({});
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
    // Restart is unnecessary when changing an active QTimer interval; Qt
    // reschedules it using the new value.
    pollingCoordinator->setPollingInterval(updateIntervalMs());
    pollingCoordinator->startPolling();

    if (statusBarController) {
        statusBarController->setUpdateIntervalSeconds(
            pollingCoordinator->pollingInterval() / 1000);
        statusBarController->showMessage(
            tr("Update interval: %1 seconds")
                .arg(pollingCoordinator->pollingInterval() / 1000),
            3000
            );
    }
}

void MainWindow::updatePollingDetailView()
{
    if (!pollingCoordinator)
        return;

    PollingCoordinator::DetailView detailView =
        PollingCoordinator::DetailView::None;
    QWidget *currentTab = ui->tabWidget->currentWidget();
    if (currentTab == ui->fileList)
        detailView = PollingCoordinator::DetailView::Files;
    else if (currentTab == ui->trackers)
        detailView = PollingCoordinator::DetailView::Trackers;
    else if (currentTab == ui->peers)
        detailView = PollingCoordinator::DetailView::Peers;
    else if (torrentGeneralController
             && torrentGeneralController->wantsLiveTorrentDetails(
                 currentTab)) {
        detailView = PollingCoordinator::DetailView::GeneralLive;
    }
    pollingCoordinator->setDetailView(detailView);
}

void MainWindow::refreshCurrentTorrentTabData()
{
    updatePollingDetailView();
    pollingCoordinator->requestSelectedTorrent(false);
}

void MainWindow::setupConnectionStatusIndicator()
{
    statusBarController = new StatusBarController(ui->statusbar, client, this);
    statusBarController->setup();
    statusBarController->setServerName(client->serverDisplayName());

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
            this, &MainWindow::showApplicationSettings);
}

void MainWindow::setupDetailsPane()
{
    const int detailsIndex = ui->splitter_2->indexOf(ui->tabWidget);
    detailsPaneStack = new QStackedWidget(ui->splitter_2);
    detailsPaneStack->setMinimumHeight(ui->tabWidget->minimumHeight());

    // Reparenting removes the tab widget from the splitter; the stack takes
    // its original slot and preserves the splitter's existing sizing model.
    ui->tabWidget->setParent(detailsPaneStack);
    sessionOverviewWidget = new SessionOverviewWidget(detailsPaneStack);
    detailsPaneStack->addWidget(sessionOverviewWidget);
    detailsPaneStack->addWidget(ui->tabWidget);
    ui->splitter_2->insertWidget(detailsIndex, detailsPaneStack);
    detailsPaneStack->setCurrentWidget(sessionOverviewWidget);
}

void MainWindow::showTorrentDetails(bool torrentSelected)
{
    if (!detailsPaneStack)
        return;

    const bool showOverview =
        sessionOverviewEnabled && !torrentSelected;
    detailsPaneStack->setCurrentWidget(
        showOverview
            ? static_cast<QWidget *>(sessionOverviewWidget)
            : static_cast<QWidget *>(ui->tabWidget));
}

void MainWindow::bringToFront()
{
    // Route external activation through the tray controller so platform state
    // such as the macOS Dock activation policy is restored before the window.
    showMainWindow();
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

    QSettings settings;
    sessionOverviewEnabled =
        settings.value(SettingsKeys::ShowSessionOverview, false).toBool();
    if (showBandwidthGraphAction) {
        const QSignalBlocker blocker(showBandwidthGraphAction);
        showBandwidthGraphAction->setChecked(sessionOverviewEnabled);
    }
    showTorrentDetails(isValidTorrentKey(currentTorrentKey()));

    if (trayController)
        trayController->applySettings();
}

void MainWindow::handleTorrentsReceived(const QVector<torrent> &torrents)
{
    // The source model is updated by an earlier connection to the same signal.
    // This handler fans the immutable snapshot out to secondary UI services.
    if (torrentListController)
        torrentListController->markTorrentListLoaded();

    if (notificationController)
        notificationController->processTorrentList(torrents);

    if (torrentFilterController)
        torrentFilterController->rebuild(torrents);

    if (statusBarController)
        statusBarController->updateTorrents(torrents);

    if (sessionOverviewWidget) {
        double downloadRate = 0.0;
        double uploadRate = 0.0;
        int downloadingCount = 0;
        int seedingCount = 0;
        int waitingCount = 0;

        for (const torrent &item : torrents) {
            downloadRate += item.getRateDownloadBytesPerSecond();
            uploadRate += item.getRateUploadBytesPerSecond();

            switch (item.getStatusValue()) {
            case 3:
                ++waitingCount;
                break;
            case 4:
                ++downloadingCount;
                break;
            case 6:
                ++seedingCount;
                break;
            default:
                break;
            }
        }

        sessionOverviewWidget->addSample(
            QDateTime::currentMSecsSinceEpoch(),
            downloadRate,
            uploadRate,
            downloadingCount,
            seedingCount,
            waitingCount);
    }

    pollingCoordinator->handleTorrentListReceived();
}

void MainWindow::refreshRemoteFreeSpace()
{
    if (!client->capabilities().freeSpaceQuery)
        return;

    if (pollingCoordinator->remoteDownloadDirectory().trimmed().isEmpty()) {
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

    pollingCoordinator->requestFreeSpaceNow();
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
        applicationCommandController->setAlternativeSpeedState(
            false, confirmedAlternativeSpeedEnabled);
        return;
    }

    // Reflect the requested state optimistically. commandFailed restores the
    // last value confirmed by session-get.
    applicationCommandController->setAlternativeSpeedState(true, enabled);

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

void MainWindow::showStatistics()
{
    StatisticsDialog dialog(client, this);
    dialog.exec();
}

void MainWindow::showSessionSettings()
{
    // Always populate the dialog from a fresh session snapshot; cached values
    // may predate external changes made through another client.
    openSessionSettingsWhenReceived = true;

    if (statusBarController) {
        statusBarController->showMessage(
            tr("Loading server settings..."),
            3000
            );
    }

    client->getSessionSettings();
}

void MainWindow::handleSessionSettingsReceived(const QJsonObject &sessionSettings)
{
    // One session-get response updates shared state first, then services any
    // pending UI flows that requested the snapshot.
    cachedSessionSettings = sessionSettings;

    const QString remoteDownloadDir =
        sessionSettings.value(QStringLiteral("download-dir")).toString();
    pollingCoordinator->setRemoteDownloadDirectory(remoteDownloadDir, false);

    confirmedAlternativeSpeedEnabled =
        sessionSettings.value(QStringLiteral("alt-speed-enabled")).toBool(false);
    alternativeSpeedSettingsAvailable =
        sessionSettings.contains(QStringLiteral("alt-speed-enabled"));
    applicationCommandController->setAlternativeSpeedState(
        alternativeSpeedSettingsAvailable,
        confirmedAlternativeSpeedEnabled);

    if (torrentListController)
        torrentListController->setSequentialDownloadSupported(
            client->capabilities().sequentialDownload
            );

    if (torrentAddController)
        torrentAddController->setDefaultDownloadDir(remoteDownloadDir);

    if (torrentListController)
        torrentListController->setDefaultDownloadDir(remoteDownloadDir);

    if (statusBarController)
        statusBarController->setSessionSettings(sessionSettings);

    if (!pollingCoordinator->requestFreeSpaceNow() && statusBarController) {
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
    dialog.configureBackend(client->backendName(), client->capabilities());
    dialog.setSessionSettings(sessionSettings);

    connect(&dialog, &SessionSettingsDialog::portTestRequested,
            client, &TorrentBackend::testPortForwarding);

    connect(client, &TorrentBackend::portTestFinished,
            &dialog, &SessionSettingsDialog::setPortTestResult);

    connect(client, &TorrentBackend::portTestFailed,
            &dialog, &SessionSettingsDialog::setPortTestFailed);

    connect(&dialog, &SessionSettingsDialog::blocklistUpdateRequested,
            client, &TorrentBackend::updateBlocklist);

    connect(client, &TorrentBackend::blocklistUpdateFinished,
            &dialog, &SessionSettingsDialog::setBlocklistUpdateResult);

    connect(client, &TorrentBackend::blocklistUpdateFailed,
            &dialog, &SessionSettingsDialog::setBlocklistUpdateFailed);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QJsonObject changes = dialog.changedSettings();

    if (changes.isEmpty()) {
        if (statusBarController) {
            statusBarController->showMessage(
                tr("No server settings changed"),
                3000
                );
        }
        return;
    }

    client->setSessionSettings(changes);

    if (changes.contains(QStringLiteral("download-dir"))) {
        const QString remoteDownloadDir =
            changes.value(QStringLiteral("download-dir")).toString();
        pollingCoordinator->setRemoteDownloadDirectory(remoteDownloadDir,
                                                       false);

        if (torrentAddController)
            torrentAddController->setDefaultDownloadDir(remoteDownloadDir);

        if (torrentListController)
            torrentListController->setDefaultDownloadDir(remoteDownloadDir);

        pollingCoordinator->requestFreeSpaceNow();
    }

    if (statusBarController) {
        statusBarController->showMessage(
            tr("Saving server settings..."),
            3000
            );
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && torrentAddController) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        auto *targetWidget = qobject_cast<QWidget *>(watched);

        if (!keyEvent->isAutoRepeat()
            && keyEvent->matches(QKeySequence::Paste)
            && targetWidget
            && targetWidget->window() == this
            && !isEditablePasteTarget(QApplication::focusWidget())) {
            const QString magnetLink = clipboardMagnetCandidate();

            if (!magnetLink.isEmpty()) {
                // Avoid opening the optional add dialog while Qt is still
                // dispatching the paste key event.
                QTimer::singleShot(0, this, [this, magnetLink]() {
                    if (torrentAddController)
                        torrentAddController->addMagnetLink(magnetLink);
                });
                return true;
            }
        }
    }

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
