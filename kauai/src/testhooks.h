/***************************************************************************
    Test automation hooks (Low Poly Pictures).

    Lets a test driver (native: command files, web: exported functions) inject
    mouse/keyboard input and dump frames, identically on desktop and in the
    browser. Inactive (zero behaviour change) until a driver turns it on.

    While input override is active:
      - real mouse events from the OS/browser are filtered out
      - KauaiGetMouseState() returns the injected (virtual) mouse state
      - APPB::PositionCurs() moves the virtual cursor instead of the real one
***************************************************************************/
#ifndef TESTHOOKS_H
#define TESTHOOKS_H

#include <stdint.h>

// Called once per main-loop iteration from APPB::TopOfLoop when set
typedef void (*PFNTESTTICK)(void);
extern PFNTESTTICK vpfnTestTick;

// SDL_GetMouseState replacement used by the Kauai SDL layer (window coordinates)
uint32_t KauaiGetMouseState(int *pxp, int *pyp);

// SDL_GetKeyboardState replacement: also reports keys a test is holding down (injected key events don't
// update SDL's own key state, which the studio polls for Up/Down during drags)
const uint8_t *KauaiGetKeyboardState(void);

// Input override
bool FTestInputActive(void);
void TestSetInputActive(bool fActive);
void TestMouseTo(int32_t xpLogical, int32_t ypLogical); // absolute move (logical 640x480 coordinates)
void TestMouseBy(int32_t dxp, int32_t dyp);             // relative move from the current virtual position
void TestMouseButton(bool fDown);                       // press/release the left button at the virtual position
void TestGetMouse(int32_t *pxpLogical, int32_t *pypLogical, bool *pfDown);
void TestWarpVirtual(int xpWindow, int ypWindow); // used by APPB::PositionCurs while input override is active
bool FTestKey(const char *pszKey, bool fDown);    // SDL key name, e.g. "Return", "Escape", "Left", "Z"
void TestSetMods(uint32_t grfkmod);

// Save the current screen (8-bit, with palette) as a BMP. Flushes pending drawing first.
bool FTestDumpScreen(const char *pszPath);

#endif // TESTHOOKS_H
