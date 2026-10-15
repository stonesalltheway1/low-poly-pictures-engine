/***************************************************************************
    Remote files (Emscripten builds)

    Content files stay on the web server and are read on demand, like a CD:
    the first read of a 64 KB block fetches it with an HTTP Range request
    (the app is suspended with Asyncify meanwhile) and keeps it in memory.

    The build preloads a manifest, /app/remote-files.txt, with one line per
    file: "<size>\t<path relative to /app>". FInitRemoteFiles creates an
    empty placeholder for each file so directory listings and existence
    checks work; FIL reads remote files through FReadRemoteFile.

    The page can override how blocks are fetched with
    Module.lppFetchRange(path, start, length) -> Promise<Uint8Array>; by
    default they come from Module.lppContentUrl (default "content/") + path.
***************************************************************************/
#ifndef FILEREMOTE_H
#define FILEREMOTE_H

#include <stdint.h>

// Read the manifest and create placeholders. Call once at startup.
bool FInitRemoteFiles(void);

// Remote file id for a virtual filesystem path (1-based), or 0 if the file is not remote
int32_t IrfFromPath(const char *pszPath);

// Size of a remote file
int32_t CbRemoteFile(int32_t irf);

// Read from a remote file, fetching missing blocks. Fails if a block is missing and the app can't be suspended
// right now (e.g. inside the audio callback).
bool FReadRemoteFile(int32_t irf, void *pv, int32_t cb, int32_t fp);

// Fetch statistics (for tests): number of requests and bytes downloaded since startup
void GetRemoteFileStats(uint64_t *pcfetch, uint64_t *pcbFetched);

#endif // FILEREMOTE_H
