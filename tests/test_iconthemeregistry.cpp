#include "appicons.h"
#include "thememanifest.h"
#include "themeregistry.h"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

namespace {

bool writeIcon(const QString &path, const QColor &color)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(color);
    return image.save(path);
}

bool writeManifest(const QString &themeDirectory,
                   const QJsonObject &manifest)
{
    QDir().mkpath(themeDirectory);
    QFile file(QDir(themeDirectory).filePath(QString::fromLatin1(
        AppThemes::ThemeManifestParser::FileName)));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(QJsonDocument(manifest).toJson()) >= 0;
}

QJsonObject manifest(const QString &id,
                     const QString &name,
                     const QJsonObject &icons)
{
    return {
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("fallback"), QStringLiteral("glass")},
        {QStringLiteral("icons"), icons}
    };
}

QColor iconColor(const QIcon &icon)
{
    return icon.pixmap(16, 16).toImage().pixelColor(8, 8);
}

} // namespace

class TestThemeRegistry : public QObject
{
    Q_OBJECT

private slots:
    void semanticNamesRoundTrip();
    void standardDirectoryUsesApplicationDataLocation();
    void exampleThemeManifestIsComplete();
    void exampleColorThemeManifestsLoad();
    void scansLoadsFallsBackAndRescans();
};

void TestThemeRegistry::semanticNamesRoundTrip()
{
    QSet<QString> names;
    for (AppIcons::Id iconId : AppIcons::allIds()) {
        const QString name = AppIcons::semanticName(iconId);
        QVERIFY(!name.isEmpty());
        QVERIFY(!names.contains(name));
        names.insert(name);
        const auto roundTrip = AppIcons::idFromSemanticName(name.toUpper());
        QVERIFY(roundTrip.has_value());
        QCOMPARE(*roundTrip, iconId);
    }
    QCOMPARE(names.size(), 27);
    QVERIFY(!AppIcons::idFromSemanticName(QStringLiteral("not-an-icon")));

    names.clear();
    for (AppColors::Role colorRole : AppColors::allRoles()) {
        const QString name = AppColors::semanticName(colorRole);
        QVERIFY(!name.isEmpty());
        QVERIFY(!names.contains(name));
        names.insert(name);
        const auto roundTrip = AppColors::roleFromSemanticName(name.toUpper());
        QVERIFY(roundTrip.has_value());
        QCOMPARE(*roundTrip, colorRole);
    }
    QCOMPARE(names.size(), 11);
    QVERIFY(!AppColors::roleFromSemanticName(
        QStringLiteral("not-a-color")));
}

void TestThemeRegistry::standardDirectoryUsesApplicationDataLocation()
{
    const QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString expected = appData.isEmpty()
                                 ? QString()
                                 : QDir(appData).filePath(
                                       QStringLiteral("icon-themes"));
    QCOMPARE(AppThemes::ThemeRegistry::standardThemeDirectory(), expected);
}

void TestThemeRegistry::exampleThemeManifestIsComplete()
{
    const QString manifestPath = QFINDTESTDATA(
        "../extras/icon-themes/polar-night/theme.json");
    QVERIFY(!manifestPath.isEmpty());

    const AppThemes::ThemeManifestResult result =
        AppThemes::ThemeManifestParser::parseFile(manifestPath);
    QVERIFY2(result.succeeded(), qPrintable(result.error));
    QCOMPARE(result.theme.id(), QStringLiteral("polar-night"));
    QVERIFY(result.theme.hasIconTheme());
    QVERIFY(result.theme.hasColorTheme());
    QCOMPARE(result.theme.colorTheme().mode(), AppColors::Mode::Dark);
    QCOMPARE(result.theme.colorTheme().color(
                 AppColors::Role::Download, QPalette()),
             QColor(QStringLiteral("#20d6f2")));
    const AppIcons::IconTheme iconTheme = result.theme.iconTheme();
    for (AppIcons::Id iconId : AppIcons::allIds()) {
        QVERIFY2(iconTheme.hasIcon(iconId),
                 qPrintable(AppIcons::semanticName(iconId)));
        QVERIFY2(QFileInfo::exists(iconTheme.iconPath(iconId)),
                 qPrintable(iconTheme.iconPath(iconId)));
        QVERIFY(!QIcon(iconTheme.iconPath(iconId)).isNull());
    }
}

void TestThemeRegistry::exampleColorThemeManifestsLoad()
{
    const QString themesPath = QFINDTESTDATA("../extras/icon-themes");
    QVERIFY(!themesPath.isEmpty());

    const QStringList expectedIds {
        QStringLiteral("catppuccin-mocha"),
        QStringLiteral("dracula"),
        QStringLiteral("gruvbox-dark"),
        QStringLiteral("gruvbox-light"),
        QStringLiteral("nord"),
        QStringLiteral("one-dark"),
        QStringLiteral("rose-pine"),
        QStringLiteral("solarized-dark"),
        QStringLiteral("solarized-light"),
        QStringLiteral("tokyo-night-storm")
    };

    for (const QString &themeId : expectedIds) {
        const QString manifestPath =
            QDir(themesPath).filePath(themeId + QStringLiteral("/theme.json"));
        const AppThemes::ThemeManifestResult result =
            AppThemes::ThemeManifestParser::parseFile(manifestPath);
        QVERIFY2(result.succeeded(),
                 qPrintable(themeId + QStringLiteral(": ") + result.error));
        QCOMPARE(result.theme.id(), themeId);
        QVERIFY(!result.theme.hasIconTheme());
        QVERIFY(result.theme.hasColorTheme());
    }
}

void TestThemeRegistry::scansLoadsFallsBackAndRescans()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString themeDirectory =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("ocean"));
    QVERIFY(writeIcon(QDir(themeDirectory).filePath(QStringLiteral("icons/start.png")),
                      Qt::red));
    QVERIFY(writeManifest(
        themeDirectory,
        manifest(QStringLiteral("ocean"),
                 QStringLiteral("Ocean"),
                 {{QStringLiteral("action-start"),
                   QStringLiteral("icons/start.png")}})));

    const QString unsafeDirectory =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("unsafe"));
    QVERIFY(writeManifest(
        unsafeDirectory,
        manifest(QStringLiteral("unsafe"),
                 QStringLiteral("Unsafe"),
                 {{QStringLiteral("action-start"),
                   QStringLiteral("../outside.png")}})));

    const QString colorsDirectory =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("dusk"));
    QVERIFY(writeManifest(
        colorsDirectory,
        {{QStringLiteral("formatVersion"), 1},
         {QStringLiteral("id"), QStringLiteral("dusk")},
         {QStringLiteral("name"), QStringLiteral("Dusk")},
         {QStringLiteral("colors"),
          QJsonObject {
              {QStringLiteral("mode"), QStringLiteral("dark")},
              {QStringLiteral("palette"),
               QJsonObject {
                   {QStringLiteral("window"), QStringLiteral("#112233")},
                   {QStringLiteral("text"), QStringLiteral("#f4f5f6")}
               }},
              {QStringLiteral("semantic"),
               QJsonObject {
                   {QStringLiteral("download"), QStringLiteral("#22ccdd")}
               }}
          }}}));

    QVERIFY(writeManifest(
        temporaryDirectory.path(),
        {{QStringLiteral("formatVersion"), 1},
         {QStringLiteral("id"), QStringLiteral("standalone")},
         {QStringLiteral("name"), QStringLiteral("Standalone")},
         {QStringLiteral("colors"), QJsonObject {}}}));

    AppThemes::ThemeRegistry registry(temporaryDirectory.path());
    QVERIFY(registry.contains(QStringLiteral("glass")));
    QVERIFY(registry.contains(QStringLiteral("classic")));
    QVERIFY(registry.contains(QStringLiteral("ocean")));
    QVERIFY(registry.contains(QStringLiteral("dusk")));
    QVERIFY(registry.contains(QStringLiteral("standalone")));
    QVERIFY(!registry.contains(QStringLiteral("unsafe")));
    QCOMPARE(registry.iconTheme(QStringLiteral("ocean")).displayName(),
             QStringLiteral("Ocean"));
    QVERIFY(registry.theme(QStringLiteral("ocean")).hasIconTheme());
    QVERIFY(!registry.theme(QStringLiteral("ocean")).hasColorTheme());
    QVERIFY(!registry.theme(QStringLiteral("dusk")).hasIconTheme());
    QVERIFY(registry.theme(QStringLiteral("dusk")).hasColorTheme());
    QVERIFY(registry.theme(QStringLiteral("standalone")).hasColorTheme());
    QCOMPARE(registry.resolvedIconThemeId(QStringLiteral("dusk")),
             QStringLiteral("glass"));
    QCOMPARE(registry.resolvedColorThemeId(QStringLiteral("ocean")),
             QStringLiteral("system"));

    const AppColors::ColorTheme dusk = registry.colorTheme(QStringLiteral("dusk"));
    QCOMPARE(dusk.mode(), AppColors::Mode::Dark);
    const QPalette duskPalette = dusk.appliedTo(QPalette());
    QCOMPARE(duskPalette.color(QPalette::Window), QColor(QStringLiteral("#112233")));
    QCOMPARE(duskPalette.color(QPalette::Text), QColor(QStringLiteral("#f4f5f6")));
    QCOMPARE(dusk.color(AppColors::Role::Download, duskPalette),
             QColor(QStringLiteral("#22ccdd")));
    QCOMPARE(iconColor(registry.icon(QStringLiteral("ocean"),
                                     AppIcons::Id::ActionStart)),
             QColor(Qt::red));

    // The external theme omits ActionStop, so the built-in Glass icon wins.
    QCOMPARE(registry.icon(QStringLiteral("ocean"), AppIcons::Id::ActionStop)
                 .pixmap(64, 64).toImage(),
             registry.icon(QStringLiteral("glass"), AppIcons::Id::ActionStop)
                 .pixmap(64, 64).toImage());

    QVERIFY(writeIcon(QDir(themeDirectory).filePath(QStringLiteral("icons/start-blue.png")),
                      Qt::blue));
    QVERIFY(writeManifest(
        themeDirectory,
        manifest(QStringLiteral("ocean"),
                 QStringLiteral("Ocean"),
                 {{QStringLiteral("action-start"),
                   QStringLiteral("icons/start-blue.png")}})));
    QSignalSpy registryChangedSpy(
        &registry, &AppThemes::ThemeRegistry::registryChanged);
    registry.rescanExternalThemes();
    QVERIFY(registryChangedSpy.count() >= 1);
    QCOMPARE(iconColor(registry.icon(QStringLiteral("ocean"),
                                     AppIcons::Id::ActionStart)),
             QColor(Qt::blue));

    QVERIFY(QDir(themeDirectory).removeRecursively());
    registry.rescanExternalThemes();
    QVERIFY(!registry.contains(QStringLiteral("ocean")));
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("PlanetaryTests"));
    QCoreApplication::setApplicationName(QStringLiteral("ThemeRegistry"));
    TestThemeRegistry test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_iconthemeregistry.moc"
