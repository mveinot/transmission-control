#include "appicons.h"
#include "iconthememanager.h"
#include "themearchive.h"
#include "thememanifest.h"
#include "themeregistry.h"

#include <QApplication>
#include <QAction>
#include <QBuffer>
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
#include <QToolBar>
#include <QToolButton>
#include <QtTest>

#ifdef PLANETARY_HAVE_MINIZ
#include <miniz/miniz.h>
#endif

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
    void stylesheetManifestLoads();
    void scansLoadsFallsBackAndRescans();
    void optionalPressedIcons();
    void toolbarPressedIcons();
#ifdef PLANETARY_HAVE_MINIZ
    void archiveIndexesAndExtractsLazily();
    void importsAndDeletesThemePacks();
#endif
};

void TestThemeRegistry::optionalPressedIcons()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(writeIcon(directory.filePath("normal.png"), Qt::red));
    QVERIFY(writeIcon(directory.filePath("pressed.png"), Qt::blue));
    QJsonObject icons {
        {"action-start", "normal.png"},
        {"action-stop", "normal.png"},
        {"action-start-all", "normal.png"}
    };
    auto root = manifest("pressed-test", "Pressed test", icons);
    root.insert("pressedIcons", QJsonObject {{"action-start", "pressed.png"},
                                            {"action-start-all", "missing.png"}});
    root.insert("futureExtension", QJsonObject {{"enabled", true}});
    QVERIFY(writeManifest(directory.path(), root));
    const auto parsed = AppThemes::ThemeManifestParser::parseFile(
        directory.filePath("theme.json"));
    QVERIFY2(parsed.error.isEmpty(), qPrintable(parsed.error));
    AppThemes::ThemeRegistry registry(directory.path());
    QVERIFY(registry.registerTheme(parsed.theme));
    QCOMPARE(iconColor(registry.icon("pressed-test", AppIcons::Id::ActionStart)), QColor(Qt::red));
    QCOMPARE(iconColor(registry.icon("pressed-test", AppIcons::Id::ActionStart, true)), QColor(Qt::blue));
    QCOMPARE(iconColor(registry.icon("pressed-test", AppIcons::Id::ActionStop, true)), QColor(Qt::red));
    QCOMPARE(iconColor(registry.icon("pressed-test", AppIcons::Id::ActionStartAll, true)), QColor(Qt::red));

    // Removing optional sections leaves the same normal artwork for old builds.
    QVERIFY(writeManifest(directory.path(), manifest("pressed-test", "Pressed test", icons)));
    const auto legacy = AppThemes::ThemeManifestParser::parseFile(directory.filePath("theme.json"));
    QVERIFY2(legacy.error.isEmpty(), qPrintable(legacy.error));
    QVERIFY(registry.registerTheme(legacy.theme));
    QCOMPARE(iconColor(registry.icon("pressed-test", AppIcons::Id::ActionStart, true)), QColor(Qt::red));

    root.insert("pressedIcons", QJsonObject {{"action-start", "../escape.png"}});
    QVERIFY(writeManifest(directory.path(), root));
    QVERIFY(!AppThemes::ThemeManifestParser::parseFile(directory.filePath("theme.json")).error.isEmpty());
}

void TestThemeRegistry::toolbarPressedIcons()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(writeIcon(directory.filePath("normal.png"), Qt::red));
    QVERIFY(writeIcon(directory.filePath("pressed.png"), Qt::blue));
    auto &registry = AppThemes::ThemeRegistry::instance();
    QVERIFY(registry.registerTheme(AppThemes::Theme(
        "toolbar-pressed-test", "Toolbar pressed test",
        AppIcons::IconTheme("toolbar-pressed-test", "Toolbar pressed test",
                            directory.path(), {{AppIcons::Id::ActionStart, "normal.png"}},
                            "glass", false, {{AppIcons::Id::ActionStart, "pressed.png"}}),
        std::nullopt)));
    auto &manager = AppIcons::IconThemeManager::instance();
    const QString previous = manager.themeId();
    manager.setThemeId("toolbar-pressed-test");
    QToolBar toolbar;
    auto *action = toolbar.addAction("Start");
    manager.bindAction(action, AppIcons::Id::ActionStart);
    auto *button = qobject_cast<QToolButton *>(toolbar.widgetForAction(action));
    QVERIFY(button);
    toolbar.resize(toolbar.sizeHint());
    QPixmap rendered(toolbar.size());
    button->setDown(true);
    toolbar.render(&rendered);
    QCOMPARE(iconColor(button->icon()), QColor(Qt::blue));
    QCOMPARE(iconColor(action->icon()), QColor(Qt::red));
    button->setDown(false);
    toolbar.render(&rendered);
    QCOMPARE(iconColor(button->icon()), QColor(Qt::red));
    button->setDown(true);
    manager.setThemeId("glass");
    toolbar.render(&rendered);
    QCOMPARE(button->icon().pixmap(16, 16).toImage(),
             registry.icon("glass", AppIcons::Id::ActionStart).pixmap(16, 16).toImage());
    manager.setThemeId(previous);
    registry.unregisterTheme("toolbar-pressed-test");
}

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
    const QString archivePath = QFINDTESTDATA(
        "../web/themes/packages/polar-night.planetarytheme");
    QVERIFY(!archivePath.isEmpty());

    const AppThemes::ThemeArchiveManifest archive =
        AppThemes::ThemeArchive::readManifest(archivePath);
    QVERIFY2(archive.succeeded(), qPrintable(archive.error));

    QTemporaryDir extractionDirectory;
    QVERIFY(extractionDirectory.isValid());
    QString extractionError;
    QVERIFY2(AppThemes::ThemeArchive::extract(
                  archivePath, archive.entryPrefix,
                  extractionDirectory.path(), &extractionError),
             qPrintable(extractionError));

    const QString manifestPath = QDir(extractionDirectory.path()).filePath(
        QString::fromLatin1(AppThemes::ThemeManifestParser::FileName));

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
    const QString themesPath = QFINDTESTDATA("../web/themes/packages");
    QVERIFY(!themesPath.isEmpty());

    const QStringList expectedIds {
        QStringLiteral("catppuccin-mocha"),
        QStringLiteral("dracula"),
        QStringLiteral("elflord"),
        QStringLiteral("gruvbox-dark"),
        QStringLiteral("gruvbox-light"),
        QStringLiteral("nord"),
        QStringLiteral("one-dark"),
        QStringLiteral("rose-pine"),
        QStringLiteral("solarized-dark"),
        QStringLiteral("solarized-light"),
        QStringLiteral("maritime-radar"),
        QStringLiteral("obsidian-neon"),
        QStringLiteral("paper-console"),
        QStringLiteral("terminal-amber"),
        QStringLiteral("terminal-cga"),
        QStringLiteral("terminal-green"),
        QStringLiteral("tokyo-night-storm"),
        QStringLiteral("verdant-crt")
    };

    for (const QString &themeId : expectedIds) {
        const QString archivePath = QDir(themesPath).filePath(
            themeId + QStringLiteral(".planetarytheme"));
        const AppThemes::ThemeArchiveManifest archive =
            AppThemes::ThemeArchive::readManifest(archivePath);
        QVERIFY2(archive.succeeded(), qPrintable(themeId + QStringLiteral(": ")
                                              + archive.error));
        const AppThemes::ThemeManifestResult result =
            AppThemes::ThemeManifestParser::parseData(
                archive.data, themesPath, false);
        QVERIFY2(result.succeeded(),
                 qPrintable(themeId + QStringLiteral(": ") + result.error));
        QCOMPARE(result.theme.id(), themeId);
        QVERIFY(!result.theme.hasIconTheme());
        QVERIFY(result.theme.hasColorTheme());
    }
}

#ifdef PLANETARY_HAVE_MINIZ
void TestThemeRegistry::archiveIndexesAndExtractsLazily()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString archivePath = QDir(temporaryDirectory.path())
                                    .filePath(QStringLiteral("packed.planetarytheme"));
    const QByteArray nativePath = QFile::encodeName(archivePath);
    mz_zip_archive archive {};
    QVERIFY(mz_zip_writer_init_file(&archive, nativePath.constData(), 0));

    const QByteArray manifestData = QJsonDocument(manifest(
        QStringLiteral("packed"), QStringLiteral("Packed"),
        {{QStringLiteral("action-start"), QStringLiteral("icons/start.png")}}))
                                        .toJson();
    QVERIFY(mz_zip_writer_add_mem(&archive, "packed/theme.json",
                                  manifestData.constData(), manifestData.size(),
                                  MZ_BEST_COMPRESSION));

    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::magenta);
    QByteArray imageData;
    QBuffer imageBuffer(&imageData);
    QVERIFY(imageBuffer.open(QIODevice::WriteOnly));
    QVERIFY(image.save(&imageBuffer, "PNG"));
    QVERIFY(mz_zip_writer_add_mem(&archive, "packed/icons/start.png",
                                  imageData.constData(), imageData.size(),
                                  MZ_BEST_COMPRESSION));
    QVERIFY(mz_zip_writer_finalize_archive(&archive));
    QVERIFY(mz_zip_writer_end(&archive));

    const QString cacheDirectory = QDir(temporaryDirectory.path())
                                       .filePath(QStringLiteral("cache"));
    AppThemes::ThemeRegistry registry(temporaryDirectory.path(), nullptr,
                                      cacheDirectory);
    QVERIFY(registry.contains(QStringLiteral("packed")));
    QCOMPARE(registry.iconThemes().constLast().displayName(),
             QStringLiteral("Packed"));
    QVERIFY(!QDir(cacheDirectory).exists());

    QCOMPARE(iconColor(registry.icon(QStringLiteral("packed"),
                                     AppIcons::Id::ActionStart)),
             QColor(Qt::magenta));
    QVERIFY(QDir(cacheDirectory).exists());
    QVERIFY(QFileInfo::exists(
        registry.iconTheme(QStringLiteral("packed"))
            .iconPath(AppIcons::Id::ActionStart)));
}

void TestThemeRegistry::importsAndDeletesThemePacks()
{
    const QString sourcePath = QFINDTESTDATA(
        "../web/themes/packages/terminal-green.planetarytheme");
    QVERIFY(!sourcePath.isEmpty());

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourceCopy = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("source.planetarytheme"));
    QVERIFY(QFile::copy(sourcePath, sourceCopy));

    const QString themeDirectory = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("installed"));
    AppThemes::ThemeRegistry registry(themeDirectory, nullptr,
                                      QDir(temporaryDirectory.path()).filePath(
                                          QStringLiteral("cache")));
    QString error;
    QVERIFY2(registry.importThemePack(sourceCopy, &error), qPrintable(error));
    QVERIFY(registry.contains(QStringLiteral("terminal-green")));
    const QString installedPath = registry.externalThemePath(
        QStringLiteral("terminal-green"));
    QVERIFY(QFileInfo::exists(installedPath));
    QVERIFY2(registry.removeExternalTheme(QStringLiteral("terminal-green"),
                                          &error),
             qPrintable(error));
    QVERIFY(!registry.contains(QStringLiteral("terminal-green")));
    QVERIFY(!QFileInfo::exists(installedPath));
}
#endif

void TestThemeRegistry::stylesheetManifestLoads()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString themeDirectory = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("styled"));
    QFile stylesheet(QDir(themeDirectory).filePath(QStringLiteral("theme.qss")));
    QVERIFY(QDir().mkpath(themeDirectory));
    QVERIFY(stylesheet.open(QIODevice::WriteOnly));
    QVERIFY(stylesheet.write("QPushButton { color: #ffffff; }\n") > 0);
    stylesheet.close();

    const QJsonObject colors {
        {QStringLiteral("mode"), QStringLiteral("dark")},
        {QStringLiteral("palette"), QJsonObject {
            {QStringLiteral("window"), QStringLiteral("#101010")}
        }}
    };
    const QJsonObject styledManifest {
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("id"), QStringLiteral("styled")},
        {QStringLiteral("name"), QStringLiteral("Styled")},
        {QStringLiteral("colors"), colors},
        {QStringLiteral("style"), QJsonObject {
            {QStringLiteral("stylesheet"), QStringLiteral("theme.qss")}
        }}
    };
    QVERIFY(writeManifest(themeDirectory, styledManifest));

    const auto result = AppThemes::ThemeManifestParser::parseFile(
        QDir(themeDirectory).filePath(QString::fromLatin1(
            AppThemes::ThemeManifestParser::FileName)));
    QVERIFY2(result.succeeded(), qPrintable(result.error));
    QVERIFY(result.theme.hasColorTheme());
    QVERIFY(result.theme.hasStyleSheet());
    QCOMPARE(result.theme.styleSheetPath(), stylesheet.fileName());
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
