#ifndef APPICONS_H
#define APPICONS_H

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QString>

class QAction;

namespace AppIcons {

inline constexpr const char *ClassicTheme = "classic";
inline constexpr const char *GlassTheme = "glass";

// Semantic icon identifiers isolate controllers from resource paths and keep
// action/status artwork consistent across menus, toolbars, and item views.
enum class Id {
    ActionAddTorrent,
    ActionAddMagnet,
    ActionStart,
    ActionStop,
    ActionStartAll,
    ActionStopAll,
    ActionForceStart,
    ActionVerify,
    ActionReannounce,
    ActionDelete,
    QueueTop,
    QueueUp,
    QueueDown,
    QueueBottom,
    FilterAll,
    FilterTracker,
    FilterFolder,
    StatusDownloading,
    StatusSeeding,
    StatusComplete,
    StatusActive,
    StatusInactive,
    StatusStopped,
    StatusError,
    StatusVerifying,
    StatusQueued,
    StatusUnknown
};

class IconManager final : public QObject
{
    Q_OBJECT

public:
    static IconManager &instance();

    QString themeId() const;
    QString normalizedThemeId(const QString &themeId) const;
    QIcon icon(Id iconId) const;
    QIcon icon(Id iconId, const QString &themeId) const;
    void bindAction(QAction *action, Id iconId);
    void setThemeId(const QString &themeId);

signals:
    void themeChanged(const QString &themeId);

private:
    IconManager();

    QString iconFileName(Id iconId) const;
    void refreshBoundActions();

    QString m_themeId;
    QHash<QAction *, Id> m_boundActions;
};

} // namespace AppIcons

#endif // APPICONS_H
