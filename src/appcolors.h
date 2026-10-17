#ifndef APPCOLORS_H
#define APPCOLORS_H

#include <QColor>
#include <QList>
#include <QPalette>
#include <QString>

#include <optional>

namespace AppColors {

inline constexpr const char *SystemTheme = "system";
inline constexpr const char *LightTheme = "light";
inline constexpr const char *DarkTheme = "dark";

enum class Role {
    Download,
    Upload,
    Success,
    Warning,
    Error,
    Inactive,
    Verification,
    Queued,
    PieceComplete,
    PieceRemaining,
    PieceBorder
};

QString semanticName(Role role);
std::optional<Role> roleFromSemanticName(const QString &name);
QList<Role> allRoles();
QColor defaultColor(Role role, const QPalette &palette);

QString paletteRoleName(QPalette::ColorRole role);
std::optional<QPalette::ColorRole> paletteRoleFromName(const QString &name);

} // namespace AppColors

#endif // APPCOLORS_H
