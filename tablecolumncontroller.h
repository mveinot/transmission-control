#ifndef TABLECOLUMNCONTROLLER_H
#define TABLECOLUMNCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVector>

class QHeaderView;
class QPoint;

class TableColumnController : public QObject
{
    Q_OBJECT

public:
    struct ColumnDefinition
    {
        int column = -1;
        QString id;
        bool defaultVisible = true;
        bool userConfigurable = true;
        bool alwaysVisible = false;
    };

    explicit TableColumnController(QHeaderView *header,
                                   QString headerStateKey,
                                   QString visibleColumnsKey,
                                   QVector<ColumnDefinition> columns,
                                   QObject *parent = nullptr);

    void setup();
    void restoreState();
    void saveState() const;
    void resetColumns();
    void applyDefaultColumnVisibility();
    void applySavedColumnVisibility();

private slots:
    void handleSectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);

private:
    void configureHeader();
    void showHeaderContextMenu(const QPoint &pos);
    void setColumnVisible(int column, bool visible);
    void restoreDefaultColumnOrder();
    QStringList defaultVisibleColumnIds() const;
    QString columnTitle(int column) const;
    const ColumnDefinition *definitionForColumn(int column) const;

    QHeaderView *m_header = nullptr;
    QString m_headerStateKey;
    QString m_visibleColumnsKey;
    QVector<ColumnDefinition> m_columns;
};

#endif // TABLECOLUMNCONTROLLER_H
