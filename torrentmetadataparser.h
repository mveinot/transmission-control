#pragma once

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
    bool multiFile = false;
    QList<TorrentFileMetadata> files;
    QString errorString;

    bool isValid() const;
};

class TorrentMetadataParser
{
public:
    static TorrentMetadata parseTorrentFile(const QString &filePath);
    static TorrentMetadata parseTorrentData(const QByteArray &data);
};
