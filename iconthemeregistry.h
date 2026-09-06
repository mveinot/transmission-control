#ifndef ICONTHEMEREGISTRY_H
#define ICONTHEMEREGISTRY_H

#include "icontheme.h"

#include <QHash>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QStringList>

namespace AppIcons {

class IconThemeRegistry final : public QObject
{
    Q_OBJECT

public:
    static IconThemeRegistry &instance();

    QList<IconTheme> themes() const;
    IconTheme theme(const QString &themeId) const;
    bool contains(const QString &themeId) const;
    QString resolvedThemeId(const QString &themeId) const;
    QString defaultThemeId() const;
    QIcon icon(const QString &themeId, Id iconId) const;

    bool registerTheme(const IconTheme &theme);
    bool unregisterTheme(const QString &themeId);

signals:
    void registryChanged(const QString &themeId);

private:
    IconThemeRegistry();

    void registerBuiltInThemes();
    QString canonicalId(const QString &themeId) const;

    QHash<QString, IconTheme> m_themes;
    QStringList m_themeOrder;
};

} // namespace AppIcons

#endif // ICONTHEMEREGISTRY_H
