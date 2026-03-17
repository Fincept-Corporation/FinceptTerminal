// macOS notification bridge — Obj-C++ implementation
// Uses NSUserNotificationCenter (macOS 10.8+)

#ifdef __APPLE__

#import <Foundation/Foundation.h>

#include <string>

namespace fincept::core::notify::platform {

bool send_mac(const std::string& title, const std::string& body, int level) {
    @autoreleasepool {
        @try {
            // NSUserNotificationCenter — deprecated but works on all macOS versions
            // UNUserNotificationCenter requires app bundle + entitlements
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wdeprecated-declarations"

            NSUserNotification* notification = [[NSUserNotification alloc] init];
            notification.title = [NSString stringWithUTF8String:title.c_str()];
            notification.informativeText = [NSString stringWithUTF8String:body.c_str()];
            notification.soundName = NSUserNotificationDefaultSoundName;

            // Set subtitle based on level
            switch (level) {
                case 0: notification.subtitle = @"Info"; break;
                case 1: notification.subtitle = @"Success"; break;
                case 2: notification.subtitle = @"Warning"; break;
                case 3: notification.subtitle = @"Error"; break;
            }

            [[NSUserNotificationCenter defaultUserNotificationCenter]
                deliverNotification:notification];

            #pragma clang diagnostic pop

            return true;
        } @catch (NSException*) {
            return false;
        }
    }
}

} // namespace fincept::core::notify::platform

#endif // __APPLE__
