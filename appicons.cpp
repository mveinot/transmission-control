#include "appicons.h"

#include "settingskeys.h"

#include <QAction>
#include <QSettings>
#include <QString>

namespace AppIcons {

IconManager &IconManager::instance()
{
    static IconManager manager;
    return manager;
}

IconManager::IconManager()
    : m_themeId(normalizedThemeId(
          QSettings().value(SettingsKeys::IconTheme,
                            QString::fromLatin1(GlassTheme)).toString()))
{
}

QString IconManager::themeId() const
{
    return m_themeId;
}

QString IconManager::normalizedThemeId(const QString &themeId) const
{
    const QString normalized = themeId.trimmed().toLower();
    if (normalized == QString::fromLatin1(ClassicTheme))
        return normalized;

    return QString::fromLatin1(GlassTheme);
}

QString IconManager::iconFileName(Id iconId) const
{
    switch (iconId) {
    case Id::ActionAddTorrent:
        return QStringLiteral("action-add-torrent.png");
    case Id::ActionAddMagnet:
        return QStringLiteral("action-add-magnet.png");
    case Id::ActionStart:
        return QStringLiteral("action-start.png");
    case Id::ActionStop:
        return QStringLiteral("action-stop.png");
    case Id::ActionStartAll:
        return QStringLiteral("action-start-all.png");
    case Id::ActionStopAll:
        return QStringLiteral("action-stop-all.png");
    case Id::ActionForceStart:
        return QStringLiteral("action-force-start.png");
    case Id::ActionVerify:
        return QStringLiteral("action-verify.png");
    case Id::ActionReannounce:
        return QStringLiteral("action-reannounce.png");
    case Id::ActionDelete:
        return QStringLiteral("action-delete.png");
    case Id::QueueTop:
        return QStringLiteral("queue-top.png");
    case Id::QueueUp:
        return QStringLiteral("queue-up.png");
    case Id::QueueDown:
        return QStringLiteral("queue-down.png");
    case Id::QueueBottom:
        return QStringLiteral("queue-bottom.png");
    case Id::FilterAll:
        return QStringLiteral("filter-all.png");
    case Id::FilterTracker:
        return QStringLiteral("filter-tracker.png");
    case Id::FilterFolder:
        return QStringLiteral("filter-folder.png");
    case Id::StatusDownloading:
        return QStringLiteral("status-downloading.png");
    case Id::StatusSeeding:
        return QStringLiteral("status-seeding.png");
    case Id::StatusComplete:
        return QStringLiteral("status-complete.png");
    case Id::StatusActive:
        return QStringLiteral("status-active.png");
    case Id::StatusInactive:
        return QStringLiteral("status-inactive.png");
    case Id::StatusStopped:
        return QStringLiteral("status-stopped.png");
    case Id::StatusError:
        return QStringLiteral("status-error.png");
    case Id::StatusVerifying:
        return QStringLiteral("status-verifying.png");
    case Id::StatusQueued:
        return QStringLiteral("status-queued.png");
    case Id::StatusUnknown:
        return QStringLiteral("status-unknown.png");
    }

    return QString();
}

QIcon IconManager::icon(Id iconId) const
{
    return icon(iconId, m_themeId);
}

QIcon IconManager::icon(Id iconId, const QString &themeId) const
{
    const QString path = QStringLiteral(":/icons/ui/%1/%2")
                             .arg(normalizedThemeId(themeId),
                                  iconFileName(iconId));
    return QIcon(path);
}

void IconManager::bindAction(QAction *action, Id iconId)
{
    if (!action)
        return;

    const bool alreadyBound = m_boundActions.contains(action);
    m_boundActions.insert(action, iconId);
    action->setProperty("planetaryIconId", static_cast<int>(iconId));
    action->setIcon(icon(iconId));

    if (!alreadyBound) {
        connect(action, &QObject::destroyed, this, [this, action]() {
            m_boundActions.remove(action);
        });
    }
}

void IconManager::setThemeId(const QString &themeId)
{
    const QString normalized = normalizedThemeId(themeId);
    if (normalized == m_themeId)
        return;

    m_themeId = normalized;
    refreshBoundActions();
    emit themeChanged(m_themeId);
}

void IconManager::refreshBoundActions()
{
    for (auto it = m_boundActions.cbegin(); it != m_boundActions.cend(); ++it)
        it.key()->setIcon(icon(it.value()));
}

} // namespace AppIcons
