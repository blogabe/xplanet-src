#ifndef DISPLAYMSWIN_H
#define DISPLAYMSWIN_H

#include "DisplayBase.h"

class DisplayMSWin : public DisplayBase
{
public:
    DisplayMSWin(const int tr);
    virtual ~DisplayMSWin();

    bool renderImage(PlanetProperties *planetProperties[]);

    std::string TmpDir();
};

#endif
