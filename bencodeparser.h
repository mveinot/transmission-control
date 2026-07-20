#pragma once

#include "bencodevalue.h"

#include <QByteArray>
#include <QString>

class BencodeParser
{
public:
    static bool parse(const QByteArray &data,
                      BencodeValue *result,
                      QString *errorString = nullptr);

private:
    explicit BencodeParser(const QByteArray &data);

    bool parseValue(BencodeValue *result);
    bool parseInteger(BencodeValue *result);
    bool parseByteString(BencodeValue *result);
    bool parseList(BencodeValue *result);
    bool parseDictionary(BencodeValue *result);

    bool atEnd() const;
    char currentChar() const;
    void setError(const QString &message);

    const QByteArray &m_data;
    qsizetype m_offset = 0;
    int m_depth = 0;
    QString m_errorString;
};
