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

// Export: stop playback, close help balloons and go to the first frame of the first scene (from the app's loop)
bool g_fRewindWant = false, g_fRewindPoll = false;
void PollRewind(void *)
{
    if (!g_fRewindWant)
        return;
    g_fRewindWant = false;
    PMVIE pmvie = PmvieCur();
    if (pmvie == pvNil || pmvie->Cscen() == 0)
        return;
    if (pmvie->FPlaying())
        pmvie->Play(); // stops it
    CloseHelpBalloons();
    if (pmvie->FSwitchScen(0) && pmvie->Pscen() != pvNil)
        pmvie->Pscen()->FGotoFrm(pmvie->Pscen()->NfrmFirst());
    pmvie->InvalViews();
}
} // namespace

// Get the open movie ready for export: the app's loop stops any playback, closes help balloons and goes to the first
// frame of the first scene (PollRewind) before it next handles a command. Returns JSON:
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
    // Stopping and rewinding can load content, which may suspend the app (Asyncify): never inside a call from the
    // page. The app's own loop does it (PollRewind), well before the page calls lpp_export_play.
    g_fRewindWant = true;
    if (!g_fRewindPoll)
    {
        AddPollProc(PollRewind, pvNil);
        g_fRewindPoll = true;
    }

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

// The page sizes the canvas's pixels to its size on screen (times the device pixel ratio). The app keeps its
// 640x480 logical size; GPT::Flip scales to the window ("sharp bilinear"), and mouse input maps back to 640x480.
// The page may call this while the app is paused in an Asyncify sleep, and presenting a frame sleeps too (SDL's
// swap yields to the browser): so only record the size here and apply it from the app's own loop.
namespace
{
int g_dxpDisplayWant = 0, g_dypDisplayWant = 0;
bool g_fDisplayPoll = false;

void PollDisplaySize(void *pvUser)
{
    SDL_Window *pwnd = (SDL_Window *)vwig.hwndApp;
    int dxpCur = 0, dypCur = 0;
    if (pwnd == pvNil || g_dxpDisplayWant == 0)
        return;
    SDL_GetWindowSize(pwnd, &dxpCur, &dypCur);
    if (dxpCur != g_dxpDisplayWant || dypCur != g_dypDisplayWant)
    {
        SDL_SetWindowSize(pwnd, g_dxpDisplayWant, g_dypDisplayWant);
        GPT::Flush(); // resizing clears the canvas: present the current frame again
    }
}
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void lpp_set_display_size(int dxp, int dyp)
{
    if (dxp < 64 || dyp < 48)
        return;
    g_dxpDisplayWant = dxp;
    g_dypDisplayWant = dyp;
    if (!g_fDisplayPoll)
    {
        AddPollProc(PollDisplaySize, pvNil);
        g_fDisplayPoll = true;
    }
}

// The current 256-colour palette as 1536 hex digits (rrggbb per entry), e.g. for GIF export (the movie's own
// colours, no quantising). The string stays valid until the next call.
extern "C" EMSCRIPTEN_KEEPALIVE const char *lpp_palette(void)
{
    static char s_szHex[256 * 6 + 1];
    static const char kszDigits[] = "0123456789abcdef";
    uint8_t rgb[256 * 3];
    ClearPb(rgb, SIZEOF(rgb));
    PGL pglclr = GPT::PglclrGetPalette();
    if (pglclr != pvNil)
    {
        for (int32_t i = 0; i < pglclr->IvMac() && i < 256; i++)
        {
            CLR clr;
            pglclr->Get(i, &clr);
            rgb[3 * i] = clr.bRed;
            rgb[3 * i + 1] = clr.bGreen;
            rgb[3 * i + 2] = clr.bBlue;
        }
        ReleasePpo(&pglclr);
    }
    for (int32_t ib = 0; ib < 256 * 3; ib++)
    {
        s_szHex[2 * ib] = kszDigits[rgb[ib] >> 4];
        s_szHex[2 * ib + 1] = kszDigits[rgb[ib] & 15];
    }
    s_szHex[256 * 6] = 0;
    return s_szHex;
}
