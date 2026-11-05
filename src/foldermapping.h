#ifndef FOLDERMAPPING_H
#define FOLDERMAPPING_H

#include <QDir>
#include <QString>

namespace FolderMappings {

inline QString normalizeRemotePath(QString path)
{
    path = path.trimmed();
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QDir::cleanPath(path);
}

} // namespace FolderMappings

// Maps a server-visible download path to its local filesystem counterpart.
// Remote paths use normalizeRemotePath(); local paths stay native to the client.
struct FolderMapping
{
    QString remotePath;
    QString localPath;
};

#endif // FOLDERMAPPING_H
