#ifndef MACNOTIFICATIONBACKEND_H
#define MACNOTIFICATIONBACKEND_H

#include <QString>

/*
 * Submits a notification through UserNotifications on supported macOS
 * versions. A true result means the native asynchronous flow was started, not
 * that authorization was granted or delivery completed. Failures after
 * submission are logged by the Objective-C++ backend.
 */
bool showMacUserNotification(const QString &title, const QString &message);

#endif // MACNOTIFICATIONBACKEND_H
