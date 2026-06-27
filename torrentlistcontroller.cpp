#include "torrentlistcontroller.h"

#include "rpc_client.h"
#include "torrentmodel.h"
#include "torrentpropertiesdialog.h"
#include "torrentsortproxymodel.h"

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
#include <QSettings>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>


namespace {

constexpr const char *TorrentTableHeaderStateKey =
    "ui/torrentTable/horizontalHeaderState/v1";
constexpr const char *TorrentTableVerticalHeaderStateKey =
    "ui/torrentTable/verticalHeaderState/v1";
constexpr const char *TorrentTableVisibleColumnsKey =
    "ui/torrentTable/visibleColumns/v1";

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
        { TorrentModel::TrackerColumn, "tracker", true, true },
        { TorrentModel::RateDownloadColumn, "down", true, true },
        { TorrentModel::RateUploadColumn, "up", true, true },
        { TorrentModel::UploadRatioColumn, "ratio", true, true },
        { TorrentModel::EtaColumn, "eta", true, true },
        { TorrentModel::QueueColumn, "queue", true, true },
        { TorrentModel::AddedColumn, "added", false, true },
        { TorrentModel::DownloadedEverColumn, "downloaded", false, true },
        { TorrentModel::UploadedEverColumn, "uploaded", false, true },
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
                                             rpc_client *client,
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
    applyDefaultColumnVisibility();
    m_tableView->setSortingEnabled(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->sortByColumn(TorrentModel::NameColumn, Qt::AscendingOrder);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);

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
                [this]() { updateActionState(); });

        connect(m_tableView->selectionModel(),
                &QItemSelectionModel::currentChanged,
                this,
                [this]() { updateActionState(); });
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

    updateActionState();
}

void TorrentListController::restoreViewState()
{
    if (!m_tableView)
        return;

    QSettings settings;

    const QByteArray horizontalState =
        settings.value(QString::fromLatin1(TorrentTableHeaderStateKey)).toByteArray();

    if (!horizontalState.isEmpty() && m_tableView->horizontalHeader())
        m_tableView->horizontalHeader()->restoreState(horizontalState);

    configureHorizontalHeader();

    const QByteArray verticalState =
        settings.value(QString::fromLatin1(TorrentTableVerticalHeaderStateKey)).toByteArray();

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
        settings.setValue(QString::fromLatin1(TorrentTableHeaderStateKey),
                          m_tableView->horizontalHeader()->saveState());
    }

    if (m_tableView->verticalHeader()) {
        settings.setValue(QString::fromLatin1(TorrentTableVerticalHeaderStateKey),
                          m_tableView->verticalHeader()->saveState());
    }

    QStringList visibleColumnIds;

    for (const TorrentColumnDefinition &definition : torrentColumnDefinitions()) {
        if (!definition.userConfigurable && definition.column != TorrentModel::NameColumn)
            continue;

        if (!m_tableView->isColumnHidden(definition.column))
            visibleColumnIds.append(QString::fromLatin1(definition.id));
    }

    settings.setValue(QString::fromLatin1(TorrentTableVisibleColumnsKey), visibleColumnIds);
}

int TorrentListController::currentTorrentId() const
{
    if (!m_tableView || !m_proxyModel)
        return -1;

    const QModelIndex proxyIndex = m_tableView->currentIndex();

    if (!proxyIndex.isValid())
        return -1;

    const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return -1;

    return sourceIndex.data(Qt::UserRole).toInt();
}

QList<int> TorrentListController::selectedTorrentIds() const
{
    QList<int> ids;

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

        const int id = sourceIndex.data(Qt::UserRole).toInt();

        if (id >= 0)
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

void TorrentListController::setCurrentTorrentDetails(int torrentId,
                                                     const QString &hashString,
                                                     const QString &magnetLink)
{
    m_currentDetailsTorrentId = torrentId;
    m_currentTorrentHashString = hashString;
    m_currentTorrentMagnetLink = magnetLink;
}

void TorrentListController::clearCurrentTorrentDetails()
{
    m_currentDetailsTorrentId = -1;
    m_currentTorrentHashString.clear();
    m_currentTorrentMagnetLink.clear();
}

void TorrentListController::setDefaultDownloadDir(const QString &downloadDir)
{
    m_defaultDownloadDir = downloadDir.trimmed();
}

void TorrentListController::setCurrentDetailsDownloadDirProvider(const std::function<QString()> &provider)
{
    m_currentDetailsDownloadDirProvider = provider;
}

void TorrentListController::handleTableClicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid() || !m_proxyModel)
        return;

    const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return;

    const int torrentId = sourceIndex.data(Qt::UserRole).toInt();

    emit torrentSelected(torrentId);
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

    if (m_actions.forceStart)
        m_actions.forceStart->setEnabled(hasSelection);
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

    QMenu menu(m_dialogParent);

    if (m_actions.start)
        menu.addAction(m_actions.start);

    if (m_actions.forceStart)
        menu.addAction(m_actions.forceStart);

    if (m_actions.stop)
        menu.addAction(m_actions.stop);

    menu.addSeparator();

    if (m_actions.verify)
        menu.addAction(m_actions.verify);

    if (m_actions.reannounce)
        menu.addAction(m_actions.reannounce);

    QAction *propertiesAction = menu.addAction(tr("Properties…"));
    propertiesAction->setEnabled(selectedTorrentIds().size() == 1);

    menu.addSeparator();

    QAction *copyMagnetAction = menu.addAction(tr("Copy Magnet Link"));
    QAction *copyHashAction = menu.addAction(tr("Copy Hash"));

    const QList<int> contextTorrentIds = selectedTorrentIds();
    const bool canCopyCurrentTorrentDetails =
        contextTorrentIds.size() == 1
        && m_currentDetailsTorrentId == contextTorrentIds.first();

    copyMagnetAction->setEnabled(
        canCopyCurrentTorrentDetails
        && !m_currentTorrentMagnetLink.trimmed().isEmpty()
        );

    copyHashAction->setEnabled(
        canCopyCurrentTorrentDetails
        && !m_currentTorrentHashString.trimmed().isEmpty()
        );

    menu.addSeparator();

    QMenu *queueMenu = menu.addMenu(tr("Queue"));

    QAction *moveTopAction = queueMenu->addAction(tr("Move to Top"));
    QAction *moveUpAction = queueMenu->addAction(tr("Move Up"));
    QAction *moveDownAction = queueMenu->addAction(tr("Move Down"));
    QAction *moveBottomAction = queueMenu->addAction(tr("Move to Bottom"));

    menu.addSeparator();

    QAction *setLocationAction = menu.addAction(tr("Set Location…"));

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
    const QList<int> ids = selectedTorrentIds();
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
            tr("Remove only the torrent from Transmission, or also delete the downloaded data?\n\n"
               "View details to see the affected torrent name.")
            );
        msgBox.setDetailedText(names.value(0));

        torrentOnlyButton = msgBox.addButton(tr("Torrent only"), QMessageBox::AcceptRole);
        torrentAndDataButton = msgBox.addButton(tr("Torrent and data"), QMessageBox::DestructiveRole);
    } else {
        const QString preview = names.join("\n");

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
    invokeSelectedTorrentCommand(&rpc_client::startTorrents,
                                 tr("Starting %1 torrent(s)..."));
}

void TorrentListController::stopSelectedTorrents()
{
    invokeSelectedTorrentCommand(&rpc_client::stopTorrents,
                                 tr("Stopping %1 torrent(s)..."));
}

void TorrentListController::reannounceSelectedTorrents()
{
    invokeSelectedTorrentCommand(&rpc_client::reannounceTorrents,
                                 tr("Reannouncing %1 torrent(s)..."));
}

void TorrentListController::verifySelectedTorrents()
{
    invokeSelectedTorrentCommand(&rpc_client::verifyTorrents,
                                 tr("Verifying %1 torrent(s)..."));
}

void TorrentListController::forceStartSelectedTorrents()
{
    invokeSelectedTorrentCommand(&rpc_client::startTorrentsNow,
                                 tr("Force starting %1 torrent(s)..."));
}

void TorrentListController::setSelectedTorrentsLocation()
{
    const QList<int> ids = selectedTorrentIds();

    if (ids.isEmpty()) {
        emit statusMessageRequested(tr("No torrent selected."), 3000);
        return;
    }

    QString initialLocation = m_defaultDownloadDir.trimmed();

    if (ids.size() == 1 && currentTorrentId() == ids.first() && m_currentDetailsDownloadDirProvider) {
        const QString currentDownloadDir = m_currentDetailsDownloadDirProvider().trimmed();

        if (!currentDownloadDir.isEmpty() && currentDownloadDir != tr("Unknown"))
            initialLocation = currentDownloadDir;
    }

    QDialog dialog(m_dialogParent);
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

    QTimer::singleShot(1500, this, [this]() {
        emit torrentListRefreshRequested();
        refreshCurrentTorrentDetails();
    });
}

void TorrentListController::showSelectedTorrentProperties()
{
    const QList<int> ids = selectedTorrentIds();

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
    invokeSelectedTorrentCommand(&rpc_client::queueMoveTop, QString());
}

void TorrentListController::queueMoveSelectedUp()
{
    invokeSelectedTorrentCommand(&rpc_client::queueMoveUp, QString());
}

void TorrentListController::queueMoveSelectedDown()
{
    invokeSelectedTorrentCommand(&rpc_client::queueMoveDown, QString());
}

void TorrentListController::queueMoveSelectedBottom()
{
    invokeSelectedTorrentCommand(&rpc_client::queueMoveBottom, QString());
}

void TorrentListController::invokeSelectedTorrentCommand(void (rpc_client::*command)(const QList<int> &),
                                                        const QString &message)
{
    const QList<int> ids = selectedTorrentIds();

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
    const int torrentId = currentTorrentId();

    if (torrentId >= 0)
        emit torrentDetailsRefreshRequested(torrentId);
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
        settings.value(QString::fromLatin1(TorrentTableVisibleColumnsKey));

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
    settings.remove(QString::fromLatin1(TorrentTableHeaderStateKey));
    settings.remove(QString::fromLatin1(TorrentTableVerticalHeaderStateKey));
    settings.remove(QString::fromLatin1(TorrentTableVisibleColumnsKey));

    if (m_tableView->horizontalHeader())
        m_tableView->horizontalHeader()->reset();

    if (m_tableView->verticalHeader())
        m_tableView->verticalHeader()->reset();

    restoreDefaultColumnOrder();
    applyDefaultColumnVisibility();
    m_tableView->sortByColumn(TorrentModel::NameColumn, Qt::AscendingOrder);
    saveViewState();
}
