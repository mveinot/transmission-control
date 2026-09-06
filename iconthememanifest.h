#ifndef ICONTHEMEMANIFEST_H
#define ICONTHEMEMANIFEST_H

#include "icontheme.h"

#include <QString>

namespace AppIcons {

struct IconThemeManifestResult
{
    IconTheme theme;
    QString error;

    bool succeeded() const { return theme.isValid() && error.isEmpty(); }
};

class IconThemeManifestParser
{
public:
    static constexpr int CurrentFormatVersion = 1;
    static constexpr const char *FileName = "theme.json";

    static IconThemeManifestResult parseFile(const QString &manifestPath);
};

} // namespace AppIcons

#endif // ICONTHEMEMANIFEST_H
