#include "appcolors.h"

#include <iterator>

namespace AppColors {
namespace {

struct SemanticColor
{
    Role role;
    const char *name;
};

constexpr SemanticColor SemanticColors[] = {
    {Role::Download, "download"},
    {Role::Upload, "upload"},
    {Role::Success, "success"},
    {Role::Warning, "warning"},
    {Role::Error, "error"},
    {Role::Inactive, "inactive"},
    {Role::Verification, "verification"},
    {Role::Queued, "queued"},
    {Role::PieceComplete, "piece-complete"},
    {Role::PieceRemaining, "piece-remaining"},
    {Role::PieceBorder, "piece-border"}
};

struct PaletteRole
{
    QPalette::ColorRole role;
    const char *name;
};

constexpr PaletteRole PaletteRoles[] = {
    {QPalette::Window, "window"},
    {QPalette::WindowText, "window-text"},
    {QPalette::Base, "base"},
    {QPalette::AlternateBase, "alternate-base"},
    {QPalette::ToolTipBase, "tool-tip-base"},
    {QPalette::ToolTipText, "tool-tip-text"},
    {QPalette::Text, "text"},
    {QPalette::Button, "button"},
    {QPalette::ButtonText, "button-text"},
    {QPalette::BrightText, "bright-text"},
    {QPalette::Highlight, "highlight"},
    {QPalette::HighlightedText, "highlighted-text"},
    {QPalette::Link, "link"},
    {QPalette::LinkVisited, "link-visited"},
    {QPalette::PlaceholderText, "placeholder-text"}
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    , {QPalette::Accent, "accent"}
#endif
};

bool darkPalette(const QPalette &palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

} // namespace

QString semanticName(Role role)
{
    for (const SemanticColor &color : SemanticColors) {
        if (color.role == role)
            return QString::fromLatin1(color.name);
    }
    return QString();
}

std::optional<Role> roleFromSemanticName(const QString &name)
{
    const QString candidate = name.trimmed().toLower();
    for (const SemanticColor &color : SemanticColors) {
        if (QString::fromLatin1(color.name) == candidate)
            return color.role;
    }
    return std::nullopt;
}

QList<Role> allRoles()
{
    QList<Role> result;
    result.reserve(static_cast<int>(std::size(SemanticColors)));
    for (const SemanticColor &color : SemanticColors)
        result.append(color.role);
    return result;
}

QColor defaultColor(Role role, const QPalette &palette)
{
    const bool dark = darkPalette(palette);
    switch (role) {
    case Role::Download:
        return palette.color(QPalette::Link);
    case Role::Upload:
        return QColor(dark ? QStringLiteral("#b28ae0")
                           : QStringLiteral("#7b4eb4"));
    case Role::Success:
        return QColor(dark ? QStringLiteral("#5bd39b")
                           : QStringLiteral("#21865b"));
    case Role::Warning:
        return QColor(dark ? QStringLiteral("#f0b45a")
                           : QStringLiteral("#b46d13"));
    case Role::Error:
        return QColor(dark ? QStringLiteral("#ff7b7b")
                           : QStringLiteral("#b43737"));
    case Role::Inactive:
        return palette.color(QPalette::Disabled, QPalette::Text);
    case Role::Verification:
        return QColor(dark ? QStringLiteral("#ad91ff")
                           : QStringLiteral("#6850bd"));
    case Role::Queued:
        return palette.color(QPalette::Mid);
    case Role::PieceComplete:
        return QColor(dark ? QStringLiteral("#8f83e8")
                           : QStringLiteral("#403878"));
    case Role::PieceRemaining:
        return palette.color(QPalette::Base);
    case Role::PieceBorder:
        return palette.color(QPalette::Mid);
    }
    return palette.color(QPalette::Text);
}

QString paletteRoleName(QPalette::ColorRole role)
{
    for (const PaletteRole &entry : PaletteRoles) {
        if (entry.role == role)
            return QString::fromLatin1(entry.name);
    }
    return QString();
}

std::optional<QPalette::ColorRole> paletteRoleFromName(const QString &name)
{
    const QString candidate = name.trimmed().toLower();
    for (const PaletteRole &entry : PaletteRoles) {
        if (QString::fromLatin1(entry.name) == candidate)
            return entry.role;
    }
    return std::nullopt;
}

} // namespace AppColors
