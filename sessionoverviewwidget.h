#ifndef SESSIONOVERVIEWWIDGET_H
#define SESSIONOVERVIEWWIDGET_H

#include <QVector>
#include <QWidget>

// Empty-selection view for the details pane. Samples come from the normal
// torrent-list refresh, so graph history never creates additional RPC traffic.
class SessionOverviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SessionOverviewWidget(QWidget *parent = nullptr);

    void addSample(qint64 timestampMs,
                   double downloadBytesPerSecond,
                   double uploadBytesPerSecond,
                   int downloadingCount,
                   int seedingCount,
                   int waitingCount);
    void markDisconnected();
    void clearHistory();
    int sampleCount() const;

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    struct Sample
    {
        qint64 timestampMs = 0;
        double downloadRate = 0.0;
        double uploadRate = 0.0;
        bool breakBefore = false;
    };

    static constexpr qint64 HistoryDurationMs = 5 * 60 * 1000;

    QVector<Sample> m_samples;
    int m_downloadingCount = 0;
    int m_seedingCount = 0;
    int m_waitingCount = 0;
    bool m_breakBeforeNextSample = false;
    int m_hoveredSample = -1;

    QRectF graphRect() const;
    int sampleIndexAt(qreal x) const;
    QString rateText(double bytesPerSecond) const;
};

#endif // SESSIONOVERVIEWWIDGET_H
