#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>

class BencodeValue
{
public:
    enum class Type {
        Invalid,
        Integer,
        ByteString,
        List,
        Dictionary
    };

    using List = QList<BencodeValue>;
    using Dictionary = QMap<QByteArray, BencodeValue>;

    BencodeValue();

    static BencodeValue integer(qint64 value);
    static BencodeValue byteString(const QByteArray &value);
    static BencodeValue list(const List &value);
    static BencodeValue dictionary(const Dictionary &value);

    Type type() const;

    bool isValid() const;
    bool isInteger() const;
    bool isByteString() const;
    bool isList() const;
    bool isDictionary() const;

    qint64 toInteger(qint64 defaultValue = 0) const;
    QByteArray toByteArray() const;
    QString toString() const;

    const List &toList() const;
    const Dictionary &toDictionary() const;

    bool contains(const QByteArray &key) const;
    const BencodeValue *value(const QByteArray &key) const;

private:
    Type m_type = Type::Invalid;
    qint64 m_integer = 0;
    QByteArray m_byteString;
    List m_list;
    Dictionary m_dictionary;
};
