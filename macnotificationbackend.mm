#include "macnotificationbackend.h"

#include <QByteArray>
#include <QDebug>

#import <UserNotifications/UserNotifications.h>

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
    const QByteArray utf8 = text.toUtf8();
    return [NSString stringWithUTF8String:utf8.constData()];
}

PlanetaryNotificationDelegate *notificationDelegate()
{
    static PlanetaryNotificationDelegate *delegate =
        [[PlanetaryNotificationDelegate alloc] init];
    return delegate;
}

void deliverNotification(UNUserNotificationCenter *center,
                         NSString *title,
                         NSString *message)
{
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
