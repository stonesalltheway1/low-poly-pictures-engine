/***************************************************************************

    webexport.cpp: Movie export helpers for the page (Emscripten build)

    The page exports a movie as a video by playing it once from the start
    while it records the movie view (cropped from the canvas) and the app's
    sound (web/shell.html). These functions prepare the movie, start
    playback the way the Play button does, and report when it has stopped.

***************************************************************************/
#include "studio.h"
#include <emscripten.h>
#include <sstream>
#include <string>
#include <vector>

ASSERTNAME

namespace
{
PMVIE PmvieCur(void)
{
    PSTDIO pstdio = vapp.Pstdio();
    return (pstdio != pvNil) ? pstdio->Pmvie() : pvNil;
}

std::string JsonString(PSTN pstn)
{
    U8SZ u8sz;
    pstn->GetUtf8Sz(u8sz);
    std::string s = "\"";
    for (const char *pch = (const char *)u8sz; *pch != 0; pch++)
    {
        unsigned char ch = (unsigned char)*pch;
        if (ch == '"' || ch == '\\')
        {
            s += '\\';
            s += (char)ch;
        }
        else if (ch < 0x20)
            s += ' ';
        else
            s += (char)ch;
    }
    return s + "\"";
}

// Help balloons (hints) can sit over the movie view; they're not part of the movie
void CloseHelpBalloons(void)
{
    std::vector<PGOB> rgpgob;
    PGOB pgobScreen = GOB::PgobScreen();
    if (pgobScreen == pvNil)
        return;
    GTE gte;
    PGOB pgob;
    uint32_t grfgte;
    gte.Init(pgobScreen, fgteNil);
    while (gte.FNextGob(&pgob, &grfgte, fgteNil))
    {
        if ((grfgte & fgtePre) && pgob->FIs(kclsHBAL))
            rgpgob.push_back(pgob);
    }
    for (PGOB pgobBalloon : rgpgob)
        pgobBalloon->Release();
}
} // namespace

// Stop any playback, close help balloons and go to the first frame of the first scene. Returns JSON:
// {"ok":true,"title":...,"scenes":N,"view":[left,top,right,bottom]} (view in canvas pixels), or {"ok":false,...}
extern "C" EMSCRIPTEN_KEEPALIVE const char *lpp_export_prepare(void)
{
    static std::string s_json;
    std::ostringstream js;
    PMVIE pmvie = PmvieCur();
    PMVU pmvu = (pmvie != pvNil) ? pmvie->PmvuCur() : pvNil;

    if (pmvie == pvNil || pmvu == pvNil || pmvie->Cscen() == 0)
    {
        s_json = "{\"ok\":false,\"error\":\"no movie\"}";
        return s_json.c_str();
    }
    if (pmvie->FPlaying())
        pmvie->Play(); // stops it
    CloseHelpBalloons();
    if (pmvie->FSwitchScen(0) && pmvie->Pscen() != pvNil)
        pmvie->Pscen()->FGotoFrm(pmvie->Pscen()->NfrmFirst());
    pmvie->InvalViews();

    RC rc;
    STN stnTitle;
    pmvu->GetRc(&rc, cooHwnd);
    pmvie->GetName(&stnTitle);
    js << "{\"ok\":true,\"title\":" << JsonString(&stnTitle) << ",\"scenes\":" << pmvie->Cscen() << ",\"view\":["
       << rc.xpLeft << "," << rc.ypTop << "," << rc.xpRight << "," << rc.ypBottom << "]}";
    s_json = js.str();
    return s_json.c_str();
}

// Start (or stop) playback like the studio's Play button
extern "C" EMSCRIPTEN_KEEPALIVE int lpp_export_play(void)
{
    PSTDIO pstdio = vapp.Pstdio();
    if (pstdio == pvNil || PmvieCur() == pvNil)
        return 0;
    vpcex->EnqueueCid(cidPlay, pstdio);
    return 1;
}

// Is the open movie playing?
extern "C" EMSCRIPTEN_KEEPALIVE int lpp_movie_playing(void)
{
    PMVIE pmvie = PmvieCur();
    return (pmvie != pvNil && pmvie->FPlaying()) ? 1 : 0;
}
