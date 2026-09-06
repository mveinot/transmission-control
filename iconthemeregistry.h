#ifndef ICONTHEMEREGISTRY_H
#define ICONTHEMEREGISTRY_H

#include "icontheme.h"

#include <QHash>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QSet>
#include <QStringList>

namespace AppIcons {

class IconThemeRegistry final : public QObject
{
    Q_OBJECT

public:
    static IconThemeRegistry &instance();
    static QString standardThemeDirectory();

    explicit IconThemeRegistry(const QString &themeDirectory,
                               QObject *parent = nullptr);

    QList<IconTheme> themes() const;
    IconTheme theme(const QString &themeId) const;
    bool contains(const QString &themeId) const;
    QString resolvedThemeId(const QString &themeId) const;
    QString defaultThemeId() const;
    QString themeDirectory() const;
    QIcon icon(const QString &themeId, Id iconId) const;

    bool registerTheme(const IconTheme &theme);
    bool unregisterTheme(const QString &themeId);
    void rescanExternalThemes();

signals:
    void registryChanged(const QString &themeId);

private:
    void registerBuiltInThemes();
    QString canonicalId(const QString &themeId) const;

    QString m_themeDirectory;
    QHash<QString, IconTheme> m_themes;
    QStringList m_themeOrder;
    QSet<QString> m_scannedThemeIds;
};

} // namespace AppIcons

#endif // ICONTHEMEREGISTRY_H
