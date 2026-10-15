/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

/***************************************************************************
    Author: ShonK, Mark Cave-Ayland
    Project: Kauai
    Copyright (c) Microsoft Corporation

    MIDI stream interface: FluidSynth

***************************************************************************/
#include "frame.h"
#include "mdev2pri.h"
#include "midistreamfluidsynth.h"
#ifdef KAUAI_NO_THREADS
#include "sndmapri.h"
#endif // KAUAI_NO_THREADS

#include <chrono>
#include <cstring>

ASSERTNAME

RTCLASS(FMS)

const int32_t kdtsMinSlip = kdtsSecond / 30;

// Frames rendered while the synth had active voices (see CframeMidiAudible)
static std::atomic<uint64_t> s_cframeMidiAudible{0};

#ifdef __EMSCRIPTEN__
/***************************************************************************
    FluidSynth log: its embedded OS layer reports each use of a stub
    function (file tests, stat) as an error. Drop those.
***************************************************************************/
static void _FluidLog(int level, const char *pszMessage, void *pvData)
{
    if (level == FLUID_DBG || strstr(pszMessage, "is a stub") != pvNil)
        return;
    fluid_default_log_function(level, pszMessage, pvData);
}
#endif // __EMSCRIPTEN__

/***************************************************************************
    Number of MIDI frames rendered with sounding notes, for tests.
***************************************************************************/
uint64_t CframeMidiAudible(bool fReset)
{
    return fReset ? s_cframeMidiAudible.exchange(0) : s_cframeMidiAudible.load();
}

#define FMS_LUTHREAD_SLEEP_DELAY 5
#define FMS_LUTHREAD_FRAMES_PENDING 8192

/***************************************************************************
    Constructor for our own midi stream api implementation.
***************************************************************************/
FMS::FMS(PFNMIDI pfn, uintptr_t luUser)
{
    _pfnCall = pfn;
    _luUser = luUser;
    _fOpen = fFalse;
    _flset = pvNil;
    _flsynth = pvNil;
    _pastream = pvNil;
    _fChanged = fFalse;
    _fStop = fFalse;
    _fDone = fFalse;
    _pglmsb = pvNil;
    _pmev = pvNil;
    _pmevLim = pvNil;
    _vlmBase = kvlmFull;
#ifdef KAUAI_NO_THREADS
    ClearPb(&_dsrc, SIZEOF(_dsrc));
    _fSound = fFalse;
    _cframeRate = 0;
    _cframeTime = 0;
    _msNext = 0;
    _fBufferActive = fFalse;
    _cmsbDone = 0;
    _cframeIdle = 0;
#endif // KAUAI_NO_THREADS
}

/***************************************************************************
    Destructor for our midi stream.
***************************************************************************/
FMS::~FMS(void)
{
#ifdef KAUAI_NO_THREADS
    RemovePollProc(_PollProc, this);
    if (_fSound)
    {
        ma_sound_uninit(&_sound);
        ma_data_source_uninit(&_dsrc.base);
        _fSound = fFalse;
    }
#endif // KAUAI_NO_THREADS

    if (_hth.joinable())
    {
        _fDone = fTrue;
        _hevt.Set();
        _hth.join();
    }

    _mutx.Enter();

    if (_hthr.joinable())
    {
        _hthr.join();
    }

    Assert(_fOpen == fFalse, "Still open");

    if (_pastream != pvNil)
    {
        _pastream->FStop();
        ReleasePpo(&_pastream);
    }

    if (_pglmsb != pvNil)
    {
        Assert(_pglmsb->IvMac() == 0, "Still have some buffers");
        ReleasePpo(&_pglmsb);
    }

    if (_flsynth != pvNil)
    {
        delete_fluid_synth(_flsynth);
        _flsynth = pvNil;
    }

    if (_flset != pvNil)
    {
        delete_fluid_settings(_flset);
        _flset = pvNil;
    }

    _fOpen = fFalse;

    _mutx.Leave();
}

/***************************************************************************
    Create a new FMS.
***************************************************************************/
PFMS FMS::PfmsNew(PFNMIDI pfn, uintptr_t luUser)
{
    PFMS poms;

    if (pvNil == (poms = NewObj FMS(pfn, luUser)))
        return pvNil;

    if (!poms->_FInit())
        ReleasePpo(&poms);

    return poms;
}

/***************************************************************************
    Initialize the FMS.
***************************************************************************/
bool FMS::_FInit(void)
{
    ma_engine *pengineDevice = MiniaudioManager::Pmanager()->Pengine();
    ma_device *pdevice;
    char buf[256];
    int id, ret;

    AssertBaseThis(0);

    // No audio output (e.g. a browser without Web Audio): no MIDI either
    if (pengineDevice == pvNil)
        return fFalse;
    pdevice = pengineDevice->pDevice;

    if (pvNil == (_pglmsb = GL::PglNew(SIZEOF(MSB))))
        return fFalse;
    _pglmsb->SetMinGrow(1);

    _mutx.Enter();

#ifdef __EMSCRIPTEN__
    for (int level = FLUID_PANIC; level < LAST_LOG_LEVEL; level++)
        fluid_set_log_function(level, _FluidLog, pvNil);
#endif // __EMSCRIPTEN__

    _flset = new_fluid_settings();
    if (_flset == pvNil)
    {
        Bug("failed to create fluidsynth settings");
        goto LFail;
    }
    ret = fluid_settings_setnum(_flset, "synth.sample-rate", pdevice->sampleRate);
    if (ret == FLUID_FAILED)
    {
        Bug("failed to set synth.sample-rate");
        goto LFail;
    }

    _flsynth = new_fluid_synth(_flset);
    if (_flsynth == pvNil)
    {
        Bug("failed to create fluidsynth synth");
        goto LFail;
    }

#ifdef __EMSCRIPTEN__
    // There is no system soundfont in the browser: use the one in the app resources
    ret = FLUID_FAILED;
#else
    ret = fluid_settings_copystr(_flset, "synth.default-soundfont", buf, sizeof(buf));
    if (ret == FLUID_FAILED)
    {
        Bug("failed to get synth.default-soundfont");
        goto LFail;
    }

    ret = fluid_synth_sfload(_flsynth, buf, true);
#endif // __EMSCRIPTEN__
    if (ret == FLUID_FAILED)
    {
        FNI fniExe;
        STN stnFileName = PszLit("soundfont.sf3");
        STN stnSoundFontPath;

        // Try app resources directory
        AssertDo(fniExe.FGetResourcesDir(), "Could not find resources directory");
        fniExe.FSetLeaf(&stnFileName);
        fniExe.GetStnPath(&stnSoundFontPath);

        ret = fluid_synth_sfload(_flsynth, stnSoundFontPath.Psz(), true);
        if (ret == FLUID_FAILED)
        {
            // ...or an uncompressed SF2
            stnFileName = PszLit("soundfont.sf2");
            fniExe.FSetLeaf(&stnFileName);
            fniExe.GetStnPath(&stnSoundFontPath);
            ret = fluid_synth_sfload(_flsynth, stnSoundFontPath.Psz(), true);
        }
        if (ret == FLUID_FAILED)
        {
            Bug("failed to load soundfont");
            goto LFail;
        }
    }

    ret = fluid_settings_getint(_flset, "audio.period-size", &_flframecount);
    if (ret == FLUID_FAILED)
    {
        Bug("failed to get audio.period-size");
        goto LFail;
    }

    // Check the output format is correct
    if (pdevice->playback.format != ma_format_f32)
    {
        Bug("expected f32 format");
        goto LFail;
    }

    if (pdevice->playback.channels != 2)
    {
        Bug("expected stereo");
        goto LFail;
    }

#ifdef KAUAI_NO_THREADS
    {
        // Render from the audio callback: wrap the synth in a data source and play it as a sound
        static ma_data_source_vtable s_vtable = {_DsrcRead, _DsrcSeek, _DsrcGetDataFormat, pvNil, pvNil, pvNil, 0};
        ma_engine *pengine = MiniaudioManager::Pmanager()->Pengine();
        ma_data_source_config dsconfig = ma_data_source_config_init();
        ma_result result;

        _cframeRate = ma_engine_get_sample_rate(pengine);
        _dsrc.pfms = this;
        dsconfig.vtable = &s_vtable;
        result = ma_data_source_init(&dsconfig, &_dsrc.base);
        if (result != MA_SUCCESS)
        {
            Bug("failed to create MIDI data source");
            goto LFail;
        }
        result = ma_sound_init_from_data_source(pengine, &_dsrc.base,
                                                MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH, pvNil,
                                                &_sound);
        if (result != MA_SUCCESS)
        {
            Bug("failed to create MIDI sound");
            ma_data_source_uninit(&_dsrc.base);
            goto LFail;
        }
        _fSound = fTrue;
        ma_sound_set_volume(&_sound, ScaleVlm(_vlmBase));
        AssertDo(ma_sound_start(&_sound) == MA_SUCCESS, "Could not play");
        AddPollProc(_PollProc, this);
    }
#else
    // Create the stream and start playing it
    _pastream = MiniaudioStream::PastreamNew(MiniaudioManager::Pmanager());
    AssertPo(_pastream, 0);
    _hth = std::thread([this] { return this->_LuThread(); });
    _hthr = std::thread([this] { return this->_LuRenderThread(); });
    AssertDo(_pastream->FPlay(), "Could not play");
#endif // KAUAI_NO_THREADS

    _mutx.Leave();

    return fTrue;

LFail:
    if (_flsynth != pvNil)
    {
        delete_fluid_synth(_flsynth);
        _flsynth = pvNil;
    }

    if (_flset != pvNil)
    {
        delete_fluid_settings(_flset);
        _flset = pvNil;
    }

    _mutx.Leave();

    return fFalse;
}

#ifdef DEBUG
/***************************************************************************
    Assert the validity of a FMS.
***************************************************************************/
void FMS::AssertValid(uint32_t grf)
{
    FMS_PAR::AssertValid(0);

    _mutx.Enter();
    AssertPo(_pglmsb, 0);
    _mutx.Leave();
}

/***************************************************************************
    Mark memory for the FMS.
***************************************************************************/
void FMS::MarkMem(void)
{
    FMS_PAR::MarkMem();

    _mutx.Enter();
    MarkMemObj(_pglmsb);
    MarkMemObj(_pastream);
    _mutx.Leave();
}
#endif // DEBUG

/***************************************************************************
    Open the stream.
***************************************************************************/
bool FMS::_FOpen(void)
{
    AssertThis(0);

    _mutx.Enter();
    if (_fOpen == fTrue)
        goto LDone;

    _fChanged = _fStop = fFalse;
    _fOpen = fTrue;

    // set our volume level
    SetVlm(_vlmBase);

LDone:
    _mutx.Leave();

    return fTrue;
}

/***************************************************************************
    Close the stream.
***************************************************************************/
bool FMS::_FClose(void)
{
    AssertThis(0);

    _mutx.Enter();

    if (_fOpen == fFalse)
    {
        _mutx.Leave();
        return fTrue;
    }

    if (_pglmsb->IvMac() > 0)
    {
        Bug("closing a stream that still has buffers!");
        _mutx.Leave();
        return fFalse;
    }

    // reset the device
    _Reset();

    _fOpen = fFalse;

    _mutx.Leave();

    return fTrue;
}

/***************************************************************************
    Reset the midi device.
***************************************************************************/
void FMS::_Reset(void)
{
    Assert(_fOpen == fTrue, 0);

    fluid_synth_system_reset(_flsynth);
}

/***************************************************************************
    Set the volume for the midi stream output device.
***************************************************************************/
void FMS::SetVlm(int32_t vlm)
{
    AssertThis(0);

    if (vlm != _vlmBase)
    {
        _vlmBase = vlm;
#ifdef KAUAI_NO_THREADS
        if (_fOpen == fTrue && _fSound)
            ma_sound_set_volume(&_sound, ScaleVlm(vlm));
#else
        if (_fOpen == fTrue)
            _pastream->SetVlm(vlm);
#endif // KAUAI_NO_THREADS
    }
}

/***************************************************************************
    Get the current volume.
***************************************************************************/
int32_t FMS::VlmCur(void)
{
    AssertThis(0);

    return _vlmBase;
}

/***************************************************************************
    Return whether the midi stream output device is active.
***************************************************************************/
bool FMS::FActive(void)
{
    return (_fOpen == fTrue);
}

/***************************************************************************
    Activate or deactivate the Midi stream output object.
***************************************************************************/
bool FMS::FActivate(bool fActivate)
{
    AssertThis(0);

    return fActivate ? _FOpen() : _FClose();
}

/***************************************************************************
    Queue a buffer to the midi stream.
***************************************************************************/
bool FMS::FQueueBuffer(void *pvData, int32_t cb, int32_t ibStart, int32_t cactPlay, uintptr_t luData)
{
    AssertThis(0);
    AssertPvCb(pvData, cb);
    AssertIn(ibStart, 0, cb);
    Assert(cb % SIZEOF(MEV) == 0, "bad cb");
    Assert(ibStart % SIZEOF(MEV) == 0, "bad cb");

    MSB msb;

    _mutx.Enter();

    if (_fOpen == fFalse)
        goto LFail;

    msb.pvData = pvData;
    msb.cb = cb;
    msb.ibStart = ibStart;
    msb.cactPlay = cactPlay;
    msb.luData = luData;

    if (!_pglmsb->FAdd(&msb))
    {
    LFail:
        _mutx.Leave();
        return fFalse;
    }

#ifdef KAUAI_NO_THREADS
    if (!_fBufferActive)
    {
        // Start the buffer (the audio callback plays it)
        _StartBuffer();
    }
#else
    if (1 == _pglmsb->IvMac())
    {
        // Start the buffer
        _fChanged = fTrue;
        _hevt.Set();
    }
#endif // KAUAI_NO_THREADS

    _mutx.Leave();

    return fTrue;
}

/***************************************************************************
    Stop the stream and release all buffers. The buffer notifies are
    asynchronous.
***************************************************************************/
void FMS::StopPlaying(void)
{
    AssertThis(0);

    _mutx.Enter();

    if (_fOpen == fTrue)
    {
#ifdef KAUAI_NO_THREADS
        // Single-threaded: release all buffers now (MSMIX waits for them)
        _fBufferActive = fFalse;
        _pmev = _pmevLim = pvNil;
        _cmsbDone = 0;
        _imsbCur = _pglmsb->IvMac();
        _ReleaseBuffers();
#else
        _fStop = fTrue;
        _hevt.Set();
        _fChanged = fTrue;
#endif // KAUAI_NO_THREADS
    }

    _mutx.Leave();
}

#ifndef KAUAI_NO_THREADS
/***************************************************************************
    AT: The midi stream playback thread.
***************************************************************************/
uint32_t FMS::_LuThread(void)
{
    AssertThis(0);
    MSB msb;
    bool fChanged; // whether the event went off
    uint32_t tsCur;
    const int32_t klwInfinite = klwMax;
    int32_t dtsWait = klwInfinite;

    for (;;)
    {
        fChanged = (dtsWait > 0 && _hevt.Wait(dtsWait == klwInfinite ? KSIGNAL_INFINITE : dtsWait));

        if (_fDone)
            return 0;

        _mutx.Enter();
        if (_fChanged && !fChanged)
        {
            // the event went off before we got the mutx.
            dtsWait = klwInfinite;
            goto LLoop;
        }

        _fChanged = fFalse;
        if (!fChanged)
        {
            // play the event
            if (_pmev < _pmevLim)
            {
                if (MEVT_SHORTMSG == (_pmev->dwEvent >> 24))
                {
                    switch (_pmev->dwEvent & 0xf0)
                    {
                    case 0x80: /* Note off */
                        fluid_synth_noteoff(_flsynth, _pmev->dwEvent & 0xf, (_pmev->dwEvent & 0x7f00) >> 8);
                        break;

                    case 0x90: /* Note on */
                        fluid_synth_noteon(_flsynth, _pmev->dwEvent & 0xf, (_pmev->dwEvent & 0x7f00) >> 8,
                                           (_pmev->dwEvent & 0x7f0000) >> 16);
                        break;

                    case 0xb0: /* Control change */
                        fluid_synth_cc(_flsynth, _pmev->dwEvent & 0xf, (_pmev->dwEvent & 0x7f00) >> 8,
                                       (_pmev->dwEvent & 0x7f0000) >> 16);
                        break;

                    case 0xc0: /* Program change */
                        fluid_synth_program_change(_flsynth, _pmev->dwEvent & 0xf, (_pmev->dwEvent & 0x7f00) >> 8);
                        break;

                    case 0xd0: /* Channel pressure */
                        fluid_synth_channel_pressure(_flsynth, _pmev->dwEvent & 0xf, (_pmev->dwEvent & 0x7f00) >> 8);
                        break;

                    case 0xe0: /* Pitch wheel */
                        fluid_synth_pitch_bend(_flsynth, _pmev->dwEvent & 0xf,
                                               ((_pmev->dwEvent & 0x7f00) >> 8) | ((_pmev->dwEvent & 0x7f0000) >> 9));
                        break;
                    }
                }

                _pmev++;
                if (_pmev >= _pmevLim)
                {
                    dtsWait = 0;
                }
                else
                {
                    uint32_t tsNew = TsCurrentSystem();

                    tsCur += _pmev->dwDeltaTime;
                    dtsWait = tsCur - tsNew;
                    if (dtsWait < -kdtsMinSlip)
                    {
                        tsCur = tsNew;
                        dtsWait = 0;
                    }
                }
                goto LLoop;
            }

            // ran out of events in the current buffer - see if we should
            // repeat it
            _pglmsb->Get(0, &msb);
            if (msb.cactPlay == 1)
            {
                _imsbCur = 1;
                _ReleaseBuffers();
            }
            else
            {
                // repeat the current buffer
                if (msb.cactPlay > 0)
                    msb.cactPlay--;
                msb.ibStart = 0;
                _pglmsb->Put(0, &msb);
            }
        }
        else if (_fStop)
        {
            // release all buffers
            _fStop = fFalse;
            _imsbCur = _pglmsb->IvMac();
            _ReleaseBuffers();
        }

        if (0 == _pglmsb->IvMac())
        {
            // no buffers to play
            dtsWait = klwInfinite;
        }
        else
        {
            // start playing the new buffers
            _pglmsb->Get(0, &msb);
            _pmev = (PMEV)PvAddBv(msb.pvData, msb.ibStart);
            _pmevLim = (PMEV)PvAddBv(msb.pvData, msb.cb);
            if (_pmev >= _pmevLim)
            {
                dtsWait = 0;
            }
            else
            {
                dtsWait = _pmev->dwDeltaTime;
                tsCur = TsCurrentSystem() + dtsWait;
            }
        }
    LLoop:
        _mutx.Leave();
    }
}

/***************************************************************************
    AT: The midi stream playback thread.
***************************************************************************/
uint32_t FMS::_LuRenderThread(void)
{
    float *flFrame = pvNil;

    if (!FAllocPv((void **)&flFrame, SIZEOF(float) * _flframecount * 2, fmemClear, mprNormal))
        goto LFail;

    for (;;)
    {
        while (_pastream->GetPendingFrames() < FMS_LUTHREAD_FRAMES_PENDING)
        {
            if (_fDone)
                return 0;

            if (!_fStop)
            {
                fluid_synth_write_float(_flsynth, _flframecount, flFrame, 0, 2, flFrame, 1, 2);
                if (fluid_synth_get_active_voice_count(_flsynth) > 0)
                    s_cframeMidiAudible += _flframecount;
                AssertDo(_pastream->FWriteAudio(flFrame, _flframecount), "could not write audio");
            }
        }

        if (_fDone)
        {
            _hevt.Set();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(FMS_LUTHREAD_SLEEP_DELAY));
    }

LFail:
    FreePpv((void **)flFrame);

    return 0;
}
#endif // !KAUAI_NO_THREADS

/***************************************************************************
    Release all buffers up to _imsbCur. Assumes that we have the mutx
    checked out exactly once.
***************************************************************************/
void FMS::_ReleaseBuffers(void)
{
    MSB msb;

    if (_imsbCur >= _pglmsb->IvMac() && _fOpen == fTrue)
        _Reset();

    while (_imsbCur > 0)
    {
        _pglmsb->Get(0, &msb);
        _pglmsb->Delete(0);
        _imsbCur--;

        _mutx.Leave();

        // call the notify proc
        (*_pfnCall)(_luUser, msb.pvData, msb.luData);

        _mutx.Enter();
    }
}

#ifdef KAUAI_NO_THREADS
/***************************************************************************
    Start playing the first buffer that hasn't finished yet (if any).
    Assumes the mutx is checked out.
***************************************************************************/
void FMS::_StartBuffer(void)
{
    MSB msb;

    if (_cmsbDone >= _pglmsb->IvMac())
    {
        _fBufferActive = fFalse;
        _pmev = _pmevLim = pvNil;
        return;
    }

    _pglmsb->Get(_cmsbDone, &msb);
    _pmev = (PMEV)PvAddBv(msb.pvData, msb.ibStart);
    _pmevLim = (PMEV)PvAddBv(msb.pvData, msb.cb);
    _fBufferActive = fTrue;

    // the first event plays its delta time from now
    _msNext = _cframeTime * 1000 / _cframeRate;
    if (_pmev < _pmevLim)
        _msNext += _pmev->dwDeltaTime;
}

/***************************************************************************
    AT: Play every event that is due at the current synth time. Assumes the
    mutx is checked out.
***************************************************************************/
void FMS::_PlayDueEvents(void)
{
    MSB msb;

    while (_fBufferActive && _msNext * _cframeRate / 1000 <= _cframeTime)
    {
        if (_pmev < _pmevLim)
        {
            uint32_t dwEvent = _pmev->dwEvent;
            int chan = dwEvent & 0xf;
            int data1 = (dwEvent & 0x7f00) >> 8;
            int data2 = (dwEvent & 0x7f0000) >> 16;

            if (MEVT_SHORTMSG == (dwEvent >> 24))
            {
                switch (dwEvent & 0xf0)
                {
                case 0x80: /* Note off */
                    fluid_synth_noteoff(_flsynth, chan, data1);
                    break;

                case 0x90: /* Note on */
                    fluid_synth_noteon(_flsynth, chan, data1, data2);
                    break;

                case 0xb0: /* Control change */
                    fluid_synth_cc(_flsynth, chan, data1, data2);
                    break;

                case 0xc0: /* Program change */
                    fluid_synth_program_change(_flsynth, chan, data1);
                    break;

                case 0xd0: /* Channel pressure */
                    fluid_synth_channel_pressure(_flsynth, chan, data1);
                    break;

                case 0xe0: /* Pitch wheel */
                    fluid_synth_pitch_bend(_flsynth, chan, data1 | (data2 << 7));
                    break;
                }
            }

            _pmev++;
            if (_pmev < _pmevLim)
                _msNext += _pmev->dwDeltaTime;
            continue;
        }

        // ran out of events in the current buffer - see if we should repeat it
        _pglmsb->Get(_cmsbDone, &msb);
        if (msb.cactPlay == 1)
        {
            // done: the main loop returns it; play the next one right away
            _cmsbDone++;
            if (_cmsbDone >= _pglmsb->IvMac())
                fluid_synth_system_reset(_flsynth);
        }
        else
        {
            // repeat the current buffer
            if (msb.cactPlay > 0)
                msb.cactPlay--;
            msb.ibStart = 0;
            _pglmsb->Put(_cmsbDone, &msb);
        }
        _StartBuffer();
    }
}

/***************************************************************************
    AT: Render audio, playing events at their sample positions.
***************************************************************************/
void FMS::_Render(float *prgfl, uint32_t cframe)
{
    // Keep rendering a few seconds after the last buffer so reverb tails finish, then skip the synth
    const int32_t kcsecIdle = 3;

    _mutx.Enter();

    uint32_t iframe = 0;
    while (iframe < cframe)
    {
        _PlayDueEvents();

        if (_fBufferActive)
            _cframeIdle = 0;
        else if (_cframeIdle >= (int32_t)_cframeRate * kcsecIdle)
        {
            // nothing playing: silence
            ClearPb(prgfl + 2 * iframe, LwMul(cframe - iframe, 2 * SIZEOF(float)));
            _cframeTime += cframe - iframe;
            break;
        }

        uint64_t cframeChunk = cframe - iframe;
        if (_fBufferActive)
        {
            uint64_t cframeNext = _msNext * _cframeRate / 1000;
            if (cframeNext > _cframeTime && cframeNext - _cframeTime < cframeChunk)
                cframeChunk = cframeNext - _cframeTime;
        }

        float *pfl = prgfl + 2 * iframe;
        fluid_synth_write_float(_flsynth, (int)cframeChunk, pfl, 0, 2, pfl, 1, 2);
        if (fluid_synth_get_active_voice_count(_flsynth) > 0)
            s_cframeMidiAudible += cframeChunk;
        iframe += (uint32_t)cframeChunk;
        _cframeTime += cframeChunk;
        if (!_fBufferActive)
            _cframeIdle += (int32_t)cframeChunk;
    }

    _mutx.Leave();
}

/***************************************************************************
    AT: miniaudio data source callbacks
***************************************************************************/
ma_result FMS::_DsrcRead(ma_data_source *pds, void *pvOut, ma_uint64 cframe, ma_uint64 *pcframeRead)
{
    PFMS pfms = ((DSRC *)pds)->pfms;

    pfms->_Render((float *)pvOut, (uint32_t)cframe);
    if (pcframeRead != pvNil)
        *pcframeRead = cframe;
    return MA_SUCCESS;
}

ma_result FMS::_DsrcSeek(ma_data_source *pds, ma_uint64 iframe)
{
    return MA_NOT_IMPLEMENTED;
}

ma_result FMS::_DsrcGetDataFormat(ma_data_source *pds, ma_format *pformat, ma_uint32 *pcchannel,
                                  ma_uint32 *pcframeRate, ma_channel *prgchannel, size_t cchannelMap)
{
    PFMS pfms = ((DSRC *)pds)->pfms;

    *pformat = ma_format_f32;
    *pcchannel = 2;
    *pcframeRate = pfms->_cframeRate;
    if (prgchannel != pvNil)
        ma_channel_map_init_standard(ma_standard_channel_map_default, prgchannel, cchannelMap, 2);
    return MA_SUCCESS;
}

/***************************************************************************
    Main loop poll proc: return buffers that finished playing (calls the
    notify proc, which may queue the next tune).
***************************************************************************/
void FMS::_PollProc(void *pvUser)
{
    PFMS pfms = (PFMS)pvUser;

    pfms->_mutx.Enter();
    if (pfms->_cmsbDone > 0)
    {
        pfms->_imsbCur = pfms->_cmsbDone;
        pfms->_cmsbDone = 0;
        pfms->_ReleaseBuffers();
    }
    pfms->_mutx.Leave();
}
#endif // KAUAI_NO_THREADS
