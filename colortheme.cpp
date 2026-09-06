#include "colortheme.h"

#include <utility>

namespace AppColors {

ColorTheme::ColorTheme(QString id,
                       QString displayName,
                       Mode mode,
                       PaletteColors paletteColors,
                       SemanticColors semanticColors,
                       bool builtIn)
    : m_id(id.trimmed().toLower())
    , m_displayName(std::move(displayName))
    , m_mode(mode)
    , m_paletteColors(std::move(paletteColors))
    , m_semanticColors(std::move(semanticColors))
    , m_builtIn(builtIn)
{
}

bool ColorTheme::isValid() const
{
    return !m_id.isEmpty() && !m_displayName.isEmpty();
}

QString ColorTheme::id() const
{
    return m_id;
}

QString ColorTheme::displayName() const
{
    return m_displayName;
}

Mode ColorTheme::mode() const
{
    return m_mode;
}

bool ColorTheme::isBuiltIn() const
{
    return m_builtIn;
}

bool ColorTheme::hasPaletteOverrides() const
{
    return !m_paletteColors.isEmpty();
}

QPalette ColorTheme::appliedTo(const QPalette &basePalette) const
{
    QPalette result = basePalette;
    for (auto it = m_paletteColors.cbegin(); it != m_paletteColors.cend(); ++it)
        result.setColor(it.key(), it.value());
    return result;
}

QColor ColorTheme::color(Role role, const QPalette &palette) const
{
    return m_semanticColors.value(role, defaultColor(role, palette));
}

} // namespace AppColors
