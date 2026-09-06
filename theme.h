#ifndef THEME_H
#define THEME_H

#include "colortheme.h"
#include "icontheme.h"

#include <QString>

#include <optional>

namespace AppThemes {

class Theme
{
public:
    Theme() = default;
    Theme(QString id,
          QString displayName,
          std::optional<AppIcons::IconTheme> iconTheme,
          std::optional<AppColors::ColorTheme> colorTheme,
          bool builtIn = false);

    bool isValid() const;
    QString id() const;
    QString displayName() const;
    bool isBuiltIn() const;
    bool hasIconTheme() const;
    bool hasColorTheme() const;
    AppIcons::IconTheme iconTheme() const;
    AppColors::ColorTheme colorTheme() const;

private:
    QString m_id;
    QString m_displayName;
    std::optional<AppIcons::IconTheme> m_iconTheme;
    std::optional<AppColors::ColorTheme> m_colorTheme;
    bool m_builtIn = false;
};

} // namespace AppThemes

#endif // THEME_H
