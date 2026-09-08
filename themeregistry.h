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
    static QString standardThemeCacheDirectory();

    explicit ThemeRegistry(const QString &themeDirectory,
                           QObject *parent = nullptr,
                           const QString &themeCacheDirectory = QString());

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
    struct ArchiveSource {
        QString archivePath;
        QString entryPrefix;
        QString cachePath;
    };

    void registerBuiltInThemes();
    bool ensureThemeMaterialized(const QString &themeId) const;
    QString archiveCachePath(const QString &archivePath,
                             const QString &themeId) const;
    QString canonicalId(const QString &themeId) const;

    QString m_themeDirectory;
    QString m_themeCacheDirectory;
    mutable QHash<QString, Theme> m_themes;
    QStringList m_themeOrder;
    QSet<QString> m_scannedThemeIds;
    QHash<QString, ArchiveSource> m_archiveSources;
    mutable QSet<QString> m_failedArchiveIds;
    mutable QSet<QString> m_materializedArchiveIds;
};

} // namespace AppThemes

#endif // THEMEREGISTRY_H
