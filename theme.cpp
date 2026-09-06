#include "theme.h"

#include <utility>

namespace AppThemes {

Theme::Theme(QString id,
             QString displayName,
             std::optional<AppIcons::IconTheme> iconTheme,
             std::optional<AppColors::ColorTheme> colorTheme,
             bool builtIn)
    : m_id(id.trimmed().toLower())
    , m_displayName(std::move(displayName))
    , m_iconTheme(std::move(iconTheme))
    , m_colorTheme(std::move(colorTheme))
    , m_builtIn(builtIn)
{
}

bool Theme::isValid() const
{
    return !m_id.isEmpty() && !m_displayName.isEmpty()
           && ((m_iconTheme && m_iconTheme->isValid())
               || (m_colorTheme && m_colorTheme->isValid()));
}

QString Theme::id() const
{
    return m_id;
}

QString Theme::displayName() const
{
    return m_displayName;
}

bool Theme::isBuiltIn() const
{
    return m_builtIn;
}

bool Theme::hasIconTheme() const
{
    return m_iconTheme && m_iconTheme->isValid();
}

bool Theme::hasColorTheme() const
{
    return m_colorTheme && m_colorTheme->isValid();
}

AppIcons::IconTheme Theme::iconTheme() const
{
    return m_iconTheme.value_or(AppIcons::IconTheme());
}

AppColors::ColorTheme Theme::colorTheme() const
{
    return m_colorTheme.value_or(AppColors::ColorTheme());
}

} // namespace AppThemes
