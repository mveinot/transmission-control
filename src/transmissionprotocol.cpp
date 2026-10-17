#include "transmissionprotocol.h"

#include <QJsonDocument>
#include <QJsonArray>

namespace {

QString modernName(const QString &legacy)
{
    QString result;
    result.reserve(legacy.size() + 4);
    for (const QChar character : legacy) {
        if (character == QLatin1Char('-')) {
            result.append(QLatin1Char('_'));
        } else if (character.isUpper()) {
            if (!result.isEmpty() && !result.endsWith(QLatin1Char('_')))
                result.append(QLatin1Char('_'));
            result.append(character.toLower());
        } else {
            result.append(character);
        }
    }
    return result;
}

QString camelName(const QString &modern)
{
    QString result;
    result.reserve(modern.size());
    bool capitalize = false;
    for (const QChar character : modern) {
        if (character == QLatin1Char('_')) {
            capitalize = true;
        } else if (capitalize) {
            result.append(character.toUpper());
            capitalize = false;
        } else {
            result.append(character);
        }
    }
    return result;
}

QJsonValue modernizeValue(const QJsonValue &value, bool fieldNames = false)
{
    if (value.isObject()) {
        QJsonObject result;
        const QJsonObject source = value.toObject();
        for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
            const QString key = modernName(it.key());
            result.insert(key,
                          modernizeValue(it.value(), key == QStringLiteral("fields")));
        }
        return result;
    }
    if (value.isArray()) {
        QJsonArray result;
        for (const QJsonValue &item : value.toArray())
            result.append(modernizeValue(item, fieldNames));
        return result;
    }
    if (fieldNames && value.isString())
        return modernName(value.toString());
    return value;
}

enum class ResultNames
{
    Torrent,
    Session,
    SessionStatistics,
    FreeSpace,
    PortTest,
    BlocklistUpdate,
    TorrentAdd
};

QString canonicalResultName(const QString &modern, ResultNames names)
{
    if (names == ResultNames::Torrent)
        return modern == QStringLiteral("peer_limit")
                   ? QStringLiteral("peer-limit")
                   : camelName(modern);
    if (names == ResultNames::SessionStatistics)
        return modern == QStringLiteral("current_stats")
                   ? QStringLiteral("current-stats")
                   : modern == QStringLiteral("cumulative_stats")
                         ? QStringLiteral("cumulative-stats")
                         : camelName(modern);
    if (names == ResultNames::FreeSpace)
        return modern == QStringLiteral("size_bytes")
                   ? QStringLiteral("size-bytes") : modern;
    if (names == ResultNames::PortTest)
        return QString(modern).replace(QLatin1Char('_'), QLatin1Char('-'));
    if (names == ResultNames::BlocklistUpdate)
        return modern == QStringLiteral("blocklist_size")
                   ? QStringLiteral("blocklist-size") : modern;
    if (names == ResultNames::TorrentAdd)
        return modern == QStringLiteral("torrent_added")
                   ? QStringLiteral("torrent-added") : camelName(modern);

    // SessionSettingsDialog's normalized contract uses Transmission's legacy
    // kebab-case names, with these historical camel-case exceptions.
    if (modern == QStringLiteral("seed_ratio_limited"))
        return QStringLiteral("seedRatioLimited");
    if (modern == QStringLiteral("seed_ratio_limit"))
        return QStringLiteral("seedRatioLimit");
    return QString(modern).replace(QLatin1Char('_'), QLatin1Char('-'));
}

QJsonValue canonicalizeValue(const QJsonValue &value, ResultNames names)
{
    if (value.isObject()) {
        QJsonObject result;
        const QJsonObject source = value.toObject();
        for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
            const QString key = canonicalResultName(it.key(), names);
            ResultNames childNames = names;
            if (names == ResultNames::SessionStatistics
                && (it.key() == QStringLiteral("current_stats")
                    || it.key() == QStringLiteral("cumulative_stats"))) {
                childNames = ResultNames::Torrent;
            }
            result.insert(key, canonicalizeValue(it.value(), childNames));
        }
        return result;
    }
    if (value.isArray()) {
        QJsonArray result;
        for (const QJsonValue &item : value.toArray())
            result.append(canonicalizeValue(item, names));
        return result;
    }
    return value;
}

ResultNames resultNamesForMethod(const QString &method)
{
    if (method == QStringLiteral("session-get"))
        return ResultNames::Session;
    if (method == QStringLiteral("session-stats"))
        return ResultNames::SessionStatistics;
    if (method == QStringLiteral("free-space"))
        return ResultNames::FreeSpace;
    if (method == QStringLiteral("port-test"))
        return ResultNames::PortTest;
    if (method == QStringLiteral("blocklist-update"))
        return ResultNames::BlocklistUpdate;
    if (method == QStringLiteral("torrent-add"))
        return ResultNames::TorrentAdd;
    return ResultNames::Torrent;
}


class TransmissionLegacyProtocol final : public TransmissionProtocol
{
public:
    TransmissionProtocolDialect dialect() const override
    {
        return TransmissionProtocolDialect::Legacy;
    }

    QByteArray encodeRequest(const QString &method,
                             const QJsonObject &parameters,
                             qint64) const override
    {
        QJsonObject root{{QStringLiteral("method"), method}};
        if (!parameters.isEmpty())
            root.insert(QStringLiteral("arguments"), parameters);
        return QJsonDocument(root).toJson(QJsonDocument::Compact);
    }

    TransmissionProtocolReply decodeReply(
        const QJsonObject &root, qint64, const QString &) const override
    {
        TransmissionProtocolReply reply;
        const QJsonValue resultValue = root.value(QStringLiteral("result"));
        if (!resultValue.isString()) {
            reply.error = QStringLiteral("Transmission RPC response omitted its result.");
            return reply;
        }

        reply.valid = true;
        const QString result = resultValue.toString();
        reply.success = result == QStringLiteral("success");
        reply.result = root.value(QStringLiteral("arguments")).toObject();
        if (!reply.success)
            reply.error = result;
        return reply;
    }
};

class TransmissionJsonRpcProtocol final : public TransmissionProtocol
{
public:
    TransmissionProtocolDialect dialect() const override
    {
        return TransmissionProtocolDialect::JsonRpc2;
    }

    QByteArray encodeRequest(const QString &method,
                             const QJsonObject &parameters,
                             qint64 requestId) const override
    {
        QJsonObject root{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("method"), modernName(method)},
            {QStringLiteral("id"), requestId}
        };
        if (!parameters.isEmpty())
            root.insert(QStringLiteral("params"), modernizeValue(parameters));
        return QJsonDocument(root).toJson(QJsonDocument::Compact);
    }

    TransmissionProtocolReply decodeReply(
        const QJsonObject &root, qint64 expectedRequestId,
        const QString &requestMethod) const override
    {
        TransmissionProtocolReply reply;
        if (root.value(QStringLiteral("jsonrpc")).toString()
                != QStringLiteral("2.0")) {
            reply.error = QStringLiteral("Transmission returned an invalid JSON-RPC version.");
            return reply;
        }
        if (!root.value(QStringLiteral("id")).isDouble()
            || root.value(QStringLiteral("id")).toVariant().toLongLong()
                   != expectedRequestId) {
            reply.error = QStringLiteral("Transmission returned a mismatched JSON-RPC request ID.");
            return reply;
        }

        reply.valid = true;
        const QJsonValue errorValue = root.value(QStringLiteral("error"));
        if (errorValue.isObject()) {
            const QJsonObject error = errorValue.toObject();
            const QString message = error.value(QStringLiteral("message")).toString();
            const int code = error.value(QStringLiteral("code")).toInt();
            reply.error = message.isEmpty()
                              ? QStringLiteral("Transmission JSON-RPC error %1").arg(code)
                              : message;
            return reply;
        }
        if (!root.contains(QStringLiteral("result"))) {
            reply.valid = false;
            reply.error = QStringLiteral("Transmission JSON-RPC response omitted its result.");
            return reply;
        }

        reply.success = true;
        reply.result = canonicalizeValue(
                           root.value(QStringLiteral("result")),
                           resultNamesForMethod(requestMethod)).toObject();
        return reply;
    }
};

} // namespace

std::unique_ptr<TransmissionProtocol> TransmissionProtocol::createLegacy()
{
    return std::make_unique<TransmissionLegacyProtocol>();
}

std::unique_ptr<TransmissionProtocol> TransmissionProtocol::createJsonRpc2()
{
    return std::make_unique<TransmissionJsonRpcProtocol>();
}
