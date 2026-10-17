#include "statisticsdialog.h"

#include "torrentbackend.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonValue>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace {
StatisticsDialog::StatisticLabels addStatisticRows(QFormLayout *layout, bool includeSessionCount)
{
    StatisticsDialog::StatisticLabels labels;

    labels.downloaded = new QLabel(QStringLiteral("—"));
    labels.uploaded = new QLabel(QStringLiteral("—"));
    labels.ratio = new QLabel(QStringLiteral("—"));
    labels.activeTime = new QLabel(QStringLiteral("—"));
    if (includeSessionCount)
        labels.sessionCount = new QLabel(QStringLiteral("—"));
    labels.filesAdded = new QLabel(QStringLiteral("—"));

    layout->addRow(StatisticsDialog::tr("Downloaded:"), labels.downloaded);
    layout->addRow(StatisticsDialog::tr("Uploaded:"), labels.uploaded);
    layout->addRow(StatisticsDialog::tr("Ratio:"), labels.ratio);
    layout->addRow(StatisticsDialog::tr("Active time:"), labels.activeTime);
    if (labels.sessionCount)
        layout->addRow(StatisticsDialog::tr("Sessions:"), labels.sessionCount);
    layout->addRow(StatisticsDialog::tr("Torrents added:"), labels.filesAdded);

    return labels;
}
}

StatisticsDialog::StatisticsDialog(TorrentBackend *client, QWidget *parent)
    : QDialog(parent)
    , m_client(client)
{
    setWindowTitle(tr("Statistics"));
    setModal(true);
    resize(540, 330);

    auto *mainLayout = new QVBoxLayout(this);
    auto *groupsLayout = new QHBoxLayout;

    auto *currentGroup = new QGroupBox(tr("Current Session"), this);
    auto *currentLayout = new QFormLayout(currentGroup);
    currentLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    m_currentLabels = addStatisticRows(currentLayout, false);

    auto *cumulativeGroup = new QGroupBox(tr("Cumulative"), this);
    auto *cumulativeLayout = new QFormLayout(cumulativeGroup);
    cumulativeLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    m_cumulativeLabels = addStatisticRows(cumulativeLayout, true);

    groupsLayout->addWidget(currentGroup);
    groupsLayout->addWidget(cumulativeGroup);
    mainLayout->addLayout(groupsLayout);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_refreshButton = buttonBox->addButton(tr("Refresh"), QDialogButtonBox::ActionRole);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_refreshButton, &QPushButton::clicked, this, &StatisticsDialog::refresh);

    connect(m_client, &TorrentBackend::sessionStatisticsReceived,
            this, &StatisticsDialog::updateStatistics);
    connect(m_client, &TorrentBackend::sessionStatisticsFailed,
            this, &StatisticsDialog::showError);

    refresh();
}

void StatisticsDialog::refresh()
{
    if (!m_client)
        return;

    m_refreshButton->setEnabled(false);
    m_statusLabel->setText(tr("Loading statistics…"));
    m_client->getSessionStatistics();
}

void StatisticsDialog::updateStatistics(const QJsonObject &statistics)
{
    populateLabels(statistics.value(QStringLiteral("current-stats")).toObject(),
                   m_currentLabels);
    populateLabels(statistics.value(QStringLiteral("cumulative-stats")).toObject(),
                   m_cumulativeLabels);

    m_statusLabel->clear();
    m_refreshButton->setEnabled(true);
}

void StatisticsDialog::showError(const QString &message)
{
    m_statusLabel->setText(tr("Could not retrieve statistics: %1").arg(message));
    m_refreshButton->setEnabled(true);
}

QString StatisticsDialog::formatBytes(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("—");

    constexpr qint64 unit = 1024;
    const char *suffixes[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };

    double value = static_cast<double>(bytes);
    int suffixIndex = 0;
    while (value >= unit && suffixIndex < 5) {
        value /= unit;
        ++suffixIndex;
    }

    const int precision = suffixIndex == 0 ? 0 : (value >= 100.0 ? 0 : value >= 10.0 ? 1 : 2);
    return tr("%1 %2").arg(QLocale().toString(value, 'f', precision),
                           QString::fromLatin1(suffixes[suffixIndex]));
}

QString StatisticsDialog::formatDuration(qint64 seconds)
{
    if (seconds < 0)
        return QStringLiteral("—");

    const qint64 days = seconds / 86400;
    seconds %= 86400;
    const qint64 hours = seconds / 3600;
    seconds %= 3600;
    const qint64 minutes = seconds / 60;

    QStringList parts;
    if (days > 0)
        parts << tr("%n day(s)", nullptr, static_cast<int>(days));
    if (hours > 0)
        parts << tr("%n hour(s)", nullptr, static_cast<int>(hours));
    if (minutes > 0 || parts.isEmpty())
        parts << tr("%n minute(s)", nullptr, static_cast<int>(minutes));

    return parts.join(QStringLiteral(" "));
}

QString StatisticsDialog::formatRatio(qint64 uploadedBytes, qint64 downloadedBytes)
{
    if (uploadedBytes < 0 || downloadedBytes < 0)
        return QStringLiteral("—");
    if (downloadedBytes == 0)
        return uploadedBytes == 0 ? QStringLiteral("0.00") : QStringLiteral("∞");

    return QLocale().toString(static_cast<double>(uploadedBytes)
                                  / static_cast<double>(downloadedBytes),
                              'f', 2);
}

qint64 StatisticsDialog::jsonInteger(const QJsonObject &object, const QString &key)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? static_cast<qint64>(value.toDouble()) : -1;
}

void StatisticsDialog::populateLabels(const QJsonObject &statistics,
                                      const StatisticLabels &labels)
{
    const qint64 downloaded = jsonInteger(statistics, QStringLiteral("downloadedBytes"));
    const qint64 uploaded = jsonInteger(statistics, QStringLiteral("uploadedBytes"));
    const qint64 activeSeconds = jsonInteger(statistics, QStringLiteral("secondsActive"));
    const qint64 sessionCount = jsonInteger(statistics, QStringLiteral("sessionCount"));
    const qint64 filesAdded = jsonInteger(statistics, QStringLiteral("filesAdded"));

    labels.downloaded->setText(formatBytes(downloaded));
    labels.uploaded->setText(formatBytes(uploaded));
    labels.ratio->setText(formatRatio(uploaded, downloaded));
    labels.activeTime->setText(formatDuration(activeSeconds));
    if (labels.sessionCount) {
        labels.sessionCount->setText(sessionCount >= 0 ? QLocale().toString(sessionCount)
                                                       : QStringLiteral("—"));
    }
    labels.filesAdded->setText(filesAdded >= 0 ? QLocale().toString(filesAdded)
                                               : QStringLiteral("—"));
}
