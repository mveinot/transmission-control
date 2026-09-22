#include "bencodeparser.h"

#include <QChar>
#include <QStringView>

namespace {

constexpr int MaximumNestingDepth = 128;
constexpr qsizetype MaximumInputBytes = 128 * 1024 * 1024;
constexpr qsizetype MaximumValueCount = 500'000;
constexpr qsizetype MaximumContainerEntries = 250'000;
constexpr qint64 MaximumByteStringBytes = 64 * 1024 * 1024;

bool isAsciiDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

} // namespace

bool BencodeParser::parse(const QByteArray &data,
                          BencodeValue *result,
                          QString *errorString)
{
    if (data.size() > MaximumInputBytes) {
        if (result)
            *result = BencodeValue();
        if (errorString)
            *errorString = QStringLiteral("Bencode input exceeds the 128 MiB limit");
        return false;
    }

    if (result)
        *result = BencodeValue();

    BencodeParser parser(data);
    BencodeValue parsedValue;

    if (!parser.parseValue(&parsedValue)) {
        if (errorString)
            *errorString = parser.m_errorString;
        return false;
    }

    if (!parser.atEnd()) {
        parser.setError(QStringLiteral("Unexpected trailing data at offset %1")
                            .arg(parser.m_offset));
        if (errorString)
            *errorString = parser.m_errorString;
        return false;
    }

    if (result)
        *result = parsedValue;

    if (errorString)
        errorString->clear();

    return true;
}

BencodeParser::BencodeParser(const QByteArray &data)
    : m_data(data)
{
}

bool BencodeParser::parseValue(BencodeValue *result)
{
    if (!consumeValue())
        return false;

    // Container parsing is recursive, so reject hostile nesting before adding
    // another stack frame.
    if (m_depth >= MaximumNestingDepth) {
        setError(QStringLiteral("Maximum nesting depth exceeded at offset %1")
                     .arg(m_offset));
        return false;
    }

    if (atEnd()) {
        setError(QStringLiteral("Unexpected end of data"));
        return false;
    }

    const char ch = currentChar();
    ++m_depth;

    bool parsed = false;

    if (ch == 'i')
        parsed = parseInteger(result);
    else if (ch == 'l')
        parsed = parseList(result);
    else if (ch == 'd')
        parsed = parseDictionary(result);
    else if (isAsciiDigit(ch))
        parsed = parseByteString(result);
    else
        setError(QStringLiteral("Unexpected token '%1' at offset %2")
                     .arg(QChar::fromLatin1(ch))
                     .arg(m_offset));

    --m_depth;
    return parsed;
}

bool BencodeParser::consumeValue()
{
    if (m_valueCount >= MaximumValueCount) {
        setError(QStringLiteral("Maximum bencode value count exceeded"));
        return false;
    }

    ++m_valueCount;
    return true;
}

bool BencodeParser::parseInteger(BencodeValue *result)
{
    ++m_offset; // i

    const qsizetype start = m_offset;

    if (atEnd()) {
        setError(QStringLiteral("Unterminated integer"));
        return false;
    }

    const bool negative = currentChar() == '-';
    if (negative)
        ++m_offset;

    if (atEnd() || !isAsciiDigit(currentChar())) {
        setError(QStringLiteral("Invalid integer at offset %1").arg(start));
        return false;
    }

    if (currentChar() == '0') {
        ++m_offset;
        if (negative || atEnd() || currentChar() != 'e') {
            setError(QStringLiteral("Invalid integer at offset %1").arg(start));
            return false;
        }
    }

    while (!atEnd() && isAsciiDigit(currentChar()))
        ++m_offset;

    if (atEnd() || currentChar() != 'e') {
        setError(QStringLiteral("Unterminated integer at offset %1").arg(start));
        return false;
    }

    const QByteArray numberBytes = m_data.mid(start, m_offset - start);
    bool ok = false;
    const qint64 number = numberBytes.toLongLong(&ok);

    if (!ok) {
        setError(QStringLiteral("Invalid integer value at offset %1").arg(start));
        return false;
    }

    ++m_offset; // e

    if (result)
        *result = BencodeValue::integer(number);

    return true;
}

bool BencodeParser::parseByteString(BencodeValue *result)
{
    const qsizetype lengthStart = m_offset;

    while (!atEnd() && isAsciiDigit(currentChar()))
        ++m_offset;

    if (atEnd() || currentChar() != ':') {
        setError(QStringLiteral("Invalid byte string length at offset %1")
                     .arg(lengthStart));
        return false;
    }

    const QByteArray lengthBytes = m_data.mid(lengthStart, m_offset - lengthStart);
    bool ok = false;
    const qlonglong length = lengthBytes.toLongLong(&ok);

    if (!ok || length < 0) {
        setError(QStringLiteral("Invalid byte string length at offset %1")
                     .arg(lengthStart));
        return false;
    }

    ++m_offset; // :

    // Avoid adding an untrusted length to m_offset; subtraction keeps the
    // bounds check valid at the signed integer limit.
    const qsizetype remaining = m_data.size() - m_offset;

    if (length > remaining) {
        setError(QStringLiteral("Byte string at offset %1 extends past end of data")
                     .arg(lengthStart));
        return false;
    }

    if (length > MaximumByteStringBytes) {
        setError(QStringLiteral("Byte string at offset %1 exceeds the 64 MiB limit")
                     .arg(lengthStart));
        return false;
    }

    const QByteArray bytes = m_data.mid(m_offset, length);
    m_offset += length;

    if (result)
        *result = BencodeValue::byteString(bytes);

    return true;
}

bool BencodeParser::parseList(BencodeValue *result)
{
    ++m_offset; // l

    BencodeValue::List list;
    qsizetype entryCount = 0;

    while (!atEnd() && currentChar() != 'e') {
        if (entryCount >= MaximumContainerEntries) {
            setError(QStringLiteral("Maximum list entry count exceeded"));
            return false;
        }
        ++entryCount;

        BencodeValue item;

        if (!parseValue(&item))
            return false;

        list.append(item);
    }

    if (atEnd()) {
        setError(QStringLiteral("Unterminated list"));
        return false;
    }

    ++m_offset; // e

    if (result)
        *result = BencodeValue::list(list);

    return true;
}

bool BencodeParser::parseDictionary(BencodeValue *result)
{
    ++m_offset; // d

    BencodeValue::Dictionary dictionary;
    QByteArray previousKey;
    qsizetype entryCount = 0;

    while (!atEnd() && currentChar() != 'e') {
        if (entryCount >= MaximumContainerEntries) {
            setError(QStringLiteral("Maximum dictionary entry count exceeded"));
            return false;
        }
        ++entryCount;

        BencodeValue keyValue;

        if (!consumeValue())
            return false;
        if (!parseByteString(&keyValue))
            return false;

        const QByteArray key = keyValue.toByteArray();
        if (entryCount > 1 && key <= previousKey) {
            setError(QStringLiteral("Dictionary keys are not strictly sorted"));
            return false;
        }
        previousKey = key;

        BencodeValue value;

        if (!parseValue(&value))
            return false;

        dictionary.insert(key, value);
    }

    if (atEnd()) {
        setError(QStringLiteral("Unterminated dictionary"));
        return false;
    }

    ++m_offset; // e

    if (result)
        *result = BencodeValue::dictionary(dictionary);

    return true;
}

bool BencodeParser::atEnd() const
{
    return m_offset >= m_data.size();
}

char BencodeParser::currentChar() const
{
    return m_data.at(m_offset);
}

void BencodeParser::setError(const QString &message)
{
    if (m_errorString.isEmpty())
        m_errorString = message;
}
