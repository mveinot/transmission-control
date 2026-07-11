#include "notificationcontroller.h"

#include "settingskeys.h"
#include "torrent.h"

#if defined(Q_OS_MACOS)
#include "macnotificationbackend.h"
#endif

#include <QCoreApplication>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

#include <utility>

NotificationController::NotificationController(QObject *parent,
                                                   DeliveryFunction deliveryFunction)
    : QObject(parent)
    , m_deliveryFunction(std::move(deliveryFunction))
{
}

bool NotificationController::notificationsEnabled() const
{
    QSettings settings;

    if (settings.contains(SettingsKeys::ShowNotifications))
        return settings.value(SettingsKeys::ShowNotifications, true).toBool();

    return settings.value(SettingsKeys::ShowTrayNotifications, true).toBool();
}

bool NotificationController::eventEnabled(const char *settingsKey, bool defaultValue)
{
    return QSettings().value(settingsKey, defaultValue).toBool();
}

void NotificationController::showNotification(const QString &title,
                                              const QString &message,
                                              int millisecondsTimeoutHint)
{
    if (!notificationsEnabled())
        return;

    if (showPlatformNotification(title, message, millisecondsTimeoutHint))
        return;

    emit statusMessageRequested(
        title.isEmpty() ? message : tr("%1: %2").arg(title, message),
        millisecondsTimeoutHint
        );
}

void NotificationController::showTestNotification()
{
    const QString title = tr("Planetary notification test");
    const QString message = tr("Notifications are working.");

    if (showPlatformNotification(title, message, 5000))
        return;

    emit statusMessageRequested(
        tr("Notification test: no supported desktop notification backend was available."),
        8000
        );
}

bool NotificationController::showPlatformNotification(const QString &title,
                                                       const QString &message,
                                                       int millisecondsTimeoutHint) const
{
    if (m_deliveryFunction)
        return m_deliveryFunction(title, message, millisecondsTimeoutHint);

#if defined(Q_OS_MACOS)
    Q_UNUSED(millisecondsTimeoutHint)
    return showMacUserNotification(
        title.isEmpty() ? QCoreApplication::applicationName() : title,
        message
        );
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
    const auto status = static_cast<torrent::Status>(torrentItem.getStatusValue());

    return torrentItem.getPercentDone() >= 99.9 ||
           status == torrent::Status::Seeding ||
           status == torrent::Status::WaitingToSeed;
}

NotificationController::TorrentState NotificationController::stateForTorrent(
    const torrent &torrentItem)
{
    TorrentState state;
    state.complete = isTorrentCompleteForNotification(torrentItem);
    state.error = torrentItem.hasError();
    state.stalled = torrentItem.isStalled();
    state.errorString = torrentItem.getErrorString();
    return state;
}

void NotificationController::resetBaseline()
{
    m_baselineLoaded = false;
    m_knownTorrentStates.clear();
    m_directlyNotifiedAddedTorrentIds.clear();
}

void NotificationController::handleTorrentAdded(int torrentId, const QString &name)
{
    if (torrentId >= 0)
        m_directlyNotifiedAddedTorrentIds.insert(torrentId);

    if (!eventEnabled(SettingsKeys::NotifyTorrentAdded))
        return;

    showNotification(
        tr("Torrent added"),
        name.isEmpty() ? tr("Transmission accepted a new torrent.") : name,
        5000
        );

}

void NotificationController::processTorrentList(const QVector<torrent> &torrents)
{
    QHash<int, TorrentState> currentStates;
    currentStates.reserve(torrents.size());

    for (const torrent &torrentItem : torrents)
        currentStates.insert(torrentItem.getId(), stateForTorrent(torrentItem));

    if (!m_baselineLoaded) {
        m_knownTorrentStates = currentStates;
        m_baselineLoaded = true;
        return;
    }

    for (const torrent &torrentItem : torrents) {
        const int id = torrentItem.getId();
        const TorrentState current = currentStates.value(id);
        const auto previousIt = m_knownTorrentStates.constFind(id);

        if (previousIt == m_knownTorrentStates.constEnd()) {
            if (m_directlyNotifiedAddedTorrentIds.remove(id) == 0
                && eventEnabled(SettingsKeys::NotifyTorrentAdded)) {
                showNotification(
                    tr("Torrent added"),
                    torrentItem.getName(),
                    5000
                    );
            }
            continue;
        }

        const TorrentState &previous = previousIt.value();

        if (!previous.complete && current.complete &&
            eventEnabled(SettingsKeys::NotifyTorrentCompleted)) {
            showNotification(
                tr("Torrent finished"),
                torrentItem.getName(),
                5000
                );
        }

        if (!previous.error && current.error &&
            eventEnabled(SettingsKeys::NotifyTorrentError)) {
            const QString details = current.errorString.isEmpty()
                                        ? torrentItem.getName()
                                        : tr("%1\n%2").arg(torrentItem.getName(),
                                                           current.errorString);
            showNotification(
                tr("Torrent error"),
                details,
                8000
                );
        }

        if (!previous.stalled && current.stalled && !current.complete &&
            eventEnabled(SettingsKeys::NotifyTorrentStalled)) {
            showNotification(
                tr("Torrent stalled"),
                torrentItem.getName(),
                8000
                );
        }
    }

    m_knownTorrentStates = currentStates;
}
