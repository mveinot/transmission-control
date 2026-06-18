#include "torrentaddcontroller.h"

#include "rpc_client.h"

#include <QFileInfo>
#include <QSettings>
#include <QWidget>

namespace {

constexpr const char *SettingsDownloadDir =
    "torrentAdd/downloadDir";

constexpr const char *SettingsStartPaused =
    "torrentAdd/startPaused";

QString displaySourceForFile(const QString &filePath)
{
    const QFileInfo info(filePath);

    if (info.exists())
        return info.absoluteFilePath();

    return filePath;
}

} // namespace

TorrentAddController::TorrentAddController(rpc_client *client,
                                           QWidget *dialogParent,
                                           QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_dialogParent(dialogParent)
{
}

void TorrentAddController::setDefaultDownloadDir(const QString &downloadDir)
{
    m_defaultDownloadDir = downloadDir;
}

void TorrentAddController::addTorrentFile(const QString &filePath)
{
    if (filePath.trimmed().isEmpty()) {
        emit addFailed(QStringLiteral("No torrent file was specified."));
        return;
    }

    const QFileInfo fileInfo(filePath);

    if (!fileInfo.exists() || !fileInfo.isFile()) {
        emit addFailed(QStringLiteral("Torrent file does not exist: %1")
                           .arg(filePath));
        return;
    }

    promptAndAdd(TorrentAddDialog::SourceType::TorrentFile,
                 fileInfo.absoluteFilePath());
}

void TorrentAddController::addMagnetLink(const QString &magnetLink)
{
    const QString trimmed = magnetLink.trimmed();

    if (trimmed.isEmpty()) {
        emit addFailed(QStringLiteral("No magnet link was specified."));
        return;
    }

    if (!trimmed.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive)) {
        emit addFailed(QStringLiteral("The supplied link is not a magnet URI."));
        return;
    }

    promptAndAdd(TorrentAddDialog::SourceType::MagnetLink, trimmed);
}

bool TorrentAddController::promptAndAdd(TorrentAddDialog::SourceType sourceType,
                                        const QString &source)
{
    if (!m_client) {
        emit addFailed(QStringLiteral("No Transmission client is available."));
        return false;
    }

    TorrentAddDialog dialog(m_dialogParent);

    dialog.setSource(sourceType,
                     sourceType == TorrentAddDialog::SourceType::TorrentFile
                         ? displaySourceForFile(source)
                         : source);

    const QString rememberedDownloadDir = savedDownloadDir();

    if (!rememberedDownloadDir.isEmpty())
        dialog.setDownloadDir(rememberedDownloadDir);
    else
        dialog.setDownloadDir(m_defaultDownloadDir);

    dialog.setStartPaused(savedStartPaused());
    dialog.setRememberOptions(true);

    if (dialog.exec() != QDialog::Accepted) {
        emit addCancelled();
        return false;
    }

    const QString downloadDir = dialog.downloadDir();
    const bool startPaused = dialog.startPaused();

    if (dialog.rememberOptions())
        saveOptions(downloadDir, startPaused);

    switch (sourceType) {
    case TorrentAddDialog::SourceType::TorrentFile:
        m_client->addTorrentFile(source, downloadDir, startPaused);
        break;

    case TorrentAddDialog::SourceType::MagnetLink:
        m_client->addMagnetLink(source, downloadDir, startPaused);
        break;
    }

    emit addStarted();
    return true;
}

QString TorrentAddController::savedDownloadDir() const
{
    QSettings settings;
    return settings.value(QString::fromLatin1(SettingsDownloadDir)).toString();
}

bool TorrentAddController::savedStartPaused() const
{
    QSettings settings;
    return settings.value(QString::fromLatin1(SettingsStartPaused), false).toBool();
}

void TorrentAddController::saveOptions(const QString &downloadDir, bool startPaused)
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(SettingsDownloadDir), downloadDir);
    settings.setValue(QString::fromLatin1(SettingsStartPaused), startPaused);
}