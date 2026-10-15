/***************************************************************************
    Author: Ben Stone
    Project: Kauai
    Reviewed:

    Miniaudio engine manager

***************************************************************************/
#include "frame.h"
ASSERTNAME

#include "sndma.h"
#include "sndmapri.h"

RTCLASS(MiniaudioManager)

MiniaudioManager::MiniaudioManager()
{
    ClearPb(&_engine, SIZEOF(_engine));
    _fInit = fFalse;
    _fInitFailed = fFalse;
}

MiniaudioManager::~MiniaudioManager()
{
    ma_engine_stop(&_engine);
    ma_engine_uninit(&_engine);
    _fInit = fFalse;
}

ma_engine *MiniaudioManager::Pengine()
{
    // Initialise the miniaudio engine if we haven't already
    ma_engine *pengine = &_engine;

    _mutxInit.Enter();
    if (_fInitFailed)
    {
        // No audio output (e.g. no Web Audio in this browser): don't retry
        pengine = pvNil;
    }
    else if (!_fInit)
    {
        ma_engine_config config = ma_engine_config_init();
        config.onProcess = _MeterProc;
        config.pProcessUserData = this;

        ma_result result = ma_engine_init(&config, &_engine);
        AssertMaSuccess(result, "ma_engine_init failed");
        if (result == MA_SUCCESS)
        {
            _fInit = fTrue;
        }
        else
        {
            _fInitFailed = fTrue;
            pengine = pvNil;
        }
    }

    _mutxInit.Leave();
    return pengine;
}

/***************************************************************************
    Engine callback (audio thread): measure the mixed output
***************************************************************************/
void MiniaudioManager::_MeterProc(void *pvUser, float *prgflOut, ma_uint64 cframe)
{
    PMiniaudioManager pmanager = (PMiniaudioManager)pvUser;
    ma_uint32 cchannel = ma_engine_get_channels(&pmanager->_engine);
    const float kflAudible = 1.0f / 1024; // about -60 dB
    float flPeak = pmanager->_flPeak.load();
    uint64_t cframeAudible = 0;
    uint32_t cframeWindow = ma_engine_get_sample_rate(&pmanager->_engine) / 50; // 20 ms
    const size_t kcbEnvMax = 50 * 60;                                           // one minute

    pmanager->_mutxEnv.Enter();

    for (ma_uint64 iframe = 0; iframe < cframe; iframe++)
    {
        float flFrame = 0;
        for (ma_uint32 ichannel = 0; ichannel < cchannel; ichannel++)
        {
            float fl = prgflOut[iframe * cchannel + ichannel];
            if (fl < 0)
                fl = -fl;
            if (fl > flFrame)
                flFrame = fl;
        }
        if (flFrame > flPeak)
            flPeak = flFrame;
        if (flFrame >= kflAudible)
            cframeAudible++;

        if (flFrame > pmanager->_flEnvPeak)
            pmanager->_flEnvPeak = flFrame;
        if (++pmanager->_cframeEnv >= cframeWindow && cframeWindow > 0)
        {
            float fl = pmanager->_flEnvPeak * 255;
            if (pmanager->_rgbEnv.size() >= kcbEnvMax)
                pmanager->_rgbEnv.erase(pmanager->_rgbEnv.begin());
            pmanager->_rgbEnv.push_back((uint8_t)(fl > 255 ? 255 : fl));
            pmanager->_flEnvPeak = 0;
            pmanager->_cframeEnv = 0;
        }
    }
    pmanager->_mutxEnv.Leave();

    pmanager->_flPeak = flPeak;
    pmanager->_cframeAudible += cframeAudible;
    pmanager->_cframe += cframe;
}

/***************************************************************************
    Return the output level meter, optionally resetting it
***************************************************************************/
void MiniaudioManager::GetMeter(float *pflPeak, uint64_t *pcframeAudible, uint64_t *pcframe, bool fReset)
{
    *pflPeak = fReset ? _flPeak.exchange(0) : _flPeak.load();
    *pcframeAudible = fReset ? _cframeAudible.exchange(0) : _cframeAudible.load();
    *pcframe = fReset ? _cframe.exchange(0) : _cframe.load();
}

/***************************************************************************
    Return the loudness envelope, optionally resetting it
***************************************************************************/
void MiniaudioManager::GetEnvelope(std::vector<uint8_t> *prgbEnv, bool fReset)
{
    _mutxEnv.Enter();
    *prgbEnv = _rgbEnv;
    if (fReset)
        _rgbEnv.clear();
    _mutxEnv.Leave();
}

PMiniaudioManager MiniaudioManager::Pmanager()
{
    static PMiniaudioManager s_pmamanager = pvNil;

    if (s_pmamanager == pvNil)
    {
        s_pmamanager = NewObj MiniaudioManager;
    }

    return s_pmamanager;
}
