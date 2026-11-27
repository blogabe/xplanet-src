#ifndef DISPLAYMACAQUA_H
#define DISPLAYMACAQUA_H

#include "DisplayBase.h"

class DisplayMacAqua : public DisplayBase
{
public:
    DisplayMacAqua(const int tr);
    virtual ~DisplayMacAqua();

    bool renderImage(PlanetProperties *planetProperties[]);

private:
};

#endif
