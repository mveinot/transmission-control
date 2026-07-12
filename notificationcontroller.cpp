#include "notificationcontroller.h"

#include "settingskeys.h"
#include "torrent.h"

#if defined(Q_OS_MACOS)
#include "macnotificationbackend.h"
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QLocale>
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

bool NotificationController::deliveryEnabled(const char *settingsKey, bool defaultValue)
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
    emit statusMessageRequested(title.isEmpty() ? message : tr("%1: %2").arg(title, message),
                                millisecondsTimeoutHint);
}

void NotificationController::showTestNotification()
{
    if (showPlatformNotification(tr("Planetary notification test"),
                                 tr("Notifications are working."), 5000))
        return;
    emit statusMessageRequested(
        tr("Notification test: no supported desktop notification backend was available."), 8000);
}

void NotificationController::showTestExternalCommand(const QString &executable,
                                                       const QString &argumentTemplate)
{
    EventContext context;
    context.event = QStringLiteral("test");
    context.name = tr("Planetary Test Torrent");
    context.id = 123;
    context.hash = QStringLiteral("0123456789abcdef0123456789abcdef01234567");
    context.sizeBytes = 1610612736;
    context.size = QLocale().formattedDataSize(context.sizeBytes);
    context.status = tr("Downloading");
    context.progress = 42.0;
    context.downloadDir = QStringLiteral("/Downloads");
    context.server = m_serverName.isEmpty() ? tr("Test Server") : m_serverName;
    context.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);

    emit statusMessageRequested(
        runExternalCommand(executable, argumentTemplate, context)
            ? tr("External notification command started.")
            : tr("Could not start the external notification command."),
        8000);
}

QStringList NotificationController::parseExternalArguments(const QString &argumentTemplate)
{
    return QProcess::splitCommand(argumentTemplate);
}

void NotificationController::dispatchEvent(const QString &title,
                                            const QString &message,
                                            const EventContext &context,
                                            int millisecondsTimeoutHint)
{
    if (!notificationsEnabled())
        return;

    bool delivered = false;
    if (deliveryEnabled(SettingsKeys::DesktopNotificationsEnabled, true))
        delivered = showPlatformNotification(title, message, millisecondsTimeoutHint);

    QSettings settings;
    if (deliveryEnabled(SettingsKeys::ExternalCommandEnabled, false)) {
        const QString executable = settings.value(SettingsKeys::ExternalCommandExecutable)
                                       .toString().trimmed();
        const QString arguments = settings.value(SettingsKeys::ExternalCommandArguments).toString();
        if (!executable.isEmpty())
            delivered = runExternalCommand(executable, arguments, context) || delivered;
    }

    if (!delivered)
        emit statusMessageRequested(title.isEmpty() ? message : tr("%1: %2").arg(title, message),
                                    millisecondsTimeoutHint);
}

bool NotificationController::runExternalCommand(const QString &executable,
                                                  const QString &argumentTemplate,
                                                  const EventContext &context) const
{
    const QString program = executable.trimmed();
    if (program.isEmpty())
        return false;

    QStringList arguments = parseExternalArguments(argumentTemplate);
    for (QString &argument : arguments)
        argument = expandArgument(argument, context);
    return QProcess::startDetached(program, arguments);
}

QString NotificationController::expandArgument(QString argument, const EventContext &context)
{
    argument.replace(QStringLiteral("{event}"), context.event);
    argument.replace(QStringLiteral("{name}"), context.name);
    argument.replace(QStringLiteral("{id}"), QString::number(context.id));
    argument.replace(QStringLiteral("{hash}"), context.hash);
    argument.replace(QStringLiteral("{size}"), context.size);
    argument.replace(QStringLiteral("{size_bytes}"), QString::number(context.sizeBytes));
    argument.replace(QStringLiteral("{status}"), context.status);
    argument.replace(QStringLiteral("{progress}"), QLocale().toString(context.progress, 'f', 1));
    argument.replace(QStringLiteral("{download_dir}"), context.downloadDir);
    argument.replace(QStringLiteral("{server}"), context.server);
    argument.replace(QStringLiteral("{error}"), context.error);
    argument.replace(QStringLiteral("{timestamp}"), context.timestamp);
    return argument;
}

NotificationController::EventContext NotificationController::contextForTorrent(
    const QString &event, const torrent &torrentItem, const QString &serverName)
{
    EventContext context;
    context.event = event;
    context.name = torrentItem.getName();
    context.id = torrentItem.getId();
    context.hash = torrentItem.getHashString();
    context.sizeBytes = torrentItem.getSizeBytes();
    context.size = torrentItem.getSize();
    context.status = torrentItem.getStatus();
    context.progress = torrentItem.getPercentDone();
    context.downloadDir = torrentItem.getDownloadDir();
    context.server = serverName;
    context.error = torrentItem.getErrorString();
    context.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    return context;
}

bool NotificationController::showPlatformNotification(const QString &title,
                                                       const QString &message,
                                                       int millisecondsTimeoutHint) const
{
    if (m_deliveryFunction)
        return m_deliveryFunction(title, message, millisecondsTimeoutHint);
#if defined(Q_OS_MACOS)
    Q_UNUSED(millisecondsTimeoutHint)
    return showMacUserNotification(title.isEmpty() ? QCoreApplication::applicationName() : title,
                                   message);
#elif defined(Q_OS_LINUX)
    const QString notifySend = QStandardPaths::findExecutable(QStringLiteral("notify-send"));
    if (notifySend.isEmpty())
        return false;
    QStringList arguments;
    arguments << QStringLiteral("--app-name=%1").arg(QCoreApplication::applicationName());
    if (millisecondsTimeoutHint > 0)
        arguments << QStringLiteral("--expire-time=%1").arg(millisecondsTimeoutHint);
    arguments << (title.isEmpty() ? QCoreApplication::applicationName() : title) << message;
    return QProcess::startDetached(notifySend, arguments);
#else
    Q_UNUSED(title)
    Q_UNUSED(message)
    Q_UNUSED(millisecondsTimeoutHint)
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

void NotificationController::setServerName(const QString &serverName)
{
    m_serverName = serverName.trimmed();
}

void NotificationController::handleTorrentAdded(int torrentId, const QString &name)
{
    emit activityEventObserved(tr("Torrent added"),
                               name.isEmpty() ? tr("Transmission accepted a new torrent.") : name,
                               m_serverName);

    if (torrentId >= 0)
        m_directlyNotifiedAddedTorrentIds.insert(torrentId);
    if (!eventEnabled(SettingsKeys::NotifyTorrentAdded))
        return;

    EventContext context;
    context.event = QStringLiteral("torrent_added");
    context.name = name;
    context.id = torrentId;
    context.server = m_serverName;
    context.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    dispatchEvent(tr("Torrent added"),
                  name.isEmpty() ? tr("Transmission accepted a new torrent.") : name,
                  context, 5000);
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
            if (m_directlyNotifiedAddedTorrentIds.remove(id) == 0) {
                emit activityEventObserved(tr("Torrent added"), torrentItem.getName(), m_serverName);
                if (eventEnabled(SettingsKeys::NotifyTorrentAdded)) {
                    dispatchEvent(tr("Torrent added"), torrentItem.getName(),
                                  contextForTorrent(QStringLiteral("torrent_added"), torrentItem,
                                                    m_serverName), 5000);
                }
            }
            continue;
        }

        const TorrentState &previous = previousIt.value();
        if (!previous.complete && current.complete) {
            emit activityEventObserved(tr("Torrent finished"), torrentItem.getName(), m_serverName);
            if (eventEnabled(SettingsKeys::NotifyTorrentCompleted)) {
                dispatchEvent(tr("Torrent finished"), torrentItem.getName(),
                          contextForTorrent(QStringLiteral("torrent_completed"), torrentItem,
                                            m_serverName), 5000);
            }
        }

        if (!previous.error && current.error) {
            const QString details = current.errorString.isEmpty()
                                        ? torrentItem.getName()
                                        : tr("%1\n%2").arg(torrentItem.getName(), current.errorString);
            emit activityEventObserved(tr("Torrent error"), details, m_serverName);
            if (eventEnabled(SettingsKeys::NotifyTorrentError))
                dispatchEvent(tr("Torrent error"), details,
                              contextForTorrent(QStringLiteral("torrent_error"), torrentItem,
                                            m_serverName), 8000);
        }

        if (!previous.stalled && current.stalled && !current.complete) {
            emit activityEventObserved(tr("Torrent stalled"), torrentItem.getName(), m_serverName);
            if (eventEnabled(SettingsKeys::NotifyTorrentStalled))
                dispatchEvent(tr("Torrent stalled"), torrentItem.getName(),
                              contextForTorrent(QStringLiteral("torrent_stalled"), torrentItem,
                                            m_serverName), 8000);
        }
    }
    m_knownTorrentStates = currentStates;
}
