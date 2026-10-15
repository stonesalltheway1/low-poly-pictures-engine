/***************************************************************************

    webrecovery.cpp: Recovery copy of the open movie (Emscripten build)

    Browsers can lose an open movie in many ways: a reload, a closed tab, a
    crash. While the movie has unsaved changes, this keeps a copy of it at
    /home/web_user/.recovery/recovery.3mm (saved to browser storage with the
    rest of the user folder), refreshed at most every 20 seconds. When the
    movie has no unsaved changes, or no movie is open, the copy is removed.
    On the next visit the page turns a leftover copy into a movie in the
    user's folder ("Recovered movie ...").

***************************************************************************/
#include "studio.h"
#include <emscripten.h>
#include <filesystem>
#include <string>

ASSERTNAME

namespace
{
const char kszRecoveryDir[] = "/home/web_user/.recovery";
const char kszRecoveryFile[] = "/home/web_user/.recovery/recovery.3mm";
const uint32_t kdtsCheck = 2 * kdtsSecond;  // how often to look at the movie
const uint32_t kdtsCopy = 20 * kdtsSecond; // how often to refresh the copy while there are unsaved changes

uint32_t g_tsCheck = 0;
uint32_t g_tsCopy = 0;

void PollRecovery(void *pvUser)
{
    uint32_t ts = TsCurrentSystem();
    if (ts - g_tsCheck < kdtsCheck)
        return;
    g_tsCheck = ts;

    std::error_code ec;
    PSTDIO pstdio = vapp.Pstdio();
    PMVIE pmvie = (pstdio != pvNil) ? pstdio->Pmvie() : pvNil;
    if (pmvie == pvNil || !pmvie->FDirty())
    {
        // Nothing unsaved: no copy needed
        if (std::filesystem::exists(kszRecoveryFile, ec))
            std::filesystem::remove(kszRecoveryFile, ec);
        g_tsCopy = 0;
        return;
    }

    if (g_tsCopy != 0 && ts - g_tsCopy < kdtsCopy)
        return;

    // Don't interrupt playback, a drag or a dialog
    if (pmvie->FPlaying() || vpcex->PgobTracking() != pvNil || vapp.FInPortfolio())
        return;

    std::filesystem::create_directories(kszRecoveryDir, ec);
    STN stn;
    FNI fni;
    stn.SetSz(kszRecoveryFile);
    if (fni.FBuildFromPath(&stn) && pmvie->FWriteRecoveryCopy(&fni))
        g_tsCopy = ts;
}
} // namespace

void WebRecoveryInit(void)
{
    AddPollProc(PollRecovery, pvNil);
}

// For the page: does the open movie have unsaved changes? (warn before the tab closes)
extern "C" EMSCRIPTEN_KEEPALIVE int lpp_movie_dirty(void)
{
    PSTDIO pstdio = vapp.Pstdio();
    PMVIE pmvie = (pstdio != pvNil) ? pstdio->Pmvie() : pvNil;
    return (pmvie != pvNil && pmvie->FDirty()) ? 1 : 0;
}
