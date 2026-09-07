#include "colorthememanager.h"
#include "theme.h"
#include "themeregistry.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QDir>
#include <QPalette>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TestColorThemeManager : public QObject
{
    Q_OBJECT

private slots:
    void selectsBuiltInColorModes();
    void appliesPaletteAndSemanticColorsImmediately();
    void appliesThemeStylesheetImmediately();
};

void TestColorThemeManager::selectsBuiltInColorModes()
{
    auto &manager = AppColors::ColorThemeManager::instance();

    manager.setThemeId(QStringLiteral("light"));
    QCOMPARE(manager.themeId(), QStringLiteral("light"));

    manager.setThemeId(QStringLiteral("dark"));
    QCOMPARE(manager.themeId(), QStringLiteral("dark"));

    manager.setThemeId(QStringLiteral("system"));
    QCOMPARE(manager.themeId(), QStringLiteral("system"));
}

void TestColorThemeManager::appliesPaletteAndSemanticColorsImmediately()
{
    auto &registry = AppThemes::ThemeRegistry::instance();
    auto &manager = AppColors::ColorThemeManager::instance();
    const QString originalThemeId = manager.themeId();

    const QString themeId = QStringLiteral("test-runtime-colors");
    const auto makeTheme = [&](const QColor &window,
                               const QColor &download) {
        AppColors::ColorTheme::PaletteColors palette {
            {QPalette::Window, window}
        };
        AppColors::ColorTheme::SemanticColors semantic {
            {AppColors::Role::Download, download}
        };
        const AppColors::ColorTheme colors(
            themeId, QStringLiteral("Test Runtime Colors"),
            AppColors::Mode::System, palette, semantic);
        return AppThemes::Theme(
            themeId, QStringLiteral("Test Runtime Colors"),
            std::nullopt, colors);
    };

    QVERIFY(registry.registerTheme(
        makeTheme(QColor(QStringLiteral("#123456")),
                  QColor(QStringLiteral("#abcdef")))));

    QSignalSpy changedSpy(&manager,
                          &AppColors::ColorThemeManager::themeChanged);
    manager.setThemeId(themeId);
    QCOMPARE(manager.themeId(), themeId);
    QCOMPARE(QApplication::palette().color(QPalette::Window),
             QColor(QStringLiteral("#123456")));
    QCOMPARE(manager.color(AppColors::Role::Download),
             QColor(QStringLiteral("#abcdef")));
    QCOMPARE(changedSpy.count(), 1);

    // Replacing an active external package applies its new values immediately.
    QVERIFY(registry.registerTheme(
        makeTheme(QColor(QStringLiteral("#654321")),
                  QColor(QStringLiteral("#fedcba")))));
    QCOMPARE(QApplication::palette().color(QPalette::Window),
             QColor(QStringLiteral("#654321")));
    QCOMPARE(manager.color(AppColors::Role::Download),
             QColor(QStringLiteral("#fedcba")));
    QCOMPARE(changedSpy.count(), 2);

    QVERIFY(registry.unregisterTheme(themeId));
    QCOMPARE(manager.themeId(), QStringLiteral("system"));
    if (originalThemeId != QStringLiteral("system"))
        manager.setThemeId(originalThemeId);
}

void TestColorThemeManager::appliesThemeStylesheetImmediately()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString stylePath = QDir(directory.path()).filePath(
        QStringLiteral("planetary.qss"));
    QFile styleFile(stylePath);
    QVERIFY(styleFile.open(QIODevice::WriteOnly));
    const QByteArray styleSheet =
        "QPushButton { border-radius: 7px; }\n";
    QCOMPARE(styleFile.write(styleSheet), styleSheet.size());
    styleFile.close();

    auto &registry = AppThemes::ThemeRegistry::instance();
    auto &manager = AppColors::ColorThemeManager::instance();
    const QString originalThemeId = manager.themeId();
    const QString themeId = QStringLiteral("test-runtime-style");
    AppColors::ColorTheme::PaletteColors palette {
        {QPalette::Window, QColor(QStringLiteral("#223344"))}
    };
    const AppColors::ColorTheme colors(
        themeId, QStringLiteral("Test Runtime Style"), AppColors::Mode::System,
        palette);
    QVERIFY(registry.registerTheme(AppThemes::Theme(
        themeId, QStringLiteral("Test Runtime Style"), std::nullopt, colors,
        false, stylePath)));

    // Stylesheets are opt-in at the manager API level; normal application
    // paths keep this disabled while the widget treatment is being revised.
    manager.setStylesheetEnabled(true);
    manager.setThemeId(themeId);
    QCOMPARE(qApp->styleSheet(), QString::fromUtf8(styleSheet));

    manager.setStylesheetEnabled(false);
    QCOMPARE(qApp->styleSheet(), QString());
    manager.setStylesheetEnabled(true);
    QCOMPARE(qApp->styleSheet(), QString::fromUtf8(styleSheet));

    QVERIFY(registry.unregisterTheme(themeId));
    QCOMPARE(qApp->styleSheet(), QString());
    if (originalThemeId != QStringLiteral("system"))
        manager.setThemeId(originalThemeId);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("PlanetaryTests"));
    QCoreApplication::setApplicationName(QStringLiteral("ColorThemeManager"));
    TestColorThemeManager test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_colorthememanager.moc"
