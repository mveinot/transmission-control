#include "torrentmetadataparser.h"

#include "bencodeparser.h"

#include <QCryptographicHash>
#include <QFile>
#include <QObject>
#include <QStringList>

#include <limits>

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

    const auto tooLarge = []() {
        TorrentMetadata metadata;
        metadata.errorString = QObject::tr(
            "Bencode input exceeds the 128 MiB limit");
        return metadata;
    };

    if (file.size() > BencodeParser::MaximumInputBytes)
        return tooLarge();

    QByteArray data = file.read(
        static_cast<qint64>(BencodeParser::MaximumInputBytes) + 1);
    if (file.error() != QFileDevice::NoError) {
        TorrentMetadata metadata;
        metadata.errorString = QObject::tr("Could not read torrent file: %1")
                                   .arg(file.errorString());
        return metadata;
    }

    if (data.size() > BencodeParser::MaximumInputBytes)
        return tooLarge();

    return parseTorrentData(data);
}

TorrentMetadata TorrentMetadataParser::parseTorrentData(const QByteArray &data)
{
    TorrentMetadata metadata;

    BencodeValue root;
    QString errorString;
    QByteArray rawInfoDictionary;

    if (!BencodeParser::parse(data, &root, &errorString,
                              &rawInfoDictionary)) {
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

    if (!rawInfoDictionary.isEmpty()) {
        const BencodeValue *metaVersion = info->value("meta version");
        const bool hasV1Pieces = info->value("pieces") != nullptr;
        const QCryptographicHash::Algorithm algorithm =
            metaVersion && metaVersion->toInteger() == 2 && !hasV1Pieces
                ? QCryptographicHash::Sha256
                : QCryptographicHash::Sha1;
        metadata.infoHash = QString::fromLatin1(
            QCryptographicHash::hash(rawInfoDictionary, algorithm).toHex());
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
        qint64 totalSize = 0;

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

            const qint64 length = lengthValue->toInteger();
            if (length < 0) {
                metadata.errorString = QObject::tr(
                    "Torrent file entry has a negative length.");
                return metadata;
            }

            if (length > std::numeric_limits<qint64>::max() - totalSize) {
                metadata.errorString = QObject::tr(
                    "Combined torrent file size exceeds the supported range.");
                return metadata;
            }
            totalSize += length;

            const QString path = filePathFromDictionary(fileValue);

            if (path.isEmpty()) {
                metadata.errorString = QObject::tr("Torrent file entry is missing a path.");
                return metadata;
            }

            TorrentFileMetadata file;
            file.index = index++;
            file.path = path;
            file.length = length;

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

    const qint64 length = lengthValue->toInteger();
    if (length < 0) {
        metadata.errorString = QObject::tr(
            "Single-file torrent metadata has a negative length.");
        return metadata;
    }

    TorrentFileMetadata file;
    file.index = 0;
    file.path = metadata.name;
    file.length = length;

    metadata.files.append(file);
    return metadata;
}
