#include "torrentaddcontroller.h"

#include "torrentbackend.h"
#include "torrentmetadataparser.h"
#include "settingskeys.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QSettings>
#include <QWidget>

#include <utility>

namespace {
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

bool TorrentAddController::showOptionsDialog(
    TorrentAddDialog::SourceType sourceType) const
{
    QSettings settings;
    const char *key = sourceType == TorrentAddDialog::SourceType::TorrentFile
        ? SettingsKeys::ShowTorrentFileOptionsDialog
        : SettingsKeys::ShowMagnetLinkOptionsDialog;
    return settings.value(key, true).toBool();
}

TorrentAddController::TorrentAddController(TorrentBackend *client,
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

    const QString source = fileInfo.absoluteFilePath();
    if (showOptionsDialog(TorrentAddDialog::SourceType::TorrentFile))
        promptAndAdd(TorrentAddDialog::SourceType::TorrentFile, source);
    else
        addUsingDefaults(TorrentAddDialog::SourceType::TorrentFile, source);
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
        emit addFailed(tr("No torrent backend is available."));
        return;
    }

    const bool showDialog =
        showOptionsDialog(TorrentAddDialog::SourceType::TorrentFile);

    for (const QString &filePath : std::as_const(normalizedFilePaths)) {
        const QFileInfo fileInfo(filePath);

        if (!fileInfo.exists() || !fileInfo.isFile()) {
            emit addFailed(tr("Torrent file does not exist: %1")
                               .arg(filePath));
            continue;
        }

        const bool added = showDialog
            ? promptAndAdd(TorrentAddDialog::SourceType::TorrentFile,
                           fileInfo.absoluteFilePath())
            : addUsingDefaults(TorrentAddDialog::SourceType::TorrentFile,
                               fileInfo.absoluteFilePath());

        if (!added) {
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

    if (showOptionsDialog(TorrentAddDialog::SourceType::MagnetLink))
        promptAndAdd(TorrentAddDialog::SourceType::MagnetLink, trimmed);
    else
        addUsingDefaults(TorrentAddDialog::SourceType::MagnetLink, trimmed);
}

bool TorrentAddController::promptAndAdd(TorrentAddDialog::SourceType sourceType,
                                        const QString &source)
{
    if (!m_client) {
        emit addFailed(tr("No torrent backend is available."));
        return false;
    }

    TorrentAddDialog dialog(m_dialogParent);

    dialog.setSource(sourceType,
                     sourceType == TorrentAddDialog::SourceType::TorrentFile
                         ? displaySourceForFile(source)
                         : source);

    // Only expose add-time file choices when the backend can submit them with
    // the add request. Existing-torrent file controls are a separate capability.
    if (sourceType == TorrentAddDialog::SourceType::TorrentFile
        && m_client->capabilities().addTorrentFileSelection) {
        dialog.setTorrentMetadata(TorrentMetadataParser::parseTorrentFile(source));
    }

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

bool TorrentAddController::addUsingDefaults(
    TorrentAddDialog::SourceType sourceType,
    const QString &source)
{
    if (!m_client) {
        emit addFailed(tr("No torrent backend is available."));
        return false;
    }

    QString downloadDir = savedDownloadDir();
    if (downloadDir.isEmpty())
        downloadDir = m_defaultDownloadDir;

    const bool startPaused = savedStartPaused();

    switch (sourceType) {
    case TorrentAddDialog::SourceType::TorrentFile:
        m_client->addTorrentFile(source,
                                 downloadDir,
                                 startPaused,
                                 {},
                                 {},
                                 {},
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
    return settings.value(downloadDirSettingKey()).toString();
}

bool TorrentAddController::savedStartPaused() const
{
    QSettings settings;
    return settings.value(SettingsKeys::StartTorrentPaused, false).toBool();
}

QString TorrentAddController::downloadDirSettingKey() const
{
    const QString baseKey =
        QString::fromLatin1(SettingsKeys::TorrentAddDownloadDir);

    if (!m_client)
        return baseKey;

    // Destination paths belong to the remote daemon, not the local machine.
    // Scope them by backend and endpoint so switching servers cannot reuse an
    // incompatible path. Keep Transmission on the legacy key for migration.
    if (m_client->backendName() == QStringLiteral("Transmission"))
        return baseKey;

    const QByteArray identity =
        (m_client->backendName() + QLatin1Char('\n') +
         m_client->endpointUrl()).toUtf8();
    const QString digest =
        QString::fromLatin1(QCryptographicHash::hash(
                               identity, QCryptographicHash::Sha256).toHex());
    return baseKey + QStringLiteral("/servers/") + digest;
}

void TorrentAddController::saveOptions(const QString &downloadDir, bool startPaused)
{
    QSettings settings;
    settings.setValue(downloadDirSettingKey(), downloadDir);
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

    addUsingDefaults(TorrentAddDialog::SourceType::TorrentFile,
                     fileInfo.absoluteFilePath());
}
