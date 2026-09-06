#ifndef COLORTHEME_H
#define COLORTHEME_H

#include "appcolors.h"

#include <QHash>
#include <QPalette>
#include <QString>

namespace AppColors {

enum class Mode {
    System,
    Light,
    Dark
};

class ColorTheme
{
public:
    using PaletteColors = QHash<QPalette::ColorRole, QColor>;
    using SemanticColors = QHash<Role, QColor>;

    ColorTheme() = default;
    ColorTheme(QString id,
               QString displayName,
               Mode mode,
               PaletteColors paletteColors = {},
               SemanticColors semanticColors = {},
               bool builtIn = false);

    bool isValid() const;
    QString id() const;
    QString displayName() const;
    Mode mode() const;
    bool isBuiltIn() const;
    bool hasPaletteOverrides() const;

    QPalette appliedTo(const QPalette &basePalette) const;
    QColor color(Role role, const QPalette &palette) const;

private:
    QString m_id;
    QString m_displayName;
    Mode m_mode = Mode::System;
    PaletteColors m_paletteColors;
    SemanticColors m_semanticColors;
    bool m_builtIn = false;
};

} // namespace AppColors

#endif // COLORTHEME_H
