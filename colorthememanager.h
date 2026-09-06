#ifndef COLORTHEMEMANAGER_H
#define COLORTHEMEMANAGER_H

#include "appcolors.h"

#include <QObject>
#include <QString>

namespace AppColors {

class ColorThemeManager final : public QObject
{
    Q_OBJECT

public:
    static ColorThemeManager &instance();

    QString themeId() const;
    QColor color(Role role) const;
    void setThemeId(const QString &themeId);

signals:
    void themeChanged(const QString &themeId);

private:
    ColorThemeManager();

    void applyTheme();

    QString m_themeId;
};

} // namespace AppColors

#endif // COLORTHEMEMANAGER_H
