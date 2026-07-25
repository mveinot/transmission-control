#include "torrentdetailstabcontroller.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QFont>
#include <QHeaderView>
#include <QLocale>
#include <QPalette>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

namespace {

bool detailsValueToBool(const QVariant &value, bool defaultValue = false)
{
    if (!value.isValid() || value.isNull())
        return defaultValue;

    if (value.metaType().id() == QMetaType::Bool)
        return value.toBool();

    if (value.canConvert<QString>()) {
        const QString text = value.toString().trimmed().toLower();

        if (text == QStringLiteral("true")
            || text == QStringLiteral("yes")
            || text == QStringLiteral("1")) {
            return true;
        }

        if (text == QStringLiteral("false")
            || text == QStringLiteral("no")
            || text == QStringLiteral("0")) {
            return false;
        }
    }

    return value.toInt() != 0;
}

} // namespace

TorrentDetailsTabController::TorrentDetailsTabController(QTabWidget *tabWidget,
                                                         QWidget *generalTab,
                                                         QObject *parent)
    : QObject(parent)
{
    m_tab = new QWidget(tabWidget);

    auto *layout = new QVBoxLayout(m_tab);
    layout->setContentsMargins(4, 4, 4, 4);

    m_table = new QTableWidget(m_tab);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({ tr("Property"), tr("Value") });
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    layout->addWidget(m_table);

    const int generalIndex = tabWidget->indexOf(generalTab);
    tabWidget->insertTab(generalIndex + 1, m_tab, tr("Details"));

    clear();
}

QWidget *TorrentDetailsTabController::widget() const
{
    return m_tab;
}

void TorrentDetailsTabController::clear()
{
    if (!m_table)
        return;

    m_table->setSortingEnabled(false);
    m_table->clearContents();
    m_table->setRowCount(0);
}

void TorrentDetailsTabController::update(const QVariantMap &details)
{
    // Rebuild from a snapshot to keep section ordering deterministic when RPC
    // versions add or omit fields.
    if (!m_table)
        return;

    m_table->setSortingEnabled(false);
    m_table->clearContents();
    m_table->setRowCount(0);

    auto jsonInt64 = [&details](const QString &key, qint64 defaultValue = 0) -> qint64 {
        const QVariant value = details.value(key);

        if (!value.isValid() || value.isNull())
            return defaultValue;

        return value.toLongLong();
    };

    auto jsonDouble = [&details](const QString &key, double defaultValue = 0.0) -> double {
        const QVariant value = details.value(key);

        if (!value.isValid() || value.isNull())
            return defaultValue;

        return value.toDouble();
    };

    auto jsonBool = [&details](const QString &key, bool defaultValue = false) -> bool {
        return detailsValueToBool(details.value(key), defaultValue);
    };

    auto formatBytesText = [](qint64 bytes) -> QString {
        return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeIecFormat);
    };

    auto formatRate = [this, &formatBytesText](qint64 bytesPerSecond) -> QString {
        return tr("%1/s").arg(formatBytesText(bytesPerSecond));
    };

    auto formatPercent = [](double fraction) -> QString {
        return QStringLiteral("%1%").arg(QLocale().toString(fraction * 100.0, 'f', 1));
    };

    auto formatRatio = [this](double ratio) -> QString {
        if (ratio < 0.0)
            return tr("None");

        return QLocale().toString(ratio, 'f', 2);
    };

    auto formatDate = [this](qint64 seconds) -> QString {
        if (seconds <= 0)
            return tr("Unknown");

        return QLocale().toString(QDateTime::fromSecsSinceEpoch(seconds), QLocale::ShortFormat);
    };

    auto formatDuration = [this](qint64 seconds) -> QString {
        if (seconds < 0)
            return tr("Unknown");

        const qint64 days = seconds / 86400;
        seconds %= 86400;
        const qint64 hours = seconds / 3600;
        seconds %= 3600;
        const qint64 minutes = seconds / 60;
        const qint64 secs = seconds % 60;

        if (days > 0)
            return tr("%1 d %2 h").arg(days).arg(hours);

        if (hours > 0)
            return tr("%1 h %2 m").arg(hours).arg(minutes);

        if (minutes > 0)
            return tr("%1 m %2 s").arg(minutes).arg(secs);

        return tr("%1 s").arg(secs);
    };

    auto formatEta = [this, &formatDuration](qint64 seconds) -> QString {
        if (seconds < 0)
            return tr("Unknown");

        return formatDuration(seconds);
    };

    auto yesNo = [this](bool value) -> QString {
        return value ? tr("Yes") : tr("No");
    };

    auto statusText = [this](int status) -> QString {
        switch (status) {
        case 0:
            return tr("Stopped");
        case 1:
            return tr("Queued for check");
        case 2:
            return tr("Checking");
        case 3:
            return tr("Queued for download");
        case 4:
            return tr("Downloading");
        case 5:
            return tr("Queued for seeding");
        case 6:
            return tr("Seeding");
        default:
            return tr("Unknown (%1)").arg(status);
        }
    };

    auto seedRatioModeText = [this](int mode) -> QString {
        switch (mode) {
        case 0:
            return tr("Use global setting");
        case 1:
            return tr("Use torrent ratio limit");
        case 2:
            return tr("Seed regardless of ratio");
        default:
            return tr("Unknown (%1)").arg(mode);
        }
    };

    auto seedIdleModeText = [this](int mode) -> QString {
        switch (mode) {
        case 0:
            return tr("Use global setting");
        case 1:
            return tr("Use torrent idle limit");
        case 2:
            return tr("Seed regardless of idle time");
        default:
            return tr("Unknown (%1)").arg(mode);
        }
    };

    auto priorityText = [this](int priority) -> QString {
        switch (priority) {
        case 1:
            return tr("High");
        case -1:
            return tr("Low");
        case 0:
            return tr("Normal");
        default:
            return tr("Unknown (%1)").arg(priority);
        }
    };

    auto addSection = [this](const QString &title) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setSpan(row, 0, 1, 2);

        auto *item = new QTableWidgetItem(title);
        QFont font = item->font();
        font.setBold(true);
        item->setFont(font);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setBackground(m_table->palette().alternateBase());

        m_table->setItem(row, 0, item);
    };

    auto addRow = [this](const QString &label, const QString &value) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto *labelItem = new QTableWidgetItem(label);
        labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);

        auto *valueItem = new QTableWidgetItem(value);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setToolTip(value);

        m_table->setItem(row, 0, labelItem);
        m_table->setItem(row, 1, valueItem);
    };

    auto addRowIfPresent = [&details, &addRow](const QString &key, const QString &label, const QString &value) {
        if (!details.contains(key))
            return;

        addRow(label, value);
    };

    const qint64 pieceSize = jsonInt64(QStringLiteral("pieceSize"));
    const int pieceCount = details.value(QStringLiteral("pieceCount")).toInt();
    const QByteArray pieces = QByteArray::fromBase64(
        details.value(QStringLiteral("pieces")).toString().toLatin1()
    );

    int completedPieces = 0;
    if (pieceCount > 0 && !pieces.isEmpty()) {
        for (int index = 0; index < pieceCount; ++index) {
            const int byteIndex = index / 8;
            const int bitIndex = 7 - (index % 8);

            if (byteIndex >= 0 && byteIndex < pieces.size()) {
                const uchar byte = static_cast<uchar>(pieces.at(byteIndex));
                if ((byte & (1u << bitIndex)) != 0)
                    ++completedPieces;
            }
        }
    }

    addSection(tr("State"));
    addRowIfPresent(QStringLiteral("status"), tr("Status"), statusText(details.value(QStringLiteral("status"), -1).toInt()));
    addRowIfPresent(QStringLiteral("isPrivate"), tr("Private torrent"), yesNo(jsonBool(QStringLiteral("isPrivate"))));
    addRowIfPresent(QStringLiteral("isStalled"), tr("Stalled"), yesNo(jsonBool(QStringLiteral("isStalled"))));
    addRowIfPresent(QStringLiteral("isFinished"), tr("Finished"), yesNo(jsonBool(QStringLiteral("isFinished"))));
    if (details.value(QStringLiteral("error")).toInt() != 0 || !details.value(QStringLiteral("errorString")).toString().isEmpty()) {
        addRow(tr("Error"), details.value(QStringLiteral("errorString"), tr("Unknown error")).toString());
    }

    addSection(tr("Progress"));
    addRowIfPresent(QStringLiteral("percentDone"), tr("Done"), formatPercent(jsonDouble(QStringLiteral("percentDone"))));
    addRowIfPresent(QStringLiteral("metadataPercentComplete"), tr("Metadata"), formatPercent(jsonDouble(QStringLiteral("metadataPercentComplete"))));
    addRowIfPresent(QStringLiteral("recheckProgress"), tr("Recheck"), formatPercent(jsonDouble(QStringLiteral("recheckProgress"))));
    if (pieceCount > 0)
        addRow(tr("Pieces complete"), tr("%1 / %2 (%3%)")
                                  .arg(completedPieces)
                                  .arg(pieceCount)
                                  .arg(QLocale().toString(100.0 * completedPieces / pieceCount, 'f', 1)));

    addSection(tr("Transfer"));
    addRowIfPresent(QStringLiteral("rateDownload"), tr("Download rate"), formatRate(jsonInt64(QStringLiteral("rateDownload"))));
    addRowIfPresent(QStringLiteral("rateUpload"), tr("Upload rate"), formatRate(jsonInt64(QStringLiteral("rateUpload"))));
    addRowIfPresent(QStringLiteral("uploadRatio"), tr("Ratio"), formatRatio(jsonDouble(QStringLiteral("uploadRatio"), -1.0)));
    addRowIfPresent(QStringLiteral("downloadedEver"), tr("Downloaded"), formatBytesText(jsonInt64(QStringLiteral("downloadedEver"))));
    addRowIfPresent(QStringLiteral("uploadedEver"), tr("Uploaded"), formatBytesText(jsonInt64(QStringLiteral("uploadedEver"))));
    addRowIfPresent(QStringLiteral("corruptEver"), tr("Wasted/corrupt"), formatBytesText(jsonInt64(QStringLiteral("corruptEver"))));
    addRowIfPresent(QStringLiteral("haveValid"), tr("Have valid"), formatBytesText(jsonInt64(QStringLiteral("haveValid"))));
    addRowIfPresent(QStringLiteral("haveUnchecked"), tr("Have unchecked"), formatBytesText(jsonInt64(QStringLiteral("haveUnchecked"))));
    addRowIfPresent(QStringLiteral("desiredAvailable"), tr("Desired available"), formatBytesText(jsonInt64(QStringLiteral("desiredAvailable"))));
    addRowIfPresent(QStringLiteral("leftUntilDone"), tr("Left until done"), formatBytesText(jsonInt64(QStringLiteral("leftUntilDone"))));
    addRowIfPresent(QStringLiteral("sizeWhenDone"), tr("Size when done"), formatBytesText(jsonInt64(QStringLiteral("sizeWhenDone"))));
    addRowIfPresent(QStringLiteral("totalSize"), tr("Total size"), formatBytesText(jsonInt64(QStringLiteral("totalSize"))));

    addSection(tr("Time"));
    addRowIfPresent(QStringLiteral("eta"), tr("ETA"), formatEta(jsonInt64(QStringLiteral("eta"), -1)));
    addRowIfPresent(QStringLiteral("etaIdle"), tr("Idle ETA"), formatEta(jsonInt64(QStringLiteral("etaIdle"), -1)));
    addRowIfPresent(QStringLiteral("secondsDownloading"), tr("Downloading time"), formatDuration(jsonInt64(QStringLiteral("secondsDownloading"))));
    addRowIfPresent(QStringLiteral("secondsSeeding"), tr("Seeding time"), formatDuration(jsonInt64(QStringLiteral("secondsSeeding"))));
    addRowIfPresent(QStringLiteral("dateCreated"), tr("Created"), formatDate(jsonInt64(QStringLiteral("dateCreated"))));
    addRowIfPresent(QStringLiteral("addedDate"), tr("Added"), formatDate(jsonInt64(QStringLiteral("addedDate"))));
    addRowIfPresent(QStringLiteral("startDate"), tr("Started"), formatDate(jsonInt64(QStringLiteral("startDate"))));
    addRowIfPresent(QStringLiteral("doneDate"), tr("Completed"), formatDate(jsonInt64(QStringLiteral("doneDate"))));
    addRowIfPresent(QStringLiteral("activityDate"), tr("Last activity"), formatDate(jsonInt64(QStringLiteral("activityDate"))));
    addRowIfPresent(QStringLiteral("editDate"), tr("Edited"), formatDate(jsonInt64(QStringLiteral("editDate"))));
    addRowIfPresent(QStringLiteral("manualAnnounceTime"), tr("Manual announce available"), formatDate(jsonInt64(QStringLiteral("manualAnnounceTime"))));

    addSection(tr("Peers"));
    addRowIfPresent(QStringLiteral("peersConnected"), tr("Connected peers"), QString::number(jsonInt64(QStringLiteral("peersConnected"))));
    addRowIfPresent(QStringLiteral("peersSendingToUs"), tr("Peers sending to us"), QString::number(jsonInt64(QStringLiteral("peersSendingToUs"))));
    addRowIfPresent(QStringLiteral("peersGettingFromUs"), tr("Peers getting from us"), QString::number(jsonInt64(QStringLiteral("peersGettingFromUs"))));
    addRowIfPresent(QStringLiteral("webseedsSendingToUs"), tr("Web seeds sending to us"), QString::number(jsonInt64(QStringLiteral("webseedsSendingToUs"))));
    addRowIfPresent(QStringLiteral("maxConnectedPeers"), tr("Max connected peers"), QString::number(jsonInt64(QStringLiteral("maxConnectedPeers"))));
    if (details.contains(QStringLiteral("peersFrom"))) {
        const QVariantMap peersFrom = details.value(QStringLiteral("peersFrom")).toMap();
        QStringList peerSources;
        for (auto it = peersFrom.constBegin(); it != peersFrom.constEnd(); ++it)
            peerSources << QStringLiteral("%1: %2").arg(it.key()).arg(it.value().toInt());
        peerSources.sort(Qt::CaseInsensitive);
        addRow(tr("Peer sources"), peerSources.join(QStringLiteral(", ")));
    }

    addSection(tr("Limits and seeding"));
    addRowIfPresent(QStringLiteral("bandwidthPriority"), tr("Bandwidth priority"), priorityText(details.value(QStringLiteral("bandwidthPriority")).toInt()));
    addRowIfPresent(QStringLiteral("honorsSessionLimits"), tr("Honor session limits"), yesNo(jsonBool(QStringLiteral("honorsSessionLimits"), true)));
    if (details.contains(QStringLiteral("downloadLimited")) || details.contains(QStringLiteral("downloadLimit"))) {
        const bool limited = jsonBool(QStringLiteral("downloadLimited"));
        addRow(tr("Download limit"), limited
                                      ? tr("%1/s").arg(formatBytesText(jsonInt64(QStringLiteral("downloadLimit")) * 1000))
                                      : tr("Unlimited"));
    }
    if (details.contains(QStringLiteral("uploadLimited")) || details.contains(QStringLiteral("uploadLimit"))) {
        const bool limited = jsonBool(QStringLiteral("uploadLimited"));
        addRow(tr("Upload limit"), limited
                                    ? tr("%1/s").arg(formatBytesText(jsonInt64(QStringLiteral("uploadLimit")) * 1000))
                                    : tr("Unlimited"));
    }
    addRowIfPresent(QStringLiteral("seedRatioMode"), tr("Seed ratio mode"), seedRatioModeText(details.value(QStringLiteral("seedRatioMode")).toInt()));
    addRowIfPresent(QStringLiteral("seedRatioLimit"), tr("Seed ratio limit"), QLocale().toString(jsonDouble(QStringLiteral("seedRatioLimit")), 'f', 2));
    addRowIfPresent(QStringLiteral("seedIdleMode"), tr("Seed idle mode"), seedIdleModeText(details.value(QStringLiteral("seedIdleMode")).toInt()));
    addRowIfPresent(QStringLiteral("seedIdleLimit"), tr("Seed idle limit"), tr("%1 minute(s)").arg(jsonInt64(QStringLiteral("seedIdleLimit"))));
    addRowIfPresent(QStringLiteral("queuePosition"), tr("Queue position"), QString::number(jsonInt64(QStringLiteral("queuePosition"))));

    addSection(tr("Torrent"));
    addRowIfPresent(QStringLiteral("pieceSize"), tr("Piece size"), formatBytesText(pieceSize));
    addRowIfPresent(QStringLiteral("pieceCount"), tr("Piece count"), QString::number(pieceCount));
    addRowIfPresent(QStringLiteral("file-count"), tr("File count"), QString::number(jsonInt64(QStringLiteral("file-count"))));
    addRowIfPresent(QStringLiteral("group"), tr("Group"), details.value(QStringLiteral("group")).toString().isEmpty()
                                                    ? tr("None")
                                                    : details.value(QStringLiteral("group")).toString());
    if (details.contains(QStringLiteral("labels"))) {
        const QVariantList labels = details.value(QStringLiteral("labels")).toList();
        QStringList labelTexts;
        for (const QVariant &label : labels)
            labelTexts << label.toString();
        addRow(tr("Labels"), labelTexts.isEmpty() ? tr("None") : labelTexts.join(QStringLiteral(", ")));
    }
    addRowIfPresent(QStringLiteral("downloadDir"), tr("Download directory"), details.value(QStringLiteral("downloadDir")).toString());
    addRowIfPresent(QStringLiteral("hashString"), tr("Hash"), details.value(QStringLiteral("hashString")).toString());

    m_table->resizeRowsToContents();
}
