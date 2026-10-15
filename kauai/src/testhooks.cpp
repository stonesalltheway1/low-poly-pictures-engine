/***************************************************************************
    Test automation hooks (Low Poly Pictures). See testhooks.h.
***************************************************************************/
#include "frame.h"
#include "testhooks.h"
ASSERTNAME

PFNTESTTICK vpfnTestTick = pvNil;

// Marks injected mouse events so the filter can drop real ones while a test drives input
static const Uint32 kmidTest = 0x4C5050; // "LPP" (SDL2 mouse event 'which' field)

static bool _fInputActive = fFalse;
static uint8_t _rgbKeyHeld[SDL_NUM_SCANCODES];  // keys held by the test
static uint8_t _rgbKeyState[SDL_NUM_SCANCODES]; // SDL's state + held keys
static int _xpWin = 0, _ypWin = 0; // virtual mouse, window coordinates
static bool _fDown = fFalse;

static SDL_Window *_PwndApp(void)
{
    return (SDL_Window *)vwig.hwndApp;
}

static int SDLCALL _TestEventFilter(void *pv, SDL_Event *pevt)
{
    if (!_fInputActive)
        return 1;
    switch (pevt->type)
    {
    case SDL_MOUSEMOTION:
        return pevt->motion.which == kmidTest;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        return pevt->button.which == kmidTest;
    case SDL_MOUSEWHEEL:
        return 0;
    default:
        return 1;
    }
}

uint32_t KauaiGetMouseState(int *pxp, int *pyp)
{
    if (!_fInputActive)
        return SDL_GetMouseState(pxp, pyp);

    if (pxp != pvNil)
        *pxp = _xpWin;
    if (pyp != pvNil)
        *pyp = _ypWin;
    return _fDown ? SDL_BUTTON(SDL_BUTTON_LEFT) : 0;
}

const uint8_t *KauaiGetKeyboardState(void)
{
    int ckey = 0;
    const uint8_t *prgbSdl = SDL_GetKeyboardState(&ckey);
    for (int isc = 0; isc < SDL_NUM_SCANCODES; isc++)
        _rgbKeyState[isc] = (prgbSdl != pvNil && isc < ckey && prgbSdl[isc]) || _rgbKeyHeld[isc];
    return _rgbKeyState;
}

bool FTestInputActive(void)
{
    return _fInputActive;
}

void TestSetInputActive(bool fActive)
{
    if (fActive == _fInputActive)
        return;
    if (fActive)
    {
        // Start from wherever the real mouse is
        SDL_GetMouseState(&_xpWin, &_ypWin);
        _fDown = fFalse;
        SDL_SetEventFilter(_TestEventFilter, pvNil);
    }
    else
    {
        SDL_SetEventFilter(pvNil, pvNil);
    }
    _fInputActive = fActive;
}

static void _LogicalToWindow(int32_t xp, int32_t yp, int *pxpWin, int *pypWin)
{
    SDL_Renderer *rdr = SDL_GetRenderer(_PwndApp());
    if (rdr != pvNil)
        SDL_RenderLogicalToWindow(rdr, (float)xp, (float)yp, pxpWin, pypWin);
    else
    {
        *pxpWin = xp;
        *pypWin = yp;
    }
}

static void _WindowToLogical(int xpWin, int ypWin, int32_t *pxp, int32_t *pyp)
{
    SDL_Renderer *rdr = SDL_GetRenderer(_PwndApp());
    float flx = (float)xpWin, fly = (float)ypWin;
    if (rdr != pvNil)
        SDL_RenderWindowToLogical(rdr, xpWin, ypWin, &flx, &fly);
    *pxp = (int32_t)flx;
    *pyp = (int32_t)fly;
}

// Push a motion event to the new window position. SDL's renderer event watcher converts window coordinates to
// logical ones, exactly as for real input.
static void _PushMotion(int xpWin, int ypWin)
{
    SDL_Event evt;
    ClearPb(&evt, SIZEOF(evt));
    evt.type = SDL_MOUSEMOTION;
    evt.motion.timestamp = SDL_GetTicks();
    evt.motion.windowID = SDL_GetWindowID(_PwndApp());
    evt.motion.which = kmidTest;
    evt.motion.state = _fDown ? SDL_BUTTON_LMASK : 0;
    evt.motion.x = xpWin;
    evt.motion.y = ypWin;
    evt.motion.xrel = xpWin - _xpWin;
    evt.motion.yrel = ypWin - _ypWin;
    _xpWin = xpWin;
    _ypWin = ypWin;
    SDL_PushEvent(&evt);
}

void TestMouseTo(int32_t xpLogical, int32_t ypLogical)
{
    int xpWin, ypWin;
    _LogicalToWindow(xpLogical, ypLogical, &xpWin, &ypWin);
    _PushMotion(xpWin, ypWin);
}

void TestMouseBy(int32_t dxp, int32_t dyp)
{
    int32_t xp, yp;
    _WindowToLogical(_xpWin, _ypWin, &xp, &yp);
    TestMouseTo(xp + dxp, yp + dyp);
}

void TestMouseButton(bool fDown)
{
    SDL_Event evt;
    ClearPb(&evt, SIZEOF(evt));
    evt.type = fDown ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    evt.button.timestamp = SDL_GetTicks();
    evt.button.windowID = SDL_GetWindowID(_PwndApp());
    evt.button.which = kmidTest;
    evt.button.button = SDL_BUTTON_LEFT;
    evt.button.state = fDown ? SDL_PRESSED : SDL_RELEASED;
    evt.button.clicks = 1;
    evt.button.x = _xpWin;
    evt.button.y = _ypWin;
    _fDown = fDown;
    SDL_PushEvent(&evt);
}

void TestGetMouse(int32_t *pxpLogical, int32_t *pypLogical, bool *pfDown)
{
    _WindowToLogical(_xpWin, _ypWin, pxpLogical, pypLogical);
    *pfDown = _fDown;
}

void TestWarpVirtual(int xpWindow, int ypWindow)
{
    // The app "moved" the cursor (it does this during drags); later relative moves continue from here
    _xpWin = xpWindow;
    _ypWin = ypWindow;
}

bool FTestKey(const char *pszKey, bool fDown)
{
    SDL_Keycode key = SDL_GetKeyFromName(pszKey);
    if (key == SDLK_UNKNOWN)
        return fFalse;

    SDL_Event evt;
    ClearPb(&evt, SIZEOF(evt));
    evt.type = fDown ? SDL_KEYDOWN : SDL_KEYUP;
    evt.key.timestamp = SDL_GetTicks();
    evt.key.windowID = SDL_GetWindowID(_PwndApp());
    evt.key.state = fDown ? SDL_PRESSED : SDL_RELEASED;
    evt.key.keysym.sym = key;
    evt.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    evt.key.keysym.mod = SDL_GetModState();
    SDL_PushEvent(&evt);
    if (evt.key.keysym.scancode > SDL_SCANCODE_UNKNOWN && evt.key.keysym.scancode < SDL_NUM_SCANCODES)
        _rgbKeyHeld[evt.key.keysym.scancode] = fDown ? 1 : 0;
    return fTrue;
}

void TestTypeText(const char *pszUtf8)
{
    // One SDL_TEXTINPUT event per character, as typing produces
    for (const char *pch = pszUtf8; *pch != 0;)
    {
        int cch = 1;
        while ((pch[cch] & 0xC0) == 0x80)
            cch++; // UTF-8 continuation bytes
        SDL_Event evt;
        ClearPb(&evt, SIZEOF(evt));
        evt.type = SDL_TEXTINPUT;
        evt.text.timestamp = SDL_GetTicks();
        evt.text.windowID = SDL_GetWindowID(_PwndApp());
        CopyPb(pch, evt.text.text, cch);
        SDL_PushEvent(&evt);
        pch += cch;
    }
}

void TestSetMods(uint32_t grfkmod)
{
    SDL_SetModState((SDL_Keymod)grfkmod);
}

bool FTestDumpScreen(const char *pszPath)
{
    PGOB pgobScreen = GOB::PgobScreen();
    if (pgobScreen == pvNil || pgobScreen->Pgpt() == pvNil)
        return fFalse;

    // Draw anything still pending so the dump matches what's on screen
    vpappb->UpdateMarked();

    STN stn;
    stn.SetSz(pszPath);
    pgobScreen->Pgpt()->DumpBitmap(&stn);
    return fTrue;
}
