#include "bencodevalue.h"

BencodeValue::BencodeValue() = default;

BencodeValue BencodeValue::integer(qint64 value)
{
    BencodeValue result;
    result.m_type = Type::Integer;
    result.m_integer = value;
    return result;
}

BencodeValue BencodeValue::byteString(const QByteArray &value)
{
    BencodeValue result;
    result.m_type = Type::ByteString;
    result.m_byteString = value;
    return result;
}

BencodeValue BencodeValue::list(const List &value)
{
    BencodeValue result;
    result.m_type = Type::List;
    result.m_list = value;
    return result;
}

BencodeValue BencodeValue::dictionary(const Dictionary &value)
{
    BencodeValue result;
    result.m_type = Type::Dictionary;
    result.m_dictionary = value;
    return result;
}

BencodeValue::Type BencodeValue::type() const
{
    return m_type;
}

bool BencodeValue::isValid() const
{
    return m_type != Type::Invalid;
}

bool BencodeValue::isInteger() const
{
    return m_type == Type::Integer;
}

bool BencodeValue::isByteString() const
{
    return m_type == Type::ByteString;
}

bool BencodeValue::isList() const
{
    return m_type == Type::List;
}

bool BencodeValue::isDictionary() const
{
    return m_type == Type::Dictionary;
}

qint64 BencodeValue::toInteger(qint64 defaultValue) const
{
    if (!isInteger())
        return defaultValue;

    return m_integer;
}

QByteArray BencodeValue::toByteArray() const
{
    if (!isByteString())
        return {};

    return m_byteString;
}

QString BencodeValue::toString() const
{
    const QByteArray bytes = toByteArray();
    return QString::fromUtf8(bytes.constData(), bytes.size());
}

const BencodeValue::List &BencodeValue::toList() const
{
    static const List emptyList;

    if (!isList())
        return emptyList;

    return m_list;
}

const BencodeValue::Dictionary &BencodeValue::toDictionary() const
{
    static const Dictionary emptyDictionary;

    if (!isDictionary())
        return emptyDictionary;

    return m_dictionary;
}

bool BencodeValue::contains(const QByteArray &key) const
{
    return isDictionary() && m_dictionary.contains(key);
}

const BencodeValue *BencodeValue::value(const QByteArray &key) const
{
    if (!isDictionary())
        return nullptr;

    const auto it = m_dictionary.constFind(key);

    if (it == m_dictionary.constEnd())
        return nullptr;

    return &it.value();
}
