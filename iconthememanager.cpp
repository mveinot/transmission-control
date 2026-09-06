#include "iconthememanager.h"

#include "colorthememanager.h"
#include "settingskeys.h"
#include "themeregistry.h"

#include <QAction>
#include <QApplication>
#include <QImage>
#include <QPixmap>
#include <QSettings>
#include <QToolButton>

namespace {

constexpr int HoverPixmapSize = 64;
constexpr double HoverAccentMix = 0.20;
constexpr int HoverLightnessPercent = 125;

QPixmap hoverPixmap(const QPixmap &source)
{
    if (source.isNull())
        return {};

    QImage image = source.toImage().convertToFormat(
        QImage::Format_ARGB32_Premultiplied);
    const QColor accent = QApplication::palette().color(QPalette::Highlight);

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QColor pixel = image.pixelColor(x, y);
            const int alpha = pixel.alpha();
            if (alpha == 0)
                continue;

            pixel.setRed(qRound(pixel.red() * (1.0 - HoverAccentMix)
                                + accent.red() * HoverAccentMix));
            pixel.setGreen(qRound(pixel.green() * (1.0 - HoverAccentMix)
                                  + accent.green() * HoverAccentMix));
            pixel.setBlue(qRound(pixel.blue() * (1.0 - HoverAccentMix)
                                 + accent.blue() * HoverAccentMix));
            pixel = pixel.lighter(HoverLightnessPercent);
            pixel.setAlpha(alpha);
            image.setPixelColor(x, y, pixel);
        }
    }

    QPixmap result = QPixmap::fromImage(image);
    result.setDevicePixelRatio(source.devicePixelRatio());
    return result;
}

QIcon withHoverVariant(const QIcon &source)
{
    const QPixmap normal = source.pixmap(
        QSize(HoverPixmapSize, HoverPixmapSize),
        QIcon::Normal,
        QIcon::Off);
    const QPixmap hover = hoverPixmap(normal);
    if (hover.isNull())
        return source;

    QIcon result(source);
    result.addPixmap(hover, QIcon::Active, QIcon::Off);
    return result;
}

QIcon hoverOnlyIcon(const QIcon &source)
{
    const QPixmap active = source.pixmap(
        QSize(HoverPixmapSize, HoverPixmapSize),
        QIcon::Active,
        QIcon::Off);
    if (active.isNull())
        return source;

    QIcon result;
    result.addPixmap(active, QIcon::Normal, QIcon::Off);
    result.addPixmap(active, QIcon::Active, QIcon::Off);
    return result;
}

} // namespace

namespace AppIcons {

IconThemeManager &IconThemeManager::instance()
{
    static IconThemeManager manager;
    return manager;
}

IconThemeManager::IconThemeManager()
{
    qApp->installEventFilter(this);
    auto &registry = AppThemes::ThemeRegistry::instance();
    m_themeId = registry.resolvedIconThemeId(
        QSettings().value(SettingsKeys::IconTheme,
                          QString::fromLatin1(GlassTheme)).toString());
    connect(&registry, &AppThemes::ThemeRegistry::registryChanged,
            this, [this](const QString &themeId) {
                clearIconCache();
                if (themeId == m_themeId)
                    refreshTheme();
            });
    connect(&AppColors::ColorThemeManager::instance(),
            &AppColors::ColorThemeManager::themeChanged,
            this,
            [this]() {
                clearIconCache();
                refreshBoundActions();
                emit themeChanged(m_themeId);
            });
}

bool IconThemeManager::eventFilter(QObject *watched, QEvent *event)
{
    auto *button = qobject_cast<QToolButton *>(watched);
    if (!button || !event)
        return QObject::eventFilter(watched, event);

    QAction *action = button->defaultAction();
    if (!action || !m_boundActions.contains(action))
        return QObject::eventFilter(watched, event);

    const Id iconId = m_boundActions.value(action);
    if (event->type() == QEvent::Enter && button->isEnabled()) {
        button->setIcon(hoverOnlyIcon(icon(iconId)));
    } else if (event->type() == QEvent::Leave) {
        button->setIcon(icon(iconId));
    }

    return QObject::eventFilter(watched, event);
}

QString IconThemeManager::themeId() const
{
    return m_themeId;
}

QIcon IconThemeManager::icon(Id iconId) const
{
    return icon(iconId, m_themeId);
}

QIcon IconThemeManager::hoverIcon(Id iconId) const
{
    return hoverOnlyIcon(icon(iconId));
}

QIcon IconThemeManager::icon(Id iconId, const QString &themeId) const
{
    const QString cacheKey = themeId.trimmed().toLower()
                             + QLatin1Char(':')
                             + QString::number(static_cast<int>(iconId));
    const auto cached = m_iconCache.constFind(cacheKey);
    if (cached != m_iconCache.cend())
        return cached.value();

    const QIcon result = withHoverVariant(
        AppThemes::ThemeRegistry::instance().icon(themeId, iconId));
    m_iconCache.insert(cacheKey, result);
    return result;
}

void IconThemeManager::bindAction(QAction *action, Id iconId)
{
    if (!action)
        return;

    const bool alreadyBound = m_boundActions.contains(action);
    m_boundActions.insert(action, iconId);
    action->setProperty("planetaryIconId", static_cast<int>(iconId));
    action->setIcon(icon(iconId));

    if (!alreadyBound) {
        connect(action, &QObject::destroyed, this, [this, action]() {
            m_boundActions.remove(action);
        });
    }
}

void IconThemeManager::setThemeId(const QString &themeId)
{
    const QString resolved =
        AppThemes::ThemeRegistry::instance().resolvedIconThemeId(themeId);
    if (resolved == m_themeId)
        return;

    m_themeId = resolved;
    clearIconCache();
    refreshBoundActions();
    emit themeChanged(m_themeId);
}

void IconThemeManager::refreshTheme()
{
    m_themeId = AppThemes::ThemeRegistry::instance()
                    .resolvedIconThemeId(m_themeId);
    clearIconCache();
    refreshBoundActions();
    emit themeChanged(m_themeId);
}

void IconThemeManager::clearIconCache()
{
    m_iconCache.clear();
}

void IconThemeManager::refreshBoundActions()
{
    for (auto it = m_boundActions.cbegin(); it != m_boundActions.cend(); ++it)
        it.key()->setIcon(icon(it.value()));
}

} // namespace AppIcons
