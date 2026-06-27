#include <QtTest/QtTest>

#include "updatechecker.h"

class TestUpdateChecker : public QObject
{
    Q_OBJECT

private slots:
    void versionComparison_data();
    void versionComparison();
};

void TestUpdateChecker::versionComparison_data()
{
    QTest::addColumn<QString>("latestVersion");
    QTest::addColumn<QString>("currentVersion");
    QTest::addColumn<bool>("isNewer");

    QTest::newRow("patch newer") << QStringLiteral("1.2.1") << QStringLiteral("1.2.0") << true;
    QTest::newRow("leading v") << QStringLiteral("v1.2.1") << QStringLiteral("1.2.0") << true;
    QTest::newRow("two digit minor") << QStringLiteral("1.10.0") << QStringLiteral("1.9.9") << true;
    QTest::newRow("missing patch equivalent") << QStringLiteral("1.2") << QStringLiteral("1.2.0") << false;
    QTest::newRow("equal full version") << QStringLiteral("1.2.0") << QStringLiteral("1.2.0") << false;
    QTest::newRow("older latest") << QStringLiteral("1.2.0") << QStringLiteral("1.2.1") << false;
    QTest::newRow("prerelease suffix treated as non-numeric tail") << QStringLiteral("v1.2.0-beta") << QStringLiteral("1.2.0") << false;
    QTest::newRow("build component newer") << QStringLiteral("1.2.0.42") << QStringLiteral("1.2.0.41") << true;
}

void TestUpdateChecker::versionComparison()
{
    QFETCH(QString, latestVersion);
    QFETCH(QString, currentVersion);
    QFETCH(bool, isNewer);

    QCOMPARE(UpdateChecker::isVersionNewer(latestVersion, currentVersion), isNewer);
}

QTEST_MAIN(TestUpdateChecker)
#include "test_updatechecker.moc"
