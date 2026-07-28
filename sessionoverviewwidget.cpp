#include "sessionoverviewwidget.h"

#include <QDateTime>
#include <QEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace {
constexpr int GraphTop = 54;
constexpr int GraphBottomMargin = 52;
constexpr int GraphSideMargin = 18;

QColor downloadColor()
{
    return QColor(45, 117, 210);
}

QColor uploadColor()
{
    return QColor(123, 78, 180);
}
}

SessionOverviewWidget::SessionOverviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SessionOverviewWidget::addSample(qint64 timestampMs,
                                      double downloadBytesPerSecond,
                                      double uploadBytesPerSecond,
                                      int downloadingCount,
                                      int seedingCount,
                                      int waitingCount)
{
    Sample sample;
    sample.timestampMs = timestampMs;
    sample.downloadRate = qMax(0.0, downloadBytesPerSecond);
    sample.uploadRate = qMax(0.0, uploadBytesPerSecond);
    sample.breakBefore = m_breakBeforeNextSample;
    m_breakBeforeNextSample = false;
    m_samples.append(sample);

    const qint64 cutoff = timestampMs - HistoryDurationMs;
    while (!m_samples.isEmpty() && m_samples.first().timestampMs < cutoff)
        m_samples.removeFirst();

    m_downloadingCount = downloadingCount;
    m_seedingCount = seedingCount;
    m_waitingCount = waitingCount;
    m_hoveredSample = -1;
    update();
}

void SessionOverviewWidget::markDisconnected()
{
    m_breakBeforeNextSample = true;
}

void SessionOverviewWidget::clearHistory()
{
    m_samples.clear();
    m_downloadingCount = 0;
    m_seedingCount = 0;
    m_waitingCount = 0;
    m_breakBeforeNextSample = false;
    m_hoveredSample = -1;
    update();
}

int SessionOverviewWidget::sampleCount() const
{
    return m_samples.size();
}

QRectF SessionOverviewWidget::graphRect() const
{
    return QRectF(GraphSideMargin,
                  GraphTop,
                  qMax(1, width() - 2 * GraphSideMargin),
                  qMax(1, height() - GraphTop - GraphBottomMargin));
}

QString SessionOverviewWidget::rateText(double bytesPerSecond) const
{
    static const QStringList units {
        tr("B/s"), tr("KiB/s"), tr("MiB/s"), tr("GiB/s")
    };

    int unit = 0;
    double value = qMax(0.0, bytesPerSecond);
    while (value >= 1024.0 && unit < units.size() - 1) {
        value /= 1024.0;
        ++unit;
    }

    const int precision = value >= 100.0 || unit == 0 ? 0 : 1;
    return tr("%1 %2").arg(QLocale().toString(value, 'f', precision),
                           units.at(unit));
}

void SessionOverviewWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().brush(QPalette::Base));

    const QColor textColor = palette().color(QPalette::Text);
    QColor secondaryText = textColor;
    secondaryText.setAlpha(165);

    QFont headingFont = font();
    headingFont.setBold(true);
    headingFont.setPointSizeF(headingFont.pointSizeF() + 1.0);
    painter.setFont(headingFont);
    painter.setPen(textColor);
    painter.drawText(QRectF(GraphSideMargin, 12, width() / 2.0, 24),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("Bandwidth Activity"));

    painter.setFont(font());
    painter.setPen(secondaryText);
    painter.drawText(QRectF(width() / 2.0, 12,
                            width() / 2.0 - GraphSideMargin, 24),
                     Qt::AlignRight | Qt::AlignVCenter,
                     tr("Last 5 minutes"));

    const QRectF plot = graphRect();
    QColor gridColor = palette().color(QPalette::Mid);
    gridColor.setAlpha(90);
    painter.setPen(QPen(gridColor, 1));
    for (int line = 0; line <= 4; ++line) {
        const qreal y = plot.top() + plot.height() * line / 4.0;
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    if (m_samples.isEmpty()) {
        painter.setPen(secondaryText);
        painter.drawText(plot, Qt::AlignCenter,
                         tr("Waiting for bandwidth data…"));
    } else {
        double maximumRate = 1.0;
        for (const Sample &sample : m_samples) {
            maximumRate = qMax(maximumRate,
                               qMax(sample.downloadRate, sample.uploadRate));
        }

        // Leave modest headroom so a current peak does not touch the frame.
        maximumRate *= 1.1;
        const qint64 newestTime = m_samples.constLast().timestampMs;
        const qint64 oldestTime = newestTime - HistoryDurationMs;

        const auto pointFor = [&](const Sample &sample, double rate) {
            const qreal xFraction =
                qBound(0.0,
                       double(sample.timestampMs - oldestTime) /
                           double(HistoryDurationMs),
                       1.0);
            const qreal yFraction = qBound(0.0, rate / maximumRate, 1.0);
            return QPointF(plot.left() + plot.width() * xFraction,
                           plot.bottom() - plot.height() * yFraction);
        };

        const auto drawSeries = [&](const QColor &color, bool download) {
            QPainterPath path;
            bool pathStarted = false;
            for (const Sample &sample : m_samples) {
                const QPointF point =
                    pointFor(sample,
                             download ? sample.downloadRate
                                      : sample.uploadRate);
                if (!pathStarted || sample.breakBefore) {
                    path.moveTo(point);
                    pathStarted = true;
                } else {
                    path.lineTo(point);
                }
            }
            painter.setPen(QPen(color, 2.0));
            painter.drawPath(path);
        };

        drawSeries(downloadColor(), true);
        drawSeries(uploadColor(), false);

        painter.setPen(secondaryText);
        painter.drawText(QRectF(plot.left(), plot.top() + 2, plot.width(), 18),
                         Qt::AlignRight | Qt::AlignTop,
                         rateText(maximumRate));

        if (m_hoveredSample >= 0 && m_hoveredSample < m_samples.size()) {
            const Sample &sample = m_samples.at(m_hoveredSample);
            const QPointF marker = pointFor(sample, 0.0);
            QColor markerColor = palette().color(QPalette::Text);
            markerColor.setAlpha(100);
            painter.setPen(QPen(markerColor, 1, Qt::DashLine));
            painter.drawLine(QPointF(marker.x(), plot.top()),
                             QPointF(marker.x(), plot.bottom()));
        }
    }

    const double currentDownload =
        m_samples.isEmpty() ? 0.0 : m_samples.constLast().downloadRate;
    const double currentUpload =
        m_samples.isEmpty() ? 0.0 : m_samples.constLast().uploadRate;
    const QString legend =
        tr("Download %1").arg(rateText(currentDownload)) +
        QStringLiteral("    ") +
        tr("Upload %1").arg(rateText(currentUpload));
    painter.setPen(textColor);
    painter.drawText(QRectF(GraphSideMargin, height() - 44,
                            width() - 2 * GraphSideMargin, 18),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     legend);

    painter.setPen(secondaryText);
    const QString counts =
        tr("Downloading %1").arg(m_downloadingCount) +
        QStringLiteral("    ") +
        tr("Seeding %1").arg(m_seedingCount) +
        QStringLiteral("    ") +
        tr("Waiting %1").arg(m_waitingCount);
    painter.drawText(QRectF(GraphSideMargin, height() - 24,
                            width() - 2 * GraphSideMargin, 18),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     counts);
}

int SessionOverviewWidget::sampleIndexAt(qreal x) const
{
    if (m_samples.isEmpty() || !graphRect().contains(x, graphRect().center().y()))
        return -1;

    const QRectF plot = graphRect();
    const qreal fraction = qBound(0.0, (x - plot.left()) / plot.width(), 1.0);
    const qint64 newestTime = m_samples.constLast().timestampMs;
    const qint64 targetTime =
        newestTime - HistoryDurationMs +
        qRound64(fraction * HistoryDurationMs);

    int closest = 0;
    qint64 closestDistance =
        std::abs(m_samples.first().timestampMs - targetTime);
    for (int index = 1; index < m_samples.size(); ++index) {
        const qint64 distance =
            std::abs(m_samples.at(index).timestampMs - targetTime);
        if (distance < closestDistance) {
            closest = index;
            closestDistance = distance;
        }
    }
    return closest;
}

void SessionOverviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    const int index = sampleIndexAt(event->position().x());
    if (m_hoveredSample != index) {
        m_hoveredSample = index;
        update();
    }

    if (index < 0) {
        QToolTip::hideText();
        return;
    }

    const Sample &sample = m_samples.at(index);
    const QString tooltip =
        QLocale().toString(QDateTime::fromMSecsSinceEpoch(sample.timestampMs),
                           QLocale::ShortFormat) +
        QLatin1Char('\n') +
        tr("Download: %1").arg(rateText(sample.downloadRate)) +
        QLatin1Char('\n') +
        tr("Upload: %1").arg(rateText(sample.uploadRate));
    QToolTip::showText(event->globalPosition().toPoint(), tooltip, this);
}

void SessionOverviewWidget::leaveEvent(QEvent *event)
{
    m_hoveredSample = -1;
    QToolTip::hideText();
    update();
    QWidget::leaveEvent(event);
}
