#ifndef ICONTHEMEMANAGER_H
#define ICONTHEMEMANAGER_H

#include "appicons.h"

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QString>

class QAction;
class QEvent;

namespace AppIcons {

// Owns the active theme and live bindings. Icons returned by icon() contain a
// software-generated QIcon::Active variant for a subtle mouse-over effect.
// Other icon consumers should call icon() when painting and respond to
// themeChanged by invalidating their view.
class IconThemeManager final : public QObject
{
    Q_OBJECT

public:
    static IconThemeManager &instance();

    QString themeId() const;
    QIcon icon(Id iconId) const;
    QIcon icon(Id iconId, const QString &themeId) const;
    QIcon hoverIcon(Id iconId) const;
    void bindAction(QAction *action, Id iconId);
    void setThemeId(const QString &themeId);

signals:
    void themeChanged(const QString &themeId);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    IconThemeManager();

    void refreshTheme();
    void refreshBoundActions();
    void clearIconCache();

    QString m_themeId;
    QHash<QAction *, Id> m_boundActions;
    mutable QHash<QString, QIcon> m_iconCache;
};

} // namespace AppIcons

#endif // ICONTHEMEMANAGER_H
