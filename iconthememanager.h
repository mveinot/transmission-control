#ifndef ICONTHEMEMANAGER_H
#define ICONTHEMEMANAGER_H

#include "appicons.h"

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QString>

class QAction;

namespace AppIcons {

// Owns the active theme and live bindings. Other icon consumers should call
// icon() when painting and respond to themeChanged by invalidating their view.
class IconThemeManager final : public QObject
{
    Q_OBJECT

public:
    static IconThemeManager &instance();

    QString themeId() const;
    QIcon icon(Id iconId) const;
    QIcon icon(Id iconId, const QString &themeId) const;
    void bindAction(QAction *action, Id iconId);
    void setThemeId(const QString &themeId);

signals:
    void themeChanged(const QString &themeId);

private:
    IconThemeManager();

    void refreshTheme();
    void refreshBoundActions();

    QString m_themeId;
    QHash<QAction *, Id> m_boundActions;
};

} // namespace AppIcons

#endif // ICONTHEMEMANAGER_H
