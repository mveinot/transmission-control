#include "macdefaulthandlerbackend.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QMetaObject>

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <utility>

namespace {

NSString *const TorrentTypeIdentifier = @"org.bittorrent.torrent";

bool applicationUrlIsPlanetary(NSURL *applicationUrl)
{
    if (applicationUrl == nil)
        return false;

    NSBundle *handlerBundle = [NSBundle bundleWithURL:applicationUrl];
    NSString *handlerIdentifier = handlerBundle.bundleIdentifier;
    NSString *planetaryIdentifier = NSBundle.mainBundle.bundleIdentifier;
    return handlerIdentifier != nil
           && planetaryIdentifier != nil
           && [handlerIdentifier isEqualToString:planetaryIdentifier];
}

void completeOnMainThread(
    std::function<void(const QString &error)> completion,
    NSError *error)
{
    if (!completion)
        return;

    const QString message =
        error ? QString::fromNSString(error.localizedDescription) : QString();
    QCoreApplication *application = QCoreApplication::instance();
    if (!application)
        return;

    // invokeMethod owns the functor until delivery. This is essential because
    // NSWorkspace completes after requestMacDefaultHandler has returned.
    QMetaObject::invokeMethod(
        application,
        [completion = std::move(completion), message]() {
            if (completion)
                completion(message);
        },
        Qt::QueuedConnection);
}

} // namespace

MacDefaultHandlerStatus macDefaultHandlerStatus()
{
    MacDefaultHandlerStatus status;

    if (@available(macOS 12.0, *)) {
        status.supported = true;

        NSWorkspace *workspace = NSWorkspace.sharedWorkspace;
        NSURL *magnetUrl =
            [NSURL URLWithString:@"magnet:?xt=urn:btih:planetary-handler-check"];
        status.magnetLinks =
            applicationUrlIsPlanetary(
                [workspace URLForApplicationToOpenURL:magnetUrl]);

        UTType *torrentType =
            [UTType typeWithIdentifier:TorrentTypeIdentifier];
        status.torrentFiles =
            torrentType != nil
            && applicationUrlIsPlanetary(
                [workspace URLForApplicationToOpenContentType:torrentType]);
    }

    return status;
}

void requestMacDefaultHandler(
    MacDefaultHandlerKind kind,
    const std::function<void(const QString &error)> &completion)
{
    if (@available(macOS 12.0, *)) {
        NSURL *applicationUrl = NSBundle.mainBundle.bundleURL;
        NSWorkspace *workspace = NSWorkspace.sharedWorkspace;
        // NSWorkspace retains its completion block. Capture an owning callback
        // rather than the reference parameter supplied by the Qt caller.
        const std::function<void(const QString &)> callback = completion;

        if (kind == MacDefaultHandlerKind::MagnetLinks) {
            [workspace
                setDefaultApplicationAtURL:applicationUrl
                toOpenURLsWithScheme:@"magnet"
                completionHandler:^(NSError *error) {
                    completeOnMainThread(callback, error);
                }];
            return;
        }

        UTType *torrentType =
            [UTType typeWithIdentifier:TorrentTypeIdentifier];
        if (torrentType == nil) {
            if (completion) {
                completion(QStringLiteral(
                    "macOS could not resolve the torrent file type."));
            }
            return;
        }

        [workspace
            setDefaultApplicationAtURL:applicationUrl
            toOpenContentType:torrentType
            completionHandler:^(NSError *error) {
                completeOnMainThread(callback, error);
            }];
        return;
    }

    if (completion) {
        completion(QStringLiteral(
            "Changing default handlers requires macOS 12 or later."));
    }
}
