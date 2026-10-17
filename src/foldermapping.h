#ifndef FOLDERMAPPING_H
#define FOLDERMAPPING_H

#include <QString>

// Maps a server-visible download path to its local filesystem counterpart.
// Matching code is responsible for normalizing separators and prefixes.
struct FolderMapping
{
    QString remotePath;
    QString localPath;
};

#endif // FOLDERMAPPING_H
