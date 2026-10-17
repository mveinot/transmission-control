#ifndef PIECEPROGRESSCONTROLLER_H
#define PIECEPROGRESSCONTROLLER_H

#include <QObject>

struct TorrentPieces;
class QVBoxLayout;
class QWidget;
class PieceProgressBarWidget;

// Places the linear piece-progress visualization above the General details and
// translates merged torrent RPC payloads into its compact display state.
class PieceProgressController : public QObject
{
    Q_OBJECT

public:
    explicit PieceProgressController(QWidget *generalTab,
                                     QVBoxLayout *generalLayout,
                                     QObject *parent = nullptr);

    QWidget *widget() const;

    void clear();
    void update(const TorrentPieces &pieces);

private:
    PieceProgressBarWidget *m_pieceProgressBar = nullptr;
};

#endif // PIECEPROGRESSCONTROLLER_H
