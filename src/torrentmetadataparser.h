#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

struct TorrentFileMetadata
{
    int index = -1;
    QString path;
    qint64 length = 0;
};

struct TorrentMetadata
{
    QString name;
    QString infoHash;
    bool multiFile = false;
    QList<TorrentFileMetadata> files;
    QString errorString;

    bool isValid() const;
};

// Extracts only add-dialog metadata from a decoded torrent; it does not verify
// piece hashes or otherwise validate payload integrity.
class TorrentMetadataParser
{
public:
    static bool readTorrentFileData(const QString &filePath,
                                    QByteArray *data,
                                    QString *errorString = nullptr);
    static TorrentMetadata parseTorrentFile(const QString &filePath);
    static TorrentMetadata parseTorrentData(const QByteArray &data);
};
