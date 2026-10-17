#ifndef TRANSMISSIONPROTOCOL_H
#define TRANSMISSIONPROTOCOL_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <memory>

enum class TransmissionProtocolDialect
{
    Legacy,
    JsonRpc2
};

struct TransmissionProtocolReply
{
    bool valid = false;
    bool success = false;
    QJsonObject result;
    QString error;
};

// Owns the Transmission wire envelope while the backend retains semantic
// request routing and conversion into Planetary's backend-neutral objects.
class TransmissionProtocol
{
public:
    virtual ~TransmissionProtocol() = default;

    virtual TransmissionProtocolDialect dialect() const = 0;
    virtual QByteArray encodeRequest(const QString &method,
                                     const QJsonObject &parameters,
                                     qint64 requestId) const = 0;
    virtual TransmissionProtocolReply decodeReply(
        const QJsonObject &root, qint64 expectedRequestId,
        const QString &requestMethod) const = 0;

    static std::unique_ptr<TransmissionProtocol> createLegacy();
    static std::unique_ptr<TransmissionProtocol> createJsonRpc2();
};

#endif // TRANSMISSIONPROTOCOL_H
