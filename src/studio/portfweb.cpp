/***************************************************************************

    portfweb.cpp: Portfolio for the Emscripten (WebAssembly) build

    The page shows its own open/save dialog: Module.lppPortfolio(options)
    returns a Promise for a path in the virtual filesystem (or null if the
    user cancelled). Asyncify suspends the app while the dialog is open.
    If the page has no dialog, the portfolio is always cancelled.

***************************************************************************/
#include "studio.h"
#include <emscripten.h>
#include <cstdlib>

ASSERTNAME

// Returns a malloc'd UTF-8 path, or nullptr if cancelled
EM_ASYNC_JS(char *, LppPortfolioJs,
            (int fOpen, const char *pszTitle, const char *pszFilterLabel, const char *pszFilterExt,
             const char *pszInitialDir, const char *pszDefName, int grfPrevType),
            {
                if (typeof Module.lppPortfolio !== 'function')
                    return 0;
                var opts = {
                    open : !!fOpen,
                    title : UTF8ToString(pszTitle),
                    filterLabel : UTF8ToString(pszFilterLabel),
                    filterExt : UTF8ToString(pszFilterExt),
                    initialDir : UTF8ToString(pszInitialDir),
                    defaultName : UTF8ToString(pszDefName),
                    preview : grfPrevType
                };
                var path = null;
                try
                {
                    path = await Module.lppPortfolio(opts);
                }
                catch (e)
                {
                    console.error('portfolio failed', e);
                }
                if (!path)
                    return 0;
                return stringToNewUTF8(path);
            });

bool FPortDisplayWithIds(FNI *pfni, bool fOpen, int32_t lFilterLabel, int32_t lFilterExt, int32_t lTitle,
                         PCSZ lpstrDefExt, PSTN pstnDefFileName, FNI *pfniInitialDir, uint32_t grfPrevType, CNO cnoWave)
{
    AssertVarMem(pfni);
    AssertNilOrPo(pstnDefFileName, 0);
    AssertNilOrPo(pfniInitialDir, 0);

    bool fRet = fFalse;
    STN stnT;
    U8SZ u8szTitle;
    U8SZ u8szInitialDir;
    U8SZ u8szFilterLabel;
    U8SZ u8szFilterExt;
    U8SZ u8szDefFileName;
    char *pszPath = pvNil;

    vpcex->Suspend(fTrue);

    // Get initial directory
    if (pfniInitialDir != pvNil)
        pfniInitialDir->GetStnPath(&stnT);
    else
        stnT.SetNil();
    stnT.GetUtf8Sz(u8szInitialDir);

    // Get default file name
    u8szDefFileName[0] = chNil;
    if (pstnDefFileName != pvNil)
        pstnDefFileName->GetUtf8Sz(u8szDefFileName);

    // Get title and file format filter strings ("*.3mm;*.wav")
    if (!vapp.FGetStnApp(lTitle, &stnT))
        stnT.SetNil();
    stnT.GetUtf8Sz(u8szTitle);
    if (!vapp.FGetStnApp(lFilterLabel, &stnT))
        goto LDone;
    stnT.GetUtf8Sz(u8szFilterLabel);
    if (!vapp.FGetStnApp(lFilterExt, &stnT))
        goto LDone;
    stnT.GetUtf8Sz(u8szFilterExt);

    StopAllMovieSounds();
    vapp.EnsureInteractive();
    vapp.SetFInPortfolio(fTrue);

    // Play sound associated with this portfolio
    if (cnoWave != cnoNil)
    {
        PCRF pcrf;
        if ((pcrf = ((APP *)vpappb)->PcrmAll()->PcrfFindChunk(kctgWave, cnoWave)) != pvNil)
        {
            vpsndm->SiiPlay(pcrf, kctgWave, cnoWave);
        }
    }

    // Show the page's dialog (the app is suspended until it closes)
    pszPath = LppPortfolioJs(fOpen, (const char *)u8szTitle, (const char *)u8szFilterLabel,
                             (const char *)u8szFilterExt, (const char *)u8szInitialDir, (const char *)u8szDefFileName,
                             (int)grfPrevType);

    if (pszPath != pvNil)
    {
        stnT.SetUtf8Sz((PU8SZ)pszPath);
        fRet = pfni->FBuildFromPath(&stnT, 0);
    }
    else
    {
        pfni->SetNil();
    }

LDone:
    vpsndm->StopAll(sqnNil, sclNil);
    vapp.SetFInPortfolio(fFalse);
    vpcex->Suspend(fFalse);

    // Notify scripts that the portfolio was closed
    vpcex->EnqueueCid(cidPortfolioClosed, 0, 0, fRet);
    vpcex->EnqueueCid(cidPortfolioResult, 0, 0, fRet);

    free(pszPath);
    return fRet;
}
