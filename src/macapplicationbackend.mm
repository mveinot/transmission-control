#include "macapplicationbackend.h"

#import <AppKit/AppKit.h>

bool setMacApplicationDockIconVisible(bool visible)
{
    if (NSApp == nil)
        return false;

    // Accessory applications remain eligible to own windows and a status item,
    // but macOS removes their Dock icon and application menu while hidden.
    const NSApplicationActivationPolicy policy =
        visible ? NSApplicationActivationPolicyRegular
                : NSApplicationActivationPolicyAccessory;

    if ([NSApp activationPolicy] == policy)
        return true;

    return [NSApp setActivationPolicy:policy];
}
