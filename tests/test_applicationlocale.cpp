#include "applicationlocale.h"

#include <QTest>

class TestApplicationLocale : public QObject
{
    Q_OBJECT

private slots:
    void includesSourceLanguageAndSystemDefault();
    void environmentOverrideTakesPrecedence();
    void savedPreferenceOverridesSystemLocale();
};

void TestApplicationLocale::includesSourceLanguageAndSystemDefault()
{
    const QList<ApplicationLocaleOption> options =
        ApplicationLocale::availableOptions();
    QStringList codes;
    for (const ApplicationLocaleOption &option : options)
        codes.append(option.code);

    QVERIFY(!options.isEmpty());
    QCOMPARE(options.constFirst().code,
             QString::fromLatin1(ApplicationLocale::SystemDefault));
    QVERIFY(codes.contains(QString::fromLatin1(ApplicationLocale::English)));
    QVERIFY(codes.contains(QStringLiteral("de")));
    QVERIFY(codes.contains(QStringLiteral("es")));
    QVERIFY(codes.contains(QStringLiteral("fr_CA")));
    QVERIFY(codes.contains(QStringLiteral("nl")));
    QVERIFY(codes.contains(QStringLiteral("pt_BR")));
}

void TestApplicationLocale::environmentOverrideTakesPrecedence()
{
    const QLocale resolved = ApplicationLocale::resolve(
        QStringLiteral("fr_CA"), QByteArrayLiteral("de"));

    QCOMPARE(resolved.language(), QLocale::German);
}

void TestApplicationLocale::savedPreferenceOverridesSystemLocale()
{
    const QLocale resolved =
        ApplicationLocale::resolve(QStringLiteral("fr_CA"));

    QCOMPARE(resolved.language(), QLocale::French);
    QCOMPARE(resolved.territory(), QLocale::Canada);
}

QTEST_MAIN(TestApplicationLocale)
#include "test_applicationlocale.moc"
