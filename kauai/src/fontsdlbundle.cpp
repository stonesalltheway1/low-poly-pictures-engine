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

// Get the first TrueType font file in the font folder
static bool FGetFirstFont(PFNI pfniFontDir, PFNI pfniDefaultFont)
{
    AssertPo(pfniFontDir, 0);
    AssertPo(pfniDefaultFont, 0);

    FTG ftgTtf = kftgTtf;
    FNE fne;

    // Return the first TrueType font file in the font directory
    if (!fne.FInit(pfniFontDir, &ftgTtf, 1, 0))
        return fFalse;

    return fne.FNextFni(pfniDefaultFont);
}

bool NTL::_FLoadFontTable()
{
    AssertThis(0);
    Assert(fInitTtf, "TTF_Init should have been called");

    bool fRet = fFalse;
    PGL pglsdlfont = pvNil;
    FNI fniFontDir, fniDefaultFont;

    // Fonts are loaded from the 'fonts' subdirectory in the resources directory.
    AssertDo(fniFontDir.FGetResourcesDir(), "Could not find resources directory");
    STN stnFontDir = PszLit("fonts");
    if (!fniFontDir.FDownDir(&stnFontDir, ffniMoveToDir))
    {
        Bug("Could not find fonts directory");
        goto LFail;
    }

    if (!FAddAllFontsInDir(&fniFontDir))
    {
        Bug("Could not add fonts from system fonts directory");
        goto LFail;
    }

    // Ensure we have at least one font
    if (_pgst->IvMac() == 0)
    {
        goto LFail;
    }

    // Add a default font
    if (FGetFirstFont(&fniFontDir, &fniDefaultFont))
    {
        int32_t onnSystem = 0;
        STN stnT = PszLit("System Default");
        if (FAddFontFile(&fniDefaultFont, &stnT, &onnSystem))
        {
            _onnSystem = onnSystem;
        }
    }
    else
    {
        Bug("Could not find a default font file");
        goto LFail;
    }

    // The original content asks for fonts by name (e.g. "Comic Sans MS"), and those can't be redistributed.
    // If a requested font isn't bundled, register an open-licensed substitute's files under that name.
    {
        static const struct
        {
            PCSZ pszFont;
            PCSZ pszSubstituteFilePrefix;
        } rgsub[] = {
            {PszLit("Comic Sans MS"), PszLit("ComicNeue-")}, // Comic Neue, SIL Open Font License
            // Liberation (SIL OFL): metric-compatible with the Windows core fonts that movies commonly use
            {PszLit("Arial"), PszLit("LiberationSans-")},
            {PszLit("Times New Roman"), PszLit("LiberationSerif-")},
            {PszLit("Courier New"), PszLit("LiberationMono-")},
        };

        for (int32_t isub = 0; isub < (int32_t)CvFromRgv(rgsub); isub++)
        {
            STN stnFont = rgsub[isub].pszFont;
            int32_t onnT;
            if (FGetOnn(&stnFont, &onnT) && onnT != _onnSystem)
                continue; // the real font is available

            FTG ftgTtf = kftgTtf;
            FNE fne;
            FNI fniFont;
            if (!fne.FInit(&fniFontDir, &ftgTtf, 1, 0))
                continue;

            STN stnPrefix = rgsub[isub].pszSubstituteFilePrefix;
            while (fne.FNextFni(&fniFont))
            {
                STN stnLeaf;
                fniFont.GetLeaf(&stnLeaf);
                if (stnLeaf.Cch() >= stnPrefix.Cch() &&
                    FEqualRgb(stnLeaf.Psz(), stnPrefix.Psz(), stnPrefix.Cch() * SIZEOF(achar)))
                {
                    FAddFontFile(&fniFont, &stnFont);
                }
            }
        }
    }

    fRet = fTrue;

LFail:
    return fRet;
}
