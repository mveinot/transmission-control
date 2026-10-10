#include "torrentaddcontroller.h"

#include "torrentbackend.h"
#include "torrentmetadataparser.h"
#include "settingskeys.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QWidget>

#include <algorithm>
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
    if (m_client) {
        connect(m_client, &TorrentBackend::serverChanged,
                this, [this]() { m_knownDownloadDirs.clear(); });
        connect(m_client, &TorrentBackend::torrentsReceived,
                this, [this](const QVector<torrent> &torrents) {
                    QSet<QString> uniqueDirs;
                    for (const torrent &item : torrents) {
                        const QString downloadDir = item.getDownloadDir().trimmed();
                        if (!downloadDir.isEmpty())
                            uniqueDirs.insert(downloadDir);
                    }
                    m_knownDownloadDirs = uniqueDirs.values();
                    std::sort(m_knownDownloadDirs.begin(), m_knownDownloadDirs.end(),
                              [](const QString &lhs, const QString &rhs) {
                                  return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
                              });
                });
        connect(m_client, &TorrentBackend::torrentAdded,
                this, [this](const TorrentKey &key, const QString &) {
                    if (m_pendingAdds.isEmpty())
                        return;

                    const PendingAdd pending = m_pendingAdds.dequeue();
                    if (!pending.downloadDir.isEmpty()) {
                        QSettings settings;
                        if (pending.userDefinedDownloadDir) {
                            QStringList acceptedDirs =
                                settings.value(acceptedDownloadDirsSettingKey())
                                    .toStringList();
                            acceptedDirs.removeAll(pending.downloadDir);
                            acceptedDirs.prepend(pending.downloadDir);
                            settings.setValue(acceptedDownloadDirsSettingKey(),
                                              acceptedDirs);
                        }
                        settings.setValue(lastAcceptedDownloadDirSettingKey(),
                                          pending.downloadDir);
                        settings.setValue(downloadDirSettingKey(),
                                          pending.downloadDir);
                    }

                    if (!pending.newName.isEmpty()
                        && pending.originalName != pending.newName) {
                        m_client->renameTorrentPath(key, pending.originalName,
                                                    pending.newName);
                    }
                });
        connect(m_client, &TorrentBackend::commandFailed,
                this, [this](const QString &method, const QString &) {
                    if (method == QStringLiteral("torrent-add")
                        && !m_pendingAdds.isEmpty())
                        m_pendingAdds.dequeue();
                });
    }
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

    QStringList downloadDirs = m_knownDownloadDirs;
    for (const QString &downloadDir : acceptedDownloadDirs()) {
        if (!downloadDirs.contains(downloadDir))
            downloadDirs.append(downloadDir);
    }
    dialog.setDownloadDirs(downloadDirs);

    QString rememberedDownloadDir = lastAcceptedDownloadDir();
    if (rememberedDownloadDir.isEmpty())
        rememberedDownloadDir = savedDownloadDir();

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
    const bool userDefinedDownloadDir =
        dialog.downloadDirWasEnteredFreeForm()
        && !m_knownDownloadDirs.contains(downloadDir);
    const bool startPaused = dialog.startPaused();
    const QString renamedFolder = dialog.topLevelFolderName();

    if (!renamedFolder.isEmpty()
        && (renamedFolder == QStringLiteral(".")
            || renamedFolder == QStringLiteral("..")
            || renamedFolder.contains(QLatin1Char('/'))
            || renamedFolder.contains(QLatin1Char('\\')))) {
        emit addFailed(tr("The top-level folder name is not valid."));
        return false;
    }

    if (dialog.rememberOptions())
        saveOptions(startPaused);

    switch (sourceType) {
    case TorrentAddDialog::SourceType::TorrentFile:
        submitAdd(sourceType, source, downloadDir, startPaused,
                  dialog.unwantedFileIndices(),
                  dialog.lowPriorityFileIndices(),
                  dialog.highPriorityFileIndices(),
                  userDefinedDownloadDir,
                  renamedFolder.isEmpty()
                      ? QString()
                      : TorrentMetadataParser::parseTorrentFile(source).name,
                  renamedFolder);
        break;

    case TorrentAddDialog::SourceType::MagnetLink:
        submitAdd(sourceType, source, downloadDir, startPaused);
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

    submitAdd(sourceType, source, downloadDir, startPaused);

    emit addStarted();
    return true;
}

QString TorrentAddController::savedDownloadDir() const
{
    QSettings settings;
    const QString currentServerValue =
        settings.value(downloadDirSettingKey()).toString();
    if (!currentServerValue.isEmpty())
        return currentServerValue;

    // Migrate the original global Transmission default once to the currently
    // selected server, so it does not leak into every server profile.
    constexpr auto migrationKey = "torrentAdd/downloadDirServerMigrationComplete";
    if (m_client && m_client->backendName() == QStringLiteral("Transmission")
        && !settings.value(migrationKey, false).toBool()) {
        const QString legacyValue = settings.value(
            QString::fromLatin1(SettingsKeys::TorrentAddDownloadDir)).toString();
        if (!legacyValue.isEmpty()) {
            settings.setValue(downloadDirSettingKey(), legacyValue);
            settings.remove(QString::fromLatin1(SettingsKeys::TorrentAddDownloadDir));
        }
        settings.setValue(migrationKey, true);
        return legacyValue;
    }
    return {};
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
    // incompatible path.
    const QByteArray identity =
        (m_client->backendName() + QLatin1Char('\n') +
         m_client->endpointUrl()).toUtf8();
    const QString digest =
        QString::fromLatin1(QCryptographicHash::hash(
                               identity, QCryptographicHash::Sha256).toHex());
    return baseKey + QStringLiteral("/servers/") + digest;
}

QString TorrentAddController::lastAcceptedDownloadDir() const
{
    QSettings settings;
    return settings.value(lastAcceptedDownloadDirSettingKey()).toString();
}

QStringList TorrentAddController::acceptedDownloadDirs() const
{
    QSettings settings;
    return settings.value(acceptedDownloadDirsSettingKey()).toStringList();
}

QStringList TorrentAddController::userDefinedDownloadDirs() const
{
    return acceptedDownloadDirs();
}

bool TorrentAddController::setUserDefinedDownloadDirs(
    const QStringList &downloadDirs)
{
    QStringList normalized;
    for (const QString &downloadDir : downloadDirs) {
        const QString trimmed = downloadDir.trimmed();
        if (!trimmed.isEmpty() && !normalized.contains(trimmed))
            normalized.append(trimmed);
    }

    const QStringList current = acceptedDownloadDirs();
    if (current == normalized)
        return false;

    QSettings settings;
    settings.setValue(acceptedDownloadDirsSettingKey(), normalized);

    const QString lastAccepted = lastAcceptedDownloadDir();
    if (!lastAccepted.isEmpty() && !normalized.contains(lastAccepted)
        && !m_knownDownloadDirs.contains(lastAccepted)) {
        settings.remove(lastAcceptedDownloadDirSettingKey());
        if (settings.value(downloadDirSettingKey()).toString() == lastAccepted)
            settings.remove(downloadDirSettingKey());
    }

    return true;
}

QString TorrentAddController::lastAcceptedDownloadDirSettingKey() const
{
    return downloadDirSettingKey() + QStringLiteral("-lastAccepted");
}

QString TorrentAddController::acceptedDownloadDirsSettingKey() const
{
    return downloadDirSettingKey() + QStringLiteral("-acceptedPaths");
}

void TorrentAddController::saveOptions(bool startPaused)
{
    QSettings settings;
    // Destination paths are saved only after server confirmation; this
    // checkbox continues to govern the start-paused preference.
    settings.setValue(SettingsKeys::StartTorrentPaused, startPaused);
}

void TorrentAddController::submitAdd(
    TorrentAddDialog::SourceType sourceType,
    const QString &source,
    const QString &downloadDir,
    bool startPaused,
    const QList<int> &unwantedFileIndices,
    const QList<int> &lowPriorityFileIndices,
    const QList<int> &highPriorityFileIndices,
    bool userDefinedDownloadDir,
    const QString &originalName,
    const QString &newName)
{
    m_pendingAdds.enqueue({downloadDir, userDefinedDownloadDir,
                            originalName, newName});

    switch (sourceType) {
    case TorrentAddDialog::SourceType::TorrentFile:
        m_client->addTorrentFile(source, downloadDir, startPaused,
                                 unwantedFileIndices, lowPriorityFileIndices,
                                 highPriorityFileIndices,
                                 deleteTorrentFileOnSuccessfulAdd());
        break;
    case TorrentAddDialog::SourceType::MagnetLink:
        m_client->addMagnetLink(source, downloadDir, startPaused);
        break;
    }
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
