#ifndef ICONTHEME_H
#define ICONTHEME_H

#include "appicons.h"

#include <QHash>
#include <QString>

namespace AppIcons {

// Describes one icon theme independently of where it came from. Resource and
// filesystem directories use the same representation so external themes can
// be registered without changing icon consumers.
class IconTheme
{
public:
    using IconFiles = QHash<Id, QString>;

    IconTheme() = default;
    IconTheme(QString id,
              QString displayName,
              QString basePath,
              IconFiles iconFiles,
              QString fallbackThemeId = QString(),
              bool builtIn = false);

    bool isValid() const;
    QString id() const;
    QString displayName() const;
    QString basePath() const;
    QString fallbackThemeId() const;
    bool isBuiltIn() const;

    bool hasIcon(Id iconId) const;
    QString iconPath(Id iconId) const;

private:
    QString m_id;
    QString m_displayName;
    QString m_basePath;
    IconFiles m_iconFiles;
    QString m_fallbackThemeId;
    bool m_builtIn = false;
};

} // namespace AppIcons

#endif // ICONTHEME_H
