#ifndef PIECEPROGRESSBARWIDGET_H
#define PIECEPROGRESSBARWIDGET_H

#include <QByteArray>
#include <QWidget>

// Full-width two-band torrent progress visualization. The upper band shows
// byte progress; the lower band compresses the piece bitfield into horizontal
// buckets sized to consume every available pixel.
class PieceProgressBarWidget : public QWidget
{
    Q_OBJECT

public:
    struct PieceRange {
        int first = 0;
        int lastExclusive = 0;
    };

    explicit PieceProgressBarWidget(QWidget *parent = nullptr);

    void clear();
    void setProgress(int pieceCount,
                     const QByteArray &pieceBitfield,
                     double percentDone);

    int pieceCount() const;
    int completedPieceCount() const;
    double percentDone() const;

    static PieceRange pieceRangeForColumn(int column,
                                          int columnCount,
                                          int pieceCount);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool hasPiece(int index) const;
    double completedFraction(const PieceRange &range) const;

    int m_pieceCount = 0;
    QByteArray m_pieceBitfield;
    double m_percentDone = 0.0;
};

#endif // PIECEPROGRESSBARWIDGET_H
