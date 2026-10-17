#include "tablecolumncontroller.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QHeaderView>
#include <QMenu>
#include <QSettings>
#include <QStringList>

#include <utility>

TableColumnController::TableColumnController(QHeaderView *header,
                                             QString headerStateKey,
                                             QString visibleColumnsKey,
                                             QVector<ColumnDefinition> columns,
                                             QObject *parent)
    : QObject(parent)
    , m_header(header)
    , m_headerStateKey(std::move(headerStateKey))
    , m_visibleColumnsKey(std::move(visibleColumnsKey))
    , m_columns(std::move(columns))
{
}

void TableColumnController::setup()
{
    configureHeader();
    applyDefaultColumnVisibility();
}

void TableColumnController::restoreState()
{
    if (!m_header)
        return;

    QSettings settings;
    const QByteArray headerState = settings.value(m_headerStateKey).toByteArray();

    if (!headerState.isEmpty())
        m_header->restoreState(headerState);

    configureHeader();
    applySavedColumnVisibility();
}

void TableColumnController::saveState() const
{
    if (!m_header)
        return;

    QSettings settings;
    settings.setValue(m_headerStateKey, m_header->saveState());

    QStringList visibleColumnIds;

    for (const ColumnDefinition &definition : m_columns) {
        if (!definition.userConfigurable && !definition.alwaysVisible)
            continue;

        if (!m_header->isSectionHidden(definition.column))
            visibleColumnIds.append(definition.id);
    }

    settings.setValue(m_visibleColumnsKey, visibleColumnIds);
}

void TableColumnController::resetColumns()
{
    if (!m_header)
        return;

    QSettings settings;
    settings.remove(m_headerStateKey);
    settings.remove(m_visibleColumnsKey);

    m_header->reset();
    configureHeader();
    restoreDefaultColumnOrder();
    applyDefaultColumnVisibility();
    saveState();
}

void TableColumnController::applyDefaultColumnVisibility()
{
    if (!m_header)
        return;

    const QStringList visibleIds = defaultVisibleColumnIds();

    for (const ColumnDefinition &definition : m_columns) {
        const bool visible = definition.alwaysVisible || visibleIds.contains(definition.id);
        m_header->setSectionHidden(definition.column, !visible);
    }
}

void TableColumnController::applySavedColumnVisibility()
{
    if (!m_header)
        return;

    QSettings settings;
    const QVariant storedValue = settings.value(m_visibleColumnsKey);

    if (!storedValue.isValid()) {
        applyDefaultColumnVisibility();
        return;
    }

    QStringList visibleIds = storedValue.toStringList();

    if (visibleIds.isEmpty())
        visibleIds = defaultVisibleColumnIds();

    for (const ColumnDefinition &definition : m_columns) {
        const bool visible = definition.alwaysVisible || visibleIds.contains(definition.id);
        m_header->setSectionHidden(definition.column, !visible);
    }
}

void TableColumnController::configureHeader()
{
    if (!m_header)
        return;

    m_header->setContextMenuPolicy(Qt::CustomContextMenu);
    m_header->setSectionsClickable(true);
    m_header->setSectionsMovable(true);
    m_header->setFirstSectionMovable(true);
    m_header->setHighlightSections(false);

    connect(m_header,
            &QHeaderView::customContextMenuRequested,
            this,
            &TableColumnController::showHeaderContextMenu,
            Qt::UniqueConnection);

    connect(m_header,
            &QHeaderView::sectionMoved,
            this,
            &TableColumnController::handleSectionMoved,
            Qt::UniqueConnection);
}

void TableColumnController::handleSectionMoved(int, int, int)
{
    saveState();
}

void TableColumnController::showHeaderContextMenu(const QPoint &pos)
{
    if (!m_header)
        return;

    QMenu menu(m_header);

    for (const ColumnDefinition &definition : m_columns) {
        if (!definition.userConfigurable)
            continue;

        QAction *action = menu.addAction(columnTitle(definition.column));
        action->setCheckable(true);
        action->setChecked(!m_header->isSectionHidden(definition.column));

        if (definition.alwaysVisible)
            action->setEnabled(false);

        connect(action,
                &QAction::toggled,
                this,
                [this, column = definition.column](bool checked) {
                    setColumnVisible(column, checked);
                });
    }

    if (!menu.isEmpty())
        menu.addSeparator();

    QAction *resetAction = menu.addAction(tr("Reset Columns"));
    connect(resetAction,
            &QAction::triggered,
            this,
            &TableColumnController::resetColumns);

    menu.exec(m_header->mapToGlobal(pos));
}

void TableColumnController::setColumnVisible(int column, bool visible)
{
    if (!m_header)
        return;

    const ColumnDefinition *definition = definitionForColumn(column);

    if (!definition)
        return;

    if (definition->alwaysVisible) {
        m_header->setSectionHidden(column, false);
        return;
    }

    m_header->setSectionHidden(column, !visible);
    saveState();
}

void TableColumnController::restoreDefaultColumnOrder()
{
    // Visual indices shift after each move; logical column IDs remain stable.
    if (!m_header)
        return;

    for (const ColumnDefinition &definition : m_columns) {
        const int currentVisualIndex = m_header->visualIndex(definition.column);

        if (currentVisualIndex >= 0 && currentVisualIndex != definition.column)
            m_header->moveSection(currentVisualIndex, definition.column);
    }
}

QStringList TableColumnController::defaultVisibleColumnIds() const
{
    QStringList ids;

    for (const ColumnDefinition &definition : m_columns) {
        if (definition.defaultVisible || definition.alwaysVisible)
            ids.append(definition.id);
    }

    return ids;
}

QString TableColumnController::columnTitle(int column) const
{
    if (!m_header || !m_header->model())
        return QString::number(column + 1);

    const QString title =
        m_header->model()->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();

    return title.isEmpty() ? QString::number(column + 1) : title;
}

const TableColumnController::ColumnDefinition *TableColumnController::definitionForColumn(int column) const
{
    for (const ColumnDefinition &definition : m_columns) {
        if (definition.column == column)
            return &definition;
    }

    return nullptr;
}
