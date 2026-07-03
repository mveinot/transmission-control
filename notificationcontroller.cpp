#include "notificationcontroller.h"

#include "settingskeys.h"
#include "torrent.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace {
QString appleScriptQuotedString(QString text)
{
    text.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    text.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    text.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    text.replace(QLatin1Char('\r'), QString());

    return QStringLiteral("\"") + text + QStringLiteral("\"");
}
}

NotificationController::NotificationController(QObject *parent)
    : QObject(parent)
{
}

bool NotificationController::notificationsEnabled() const
{
    QSettings settings;

    if (settings.contains(SettingsKeys::ShowNotifications))
        return settings.value(SettingsKeys::ShowNotifications, true).toBool();

    // Backward-compatible migration from the old tray-coupled setting. Do not
    // require the tray icon to be enabled: notifications are now their own
    // feature, with the platform notifier used where available.
    return settings.value(SettingsKeys::ShowTrayNotifications, true).toBool();
}

void NotificationController::showNotification(const QString &title,
                                              const QString &message,
                                              int millisecondsTimeoutHint)
{
    if (!notificationsEnabled())
        return;

    if (showPlatformNotification(title, message, millisecondsTimeoutHint))
        return;

    // Last-resort fallback: keep the user-visible event somewhere, even if the
    // current platform has no simple notification command available.
    emit statusMessageRequested(
        title.isEmpty() ? message : tr("%1: %2").arg(title, message),
        millisecondsTimeoutHint
        );
}

bool NotificationController::showPlatformNotification(const QString &title,
                                                       const QString &message,
                                                       int millisecondsTimeoutHint) const
{
    Q_UNUSED(millisecondsTimeoutHint)

#if defined(Q_OS_MACOS)
    const QString osascript = QStringLiteral("/usr/bin/osascript");

    if (!QFileInfo::exists(osascript))
        return false;

    const QString script = QStringLiteral("display notification %1 with title %2")
                               .arg(appleScriptQuotedString(message),
                                    appleScriptQuotedString(title.isEmpty()
                                                            ? QCoreApplication::applicationName()
                                                            : title));

    return QProcess::startDetached(osascript, {QStringLiteral("-e"), script});
#elif defined(Q_OS_LINUX)
    const QString notifySend = QStandardPaths::findExecutable(QStringLiteral("notify-send"));

    if (notifySend.isEmpty())
        return false;

    QStringList arguments;
    arguments << QStringLiteral("--app-name=%1").arg(QCoreApplication::applicationName());

    if (millisecondsTimeoutHint > 0)
        arguments << QStringLiteral("--expire-time=%1").arg(millisecondsTimeoutHint);

    arguments << (title.isEmpty() ? QCoreApplication::applicationName() : title)
              << message;

    return QProcess::startDetached(notifySend, arguments);
#else
    Q_UNUSED(title)
    Q_UNUSED(message)
    return false;
#endif
}

bool NotificationController::isTorrentCompleteForNotification(const torrent &torrentItem)
{
    const QString status = torrentItem.getStatus();

    return torrentItem.getPercentDone() >= 99.9 ||
           status == QStringLiteral("Seeding") ||
           status == QStringLiteral("Waiting to Seed");
}

void NotificationController::processTorrentList(const QVector<torrent> &torrents)
{
    QSet<int> currentlyCompleted;

    for (const torrent &torrentItem : torrents) {
        if (isTorrentCompleteForNotification(torrentItem))
            currentlyCompleted.insert(torrentItem.getId());
    }

    if (!m_completedTorrentNotificationBaselineLoaded) {
        m_knownCompletedTorrentIds = currentlyCompleted;
        m_completedTorrentNotificationBaselineLoaded = true;
        return;
    }

    for (const torrent &torrentItem : torrents) {
        const int id = torrentItem.getId();

        if (!currentlyCompleted.contains(id))
            continue;

        if (m_knownCompletedTorrentIds.contains(id))
            continue;

        showNotification(
            tr("Torrent finished"),
            torrentItem.getName(),
            5000
            );
    }

    m_knownCompletedTorrentIds = currentlyCompleted;
}
