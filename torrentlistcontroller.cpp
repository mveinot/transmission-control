#include "torrentlistcontroller.h"

#include "torrentbackend.h"
#include "settingskeys.h"
#include "torrentmodel.h"
#include "torrentpropertiesdialog.h"
#include "torrentsortproxymodel.h"
#include "tableplaceholdercontroller.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QPair>
#include <QSettings>
#include <QSize>
#include <QTableView>
#include <QtGlobal>
#include <QVBoxLayout>
#include <QVector>


namespace {

struct TorrentColumnDefinition
{
    TorrentModel::Column column;
    const char *id;
    bool defaultVisible;
    bool userConfigurable;
};

const QVector<TorrentColumnDefinition> &torrentColumnDefinitions()
{
    static const QVector<TorrentColumnDefinition> definitions = {
        { TorrentModel::IdColumn, "id", false, false },
        { TorrentModel::NameColumn, "name", true, false },
        { TorrentModel::SizeColumn, "size", true, true },
        { TorrentModel::PercentDoneColumn, "completed", true, true },
        { TorrentModel::StatusColumn, "status", true, true },
        { TorrentModel::HealthColumn, "health", true, true },
        { TorrentModel::TrackerColumn, "tracker", true, true },
        { TorrentModel::RateDownloadColumn, "down", true, true },
        { TorrentModel::RateUploadColumn, "up", true, true },
        { TorrentModel::UploadRatioColumn, "ratio", true, true },
        { TorrentModel::EtaColumn, "eta", true, true },
        { TorrentModel::QueueColumn, "queue", true, true },
        { TorrentModel::AddedColumn, "added", false, true },
        { TorrentModel::DownloadedEverColumn, "downloaded", false, true },
        { TorrentModel::UploadedEverColumn, "uploaded", false, true },
        { TorrentModel::DownloadDirColumn, "downloadDir", false, true },
        { TorrentModel::SeedsColumn, "seeds", false, true },
        { TorrentModel::PeersConnectedColumn, "peers", false, true },
    };

    return definitions;
}

const TorrentColumnDefinition *definitionForColumn(int column)
{
    for (const TorrentColumnDefinition &definition : torrentColumnDefinitions()) {
        if (definition.column == column)
            return &definition;
    }

    return nullptr;
}


QStringList defaultVisibleColumnIds()
{
    QStringList ids;

    for (const TorrentColumnDefinition &definition : torrentColumnDefinitions()) {
        if (definition.defaultVisible)
            ids.append(QString::fromLatin1(definition.id));
    }

    return ids;
}

QString columnTitle(QAbstractItemModel *model, int column)
{
    if (!model)
        return QString();

    return model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
}

} // namespace

TorrentListController::TorrentListController(QTableView *tableView,
                                             TorrentSortProxyModel *proxyModel,
                                             TorrentBackend *client,
                                             QWidget *dialogParent,
                                             QObject *parent)
    : QObject(parent)
    , m_tableView(tableView)
    , m_proxyModel(proxyModel)
    , m_client(client)
    , m_dialogParent(dialogParent)
{
}

void TorrentListController::setup(const ActionSet &actions)
{
    m_actions = actions;

    if (!m_tableView)
        return;

    m_tableView->setModel(m_proxyModel);

    m_placeholderController = std::make_unique<TablePlaceholderController>(m_tableView, this);
    m_placeholderController->setMessage(tr("Loading torrents…"));

    applyDefaultColumnVisibility();
    m_tableView->setSortingEnabled(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->sortByColumn(TorrentModel::NameColumn, Qt::AscendingOrder);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tableView->setIconSize(QSize(16, 16));

    connect(m_tableView,
            &QTableView::doubleClicked,
            this,
            [this](const QModelIndex &index) {
                if (index.isValid())
                    showSelectedTorrentProperties();
            });

    if (QHeaderView *header = m_tableView->horizontalHeader()) {
        configureHorizontalHeader();

        connect(header,
                &QHeaderView::customContextMenuRequested,
                this,
                &TorrentListController::showHeaderContextMenu);

        connect(header,
                &QHeaderView::sectionMoved,
                this,
                [this]() { saveViewState(); });
    }

    if (m_tableView->selectionModel()) {
        connect(m_tableView->selectionModel(),
                &QItemSelectionModel::selectionChanged,
                this,
                [this]() {
                    updateActionState();
                    updateCurrentTorrentSelection();
                });

        connect(m_tableView->selectionModel(),
                &QItemSelectionModel::currentChanged,
                this,
                [this]() {
                    updateActionState();
                    updateCurrentTorrentSelection();
                });
    }

    if (m_proxyModel) {
        connect(m_proxyModel,
                &QAbstractItemModel::modelReset,
                this,
                [this]() {
                    updatePlaceholder();
                    updateCurrentTorrentSelection();
                });
        connect(m_proxyModel,
                &QAbstractItemModel::rowsInserted,
                this,
                &TorrentListController::updatePlaceholder);
        connect(m_proxyModel,
                &QAbstractItemModel::rowsRemoved,
                this,
                [this]() {
                    updatePlaceholder();
                    updateCurrentTorrentSelection();
                });
        connect(m_proxyModel,
                &QAbstractItemModel::layoutChanged,
                this,
                [this]() {
                    updatePlaceholder();
                    updateCurrentTorrentSelection();
                });
    }

    connect(m_tableView,
            &QTableView::customContextMenuRequested,
            this,
            &TorrentListController::showContextMenu);

    if (m_actions.forceStart) {
        connect(m_actions.forceStart,
                &QAction::triggered,
                this,
                &TorrentListController::forceStartSelectedTorrents);
    }

    if (m_client) {
        // Server switches can change capabilities without changing the current
        // table selection, so refresh persistent action state explicitly.
        connect(m_client, &TorrentBackend::capabilitiesChanged,
                this, [this](const TorrentBackendCapabilities &) {
                    updateActionState();
                });
    }

    updateActionState();
    updatePlaceholder();
}

void TorrentListController::beginTorrentListRefresh()
{
    m_torrentListLoadFailed = false;
    m_torrentListLoadFailureMessage.clear();

    if (!m_torrentListLoaded)
        updatePlaceholder();
}

void TorrentListController::markTorrentListLoaded()
{
    m_torrentListLoaded = true;
    m_torrentListLoadFailed = false;
    m_torrentListLoadFailureMessage.clear();
    updatePlaceholder();
}

void TorrentListController::markTorrentListLoadFailed(const QString &message)
{
    if (m_torrentListLoaded)
        return;

    m_torrentListLoadFailed = true;
    m_torrentListLoadFailureMessage = message;
    updatePlaceholder();
}

void TorrentListController::restoreViewState()
{
    if (!m_tableView)
        return;

    QSettings settings;

    const QByteArray horizontalState =
        settings.value(SettingsKeys::TorrentTableHeaderState).toByteArray();

    if (!horizontalState.isEmpty() && m_tableView->horizontalHeader())
        m_tableView->horizontalHeader()->restoreState(horizontalState);

    configureHorizontalHeader();

    const QByteArray verticalState =
        settings.value(SettingsKeys::TorrentTableVerticalHeaderState).toByteArray();

    if (!verticalState.isEmpty() && m_tableView->verticalHeader())
        m_tableView->verticalHeader()->restoreState(verticalState);

    applySavedColumnVisibility();
}

void TorrentListController::saveViewState() const
{
    if (!m_tableView)
        return;

    QSettings settings;

    if (m_tableView->horizontalHeader()) {
        settings.setValue(SettingsKeys::TorrentTableHeaderState,
                          m_tableView->horizontalHeader()->saveState());
    }

    if (m_tableView->verticalHeader()) {
        settings.setValue(SettingsKeys::TorrentTableVerticalHeaderState,
                          m_tableView->verticalHeader()->saveState());
    }

    QStringList visibleColumnIds;

    for (const TorrentColumnDefinition &definition : torrentColumnDefinitions()) {
        if (!definition.userConfigurable && definition.column != TorrentModel::NameColumn)
            continue;

        if (!m_tableView->isColumnHidden(definition.column))
            visibleColumnIds.append(QString::fromLatin1(definition.id));
    }

    settings.setValue(SettingsKeys::TorrentTableVisibleColumns, visibleColumnIds);
}

TorrentKey TorrentListController::currentTorrentKey() const
{
    if (!m_tableView || !m_proxyModel)
        return {};

    const QItemSelectionModel *selection = m_tableView->selectionModel();

    if (!selection)
        return {};

    const QModelIndexList proxyRows = selection->selectedRows();

    if (proxyRows.isEmpty())
        return {};

    QModelIndex proxyIndex = m_tableView->currentIndex();

    if (!proxyIndex.isValid()
        || !selection->isRowSelected(proxyIndex.row(), proxyIndex.parent())) {
        proxyIndex = proxyRows.first();
    }

    const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return {};

    return sourceIndex.data(Qt::UserRole).toString();
}

QList<TorrentKey> TorrentListController::selectedTorrentKeys() const
{
    QList<TorrentKey> ids;

    if (!m_tableView || !m_proxyModel)
        return ids;

    const QItemSelectionModel *selection = m_tableView->selectionModel();

    if (!selection)
        return ids;

    const QModelIndexList proxyRows = selection->selectedRows();

    for (const QModelIndex &proxyIndex : proxyRows) {
        const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);

        if (!sourceIndex.isValid())
            continue;

        const TorrentKey id = sourceIndex.data(Qt::UserRole).toString();

        if (isValidTorrentKey(id))
            ids.append(id);
    }

    return ids;
}

QStringList TorrentListController::selectedTorrentNames() const
{
    QStringList names;

    if (!m_tableView || !m_proxyModel)
        return names;

    const QItemSelectionModel *selection = m_tableView->selectionModel();

    if (!selection)
        return names;

    const QModelIndexList proxyRows = selection->selectedRows();

    for (const QModelIndex &proxyIndex : proxyRows) {
        const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);

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

void TorrentListController::setCurrentTorrentDetails(TorrentKey torrentKey,
                                                     const QString &hashString,
                                                     const QString &magnetLink)
{
    m_currentDetailsTorrentKey = torrentKey;
    m_currentTorrentHashString = hashString;
    m_currentTorrentMagnetLink = magnetLink;
}

void TorrentListController::clearCurrentTorrentDetails()
{
    m_currentDetailsTorrentKey.clear();
    m_currentTorrentHashString.clear();
    m_currentTorrentMagnetLink.clear();
    m_currentSequentialDownloadTorrentKey.clear();
    m_currentSequentialDownloadEnabled = false;
    m_currentSequentialDownloadKnown = false;
    m_currentBandwidthPriorityTorrentKey.clear();
    m_currentBandwidthPriority = 0;
    m_currentBandwidthPriorityKnown = false;
}

void TorrentListController::setDefaultDownloadDir(const QString &downloadDir)
{
    m_defaultDownloadDir = downloadDir.trimmed();
}

void TorrentListController::setSequentialDownloadSupported(bool supported)
{
    m_sequentialDownloadSupported = supported;
}

void TorrentListController::setCurrentTorrentSequentialDownload(TorrentKey torrentKey,
                                                                bool enabled,
                                                                bool known)
{
    m_currentSequentialDownloadTorrentKey = torrentKey;
    m_currentSequentialDownloadEnabled = enabled;
    m_currentSequentialDownloadKnown = known;
}

void TorrentListController::setCurrentTorrentBandwidthPriority(TorrentKey torrentKey,
                                                              int priority,
                                                              bool known)
{
    m_currentBandwidthPriorityTorrentKey = torrentKey;
    m_currentBandwidthPriority = priority;
    m_currentBandwidthPriorityKnown = known;
}

void TorrentListController::setCurrentDetailsDownloadDirProvider(const std::function<QString()> &provider)
{
    m_currentDetailsDownloadDirProvider = provider;
}

void TorrentListController::handleTableClicked(const QModelIndex &proxyIndex)
{
    Q_UNUSED(proxyIndex);
    updateCurrentTorrentSelection();
}

void TorrentListController::updateCurrentTorrentSelection()
{
    // Downstream controllers receive stable IDs, never volatile proxy rows.
    const TorrentKey torrentKey = currentTorrentKey();

    if (torrentKey == m_lastEmittedTorrentKey)
        return;

    m_lastEmittedTorrentKey = torrentKey;

    if (isValidTorrentKey(torrentKey)) {
        emit torrentSelected(torrentKey);
        return;
    }

    clearCurrentTorrentDetails();
    emit torrentSelectionCleared();
}

void TorrentListController::updateActionState()
{
    const bool hasSelection =
        m_tableView &&
        m_tableView->selectionModel() &&
        !m_tableView->selectionModel()->selectedRows().isEmpty();

    if (m_actions.start)
        m_actions.start->setEnabled(hasSelection);

    if (m_actions.stop)
        m_actions.stop->setEnabled(hasSelection);

    if (m_actions.deleteTorrent)
        m_actions.deleteTorrent->setEnabled(hasSelection);

    if (m_actions.verify)
        m_actions.verify->setEnabled(hasSelection);

    if (m_actions.reannounce)
        m_actions.reannounce->setEnabled(hasSelection);

    if (m_actions.forceStart) {
        const bool supported =
            m_client && m_client->capabilities().forceStart;
        m_actions.forceStart->setVisible(supported);
        m_actions.forceStart->setEnabled(supported && hasSelection);
    }
}

void TorrentListController::showContextMenu(const QPoint &pos)
{
    if (!m_tableView)
        return;

    const QModelIndex index = m_tableView->indexAt(pos);

    if (!index.isValid())
        return;

    QItemSelectionModel *selection = m_tableView->selectionModel();

    if (selection && !selection->isSelected(index)) {
        selection->select(
            index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
            );

        m_tableView->setCurrentIndex(index);
    }

    const QList<TorrentKey> contextTorrentIds = selectedTorrentKeys();
    const bool hasSelection = !contextTorrentIds.isEmpty();
    const bool hasSingleSelection = contextTorrentIds.size() == 1;
    const bool canCopyCurrentTorrentDetails =
        hasSingleSelection
        && m_currentDetailsTorrentKey == contextTorrentIds.first();
    const TorrentBackendCapabilities capabilities =
        m_client ? m_client->capabilities() : TorrentBackendCapabilities{};

    QMenu menu(m_dialogParent);

    if (m_actions.start)
        menu.addAction(m_actions.start);

    if (m_actions.stop)
        menu.addAction(m_actions.stop);

    if (m_actions.forceStart && capabilities.forceStart)
        menu.addAction(m_actions.forceStart);

    menu.addSeparator();

    if (m_actions.verify)
        menu.addAction(m_actions.verify);

    if (m_actions.reannounce)
        menu.addAction(m_actions.reannounce);

    menu.addSeparator();

    QMenu *priorityMenu = menu.addMenu(tr("Priority"));
    // A submenu is represented in its parent by menuAction(). Showing the
    // QMenu itself can create a detached popup at the screen origin.
    priorityMenu->menuAction()->setVisible(capabilities.torrentBandwidthPriority);
    priorityMenu->setEnabled(
        capabilities.torrentBandwidthPriority && hasSelection);

    QActionGroup *priorityGroup = new QActionGroup(priorityMenu);
    priorityGroup->setExclusive(true);

    QAction *lowPriorityAction = priorityMenu->addAction(tr("Low"));
    QAction *normalPriorityAction = priorityMenu->addAction(tr("Normal"));
    QAction *highPriorityAction = priorityMenu->addAction(tr("High"));

    const QList<QPair<QAction *, int>> priorityActions = {
        { lowPriorityAction, -1 },
        { normalPriorityAction, 0 },
        { highPriorityAction, 1 }
    };

    const bool canShowCurrentPriority =
        hasSingleSelection
        && m_currentBandwidthPriorityKnown
        && m_currentBandwidthPriorityTorrentKey == contextTorrentIds.first();

    for (const auto &priorityAction : priorityActions) {
        QAction *action = priorityAction.first;
        const int priority = priorityAction.second;

        action->setCheckable(true);
        action->setData(priority);
        action->setChecked(canShowCurrentPriority && m_currentBandwidthPriority == priority);
        priorityGroup->addAction(action);

        connect(action,
                &QAction::triggered,
                this,
                [this, priority]() {
                    setSelectedTorrentsBandwidthPriority(priority);
                });
    }

    QAction *sequentialDownloadAction = menu.addAction(tr("Sequential Download"));
    sequentialDownloadAction->setCheckable(true);

    const bool canSetSequentialDownload =
        m_sequentialDownloadSupported
        && hasSingleSelection
        && m_currentSequentialDownloadKnown
        && m_currentSequentialDownloadTorrentKey == contextTorrentIds.first();

    sequentialDownloadAction->setEnabled(canSetSequentialDownload);
    sequentialDownloadAction->setChecked(
        canSetSequentialDownload && m_currentSequentialDownloadEnabled
        );

    if (!m_sequentialDownloadSupported) {
        sequentialDownloadAction->setToolTip(
            tr("Sequential download is not supported by the active torrent backend.")
            );
    } else if (!canSetSequentialDownload) {
        sequentialDownloadAction->setToolTip(
            tr("Select one torrent and wait for its details to load.")
            );
    }

    menu.addSeparator();

    QMenu *queueMenu = menu.addMenu(tr("Queue"));
    queueMenu->menuAction()->setVisible(capabilities.queueManagement);
    queueMenu->setEnabled(capabilities.queueManagement && hasSelection);

    QAction *moveTopAction = queueMenu->addAction(tr("Move to Top"));
    QAction *moveUpAction = queueMenu->addAction(tr("Move Up"));
    QAction *moveDownAction = queueMenu->addAction(tr("Move Down"));
    QAction *moveBottomAction = queueMenu->addAction(tr("Move to Bottom"));

    menu.addSeparator();

    QAction *setLocationAction = menu.addAction(tr("Set Location…"));
    setLocationAction->setVisible(capabilities.torrentLocation);
    setLocationAction->setEnabled(
        capabilities.torrentLocation && hasSelection);

    QAction *propertiesAction = menu.addAction(tr("Properties…"));
    propertiesAction->setEnabled(hasSingleSelection);

    menu.addSeparator();

    QMenu *copyMenu = menu.addMenu(tr("Copy"));

    QAction *copyMagnetAction = copyMenu->addAction(tr("Magnet Link"));
    QAction *copyHashAction = copyMenu->addAction(tr("Hash"));

    copyMagnetAction->setEnabled(
        canCopyCurrentTorrentDetails
        && !m_currentTorrentMagnetLink.trimmed().isEmpty()
        );

    copyHashAction->setEnabled(
        canCopyCurrentTorrentDetails
        && !m_currentTorrentHashString.trimmed().isEmpty()
        );

    copyMenu->setEnabled(copyMagnetAction->isEnabled() || copyHashAction->isEnabled());

    menu.addSeparator();

    if (m_actions.deleteTorrent)
        menu.addAction(m_actions.deleteTorrent);

    connect(moveTopAction,
            &QAction::triggered,
            this,
            &TorrentListController::queueMoveSelectedTop);

    connect(moveUpAction,
            &QAction::triggered,
            this,
            &TorrentListController::queueMoveSelectedUp);

    connect(moveDownAction,
            &QAction::triggered,
            this,
            &TorrentListController::queueMoveSelectedDown);

    connect(moveBottomAction,
            &QAction::triggered,
            this,
            &TorrentListController::queueMoveSelectedBottom);

    connect(setLocationAction,
            &QAction::triggered,
            this,
            &TorrentListController::setSelectedTorrentsLocation);

    connect(sequentialDownloadAction,
            &QAction::triggered,
            this,
            &TorrentListController::setSelectedTorrentsSequentialDownload);

    connect(propertiesAction,
            &QAction::triggered,
            this,
            &TorrentListController::showSelectedTorrentProperties);

    connect(copyMagnetAction,
            &QAction::triggered,
            this,
            &TorrentListController::copySelectedTorrentMagnetLink);

    connect(copyHashAction,
            &QAction::triggered,
            this,
            &TorrentListController::copySelectedTorrentHash);

    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}

void TorrentListController::showHeaderContextMenu(const QPoint &pos)
{
    if (!m_tableView || !m_tableView->horizontalHeader())
        return;

    QMenu menu(m_dialogParent);
    QActionGroup *columnGroup = new QActionGroup(&menu);
    columnGroup->setExclusive(false);

    for (const TorrentColumnDefinition &definition : torrentColumnDefinitions()) {
        if (!definition.userConfigurable && definition.column != TorrentModel::NameColumn)
            continue;

        QAction *action = menu.addAction(
            columnTitle(m_proxyModel, definition.column)
            );

        action->setCheckable(true);
        action->setChecked(!m_tableView->isColumnHidden(definition.column));
        action->setData(definition.column);

        if (definition.column == TorrentModel::NameColumn)
            action->setEnabled(false);

        columnGroup->addAction(action);

        connect(action,
                &QAction::toggled,
                this,
                [this, column = int(definition.column)](bool checked) {
                    setColumnVisible(column, checked);
                });
    }

    menu.addSeparator();

    QAction *resetAction = menu.addAction(tr("Reset Columns"));

    connect(resetAction,
            &QAction::triggered,
            this,
            &TorrentListController::resetColumns);

    menu.exec(m_tableView->horizontalHeader()->mapToGlobal(pos));
}

void TorrentListController::deleteSelectedTorrents()
{
    const QList<TorrentKey> ids = selectedTorrentKeys();
    const QStringList names = selectedTorrentNames();

    if (ids.isEmpty()) {
        QMessageBox::information(
            m_dialogParent,
            tr("Delete Torrent"),
            tr("No torrent is selected.")
            );
        return;
    }

    QMessageBox msgBox(m_dialogParent);
    msgBox.setWindowTitle(tr("Delete Torrent"));
    msgBox.setIcon(QMessageBox::Warning);
    QPushButton *torrentOnlyButton = nullptr;
    QPushButton *torrentAndDataButton = nullptr;
    QPushButton *noButton = nullptr;

    if (ids.size() == 1) {
        msgBox.setText(tr("Delete the selected torrent?"));
        msgBox.setInformativeText(
            tr("Remove only the torrent from the server, or also delete the downloaded data?\n\n"
               "View details to see the affected torrent name.")
            );
        msgBox.setDetailedText(names.value(0));

        torrentOnlyButton = msgBox.addButton(tr("Torrent only"), QMessageBox::AcceptRole);
        torrentAndDataButton = msgBox.addButton(tr("Torrent and data"), QMessageBox::DestructiveRole);
    } else {
        const QString preview = names.join("\n");

        msgBox.setText(tr("Delete %1 selected torrents?").arg(ids.size()));
        msgBox.setInformativeText(
            tr("Remove only the torrents from the server, or also delete the downloaded data?\n\n"
               "View details to see the affected torrent names.")
            );
        msgBox.setDetailedText(preview);

        torrentOnlyButton = msgBox.addButton(tr("Torrents only"), QMessageBox::AcceptRole);
        torrentAndDataButton = msgBox.addButton(tr("Torrents and data"), QMessageBox::DestructiveRole);
    }

    noButton = msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);

    msgBox.setDefaultButton(noButton);
#ifdef Q_OS_MACOS
    msgBox.setWindowModality(Qt::WindowModal);
    msgBox.setWindowFlag(Qt::Sheet, true);
#endif
    msgBox.exec();

    if (msgBox.clickedButton() == noButton)
        return;

    if (msgBox.clickedButton() == torrentOnlyButton) {
        m_client->removeTorrents(ids, false);
        return;
    }

    if (msgBox.clickedButton() == torrentAndDataButton) {
        m_client->removeTorrents(ids, true);
        return;
    }
}

void TorrentListController::startSelectedTorrents()
{
    invokeSelectedTorrentCommand(&TorrentBackend::startTorrents,
                                 tr("Starting %1 torrent(s)..."));
}

void TorrentListController::stopSelectedTorrents()
{
    invokeSelectedTorrentCommand(&TorrentBackend::stopTorrents,
                                 tr("Stopping %1 torrent(s)..."));
}

void TorrentListController::reannounceSelectedTorrents()
{
    invokeSelectedTorrentCommand(&TorrentBackend::reannounceTorrents,
                                 tr("Reannouncing %1 torrent(s)..."));
}

void TorrentListController::verifySelectedTorrents()
{
    invokeSelectedTorrentCommand(&TorrentBackend::verifyTorrents,
                                 tr("Verifying %1 torrent(s)..."));
}

void TorrentListController::forceStartSelectedTorrents()
{
    invokeSelectedTorrentCommand(&TorrentBackend::startTorrentsNow,
                                 tr("Force starting %1 torrent(s)..."));
}

void TorrentListController::setSelectedTorrentsLocation()
{
    const QList<TorrentKey> ids = selectedTorrentKeys();

    if (ids.isEmpty()) {
        emit statusMessageRequested(tr("No torrent selected."), 3000);
        return;
    }

    QString initialLocation = m_defaultDownloadDir.trimmed();

    if (ids.size() == 1 && currentTorrentKey() == ids.first() && m_currentDetailsDownloadDirProvider) {
        const QString currentDownloadDir = m_currentDetailsDownloadDirProvider().trimmed();

        if (!currentDownloadDir.isEmpty() && currentDownloadDir != tr("Unknown"))
            initialLocation = currentDownloadDir;
    }

    QDialog dialog(m_dialogParent);
    dialog.setWindowTitle(tr("Set Location"));

    auto *layout = new QVBoxLayout(&dialog);

    auto *descriptionLabel = new QLabel(
        tr("Set the download location on the torrent server."),
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
    if (!m_client->capabilities().torrentLocationModeSelection) {
        // Some backends expose relocation only and cannot merely associate
        // already-present data with a new path.
        moveDataCheckBox->setChecked(true);
        moveDataCheckBox->hide();
        descriptionLabel->setText(
            tr("Move the torrent's existing data to a new location on the "
               "torrent server."));
    }
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
            m_dialogParent,
            tr("Set Location"),
            tr("The download location cannot be empty.")
            );
        return;
    }

    const bool moveData = moveDataCheckBox->isChecked();

    m_client->setTorrentLocation(ids, location, moveData);

    emit statusMessageRequested(
        moveData
            ? tr("Moving %1 torrent(s) to %2...").arg(ids.size()).arg(location)
            : tr("Setting location for %1 torrent(s) to %2...").arg(ids.size()).arg(location),
        5000
        );
}

void TorrentListController::setSelectedTorrentsSequentialDownload(bool enabled)
{
    const QList<TorrentKey> ids = selectedTorrentKeys();

    if (ids.isEmpty()) {
        emit statusMessageRequested(tr("No torrent selected."), 3000);
        return;
    }

    if (!m_sequentialDownloadSupported) {
        emit statusMessageRequested(
            tr("Sequential download is not supported by the active torrent backend."),
            5000
            );
        return;
    }

    m_client->setTorrentsSequentialDownload(ids, enabled);

    emit statusMessageRequested(
        enabled
            ? tr("Enabling sequential download for %1 torrent(s)...").arg(ids.size())
            : tr("Disabling sequential download for %1 torrent(s)...").arg(ids.size()),
        3000
        );
}

void TorrentListController::setSelectedTorrentsBandwidthPriority(int priority)
{
    const QList<TorrentKey> ids = selectedTorrentKeys();

    if (ids.isEmpty()) {
        emit statusMessageRequested(tr("No torrent selected."), 3000);
        return;
    }

    const int normalizedPriority = qBound(-1, priority, 1);

    m_client->setTorrentsBandwidthPriority(ids, normalizedPriority);

    QString priorityLabel;

    switch (normalizedPriority) {
    case -1:
        priorityLabel = tr("Low");
        break;
    case 1:
        priorityLabel = tr("High");
        break;
    case 0:
    default:
        priorityLabel = tr("Normal");
        break;
    }

    emit statusMessageRequested(
        tr("Setting priority for %1 torrent(s) to %2...").arg(ids.size()).arg(priorityLabel),
        3000
        );
}

void TorrentListController::showSelectedTorrentProperties()
{
    const QList<TorrentKey> ids = selectedTorrentKeys();

    if (ids.size() != 1) {
        emit statusMessageRequested(tr("Select one torrent to edit properties."), 3000);
        return;
    }

    TorrentPropertiesDialog dialog(m_client, ids.first(), m_dialogParent);
    dialog.exec();

    emit torrentListRefreshRequested();
    refreshCurrentTorrentDetails();
}

void TorrentListController::copySelectedTorrentMagnetLink()
{
    copyTextToClipboard(
        m_currentTorrentMagnetLink,
        tr("Magnet link copied to clipboard.")
        );
}

void TorrentListController::copySelectedTorrentHash()
{
    copyTextToClipboard(
        m_currentTorrentHashString,
        tr("Torrent hash copied to clipboard.")
        );
}

void TorrentListController::queueMoveSelectedTop()
{
    invokeSelectedTorrentCommand(&TorrentBackend::queueMoveTop, QString());
}

void TorrentListController::queueMoveSelectedUp()
{
    invokeSelectedTorrentCommand(&TorrentBackend::queueMoveUp, QString());
}

void TorrentListController::queueMoveSelectedDown()
{
    invokeSelectedTorrentCommand(&TorrentBackend::queueMoveDown, QString());
}

void TorrentListController::queueMoveSelectedBottom()
{
    invokeSelectedTorrentCommand(&TorrentBackend::queueMoveBottom, QString());
}

void TorrentListController::invokeSelectedTorrentCommand(
    void (TorrentBackend::*command)(const QList<TorrentKey> &),
    const QString &message)
{
    const QList<TorrentKey> ids = selectedTorrentKeys();

    if (ids.isEmpty()) {
        emit statusMessageRequested(tr("No torrent selected."), 3000);
        return;
    }

    (m_client->*command)(ids);

    if (!message.isEmpty())
        emit statusMessageRequested(message.arg(ids.size()), 3000);
}

void TorrentListController::copyTextToClipboard(const QString &text,
                                                const QString &statusMessage)
{
    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty())
        return;

    QApplication::clipboard()->setText(trimmed);

    if (!statusMessage.isEmpty())
        emit statusMessageRequested(statusMessage, 3000);
}

void TorrentListController::refreshCurrentTorrentDetails()
{
    const TorrentKey torrentKey = currentTorrentKey();

    if (isValidTorrentKey(torrentKey))
        emit torrentDetailsRefreshRequested(torrentKey);
}

void TorrentListController::applyDefaultColumnVisibility()
{
    if (!m_tableView)
        return;

    const QStringList visibleIds = defaultVisibleColumnIds();

    for (const TorrentColumnDefinition &definition : torrentColumnDefinitions()) {
        const bool visible = visibleIds.contains(QString::fromLatin1(definition.id));
        m_tableView->setColumnHidden(definition.column, !visible);
    }

    m_tableView->setColumnHidden(TorrentModel::IdColumn, true);
    m_tableView->setColumnHidden(TorrentModel::NameColumn, false);
}

void TorrentListController::applySavedColumnVisibility()
{
    if (!m_tableView)
        return;

    QSettings settings;
    const QVariant storedValue =
        settings.value(SettingsKeys::TorrentTableVisibleColumns);

    if (!storedValue.isValid()) {
        applyDefaultColumnVisibility();
        return;
    }

    QStringList visibleIds = storedValue.toStringList();

    if (visibleIds.isEmpty())
        visibleIds = defaultVisibleColumnIds();

    if (!visibleIds.contains(QStringLiteral("name")))
        visibleIds.append(QStringLiteral("name"));

    for (const TorrentColumnDefinition &definition : torrentColumnDefinitions()) {
        const QString id = QString::fromLatin1(definition.id);
        const bool visible = visibleIds.contains(id);
        m_tableView->setColumnHidden(definition.column, !visible);
    }

    m_tableView->setColumnHidden(TorrentModel::IdColumn, true);
    m_tableView->setColumnHidden(TorrentModel::NameColumn, false);
}

void TorrentListController::setColumnVisible(int column, bool visible)
{
    if (!m_tableView)
        return;

    if (column == TorrentModel::IdColumn)
        return;

    if (column == TorrentModel::NameColumn) {
        m_tableView->setColumnHidden(TorrentModel::NameColumn, false);
        return;
    }

    if (!definitionForColumn(column))
        return;

    m_tableView->setColumnHidden(column, !visible);
    saveViewState();
}

void TorrentListController::restoreDefaultColumnOrder()
{
    if (!m_tableView || !m_tableView->horizontalHeader())
        return;

    QHeaderView *header = m_tableView->horizontalHeader();

    for (int logicalColumn = 0; logicalColumn < TorrentModel::ColumnCount; ++logicalColumn) {
        const int currentVisualIndex = header->visualIndex(logicalColumn);

        if (currentVisualIndex >= 0 && currentVisualIndex != logicalColumn)
            header->moveSection(currentVisualIndex, logicalColumn);
    }
}

void TorrentListController::configureHorizontalHeader()
{
    if (!m_tableView || !m_tableView->horizontalHeader())
        return;

    QHeaderView *header = m_tableView->horizontalHeader();

    header->setContextMenuPolicy(Qt::CustomContextMenu);
    header->setSectionsClickable(true);
    header->setSectionsMovable(true);
    header->setFirstSectionMovable(true);
    header->setHighlightSections(false);
}

void TorrentListController::resetColumns()
{
    if (!m_tableView)
        return;

    QSettings settings;
    settings.remove(SettingsKeys::TorrentTableHeaderState);
    settings.remove(SettingsKeys::TorrentTableVerticalHeaderState);
    settings.remove(SettingsKeys::TorrentTableVisibleColumns);

    if (m_tableView->horizontalHeader())
        m_tableView->horizontalHeader()->reset();

    if (m_tableView->verticalHeader())
        m_tableView->verticalHeader()->reset();

    restoreDefaultColumnOrder();
    applyDefaultColumnVisibility();
    m_tableView->sortByColumn(TorrentModel::NameColumn, Qt::AscendingOrder);
    saveViewState();
}

void TorrentListController::updatePlaceholder()
{
    if (!m_placeholderController || !m_proxyModel)
        return;

    if (m_torrentListLoadFailed) {
        const QString message = m_torrentListLoadFailureMessage.trimmed().isEmpty()
                                    ? tr("Could not load torrents.")
                                    : tr("Could not load torrents.\n%1").arg(m_torrentListLoadFailureMessage);
        m_placeholderController->setMessage(message);
        return;
    }

    if (!m_torrentListLoaded) {
        m_placeholderController->setMessage(tr("Loading torrents…"));
        return;
    }

    const QAbstractItemModel *sourceModel = m_proxyModel->sourceModel();
    const int sourceRows = sourceModel ? sourceModel->rowCount() : 0;
    const int visibleRows = m_proxyModel->rowCount();

    if (sourceRows == 0) {
        m_placeholderController->setMessage(tr("No torrents."));
    } else if (visibleRows == 0) {
        m_placeholderController->setMessage(tr("No torrents match the current filters."));
    } else {
        m_placeholderController->clearMessage();
    }
}
