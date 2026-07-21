#include <QtTest/QtTest>

#include "bencodeparser.h"
#include "torrentmetadataparser.h"

class TestBencodeParser : public QObject
{
    Q_OBJECT

private slots:
    void parsesInteger();
    void parsesByteString();
    void parsesList();
    void parsesDictionary();
    void rejectsTrailingData();
    void rejectsOverflowingByteStringLength();
    void rejectsExcessiveNesting();
    void parsesSingleFileTorrentMetadata();
    void parsesMultiFileTorrentMetadata();
};

void TestBencodeParser::parsesInteger()
{
    BencodeValue value;
    QString error;

    QVERIFY2(BencodeParser::parse("i42e", &value, &error), qPrintable(error));
    QVERIFY(value.isInteger());
    QCOMPARE(value.toInteger(), 42);
}

void TestBencodeParser::parsesByteString()
{
    BencodeValue value;
    QString error;

    QVERIFY2(BencodeParser::parse("4:spam", &value, &error), qPrintable(error));
    QVERIFY(value.isByteString());
    QCOMPARE(value.toByteArray(), QByteArray("spam"));
}

void TestBencodeParser::parsesList()
{
    BencodeValue value;
    QString error;

    QVERIFY2(BencodeParser::parse("l4:spami7ee", &value, &error), qPrintable(error));
    QVERIFY(value.isList());
    QCOMPARE(value.toList().size(), 2);
    QCOMPARE(value.toList().at(0).toByteArray(), QByteArray("spam"));
    QCOMPARE(value.toList().at(1).toInteger(), 7);
}

void TestBencodeParser::parsesDictionary()
{
    BencodeValue value;
    QString error;

    QVERIFY2(BencodeParser::parse("d3:cow3:moo4:spam4:eggse", &value, &error), qPrintable(error));
    QVERIFY(value.isDictionary());
    QVERIFY(value.contains("cow"));
    QCOMPARE(value.value("cow")->toByteArray(), QByteArray("moo"));
    QCOMPARE(value.value("spam")->toByteArray(), QByteArray("eggs"));
}

void TestBencodeParser::rejectsTrailingData()
{
    BencodeValue value;
    QString error;

    QVERIFY(!BencodeParser::parse("i42ejunk", &value, &error));
    QVERIFY(!error.isEmpty());
}

void TestBencodeParser::rejectsOverflowingByteStringLength()
{
    BencodeValue value;
    QString error;

    QVERIFY(!BencodeParser::parse("9223372036854775807:x", &value, &error));
    QVERIFY(error.contains(QStringLiteral("extends past end")));
}

void TestBencodeParser::rejectsExcessiveNesting()
{
    const QByteArray data = QByteArray(300, 'l') + QByteArray(300, 'e');
    BencodeValue value;
    QString error;

    QVERIFY(!BencodeParser::parse(data, &value, &error));
    QVERIFY(error.contains(QStringLiteral("Maximum nesting depth")));
}

void TestBencodeParser::parsesSingleFileTorrentMetadata()
{
    const QByteArray torrent =
        "d"
          "4:info"
          "d"
            "6:length" "i123e"
            "4:name" "8:file.mkv"
          "e"
        "e";

    const TorrentMetadata metadata =
        TorrentMetadataParser::parseTorrentData(torrent);

    QVERIFY2(metadata.isValid(), qPrintable(metadata.errorString));
    QVERIFY(!metadata.multiFile);
    QCOMPARE(metadata.name, QStringLiteral("file.mkv"));
    QCOMPARE(metadata.files.size(), 1);
    QCOMPARE(metadata.files.at(0).index, 0);
    QCOMPARE(metadata.files.at(0).path, QStringLiteral("file.mkv"));
    QCOMPARE(metadata.files.at(0).length, 123);
}

void TestBencodeParser::parsesMultiFileTorrentMetadata()
{
    const QByteArray torrent =
        "d"
          "4:info"
          "d"
            "5:files"
            "l"
              "d"
                "6:length" "i100e"
                "4:path" "l3:dir9:file1.mkve"
              "e"
              "d"
                "6:length" "i200e"
                "4:path" "l3:dir9:file2.mkve"
              "e"
            "e"
            "4:name" "3:Top"
          "e"
        "e";

    const TorrentMetadata metadata =
        TorrentMetadataParser::parseTorrentData(torrent);

    QVERIFY2(metadata.isValid(), qPrintable(metadata.errorString));
    QVERIFY(metadata.multiFile);
    QCOMPARE(metadata.name, QStringLiteral("Top"));
    QCOMPARE(metadata.files.size(), 2);
    QCOMPARE(metadata.files.at(0).index, 0);
    QCOMPARE(metadata.files.at(0).path, QStringLiteral("dir/file1.mkv"));
    QCOMPARE(metadata.files.at(0).length, 100);
    QCOMPARE(metadata.files.at(1).index, 1);
    QCOMPARE(metadata.files.at(1).path, QStringLiteral("dir/file2.mkv"));
    QCOMPARE(metadata.files.at(1).length, 200);
}

QTEST_MAIN(TestBencodeParser)
#include "test_bencodeparser.moc"
