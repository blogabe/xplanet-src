#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
using namespace std;

#include "keywords.h"
#include "Options.h"
#include "PlanetProperties.h"
#include "xpUtil.h"

#include "DisplayMacAqua.h"

#include "libimage/Image.h"

#include <Carbon/Carbon.h>

extern "C" {
    OSStatus SetDesktopPictureFromCharString(const char *file);
}

DisplayMacAqua::DisplayMacAqua(const int tr) : DisplayBase(tr)
{
    fullWidth_ = static_cast<int> (CGDisplayPixelsWide(kCGDirectMainDisplay));
    fullHeight_ = static_cast<int> (CGDisplayPixelsHigh(kCGDirectMainDisplay));

    if (fullWidth_ == 0 || fullHeight_ == 0)
        xpExit("Can't set Aqua display\n", __FILE__, __LINE__);

    Options *options = Options::getInstance();
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

    allocateRGBData();

}

DisplayMacAqua::~DisplayMacAqua()
{
}

// This was pretty much written by trial and error once I found
// DesktopPicture.m on developer.apple.com
void 
DisplayMacAqua::renderImage(PlanetProperties *planetProperties[])
{
    drawLabel(planetProperties);

    // Setting the desktop picture doesn't seem to work if you give it
    // the same filename over and over again.
    char templateFile[16];
    strncpy(templateFile, "Xplanet.XXXXXX", 16);
    char *tmpFile = mktemp(templateFile);

    ostringstream outputStream;
    outputStream << TmpDir() << "/" << tmpFile << ".png";

    Options *options = Options::getInstance();
    if (options->GeometrySelected()) PlaceImageOnRoot();

    Image i(fullWidth_, fullHeight_, rgb_data, alpha);
    if (!i.Write(outputStream.str().c_str()))
    {
        ostringstream errStr;
        errStr << "Can't create image file " << outputStream.str() << "\n";
        xpExit(errStr.str(), __FILE__, __LINE__);
    }

    if (options->Verbosity() > 1)
    {
        ostringstream msg;
        msg << "Created image file " << outputStream.str() << "\n";
        xpMsg(msg.str(), __FILE__, __LINE__);
    }

    OSStatus status = SetDesktopPictureFromCharString(outputStream.str().c_str());
    if (status != noErr)
    {
        ostringstream errStr;
        errStr << "Failed to set desktop (macOS error " << status
               << "); image retained at " << outputStream.str() << "\n";
        if (status == errAEEventNotPermitted)
        {
            errStr << "Finder Automation access was denied. In System Settings > "
                   << "Privacy & Security > Automation, allow Xplanet or the app "
                   << "that launched it to control Finder.\n"
                   << "Recovery: https://github.com/blogabe/Xplanet/blob/main/"
                   << "docs/management.md#finder-automation-permission\n";
        }
        xpWarn(errStr.str(), __FILE__, __LINE__);
        return;
    }

    // Finder has acknowledged the request. Retain the existing grace period
    // for the wallpaper display before removing the temporary image.
    sleep(1);
    if (!options->SaveDesktopFile())
        unlinkFile(outputStream.str().c_str());
}
