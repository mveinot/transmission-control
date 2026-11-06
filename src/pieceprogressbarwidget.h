#ifndef PIECEPROGRESSBARWIDGET_H
#define PIECEPROGRESSBARWIDGET_H

#include <QByteArray>
#include <QVector>
#include <QWidget>

// Full-width two-band torrent progress visualization with a fixed,
// high-contrast white/indigo scheme. The upper band shows byte progress; the
// lower band compresses the piece bitfield into width-filling buckets.
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
                     double percentDone,
                     int completedPieceCount = -1);

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
    void updatePieceColumnCompletionFractions(int piecesWidth);

    int m_pieceCount = 0;
    int m_completedPieceCount = 0;
    QByteArray m_pieceBitfield;
    double m_percentDone = 0.0;
    QVector<double> m_pieceColumnCompletionFractions;
    int m_cachedColumnCount = -1;
    bool m_pieceColumnCompletionFractionsValid = false;
};

#endif // PIECEPROGRESSBARWIDGET_H
