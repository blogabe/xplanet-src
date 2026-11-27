#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <unistd.h>
using namespace std;

#include "keywords.h"
#include "Options.h"
#include "PlanetProperties.h"
#include "xpUtil.h"

#include "DisplayMacAqua.h"

#include "libimage/Image.h"

#include <Carbon/Carbon.h>

extern "C" {
#ifdef HAVE_DESKTOP_LAYER
    int write_png(FILE *, int, int, unsigned char *, unsigned char *);
#endif
    OSStatus SetDesktopPictureFromCharString(const char *file);
    int SetWallpaperWithAppKit(const char *file, char *failure, size_t capacity);
}

DisplayMacAqua::DisplayMacAqua(const int tr) : DisplayBase(tr)
{
    Options *options = Options::getInstance();
    fullWidth_ = options->FramePipe() ? options->FrameWidth()
        : static_cast<int>(CGDisplayPixelsWide(kCGDirectMainDisplay));
    fullHeight_ = options->FramePipe() ? options->FrameHeight()
        : static_cast<int>(CGDisplayPixelsHigh(kCGDirectMainDisplay));

    if (fullWidth_ == 0 || fullHeight_ == 0)
        xpExit("Can't set Aqua display\n", __FILE__, __LINE__);

    switch (options->DisplayMode())
    {
    case WINDOW:
        xpWarn("-window option not supported for Aqua.\n", 
               __FILE__, __LINE__);
        // fall through
    case ROOT:
        if (options->GeometrySelected())
        {
            width_ = options->getWidth();
            height_ = options->getHeight();
        }
        else
        {
            width_ = fullWidth_;
            height_ = fullHeight_;
        }
        
        break;
    }
  
    if (!options->CenterSelected())
    {
        if (width_ % 2 == 0)
            options->CenterX(width_/2 - 0.5);
        else
            options->CenterX(width_/2);

        if (height_ % 2 == 0)
            options->CenterY(height_/2 - 0.5);
        else
            options->CenterY(height_/2);
    }

    if (options->FramePipe()
        && (width_ <= 0 || height_ <= 0 || width_ > 32768 || height_ > 32768
            || static_cast<long long>(width_) * height_ > 24000000
            || options->getWindowX() < -32768 || options->getWindowX() > 32768
            || options->getWindowY() < -32768 || options->getWindowY() > 32768))
        xpExit("Desktop frame geometry exceeds the supported limits.\n", __FILE__, __LINE__);
    allocateRGBData();

}

DisplayMacAqua::~DisplayMacAqua()
{
}

namespace
{
void
removeWallpaperDirectory(const string &path)
{
    if (path.empty()) return;
    unlink((path + "/wallpaper.png").c_str());
    rmdir(path.c_str());
}

string lastFailedWallpaper;
}

// This was pretty much written by trial and error once I found
// DesktopPicture.m on developer.apple.com
bool
DisplayMacAqua::renderImage(PlanetProperties *planetProperties[])
{
    drawLabel(planetProperties);
    Options *options = Options::getInstance();
#ifdef HAVE_DESKTOP_LAYER
    if (FILE *stream = options->FramePipe())
    {
        if (options->GeometrySelected()) PlaceImageOnRoot();
        if (!write_png(stream, fullWidth_, fullHeight_, rgb_data, nullptr)
            || fflush(stream) != 0 || ferror(stream))
            xpExit("Cannot write the desktop frame.\n", __FILE__, __LINE__);
        return true;
    }
#endif

    // Setting the desktop picture doesn't seem to work if you give it
    // the same filename over and over again.
    const string pattern = TmpDir() + "/Xplanet.XXXXXX";
    vector<char> path(pattern.begin(), pattern.end());
    path.push_back('\0');
    if (mkdtemp(&path[0]) == NULL)
        xpExit("Cannot create a private wallpaper staging directory.\n", __FILE__, __LINE__);
    const string directory(&path[0]);
    const string output = directory + "/wallpaper.png";

    if (options->GeometrySelected()) PlaceImageOnRoot();

    Image i(fullWidth_, fullHeight_, rgb_data, alpha);
    if (!i.Write(output.c_str()))
    {
        removeWallpaperDirectory(directory);
        ostringstream errStr;
        errStr << "Can't create image file " << output << "\n";
        xpExit(errStr.str(), __FILE__, __LINE__);
    }

    if (options->Verbosity() > 1)
    {
        ostringstream msg;
        msg << "Created image file " << output << "\n";
        xpMsg(msg.str(), __FILE__, __LINE__);
    }

    const bool useFinder = options->FinderWallpaper();
    char failure[512] = {};
    const OSStatus status = useFinder
        ? SetDesktopPictureFromCharString(output.c_str()) : noErr;
    const bool published = useFinder ? status == noErr
        : SetWallpaperWithAppKit(output.c_str(), failure, sizeof(failure)) != 0;
    if (!published)
    {
        removeWallpaperDirectory(lastFailedWallpaper);
        lastFailedWallpaper = directory;
        ostringstream errStr;
        errStr << "Failed to set desktop (";
        if (useFinder)
            errStr << "Finder error " << status;
        else
            errStr << "AppKit: " << failure;
        errStr << "); image retained at " << output << "\n";
        if (status == errAEEventNotPermitted)
        {
            errStr << "Finder Automation access was denied. In System Settings > "
                   << "Privacy & Security > Automation, allow Xplanet or the app "
                   << "that launched it to control Finder.\n"
                   << "Recovery: https://github.com/blogabe/Xplanet/blob/main/"
                   << "docs/management.md#finder-automation-permission\n";
        }
        xpWarn(errStr.str(), __FILE__, __LINE__);
        return false;
    }

    // Acceptance precedes the visible update. Leave time for macOS to import
    // the image before removing our unique staging file.
    sleep(1);
    removeWallpaperDirectory(lastFailedWallpaper);
    lastFailedWallpaper.clear();
    if (!options->SaveDesktopFile())
        removeWallpaperDirectory(directory);
    return true;
}
