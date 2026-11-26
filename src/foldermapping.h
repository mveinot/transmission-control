#ifndef FOLDERMAPPING_H
#define FOLDERMAPPING_H

#include <QDir>
#include <QList>
#include <QString>

// Maps a server-visible download path to its local filesystem counterpart.
// Remote paths use normalizeRemotePath(); local paths stay native to the client.
struct FolderMapping
{
    QString remotePath;
    QString localPath;
};

namespace FolderMappings {

inline QString normalizeRemotePath(QString path)
{
    path = path.trimmed();
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QDir::cleanPath(path);
}

inline QString mapRemotePathToLocalPath(
    const QString &remotePath, const QList<FolderMapping> &mappings)
{
    const QString normalizedRemotePath =
        normalizeRemotePath(remotePath);
    if (normalizedRemotePath.isEmpty())
        return {};
    const FolderMapping *bestMatch = nullptr;
    QString bestRemotePrefix;
    for (const FolderMapping &mapping : mappings) {
        const QString remotePrefix =
            normalizeRemotePath(mapping.remotePath);
        if (remotePrefix.isEmpty())
            continue;
        const bool exact = normalizedRemotePath == remotePrefix;
        const QString childPrefix = remotePrefix == QStringLiteral("/")
            ? remotePrefix
            : remotePrefix + QLatin1Char('/');
        const bool child = normalizedRemotePath.startsWith(childPrefix)
            && normalizedRemotePath != remotePrefix;
        if ((exact || child) && (!bestMatch || remotePrefix.length() > bestRemotePrefix.length())) {
            bestMatch = &mapping;
            bestRemotePrefix = remotePrefix;
        }
    }
    if (!bestMatch)
        return {};
    const QString localPrefix = QDir::cleanPath(bestMatch->localPath.trimmed());
    QString suffix = normalizedRemotePath.mid(bestRemotePrefix.length());
    if (suffix.startsWith(QLatin1Char('/')))
        suffix.remove(0, 1);
    return suffix.isEmpty() ? localPrefix : QDir(localPrefix).filePath(suffix);
}

} // namespace FolderMappings

#endif // FOLDERMAPPING_H
