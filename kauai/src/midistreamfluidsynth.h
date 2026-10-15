/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

/***************************************************************************
    Author: ShonK, Mark Cave-Ayland
    Project: Kauai
    Reviewed:
    Copyright (c) Microsoft Corporation

    MIDI stream interface: FluidSynth

***************************************************************************/
#ifndef MIDISTREAMFLUIDSYNTH_H
#define MIDISTREAMFLUIDSYNTH_H

#include <fluidsynth.h>
#include "sndma.h"

/***************************************************************************
    FluidSynth midi stream class.
***************************************************************************/
typedef class FMS *PFMS;
#define FMS_PAR MISI
#define kclsFMS KLCONST3('F', 'M', 'S')
class FMS : public FMS_PAR
{
    RTCLASS_DEC
    ASSERT
    MARKMEM

  protected:
    struct MSB
    {
        void *pvData;
        int32_t cb;
        int32_t ibStart;
        int32_t cactPlay;

        uintptr_t luData;
    };

    PFNMIDI _pfnCall;         // call back function
    uintptr_t _luUser;        // user data to send back
    std::atomic<bool> _fOpen; // device is open

    MUTX _mutx;

    Signal _hevt;
    std::thread _hth;
    std::thread _hthr;

    fluid_settings_t *_flset;
    fluid_synth_t *_flsynth;
    int _flframecount;

    PMiniaudioStream _pastream;

    int32_t _vlmBase; // our current volume

    std::atomic<bool> _fChanged; // the event has been signalled
    std::atomic<bool> _fStop;    // tells the aux thread to stop all buffers
    std::atomic<bool> _fDone;    // tells the aux thread to return

    int32_t _imsbCur;
    PGL _pglmsb;
    PMEV _pmev;
    PMEV _pmevLim;
    uint32_t _tsCur;

    FMS(PFNMIDI pfn, uintptr_t luUser);
    bool _FInit(void);

    virtual bool _FOpen(void);
    virtual bool _FClose(void);

    void _Reset(void);

    uint32_t _LuThread(void);
    uint32_t _LuRenderThread(void);

    void _ReleaseBuffers(void);

#ifdef KAUAI_NO_THREADS
    // Single-threaded builds: FluidSynth renders inside the audio callback, from a miniaudio data source,
    // and plays each event at its exact sample position. Finished buffers are returned from the main loop.
    struct DSRC
    {
        ma_data_source_base base;
        PFMS pfms;
    };
    DSRC _dsrc;
    ma_sound _sound;
    bool _fSound;

    uint32_t _cframeRate;   // output sample rate
    uint64_t _cframeTime;   // synth clock: frames rendered so far
    uint64_t _msNext;       // synth clock time of the next event, in milliseconds
    bool _fBufferActive;    // whether _pmev/_pmevLim point into a playing buffer
    int32_t _cmsbDone;      // buffers at the front of _pglmsb that finished playing, not yet returned
    int32_t _cframeIdle;    // frames rendered since the synth last had work to do

    void _StartBuffer(void);
    void _PlayDueEvents(void);
    void _Render(float *prgfl, uint32_t cframe);

    static ma_result _DsrcRead(ma_data_source *pds, void *pvOut, ma_uint64 cframe, ma_uint64 *pcframeRead);
    static ma_result _DsrcSeek(ma_data_source *pds, ma_uint64 iframe);
    static ma_result _DsrcGetDataFormat(ma_data_source *pds, ma_format *pformat, ma_uint32 *pcchannel,
                                        ma_uint32 *pcframeRate, ma_channel *prgchannel, size_t cchannelMap);
    static void _PollProc(void *pvUser);
#endif // KAUAI_NO_THREADS

  public:
    static PFMS PfmsNew(PFNMIDI pfn, uintptr_t luUser);
    ~FMS(void);

    virtual void SetVlm(int32_t vlm);
    virtual int32_t VlmCur(void);

    virtual bool FActive(void);
    virtual bool FActivate(bool fActivate);

    virtual bool FQueueBuffer(void *pvData, int32_t cb, int32_t ibStart, int32_t cactPlay, uintptr_t luData) override;
    virtual void StopPlaying(void) override;
};

// Number of MIDI frames rendered with sounding notes since the last reset (for tests)
uint64_t CframeMidiAudible(bool fReset);

#endif //! MIDISTREAMFLUIDSYNTH_H
