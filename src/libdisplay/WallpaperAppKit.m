#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include <stdio.h>

int SetWallpaperWithAppKit(const char *file, char *failure, size_t capacity)
{
    @autoreleasepool
    {
        NSApplication *application = [NSApplication sharedApplication];
        [application setActivationPolicy:NSApplicationActivationPolicyProhibited];

        NSScreen *display = nil;
        for (NSScreen *screen in [NSScreen screens])
        {
            NSNumber *number = [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
            if ([number unsignedIntValue] == CGMainDisplayID())
            {
                display = screen;
                break;
            }
        }

        if (display == nil)
        {
            snprintf(failure, capacity, "no main display in the current GUI session");
            return 0;
        }

        NSURL *url = [NSURL fileURLWithFileSystemRepresentation:file
                                                    isDirectory:NO
                                                  relativeToURL:nil];
        NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
        NSDictionary *options = [workspace desktopImageOptionsForScreen:display];
        NSError *error = nil;
        if ([workspace setDesktopImageURL:url forScreen:display
                                  options:options ?: @{} error:&error])
            return 1;

        const char *detail = [[error localizedDescription] UTF8String];
        snprintf(failure, capacity, "%s", detail ? detail : "request rejected");
        return 0;
    }
}
