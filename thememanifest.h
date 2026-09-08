#ifndef THEMEMANIFEST_H
#define THEMEMANIFEST_H

#include "theme.h"

#include <QByteArray>
#include <QString>

namespace AppThemes {

struct ThemeManifestResult
{
    Theme theme;
    QString error;

    bool succeeded() const { return theme.isValid() && error.isEmpty(); }
};

class ThemeManifestParser
{
public:
    static constexpr int CurrentFormatVersion = 1;
    static constexpr const char *FileName = "theme.json";

    static ThemeManifestResult parseFile(const QString &manifestPath);
    static ThemeManifestResult parseData(const QByteArray &manifestData,
                                         const QString &basePath,
                                         bool verifyReferencedFiles = true);
};

} // namespace AppThemes

#endif // THEMEMANIFEST_H
