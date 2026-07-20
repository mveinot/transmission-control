#ifndef TABLEPLACEHOLDERCONTROLLER_H
#define TABLEPLACEHOLDERCONTROLLER_H

#include <QObject>
#include <QString>

class QLabel;
class QAbstractScrollArea;
class QEvent;

// Overlays an empty/loading/error message on an item-view viewport without
// affecting the view's model, selection, or input handling.
class TablePlaceholderController : public QObject
{
    Q_OBJECT

public:
    explicit TablePlaceholderController(QAbstractScrollArea *view,
                                        QObject *parent = nullptr);

    void setMessage(const QString &message);
    void clearMessage();
    QString message() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QAbstractScrollArea *m_view = nullptr;
    QLabel *m_label = nullptr;
    QString m_message;

    void updateGeometry();
    void updateVisibility();
};

#endif // TABLEPLACEHOLDERCONTROLLER_H
