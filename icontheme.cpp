#include "icontheme.h"

#include <QDir>

#include <utility>

namespace AppIcons {

IconTheme::IconTheme(QString id,
                     QString displayName,
                     QString basePath,
                     IconFiles iconFiles,
                     QString fallbackThemeId,
                     bool builtIn)
    : m_id(id.trimmed().toLower())
    , m_displayName(std::move(displayName))
    , m_basePath(std::move(basePath))
    , m_iconFiles(std::move(iconFiles))
    , m_fallbackThemeId(fallbackThemeId.trimmed().toLower())
    , m_builtIn(builtIn)
{
}

bool IconTheme::isValid() const
{
    return !m_id.isEmpty() && !m_displayName.isEmpty() && !m_basePath.isEmpty();
}

QString IconTheme::id() const
{
    return m_id;
}

QString IconTheme::displayName() const
{
    return m_displayName;
}

QString IconTheme::basePath() const
{
    return m_basePath;
}

QString IconTheme::fallbackThemeId() const
{
    return m_fallbackThemeId;
}

bool IconTheme::isBuiltIn() const
{
    return m_builtIn;
}

bool IconTheme::hasIcon(Id iconId) const
{
    return m_iconFiles.contains(iconId) && !m_iconFiles.value(iconId).isEmpty();
}

QString IconTheme::iconPath(Id iconId) const
{
    const QString fileName = m_iconFiles.value(iconId);
    if (fileName.isEmpty())
        return QString();

    if (fileName.startsWith(QStringLiteral(":/"))
        || QDir::isAbsolutePath(fileName)) {
        return fileName;
    }

    return QDir(m_basePath).filePath(fileName);
}

} // namespace AppIcons
