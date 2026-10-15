/***************************************************************************
    Author: Ben Stone
    Project: Kauai
    Reviewed:

    SDL font enumeration for bundled fonts

***************************************************************************/
#include "frame.h"
ASSERTNAME

#include "gfx.h"
#include "fontsdl.h"

bool NTL::_FLoadFontTable()
{
    AssertThis(0);
    Assert(fInitTtf, "TTF_Init should have been called");

    FNI fniFontDir;

    // Fonts are loaded from the 'fonts' subdirectory in the resources directory.
    AssertDo(fniFontDir.FGetResourcesDir(), "Could not find resources directory");
    STN stnFontDir = PszLit("fonts");
    if (!fniFontDir.FDownDir(&stnFontDir, ffniMoveToDir))
    {
        Bug("Could not find fonts directory");
        return fFalse;
    }

    // Bundled fonts only, with substitutes for the Windows fonts movies ask for (fontsdl.cpp)
    if (!_FLoadBundledFonts(&fniFontDir))
    {
        Bug("Could not load the bundled fonts");
        return fFalse;
    }
    return fTrue;
}
