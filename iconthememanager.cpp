#include "iconthememanager.h"

#include "iconthemeregistry.h"
#include "settingskeys.h"

#include <QAction>
#include <QSettings>

namespace AppIcons {

IconThemeManager &IconThemeManager::instance()
{
    static IconThemeManager manager;
    return manager;
}

IconThemeManager::IconThemeManager()
{
    auto &registry = IconThemeRegistry::instance();
    m_themeId = registry.resolvedThemeId(
        QSettings().value(SettingsKeys::IconTheme,
                          QString::fromLatin1(GlassTheme)).toString());
    connect(&registry, &IconThemeRegistry::registryChanged,
            this, [this](const QString &themeId) {
                if (themeId == m_themeId)
                    refreshTheme();
            });
}

QString IconThemeManager::themeId() const
{
    return m_themeId;
}

QIcon IconThemeManager::icon(Id iconId) const
{
    return icon(iconId, m_themeId);
}

QIcon IconThemeManager::icon(Id iconId, const QString &themeId) const
{
    return IconThemeRegistry::instance().icon(themeId, iconId);
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
    const QString resolved = IconThemeRegistry::instance().resolvedThemeId(themeId);
    if (resolved == m_themeId)
        return;

    m_themeId = resolved;
    refreshBoundActions();
    emit themeChanged(m_themeId);
}

void IconThemeManager::refreshTheme()
{
    m_themeId = IconThemeRegistry::instance().resolvedThemeId(m_themeId);
    refreshBoundActions();
    emit themeChanged(m_themeId);
}

void IconThemeManager::refreshBoundActions()
{
    for (auto it = m_boundActions.cbegin(); it != m_boundActions.cend(); ++it)
        it.key()->setIcon(icon(it.value()));
}

} // namespace AppIcons
