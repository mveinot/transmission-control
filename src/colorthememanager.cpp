#include "colorthememanager.h"

#include "settingskeys.h"
#include "themeregistry.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>
#include <QtGlobal>

namespace AppColors {

ColorThemeManager &ColorThemeManager::instance()
{
    static ColorThemeManager manager;
    return manager;
}

ColorThemeManager::ColorThemeManager()
{
    auto &registry = AppThemes::ThemeRegistry::instance();
    m_themeId = registry.resolvedColorThemeId(
        QSettings().value(SettingsKeys::ColorTheme,
                          QString::fromLatin1(SystemTheme)).toString());
    // Keep stylesheet support implemented for future use, but do not activate
    // it from normal application paths until the widget treatment is ready.
    m_stylesheetEnabled = false;
    connect(&registry, &AppThemes::ThemeRegistry::registryChanged,
            this, [this](const QString &themeId) {
                if (themeId == m_themeId) {
                    m_themeId = AppThemes::ThemeRegistry::instance()
                                    .resolvedColorThemeId(m_themeId);
                    applyTheme();
                    emit themeChanged(m_themeId);
                }
            });

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this]() {
                const ColorTheme theme =
                    AppThemes::ThemeRegistry::instance().colorTheme(m_themeId);
                if (theme.mode() == Mode::System) {
                    applyTheme();
                    emit themeChanged(m_themeId);
                }
            });
#endif

    applyTheme();
}

QString ColorThemeManager::themeId() const
{
    return m_themeId;
}

bool ColorThemeManager::stylesheetEnabled() const
{
    return m_stylesheetEnabled;
}

QColor ColorThemeManager::color(Role role) const
{
    const ColorTheme theme =
        AppThemes::ThemeRegistry::instance().colorTheme(m_themeId);
    return theme.color(role, QApplication::palette());
}

void ColorThemeManager::setThemeId(const QString &themeId)
{
    const QString resolved =
        AppThemes::ThemeRegistry::instance().resolvedColorThemeId(themeId);
    if (resolved == m_themeId)
        return;

    m_themeId = resolved;
    applyTheme();
    emit themeChanged(m_themeId);
}

void ColorThemeManager::setStylesheetEnabled(bool enabled)
{
    if (enabled == m_stylesheetEnabled)
        return;

    m_stylesheetEnabled = enabled;
    applyTheme();
    emit themeChanged(m_themeId);
}

void ColorThemeManager::applyTheme()
{
    const AppThemes::Theme registeredTheme =
        AppThemes::ThemeRegistry::instance().theme(m_themeId);
    const ColorTheme theme =
        AppThemes::ThemeRegistry::instance().colorTheme(m_themeId);

    // Clear any previous theme's stylesheet and explicit palette before asking
    // Qt to resolve native colours for the new Light/Dark/System mode.
    qApp->setStyleSheet(QString());
    QApplication::setPalette(QPalette());

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QStyleHints *styleHints = QGuiApplication::styleHints();
    switch (theme.mode()) {
    case Mode::Light:
        styleHints->setColorScheme(Qt::ColorScheme::Light);
        break;
    case Mode::Dark:
        styleHints->setColorScheme(Qt::ColorScheme::Dark);
        break;
    case Mode::System:
        styleHints->unsetColorScheme();
        break;
    }
#endif

    if (theme.hasPaletteOverrides()) {
        // The unresolved roles in this partial palette continue to inherit
        // from Qt's native palette for the selected mode.
        QApplication::setPalette(theme.appliedTo(QPalette()));
    }

    if (!m_stylesheetEnabled || !registeredTheme.hasStyleSheet())
        return;

    QFile styleSheetFile(registeredTheme.styleSheetPath());
    if (!styleSheetFile.open(QIODevice::ReadOnly)) {
        qWarning().noquote() << "Could not open theme stylesheet:"
                             << styleSheetFile.fileName();
        return;
    }

    constexpr qint64 MaxStyleSheetBytes = 256 * 1024;
    const QByteArray contents = styleSheetFile.read(MaxStyleSheetBytes + 1);
    if (contents.size() > MaxStyleSheetBytes) {
        qWarning().noquote() << "Ignoring oversized theme stylesheet:"
                             << styleSheetFile.fileName();
        return;
    }
    qApp->setStyleSheet(QString::fromUtf8(contents));
}

} // namespace AppColors
