#include "torrentaddcontroller.h"

#include "rpc_client.h"
#include "torrentmetadataparser.h"
#include "settingskeys.h"

#include <QFileInfo>
#include <QSettings>
#include <QWidget>

#include <utility>

namespace {

constexpr const char *SettingsDownloadDir =
    "torrentAdd/downloadDir";

QString displaySourceForFile(const QString &filePath)
{
    const QFileInfo info(filePath);

    if (info.exists())
        return info.absoluteFilePath();

    return filePath;
}

} // namespace


bool TorrentAddController::deleteTorrentFileOnSuccessfulAdd() const
{
    QSettings settings;
    return settings.value(SettingsKeys::DeleteTorrentOnAdd, false).toBool();
}

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
        emit addFailed(tr("No torrent file was specified."));
        return;
    }

    const QFileInfo fileInfo(filePath);

    if (!fileInfo.exists() || !fileInfo.isFile()) {
        emit addFailed(tr("Torrent file does not exist: %1")
                           .arg(filePath));
        return;
    }

    promptAndAdd(TorrentAddDialog::SourceType::TorrentFile,
                 fileInfo.absoluteFilePath());
}


void TorrentAddController::addTorrentFiles(const QStringList &filePaths)
{
    QStringList normalizedFilePaths;

    for (const QString &filePath : filePaths) {
        const QString trimmed = filePath.trimmed();

        if (!trimmed.isEmpty())
            normalizedFilePaths.append(trimmed);
    }

    if (normalizedFilePaths.isEmpty()) {
        emit addFailed(tr("No torrent files were specified."));
        return;
    }

    if (!m_client) {
        emit addFailed(tr("No Transmission client is available."));
        return;
    }

    for (const QString &filePath : std::as_const(normalizedFilePaths)) {
        const QFileInfo fileInfo(filePath);

        if (!fileInfo.exists() || !fileInfo.isFile()) {
            emit addFailed(tr("Torrent file does not exist: %1")
                               .arg(filePath));
            continue;
        }

        if (!promptAndAdd(TorrentAddDialog::SourceType::TorrentFile,
                          fileInfo.absoluteFilePath())) {
            break;
        }
    }
}

void TorrentAddController::addMagnetLink(const QString &magnetLink)
{
    const QString trimmed = magnetLink.trimmed();

    if (trimmed.isEmpty()) {
        emit addFailed(tr("No magnet link was specified."));
        return;
    }

    if (!trimmed.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive)) {
        emit addFailed(tr("The supplied link is not a magnet URI."));
        return;
    }

    promptAndAdd(TorrentAddDialog::SourceType::MagnetLink, trimmed);
}

bool TorrentAddController::promptAndAdd(TorrentAddDialog::SourceType sourceType,
                                        const QString &source)
{
    if (!m_client) {
        emit addFailed(tr("No Transmission client is available."));
        return false;
    }

    TorrentAddDialog dialog(m_dialogParent);

    dialog.setSource(sourceType,
                     sourceType == TorrentAddDialog::SourceType::TorrentFile
                         ? displaySourceForFile(source)
                         : source);

    // Local parsing is preview-only; Transmission remains authoritative.
    if (sourceType == TorrentAddDialog::SourceType::TorrentFile)
        dialog.setTorrentMetadata(TorrentMetadataParser::parseTorrentFile(source));

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
        m_client->addTorrentFile(source,
                                 downloadDir,
                                 startPaused,
                                 dialog.unwantedFileIndices(),
                                 dialog.lowPriorityFileIndices(),
                                 dialog.highPriorityFileIndices(),
                                 deleteTorrentFileOnSuccessfulAdd());
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
    return settings.value(SettingsKeys::StartTorrentPaused, false).toBool();
}

void TorrentAddController::saveOptions(const QString &downloadDir, bool startPaused)
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(SettingsDownloadDir), downloadDir);
    // Preferences and the add dialog intentionally share this default; the
    // dialog may override it for subsequent additions when options are saved.
    settings.setValue(SettingsKeys::StartTorrentPaused, startPaused);
}

void TorrentAddController::addTorrentFileUsingDefaults(const QString &filePath)
{
    if (filePath.trimmed().isEmpty()) {
        emit addFailed(tr("No torrent file was specified."));
        return;
    }

    const QFileInfo fileInfo(filePath);

    if (!fileInfo.exists() || !fileInfo.isFile()) {
        emit addFailed(tr("Torrent file does not exist: %1")
                           .arg(filePath));
        return;
    }

    if (!m_client) {
        emit addFailed(tr("No Transmission client is available."));
        return;
    }

    QString downloadDir = savedDownloadDir();

    if (downloadDir.isEmpty())
        downloadDir = m_defaultDownloadDir;

    const bool startPaused = savedStartPaused();

    m_client->addTorrentFile(
        fileInfo.absoluteFilePath(),
        downloadDir,
        startPaused,
        {},
        {},
        {},
        deleteTorrentFileOnSuccessfulAdd()
        );

    emit addStarted();
}
