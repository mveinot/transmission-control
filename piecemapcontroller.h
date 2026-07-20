#ifndef PIECEMAPCONTROLLER_H
#define PIECEMAPCONTROLLER_H

#include <QObject>

class QGroupBox;
class QJsonObject;
class QVBoxLayout;
class QWidget;
class PieceMapWidget;

// Owns the lazily constructed piece-map tab widgets and translates torrent RPC
// payloads into PieceMapWidget state.
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
