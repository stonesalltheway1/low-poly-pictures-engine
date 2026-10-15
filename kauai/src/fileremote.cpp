/***************************************************************************
    Remote files (Emscripten builds). See fileremote.h.
***************************************************************************/
#include "fileremote.h"
#include <emscripten.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace
{
const int32_t kcbBlock = 64 * 1024;
const int32_t kcblkFetchMax = 32; // at most 2 MB per request
const char kszManifest[] = "/app/remote-files.txt";
const char kszRoot[] = "/app/";

struct RF
{
    std::string stnRel; // path relative to /app (what the server sees)
    int32_t cb;
    std::unordered_map<int32_t, std::unique_ptr<uint8_t[]>> mpiblkpb;
};

std::vector<RF> g_rgrf;
std::unordered_map<std::string, int32_t> g_mpstnirf;
uint64_t g_cfetch = 0;
uint64_t g_cbFetched = 0;
} // namespace

// Whether the app can be suspended now (not while an async operation is already in flight, e.g. inside the
// audio callback that runs during a suspension)
EM_JS(int, LppCanSuspendJs, (void), { return Asyncify.currData ? 0 : 1; });

// Fetch [fp, fp + cb) of a remote file into pbDst. Returns cb, or -1 on failure.
EM_ASYNC_JS(int, LppFetchRangeJs, (const char *pszRel, double fp, int cb, uint8_t *pbDst), {
    var rel = UTF8ToString(pszRel);
    var fetchRange = Module.lppFetchRange || async function(path, start, length)
    {
        var base = Module.lppContentUrl || 'content/';
        var url = base + path.split('/').map(encodeURIComponent).join('/');
        var resp = await fetch(url, {headers : {Range : 'bytes=' + start + '-' + (start + length - 1)}});
        if (resp.status != 206 && resp.status != 200)
            throw new Error('HTTP ' + resp.status + ' for ' + url);
        var data = new Uint8Array(await resp.arrayBuffer());
        if (resp.status == 200)
            data = data.subarray(start, start + length); // the server ignored the Range header
        return data;
    };
    var t0 = performance.now();
    for (var attempt = 0;; attempt++)
    {
        try
        {
            var data = await fetchRange(rel, fp, cb);
            if (data.length != cb)
                throw new Error('got ' + data.length + ' of ' + cb + ' bytes');
            HEAPU8.set(data, pbDst);
            // Log for tuning and boot profiles: [path, start, length, milliseconds]
            (Module.lppFetchLog = Module.lppFetchLog || []).push([ rel, fp, cb, Math.round(performance.now() - t0) ]);
            return cb;
        }
        catch (e)
        {
            if (attempt >= 2)
            {
                console.error('[lpp] could not load ' + rel + ' @' + fp + ': ' + e);
                return -1;
            }
            await new Promise(function(resolve) { setTimeout(resolve, 400 * (attempt + 1)); });
        }
    }
});



bool FInitRemoteFiles(void)
{
    std::ifstream in(kszManifest);
    if (!in)
        return false;

    std::string line;
    while (std::getline(in, line))
    {
        size_t ich = line.find('\t');
        if (ich == std::string::npos)
            continue;
        RF rf;
        rf.cb = (int32_t)std::stol(line.substr(0, ich));
        rf.stnRel = line.substr(ich + 1);
        while (!rf.stnRel.empty() && (rf.stnRel.back() == '\r' || rf.stnRel.back() == '\n'))
            rf.stnRel.pop_back();
        if (rf.stnRel.empty())
            continue;

        // Placeholder so the file shows up in directory listings
        std::string stnPath = kszRoot + rf.stnRel;
        std::error_code ec;
        fs::create_directories(fs::path(stnPath).parent_path(), ec);
        if (!fs::exists(stnPath, ec))
            std::ofstream(stnPath, std::ios::binary);
        // Read-only, like the original CD: movies opened from here are saved elsewhere ("Save As")
        fs::permissions(stnPath, fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read, ec);

        g_mpstnirf[stnPath] = (int32_t)g_rgrf.size() + 1;
        g_rgrf.push_back(std::move(rf));
    }
    return true;
}

int32_t IrfFromPath(const char *pszPath)
{
    if (pszPath == nullptr || g_mpstnirf.empty())
        return 0;
    auto it = g_mpstnirf.find(pszPath);
    return it == g_mpstnirf.end() ? 0 : it->second;
}

int32_t CbRemoteFile(int32_t irf)
{
    if (irf <= 0 || irf > (int32_t)g_rgrf.size())
        return 0;
    return g_rgrf[irf - 1].cb;
}

// Fetch blocks [iblkMin, iblkLim) of a remote file with one request
static bool _FFetchBlocks(RF *prf, int32_t iblkMin, int32_t iblkLim)
{
    if (!LppCanSuspendJs())
        return false;

    int32_t fpMin = iblkMin * kcbBlock;
    int32_t fpLim = iblkLim * kcbBlock;
    if (fpLim > prf->cb)
        fpLim = prf->cb;
    int32_t cb = fpLim - fpMin;
    std::vector<uint8_t> rgb(cb);
    if (LppFetchRangeJs(prf->stnRel.c_str(), (double)fpMin, cb, rgb.data()) != cb)
        return false;

    g_cfetch++;
    g_cbFetched += cb;
    for (int32_t iblk = iblkMin; iblk < iblkLim; iblk++)
    {
        int32_t ib = (iblk - iblkMin) * kcbBlock;
        int32_t cbBlock = cb - ib < kcbBlock ? cb - ib : kcbBlock;
        std::unique_ptr<uint8_t[]> pb(new uint8_t[kcbBlock]);
        memcpy(pb.get(), rgb.data() + ib, cbBlock);
        prf->mpiblkpb[iblk] = std::move(pb);
    }
    return true;
}

bool FReadRemoteFile(int32_t irf, void *pv, int32_t cb, int32_t fp)
{
    if (irf <= 0 || irf > (int32_t)g_rgrf.size() || cb < 0 || fp < 0)
        return false;
    RF *prf = &g_rgrf[irf - 1];
    if (fp > prf->cb || cb > prf->cb - fp)
        return false;
    if (cb == 0)
        return true;

    int32_t iblkMin = fp / kcbBlock;
    int32_t iblkLim = (fp + cb + kcbBlock - 1) / kcbBlock;

    // Fetch missing runs of blocks
    for (int32_t iblk = iblkMin; iblk < iblkLim;)
    {
        if (prf->mpiblkpb.count(iblk) != 0)
        {
            iblk++;
            continue;
        }
        int32_t iblkRunLim = iblk + 1;
        while (iblkRunLim < iblkLim && iblkRunLim - iblk < kcblkFetchMax && prf->mpiblkpb.count(iblkRunLim) == 0)
            iblkRunLim++;
        if (!_FFetchBlocks(prf, iblk, iblkRunLim))
            return false;
        iblk = iblkRunLim;
    }

    // Copy out of the cache
    uint8_t *pbDst = (uint8_t *)pv;
    int32_t fpCur = fp;
    int32_t cbLeft = cb;
    while (cbLeft > 0)
    {
        int32_t iblk = fpCur / kcbBlock;
        int32_t ib = fpCur % kcbBlock;
        int32_t cbCopy = kcbBlock - ib < cbLeft ? kcbBlock - ib : cbLeft;
        memcpy(pbDst, prf->mpiblkpb[iblk].get() + ib, cbCopy);
        pbDst += cbCopy;
        fpCur += cbCopy;
        cbLeft -= cbCopy;
    }
    return true;
}

}

}

void GetRemoteFileStats(uint64_t *pcfetch, uint64_t *pcbFetched)
{
    *pcfetch = g_cfetch;
    *pcbFetched = g_cbFetched;
}
