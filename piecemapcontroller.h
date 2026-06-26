#ifndef PIECEMAPCONTROLLER_H
#define PIECEMAPCONTROLLER_H

#include <QObject>

class QGroupBox;
class QJsonObject;
class QVBoxLayout;
class QWidget;
class PieceMapWidget;

class PieceMapController : public QObject
{
    Q_OBJECT

public:
    explicit PieceMapController(QWidget *generalTab,
                                QVBoxLayout *generalLayout,
                                QGroupBox *generalInfoGroup,
                                QObject *parent = nullptr);

    QWidget *widget() const;

    void clear();
    void update(const QJsonObject &details);

private:
    QGroupBox *m_group = nullptr;
    PieceMapWidget *m_pieceMap = nullptr;
};

#endif // PIECEMAPCONTROLLER_H
