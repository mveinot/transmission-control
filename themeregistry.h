#ifndef THEMEREGISTRY_H
#define THEMEREGISTRY_H

#include "theme.h"

#include <QHash>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QSet>
#include <QStringList>

namespace AppThemes {

class ThemeRegistry final : public QObject
{
    Q_OBJECT

public:
    static ThemeRegistry &instance();
    static QString standardThemeDirectory();

    explicit ThemeRegistry(const QString &themeDirectory,
                           QObject *parent = nullptr);

    QList<Theme> themes() const;
    QList<AppIcons::IconTheme> iconThemes() const;
    QList<AppColors::ColorTheme> colorThemes() const;
    Theme theme(const QString &themeId) const;
    AppIcons::IconTheme iconTheme(const QString &themeId) const;
    AppColors::ColorTheme colorTheme(const QString &themeId) const;
    bool contains(const QString &themeId) const;
    QString resolvedIconThemeId(const QString &themeId) const;
    QString resolvedColorThemeId(const QString &themeId) const;
    QString defaultIconThemeId() const;
    QString defaultColorThemeId() const;
    QString themeDirectory() const;
    QIcon icon(const QString &themeId, AppIcons::Id iconId) const;

    bool registerTheme(const Theme &theme);
    bool unregisterTheme(const QString &themeId);
    void rescanExternalThemes();

signals:
    void registryChanged(const QString &themeId);

private:
    void registerBuiltInThemes();
    QString canonicalId(const QString &themeId) const;

    QString m_themeDirectory;
    QHash<QString, Theme> m_themes;
    QStringList m_themeOrder;
    QSet<QString> m_scannedThemeIds;
};

} // namespace AppThemes

#endif // THEMEREGISTRY_H
