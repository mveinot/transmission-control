#include "statusbarcontroller.h"

#include "torrentbackend.h"

#include <QCursor>
#include <QEvent>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QStatusBar>

StatusBarController::StatusBarController(QStatusBar *statusBar,
                                         TorrentBackend *client,
                                         QObject *parent)
    : QObject(parent)
    , m_statusBar(statusBar)
    , m_client(client)
{
}

void StatusBarController::setup()
{
    if (!m_statusBar)
        return;

    m_activityLabel = makeSectionLabel(tr("Not connected"));
    m_serverLabel = makeSectionLabel();
    m_torrentCountLabel = makeSectionLabel(tr("Torrents: 0"));
    m_filterLabel = makeSectionLabel();
    m_rateLabel = makeSectionLabel();
    m_freeSpaceLabel = makeSectionLabel();
    m_speedModeLabel = makeSectionLabel();
    m_intervalLabel = makeSectionLabel();

    m_statusBar->addPermanentWidget(m_activityLabel);
    m_statusBar->addPermanentWidget(m_serverLabel);
    m_statusBar->addPermanentWidget(m_torrentCountLabel);
    m_statusBar->addPermanentWidget(m_filterLabel);
    m_statusBar->addPermanentWidget(m_rateLabel);
    m_statusBar->addPermanentWidget(m_freeSpaceLabel);
    m_statusBar->addPermanentWidget(m_speedModeLabel);
    m_statusBar->addPermanentWidget(m_intervalLabel);

    makeLabelClickable(m_serverLabel, tr("Open server setup"));
    makeLabelClickable(m_rateLabel, tr("Open quick speed limits"));
    makeLabelClickable(m_freeSpaceLabel, tr("Refresh free-space information"));
    makeLabelClickable(m_speedModeLabel, tr("Toggle alternative speed mode"));
    makeLabelClickable(m_intervalLabel, tr("Open application settings"));

    refreshServerLabel();
    refreshTorrentCountLabel();
    refreshFilterLabel();
    refreshRateLabel();
    refreshFreeSpaceLabel();
    refreshSpeedModeLabel();
    refreshIntervalLabel();

    if (!m_client)
        return;

    connect(m_client, &TorrentBackend::updateStarted,
            this, [this]() {
                setActivityText(tr("Updating…"));
            });

    connect(m_client, &TorrentBackend::updateFinished,
            this, [this]() {
                setActivityText(tr("Connected"));
            });

    connect(m_client, &TorrentBackend::updateFailed,
            this, [this](const QString &message) {
                const QString displayMessage = normalizedErrorMessage(message);
                setActivityText(tr("Error"), true);

                if (m_activityLabel)
                    m_activityLabel->setToolTip(displayMessage);
            });

    connect(m_client, &TorrentBackend::serverChanged,
            this, [this]() {
                setServerName(m_client ? m_client->serverDisplayName() : QString());
                setActivityText(tr("Server changed"));
            });
}

void StatusBarController::showMessage(const QString &message, int timeoutMs)
{
    if (m_statusBar)
        m_statusBar->showMessage(message, timeoutMs);
}

void StatusBarController::updateTorrents(const QVector<torrent> &torrents)
{
    // Aggregate one immutable snapshot so counts and rates share a generation.
    m_torrentCount = torrents.size();
    m_downloadRateBytesPerSecond = 0.0;
    m_uploadRateBytesPerSecond = 0.0;

    for (const torrent &item : torrents) {
        m_downloadRateBytesPerSecond += item.getRateDownloadBytesPerSecond();
        m_uploadRateBytesPerSecond += item.getRateUploadBytesPerSecond();
    }

    refreshTorrentCountLabel();
    refreshFilterLabel();
    refreshRateLabel();
}

void StatusBarController::setFreeSpace(qint64 sizeBytes)
{
    m_freeSpaceBytes = sizeBytes;
    refreshFreeSpaceLabel();
}

void StatusBarController::clearFreeSpace()
{
    m_freeSpaceBytes = -1;
    refreshFreeSpaceLabel();
}

void StatusBarController::setSessionSettings(const QJsonObject &settings)
{
    m_altSpeedEnabled = settings.value(QStringLiteral("alt-speed-enabled")).toBool(false);
    refreshSpeedModeLabel();
}

void StatusBarController::setUpdateIntervalSeconds(int seconds)
{
    m_updateIntervalSeconds = seconds;
    refreshIntervalLabel();
}

void StatusBarController::setServerName(const QString &serverName)
{
    m_serverName = serverName.trimmed();
    refreshServerLabel();
}

void StatusBarController::setTorrentResultCount(int visibleCount, int totalCount)
{
    m_visibleTorrentCount = visibleCount;
    m_torrentCount = totalCount;
    refreshTorrentCountLabel();
}

void StatusBarController::setFilterSummary(const QString &summary)
{
    m_filterSummary = summary.trimmed();
    refreshFilterLabel();
}

QLabel *StatusBarController::makeSectionLabel(const QString &text) const
{
    auto *label = new QLabel(text, m_statusBar);
    label->setAlignment(Qt::AlignCenter);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setContentsMargins(8, 0, 8, 0);
    return label;
}

void StatusBarController::makeLabelClickable(QLabel *label, const QString &tooltip)
{
    if (!label)
        return;

    label->setCursor(Qt::PointingHandCursor);
    label->setToolTip(tooltip);
    label->installEventFilter(this);
}

bool StatusBarController::eventFilter(QObject *watched, QEvent *event)
{
    if (event && event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);

        if (mouseEvent->button() == Qt::LeftButton) {
            if (watched == m_serverLabel) {
                emit serverSetupRequested();
                return true;
            }

            if (watched == m_rateLabel) {
                emit speedLimitsDialogRequested();
                return true;
            }

            if (watched == m_freeSpaceLabel) {
                emit freeSpaceRefreshRequested();
                return true;
            }

            if (watched == m_speedModeLabel) {
                emit alternativeSpeedToggleRequested();
                return true;
            }

            if (watched == m_intervalLabel) {
                emit appSettingsRequested();
                return true;
            }
        }
    }

    return QObject::eventFilter(watched, event);
}

QString StatusBarController::normalizedErrorMessage(const QString &message) const
{
    if (message.contains(QStringLiteral("timed out"), Qt::CaseInsensitive))
        return tr("Timed out contacting the torrent server");

    if (message.contains(QStringLiteral("connection refused"), Qt::CaseInsensitive))
        return tr("Connection refused by the torrent server");

    if (message.contains(QStringLiteral("host not found"), Qt::CaseInsensitive))
        return tr("Host not found");

    return message;
}

QString StatusBarController::formattedRate(double bytesPerSecond) const
{
    if (bytesPerSecond <= 0.0)
        return tr("0 B/s");

    return QLocale().formattedDataSize(
               static_cast<qint64>(bytesPerSecond),
               1,
               QLocale::DataSizeIecFormat
               ) + tr("/s");
}

QString StatusBarController::formattedBytes(qint64 bytes) const
{
    if (bytes < 0)
        return QString();

    return QLocale().formattedDataSize(
        bytes,
        1,
        QLocale::DataSizeIecFormat
        );
}

void StatusBarController::refreshServerLabel()
{
    if (!m_serverLabel)
        return;

    const QString server = !m_serverName.isEmpty()
                               ? m_serverName
                               : (m_client ? m_client->serverDisplayName() : QString());

    m_serverLabel->setText(tr("Server: %1").arg(server.isEmpty()
                                                    ? tr("Not configured")
                                                    : server));
}

void StatusBarController::refreshTorrentCountLabel()
{
    if (!m_torrentCountLabel)
        return;

    if (m_visibleTorrentCount >= 0 && m_visibleTorrentCount != m_torrentCount) {
        m_torrentCountLabel->setText(
            tr("Torrents: %1 of %2").arg(m_visibleTorrentCount).arg(m_torrentCount)
            );
        return;
    }

    m_torrentCountLabel->setText(tr("Torrents: %1").arg(m_torrentCount));
}

void StatusBarController::refreshFilterLabel()
{
    if (!m_filterLabel)
        return;

    m_filterLabel->setText(m_filterSummary);
    m_filterLabel->setToolTip(m_filterSummary);
    m_filterLabel->setVisible(!m_filterSummary.isEmpty());
}

void StatusBarController::refreshRateLabel()
{
    if (!m_rateLabel)
        return;

    m_rateLabel->setText(
        tr("↓ %1  ↑ %2").arg(
            formattedRate(m_downloadRateBytesPerSecond),
            formattedRate(m_uploadRateBytesPerSecond)
            )
        );
}

void StatusBarController::refreshFreeSpaceLabel()
{
    if (!m_freeSpaceLabel)
        return;

    if (m_freeSpaceBytes < 0) {
        m_freeSpaceLabel->setText(tr("Free: —"));
        return;
    }

    m_freeSpaceLabel->setText(tr("Free: %1").arg(formattedBytes(m_freeSpaceBytes)));
}

void StatusBarController::refreshSpeedModeLabel()
{
    if (m_speedModeLabel)
        m_speedModeLabel->setText(m_altSpeedEnabled ? tr("Alt speed: On")
                                                    : tr("Alt speed: Off"));
}

void StatusBarController::refreshIntervalLabel()
{
    if (!m_intervalLabel)
        return;

    if (m_updateIntervalSeconds <= 0) {
        m_intervalLabel->setText(tr("Refresh: —"));
        return;
    }

    m_intervalLabel->setText(tr("Refresh: %1s").arg(m_updateIntervalSeconds));
}

void StatusBarController::setActivityText(const QString &text, bool error)
{
    if (!m_activityLabel)
        return;

    m_activityLabel->setText(text);
    m_activityLabel->setStyleSheet(error ? QStringLiteral("color: #ff6b6b;")
                                         : QString());

    if (!error)
        m_activityLabel->setToolTip(QString());
}
