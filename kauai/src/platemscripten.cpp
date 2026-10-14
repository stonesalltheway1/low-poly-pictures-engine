/*
 * Emscripten (WebAssembly) platform functions
 *
 * Based on platlinux.cpp. The browser has no /proc, no login user and no real home directory, so paths
 * point into Emscripten's virtual filesystem:
 *   /app                   - application + content (preloaded)
 *   /home/web_user         - documents (user movies)
 *   /home/web_user/.config - settings
 */

#include "platform.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <emscripten.h>

static const char kszExecutablePath[] = "/app/3dmovie";
static const char kszHomeDir[] = "/home/web_user";

static bool FCopySz(const char *pszSrc, char *psz, int32_t cchMax)
{
    int written = snprintf(psz, cchMax, "%s", pszSrc);
    return written > 0 && written < cchMax;
}

void Debugger(void)
{
    emscripten_debugger();
}

/***************************************************************************
    Universal scalable application clock and other time stuff
***************************************************************************/

const uint32_t kdtsSecond = 1000;

uint32_t TsCurrentSystem(void)
{
    struct timespec ts;
    static int64_t clockzero_ns;
    int64_t clock_ns;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    clock_ns = ts.tv_sec * 1000000000LL + ts.tv_nsec;

    if (clockzero_ns == 0)
    {
        clockzero_ns = clock_ns - 1000000000LL;
    }

    return (clock_ns - clockzero_ns) / 1000000;
}

uint32_t DtsCaret(void)
{
    return 1000; /* 1s for now */
}

/****************************************
    Current executable name
****************************************/
void GetExecutableName(char *psz, int cchMax)
{
    FCopySz(kszExecutablePath, psz, cchMax);
}

/****************************************
    Get environment variable
****************************************/
uint32_t GetEnvironmentVariable(const char *pcszName, char *pszValue, uint32_t cchMax)
{
    char *psz = getenv(pcszName);
    size_t ilen;

    if (psz == NULL)
    {
        return 0;
    }

    ilen = strlen(psz);
    if (ilen > cchMax)
    {
        ilen = cchMax;
    }

    memcpy(pszValue, psz, ilen);
    pszValue[ilen] = '\0';
    return ilen;
}

/****************************************
    Current username
****************************************/
bool GetUserName(char *psz, int cchMax)
{
    return FCopySz("Director", psz, cchMax);
}

bool FGetAppConfigDir(char *psz, int32_t cchMax)
{
    int written = snprintf(psz, cchMax, "%s/.config", kszHomeDir);
    if (written <= 0 || written >= cchMax)
        return false;

    // The virtual filesystem starts with only /home/web_user; callers create subfolders with a non-recursive
    // create_directory, so make sure the config folder itself exists.
    mkdir(kszHomeDir, 0777);
    mkdir(psz, 0777);
    return true;
}

bool FGetDocumentsDir(char *psz, int32_t cchMax)
{
    return FCopySz(kszHomeDir, psz, cchMax);
}

bool FGetResourcesDir(char *psz, int32_t cchMax)
{
    // not used
    return false;
}
