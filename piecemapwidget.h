#ifndef PIECEMAPWIDGET_H
#define PIECEMAPWIDGET_H

#include <QByteArray>
#include <QSize>
#include <QWidget>

// Compact bitfield renderer. Piece zero maps to the high bit of byte zero,
// matching Transmission's wire representation.
class PieceMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PieceMapWidget(QWidget *parent = nullptr);

    void clear();
    void setPieces(int pieceCount, const QByteArray &pieceBitfield);

    int pieceCount() const;
    int completedPieceCount() const;

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool hasPiece(int index) const;

    int m_pieceCount = 0;
    QByteArray m_pieceBitfield;
};

#endif // PIECEMAPWIDGET_H
