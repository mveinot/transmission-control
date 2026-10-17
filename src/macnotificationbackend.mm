#include "macnotificationbackend.h"

#include <QByteArray>
#include <QDebug>

#import <UserNotifications/UserNotifications.h>

// UserNotifications suppresses foreground presentation unless its delegate
// explicitly selects presentation options for the active application.
@interface PlanetaryNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation PlanetaryNotificationDelegate

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler
{
    (void)center;
    (void)notification;

    UNNotificationPresentationOptions options = UNNotificationPresentationOptionSound;

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 110000
    // Banner/List replaced the deprecated Alert option in the macOS 11 SDK,
    // while the deployment target still requires the legacy runtime branch.
    if (@available(macOS 11.0, *)) {
        options |= UNNotificationPresentationOptionBanner
                   | UNNotificationPresentationOptionList;
    } else
#endif
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        options |= UNNotificationPresentationOptionAlert;
#pragma clang diagnostic pop
    }

    completionHandler(options);
}

@end

namespace {

NSString *toNSString(const QString &text)
{
    // stringWithUTF8String copies the temporary QByteArray contents into an
    // autoreleased NSString before utf8 leaves scope.
    const QByteArray utf8 = text.toUtf8();
    return [NSString stringWithUTF8String:utf8.constData()];
}

PlanetaryNotificationDelegate *notificationDelegate()
{
    // UNUserNotificationCenter.delegate is not an ownership boundary. Retain a
    // process-lifetime delegate so foreground callbacks never target a dead
    // temporary object.
    static PlanetaryNotificationDelegate *delegate =
        [[PlanetaryNotificationDelegate alloc] init];
    return delegate;
}

void deliverNotification(UNUserNotificationCenter *center,
                         NSString *title,
                         NSString *message)
{
    // This translation unit is compiled with ARC; content and request remain
    // alive through addNotificationRequest without manual release handling.
    UNMutableNotificationContent *content =
        [[UNMutableNotificationContent alloc] init];
    content.title = title;
    content.body = message;
    content.sound = [UNNotificationSound defaultSound];

    UNNotificationRequest *request =
        [UNNotificationRequest requestWithIdentifier:[[NSUUID UUID] UUIDString]
                                             content:content
                                             trigger:nil];

    [center addNotificationRequest:request
                     withCompletionHandler:^(NSError *error) {
        if (error) {
            qWarning() << "Failed to schedule macOS notification:"
                       << QString::fromNSString(error.localizedDescription);
        }
    }];
}

} // namespace

bool showMacUserNotification(const QString &title, const QString &message)
{
    if (@available(macOS 10.14, *)) {
        UNUserNotificationCenter *center =
            [UNUserNotificationCenter currentNotificationCenter];
        center.delegate = notificationDelegate();

        // Settings and authorization callbacks may execute after this function
        // returns. Strong copies make the captured strings independent of the
        // Qt arguments and the autorelease pool that produced them.
        NSString *nativeTitle = [toNSString(title) copy];
        NSString *nativeMessage = [toNSString(message) copy];

        [center getNotificationSettingsWithCompletionHandler:
            ^(UNNotificationSettings *settings) {
                if (settings.authorizationStatus == UNAuthorizationStatusAuthorized
                    || settings.authorizationStatus == UNAuthorizationStatusProvisional) {
                    deliverNotification(center, nativeTitle, nativeMessage);
                    return;
                }

                if (settings.authorizationStatus == UNAuthorizationStatusNotDetermined) {
                    // Prompt only from the undetermined state. Re-requesting
                    // after denial cannot change authorization and would add
                    // needless asynchronous work.
                    const UNAuthorizationOptions options =
                        UNAuthorizationOptionAlert | UNAuthorizationOptionSound;

                    [center requestAuthorizationWithOptions:options
                                          completionHandler:^(BOOL granted, NSError *error) {
                        if (error) {
                            qWarning() << "macOS notification authorization failed:"
                                       << QString::fromNSString(error.localizedDescription);
                            return;
                        }

                        if (granted) {
                            deliverNotification(center, nativeTitle, nativeMessage);
                        } else {
                            qWarning() << "macOS notification authorization was denied";
                        }
                    }];
                } else {
                    qWarning() << "macOS notifications are not authorized; status:"
                               << static_cast<int>(settings.authorizationStatus);
                }
            }];

        return true;
    }

    return false;
}
