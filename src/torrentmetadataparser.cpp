#include "torrentmetadataparser.h"

#include "bencodeparser.h"

#include <QFile>
#include <QObject>
#include <QStringList>

namespace {

QString valueToString(const BencodeValue *value)
{
    if (!value || !value->isByteString())
        return {};

    return value->toString();
}

QString dictionaryString(const BencodeValue &dictionary,
                         const QByteArray &utf8Key,
                         const QByteArray &fallbackKey)
{
    QString text = valueToString(dictionary.value(utf8Key));

    if (!text.isEmpty())
        return text;

    return valueToString(dictionary.value(fallbackKey));
}

QString pathFromList(const BencodeValue *value)
{
    if (!value || !value->isList())
        return {};

    QStringList parts;

    for (const BencodeValue &part : value->toList()) {
        if (!part.isByteString())
            return {};

        parts.append(part.toString());
    }

    return parts.join(QLatin1Char('/'));
}

QString filePathFromDictionary(const BencodeValue &fileDictionary)
{
    QString path = pathFromList(fileDictionary.value("path.utf-8"));

    if (!path.isEmpty())
        return path;

    return pathFromList(fileDictionary.value("path"));
}

} // namespace

bool TorrentMetadata::isValid() const
{
    return errorString.isEmpty() && !name.isEmpty() && !files.isEmpty();
}

TorrentMetadata TorrentMetadataParser::parseTorrentFile(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        TorrentMetadata metadata;
        metadata.errorString = QObject::tr("Could not open torrent file: %1")
                                   .arg(file.errorString());
        return metadata;
    }

    return parseTorrentData(file.readAll());
}

TorrentMetadata TorrentMetadataParser::parseTorrentData(const QByteArray &data)
{
    TorrentMetadata metadata;

    BencodeValue root;
    QString errorString;

    if (!BencodeParser::parse(data, &root, &errorString)) {
        metadata.errorString = errorString;
        return metadata;
    }

    if (!root.isDictionary()) {
        metadata.errorString = QObject::tr("Torrent metadata root is not a dictionary.");
        return metadata;
    }

    const BencodeValue *info = root.value("info");

    if (!info || !info->isDictionary()) {
        metadata.errorString = QObject::tr("Torrent metadata is missing the info dictionary.");
        return metadata;
    }

    metadata.name = dictionaryString(*info, "name.utf-8", "name");

    if (metadata.name.isEmpty()) {
        metadata.errorString = QObject::tr("Torrent metadata is missing a name.");
        return metadata;
    }

    const BencodeValue *files = info->value("files");

    if (files) {
        if (!files->isList()) {
            metadata.errorString = QObject::tr("Torrent metadata files entry is not a list.");
            return metadata;
        }

        metadata.multiFile = true;
        int index = 0;

        for (const BencodeValue &fileValue : files->toList()) {
            if (!fileValue.isDictionary()) {
                metadata.errorString = QObject::tr("Torrent file entry is not a dictionary.");
                return metadata;
            }

            const BencodeValue *lengthValue = fileValue.value("length");

            if (!lengthValue || !lengthValue->isInteger()) {
                metadata.errorString = QObject::tr("Torrent file entry is missing a length.");
                return metadata;
            }

            const QString path = filePathFromDictionary(fileValue);

            if (path.isEmpty()) {
                metadata.errorString = QObject::tr("Torrent file entry is missing a path.");
                return metadata;
            }

            TorrentFileMetadata file;
            file.index = index++;
            file.path = path;
            file.length = lengthValue->toInteger();

            metadata.files.append(file);
        }

        if (metadata.files.isEmpty())
            metadata.errorString = QObject::tr("Torrent metadata contains no files.");

        return metadata;
    }

    const BencodeValue *lengthValue = info->value("length");

    if (!lengthValue || !lengthValue->isInteger()) {
        metadata.errorString = QObject::tr("Single-file torrent metadata is missing a length.");
        return metadata;
    }

    TorrentFileMetadata file;
    file.index = 0;
    file.path = metadata.name;
    file.length = lengthValue->toInteger();

    metadata.files.append(file);
    return metadata;
}
