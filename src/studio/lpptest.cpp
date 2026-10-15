/***************************************************************************
    Low Poly Pictures test driver.

    Runs command batches from the test runner (tools/lpptest.py) and reports
    results as JSON. Identical on desktop and in the browser.

    Desktop: set LPP_TEST_DIR. Batches are files <dir>/in/*.txt, results are
             written to <dir>/out/<name>.json, screenshots to <dir>/shots/.
    Browser: lpp_test_submit(text) -> batch id; lpp_test_result(id) -> JSON
             ("" until finished). Screenshots go to /lpptest/shots/ in the
             virtual filesystem.

    Commands (one per line, '#' comments; coordinates are logical 640x480):
      click X Y [HOLDMS]       move, press, hold (default 150 ms), release
      drag X Y DX DY [STEPS] [MS]   press at X,Y then move by DX,DY (relative,
                               so it works with the app's own cursor warps)
      move X Y | moveby DX DY | down | up
      key NAME [ctrl|shift|alt]   SDL key name: Return, Escape, Left, Z, ...
      keydown NAME | keyup NAME    hold / release a key (Up/Down during a drag change height)
      wait MS
      waitfor PLACE [TIMEOUTMS]   wait until a place is on screen: studio, scene, map, ticket1,
                               ticket2, lobby1, backstage1, imagin, street, theatre1, ... (see kplaces),
                               balloon (any speech-bubble menu), playing (a movie is playing);
                               "!PLACE" waits until it is gone
      shot NAME                save the screen (8-bit BMP with palette)
      state                    app/movie/actor state as JSON
      open PATH                open a movie file in the studio ($APPDIR = folder of the app;
                               e.g. $APPDIR/Microsoft Kids/Users/Melanie/dialogue.3mm)
      scene N | frame N        jump to scene N (0-based) / frame N of the current scene
      midiscan DIR             instruments used by all MIDI chunks in the chunky files under DIR (for the soundfont)
      release                  hand the mouse back to the real user
      echo TEXT
***************************************************************************/
#include "studio.h"
#include "testhooks.h"
#include "sndma.h"
#ifdef KAUAI_MIDI_FLUIDSYNTH
uint64_t CframeMidiAudible(bool fReset); // midistreamfluidsynth.cpp
#endif
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "fileremote.h"
#endif
ASSERTNAME

namespace fs = std::filesystem;

namespace
{
enum class SK
{
    MouseTo,
    MouseBy,
    Down,
    Up,
    Wait,
    KeyDown,
    KeyUp,
    Mods,
    Shot,
    State,
    Open,
    Release,
    Echo,
    WaitFor,
    Scene,
    Frame,
    MidiScan,
    Error
};

struct Step
{
    SK sk;
    int32_t a = 0, b = 0;
    std::string str;
};

struct Batch
{
    int id = 0;
    std::string name;
    std::deque<Step> steps;
    std::vector<std::string> results; // JSON objects
    std::vector<std::string> errors;
    size_t cchText = 0, csteps = 0;
};

std::deque<Batch> g_queue;
Batch g_cur;
bool g_fBusy = false;
bool g_fReady = false;
uint32_t g_tsNext = 0;
int g_idNext = 1;
std::string g_dir;     // desktop test directory ("" in the browser)
std::string g_dirShots;
std::vector<std::pair<int, std::string>> g_done; // finished batches (browser polling)

std::string JsonStr(const std::string &s)
{
    std::string out = "\"";
    for (unsigned char ch : s)
    {
        switch (ch)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20)
            {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", ch);
                out += buf;
            }
            else
                out += (char)ch;
        }
    }
    return out + "\"";
}

std::string StnToStd(STN *pstn)
{
    U8SZ u8sz;
    pstn->GetUtf8Sz(u8sz);
    return std::string((const char *)u8sz);
}

void Parse(const std::string &text, Batch *pbatch)
{
    std::istringstream lines(text);
    std::string line;
    int nline = 0;
    auto add = [&](SK sk, int32_t a = 0, int32_t b = 0, const std::string &str = "") {
        Step st;
        st.sk = sk;
        st.a = a;
        st.b = b;
        st.str = str;
        pbatch->steps.push_back(st);
    };

    while (std::getline(lines, line))
    {
        nline++;
        size_t hash = line.find('#');
        if (hash != std::string::npos)
            line = line.substr(0, hash);
        std::istringstream tok(line);
        std::string cmd;
        if (!(tok >> cmd))
            continue;
        std::vector<long> n;
        std::vector<std::string> words;
        std::string w;
        while (tok >> w)
        {
            words.push_back(w);
            char *pchEnd = nullptr;
            long v = strtol(w.c_str(), &pchEnd, 10);
            n.push_back(*pchEnd == 0 ? v : 0);
        }
        std::string rest;
        {
            size_t pos = line.find(cmd);
            rest = line.substr(pos + cmd.size());
            size_t first = rest.find_first_not_of(" \t");
            size_t last = rest.find_last_not_of(" \t\r");
            rest = (first == std::string::npos) ? "" : rest.substr(first, last - first + 1);
        }

        if (cmd == "click" && n.size() >= 2)
        {
            add(SK::MouseTo, n[0], n[1]);
            add(SK::Wait, 60);
            add(SK::Down);
            add(SK::Wait, n.size() >= 3 ? n[2] : 150);
            add(SK::Up);
            add(SK::Wait, 120);
        }
        else if (cmd == "drag" && n.size() >= 4)
        {
            int32_t steps = n.size() >= 5 && n[4] > 0 ? n[4] : 12;
            int32_t ms = n.size() >= 6 ? n[5] : 360;
            add(SK::MouseTo, n[0], n[1]);
            add(SK::Wait, 80);
            add(SK::Down);
            add(SK::Wait, 150);
            for (int32_t i = 1; i <= steps; i++)
            {
                int32_t dx = (int32_t)(n[2] * i / steps) - (int32_t)(n[2] * (i - 1) / steps);
                int32_t dy = (int32_t)(n[3] * i / steps) - (int32_t)(n[3] * (i - 1) / steps);
                add(SK::MouseBy, dx, dy);
                add(SK::Wait, ms / steps);
            }
            add(SK::Wait, 100);
            add(SK::Up);
            add(SK::Wait, 150);
        }
        else if (cmd == "move" && n.size() >= 2)
            add(SK::MouseTo, n[0], n[1]);
        else if (cmd == "moveby" && n.size() >= 2)
            add(SK::MouseBy, n[0], n[1]);
        else if (cmd == "down")
            add(SK::Down);
        else if (cmd == "up")
            add(SK::Up);
        else if (cmd == "key" && !words.empty())
        {
            uint32_t grfmod = 0;
            for (size_t i = 1; i < words.size(); i++)
            {
                if (words[i] == "ctrl")
                    grfmod |= KMOD_LCTRL;
                else if (words[i] == "shift")
                    grfmod |= KMOD_LSHIFT;
                else if (words[i] == "alt")
                    grfmod |= KMOD_LALT;
            }
            add(SK::Mods, grfmod);
            add(SK::KeyDown, 0, 0, words[0]);
            add(SK::Wait, 50);
            add(SK::KeyUp, 0, 0, words[0]);
            add(SK::Mods, 0);
            add(SK::Wait, 80);
        }
        else if ((cmd == "keydown" || cmd == "keyup") && !words.empty())
        {
            // hold / release a key (e.g. keydown Up during a drag raises the actor)
            add(cmd == "keydown" ? SK::KeyDown : SK::KeyUp, 0, 0, words[0]);
            add(SK::Wait, 30);
        }
        else if (cmd == "wait" && n.size() >= 1)
            add(SK::Wait, n[0]);
        else if (cmd == "waitfor" && !words.empty())
            add(SK::WaitFor, words.size() >= 2 ? n[1] : 60000, 0, words[0]);
        else if (cmd == "shot" && !words.empty())
            add(SK::Shot, 0, 0, words[0]);
        else if (cmd == "state")
            add(SK::State);
        else if (cmd == "open" && !rest.empty())
            add(SK::Open, 0, 0, rest);
        else if (cmd == "scene" && n.size() >= 1)
            add(SK::Scene, n[0]);
        else if (cmd == "frame" && n.size() >= 1)
            add(SK::Frame, n[0]);
        else if (cmd == "midiscan" && !rest.empty())
            add(SK::MidiScan, 0, 0, rest);
        else if (cmd == "release")
            add(SK::Release);
        else if (cmd == "echo")
            add(SK::Echo, 0, 0, rest);
        else
            add(SK::Error, nline, 0, line);
    }
}


// Building places (inc/buildgob.h) and other screens a test can wait for
struct PLACE
{
    const char *psz;
    int32_t hid;
};
const PLACE kplaces[] = {
    {"login", 0x00010200},       {"closet", 0x00010280},      {"imagin", 0x00010300},      {"street", 0x00010400},
    {"ticket1", 0x00010500},     {"ticket2", 0x00010580},     {"lobby1", 0x00010600},      {"lobby2", 0x00010700},
    {"snackbar", 0x00010800},    {"waiting", 0x00010900},     {"theatre1", 0x00010c00},    {"theatre2", 0x00010d00},
    {"theatre3", 0x00010e00},    {"theatre4", 0x00010f00},    {"backstage1", 0x00011000},  {"backstage2", 0x00011100},
    {"inspiration1", 0x00011400}, {"inspiration2", 0x00011500}, {"inspiration3", 0x00011600}, {"inspiration4", 0x00011700},
    {"studio1", 0x00011800},     {"studio2", 0x00011900},     {"studio3", 0x00011a00},     {"studio4", 0x00011b00},
    {"projects1", 0x00011c00},   {"projects2", 0x00011d00},   {"projects3", 0x00011e00},   {"projects4", 0x00011f00},
    {"map", kidMap},
};

std::vector<std::string> CurrentPlaces(void)
{
    std::vector<std::string> v;
    if (vapp.Pstdio() != pvNil)
        v.push_back("studio");
    if (vapp.Pstdio() != pvNil && vapp.Pstdio()->Pmvie() != pvNil && vapp.Pstdio()->Pmvie()->Pscen() != pvNil)
        v.push_back("scene");
    if (vapp.Pstdio() != pvNil && vapp.Pstdio()->Pmvie() != pvNil && vapp.Pstdio()->Pmvie()->FPlaying())
        v.push_back("playing");
    for (const PLACE &pl : kplaces)
        if (GOB::PgobFromHidScr(pl.hid) != pvNil)
            v.push_back(pl.psz);

    // Speech-bubble menus (help balloons) are how 3DMM asks most questions
    PGOB pgobScreen = GOB::PgobScreen();
    if (pgobScreen != pvNil)
    {
        GTE gte;
        PGOB pgob;
        uint32_t grfgte;
        gte.Init(pgobScreen, fgteNil);
        while (gte.FNextGob(&pgob, &grfgte, fgteNil))
        {
            if ((grfgte & fgtePre) && pgob->FIs(kclsHBAL))
            {
                v.push_back("balloon");
                break;
            }
        }
    }
    return v;
}

// "!place" means "not at place"
bool FAtPlace(const std::string &place)
{
    if (!place.empty() && place[0] == '!')
        return !FAtPlace(place.substr(1));
    for (const std::string &s : CurrentPlaces())
        if (s == place)
            return true;
    return false;
}

// List the instruments used by every MIDI chunk ('MIDS') in the chunky files under a folder: melodic presets
// (bank, program) that play notes with their note range, drum kits, and drum notes. Used to trim the soundfont.
std::string MidiScanJson(const std::string &dir)
{
    struct USE
    {
        int32_t cnote = 0, noteMin = 127, noteMax = 0;
    };
    std::map<std::pair<int, int>, USE> mpprog;  // (bank, program) -> use, melodic channels
    std::map<int, USE> mpkit;                   // drum kit (program on channel 10) -> use
    std::map<int, int32_t> mpdrum;              // drum note -> count
    std::map<std::string, int32_t> mpfilecmids; // file -> number of MIDI chunks
    int32_t cmids = 0;

    std::error_code ec;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    {
        std::string ext = it->path().extension().string();
        for (char &ch : ext)
            ch = (char)tolower((unsigned char)ch);
        if (ext != ".chk" && ext != ".3cn" && ext != ".3th" && ext != ".3mm")
            continue;

        STN stn;
        FNI fni;
        std::string path = it->path().string();
        stn.SetUtf8Sz((PU8SZ)path.c_str());
        if (!fni.FBuildFromPath(&stn))
            continue;
        PCFL pcfl = CFL::PcflOpen(&fni, fcflNil);
        if (pcfl == pvNil)
            continue;

        int32_t cmidsFile = pcfl->CckiCtg(kctgMidi);
        for (int32_t icki = 0; icki < cmidsFile; icki++)
        {
            CKI cki;
            BLCK blck;
            if (!pcfl->FGetCkiCtg(kctgMidi, icki, &cki, pvNil, &blck))
                continue;
            PMIDS pmids = MIDS::PmidsRead(&blck);
            if (pmids == pvNil)
                continue;
            cmids++;

            int rgbank[16] = {0}, rgprog[16] = {0};
            MSTP mstp;
            MIDEV midev;
            mstp.Init(pmids);
            while (mstp.FGetEvent(&midev))
            {
                if (midev.cb <= 0)
                    continue;
                int status = midev.rgbSend[0], ch = status & 0x0f, data1 = midev.rgbSend[1] & 0x7f;
                int data2 = midev.rgbSend[2] & 0x7f;
                switch (status & 0xf0)
                {
                case 0xb0:
                    if (data1 == 0)
                        rgbank[ch] = data2; // bank select (MSB)
                    break;
                case 0xc0:
                    rgprog[ch] = data1;
                    break;
                case 0x90: {
                    if (data2 == 0)
                        break; // note off
                    USE *puse = (ch == 9) ? &mpkit[rgprog[ch]] : &mpprog[{rgbank[ch], rgprog[ch]}];
                    puse->cnote++;
                    puse->noteMin = LwMin(puse->noteMin, data1);
                    puse->noteMax = LwMax(puse->noteMax, data1);
                    if (ch == 9)
                        mpdrum[data1]++;
                    break;
                }
                }
            }
            ReleasePpo(&pmids);
        }
        if (cmidsFile > 0)
            mpfilecmids[it->path().filename().string()] = cmidsFile;
        ReleasePpo(&pcfl);
    }

    std::ostringstream js;
    js << "{\"chunks\":" << cmids << ",\"files\":{";
    bool fFirst = true;
    for (auto &kv : mpfilecmids)
    {
        js << (fFirst ? "" : ",") << JsonStr(kv.first) << ":" << kv.second;
        fFirst = false;
    }
    js << "},\"programs\":[";
    fFirst = true;
    for (auto &kv : mpprog)
    {
        js << (fFirst ? "" : ",") << "{\"bank\":" << kv.first.first << ",\"program\":" << kv.first.second
           << ",\"notes\":" << kv.second.cnote << ",\"min\":" << kv.second.noteMin << ",\"max\":" << kv.second.noteMax
           << "}";
        fFirst = false;
    }
    js << "],\"drumkits\":[";
    fFirst = true;
    for (auto &kv : mpkit)
    {
        js << (fFirst ? "" : ",") << "{\"program\":" << kv.first << ",\"notes\":" << kv.second.cnote << "}";
        fFirst = false;
    }
    js << "],\"drumnotes\":[";
    fFirst = true;
    for (auto &kv : mpdrum)
    {
        js << (fFirst ? "" : ",") << kv.first;
        fFirst = false;
    }
    js << "]}";
    return js.str();
}

// Audio output since the previous state: peak level (0..1), audible ms, mixed ms, Web Audio context state
std::string AudioJson(void)
{
    std::ostringstream js;
    float flPeak = 0;
    uint64_t cframeAudible = 0, cframe = 0;
    PMiniaudioManager pmanager = MiniaudioManager::Pmanager();
    ma_engine *pengine = pmanager != pvNil ? pmanager->Pengine() : pvNil;
    uint32_t rate = pengine != pvNil ? ma_engine_get_sample_rate(pengine) : 0;
    if (pengine != pvNil)
        pmanager->GetMeter(&flPeak, &cframeAudible, &cframe, true);
    js << "{\"peak\":" << flPeak << ",\"audible_ms\":" << (rate ? cframeAudible * 1000 / rate : 0)
       << ",\"mixed_ms\":" << (rate ? cframe * 1000 / rate : 0) << ",\"rate\":" << rate;
#ifdef KAUAI_MIDI_FLUIDSYNTH
    js << ",\"midi_ms\":" << (rate ? CframeMidiAudible(true) * 1000 / rate : 0);
#endif
#ifdef __EMSCRIPTEN__
    // 0 = no context, 1 = suspended, 2 = running, 3 = closed
    int ctx = EM_ASM_INT({
        var m = (typeof window !== 'undefined') ? window.miniaudio : undefined;
        if (!m || !m.devices) return 0;
        for (var i = 0; i < m.devices.length; i++) {
            var d = m.devices[i];
            if (d && d.webaudio) return d.webaudio.state == 'running' ? 2 : (d.webaudio.state == 'closed' ? 3 : 1);
        }
        return 0;
    });
    static const char *const rgpszCtx[] = {"none", "suspended", "running", "closed"};
    js << ",\"context\":\"" << rgpszCtx[ctx] << "\"";
#endif
    js << "}";
    return js.str();
}

std::string StateJson(void)
{
    std::ostringstream js;
    int32_t xp, yp;
    bool fDown;
    TestGetMouse(&xp, &yp, &fDown);
    js << "{\"ticks\":" << SDL_GetTicks() << ",\"mouse\":[" << xp << "," << yp << "," << (fDown ? 1 : 0) << "]";

    PSTDIO pstdio = vapp.Pstdio();
    PMVIE pmvie = (pstdio != pvNil) ? pstdio->Pmvie() : pvNil;
    js << ",\"studio\":" << (pstdio != pvNil ? "true" : "false");
    js << ",\"audio\":" << AudioJson();
#ifdef __EMSCRIPTEN__
    {
        // Content downloaded on demand since startup (see fileremote.h)
        uint64_t cfetch, cbFetched;
        GetRemoteFileStats(&cfetch, &cbFetched);
        js << ",\"content\":{\"fetches\":" << cfetch << ",\"bytes\":" << cbFetched << "}";
    }
#endif
    js << ",\"places\":[";
    {
        std::vector<std::string> places = CurrentPlaces();
        for (size_t i = 0; i < places.size(); i++)
            js << (i ? "," : "") << JsonStr(places[i]);
    }
    js << "]";
    if (pmvie != pvNil)
    {
        STN stn;
        pmvie->GetName(&stn);
        js << ",\"movie\":" << JsonStr(StnToStd(&stn)) << ",\"scene\":" << pmvie->Iscen()
           << ",\"scenes\":" << pmvie->Cscen() << ",\"playing\":" << (pmvie->FPlaying() ? "true" : "false");

        PMVU pmvu = pmvie->PmvuCur();
        RC rcView;
        rcView.Zero();
        if (pmvu != pvNil)
            pmvu->GetRc(&rcView, cooGlobal);
        js << ",\"view\":[" << rcView.xpLeft << "," << rcView.ypTop << "," << rcView.xpRight << "," << rcView.ypBottom
           << "]";

        PSCEN pscen = pmvie->Pscen();
        if (pscen != pvNil)
        {
            js << ",\"frame\":" << pscen->Nfrm() << ",\"frames\":[" << pscen->NfrmFirst() << "," << pscen->NfrmLast()
               << "]";
            PACTR pactrSel = pscen->PactrSelected();
            PGL pgl = pscen->PglRollCall();
            js << ",\"actors\":[";
            for (int32_t i = 0; pgl != pvNil && i < pgl->IvMac(); i++)
            {
                PACTR pactr;
                pgl->Get(i, &pactr);
                if (pactr == pvNil)
                    continue;
                STN stnName;
                pactr->GetName(&stnName);
                int32_t xpC = 0, ypC = 0;
                RC rc;
                rc.Zero();
                bool fInView = pactr->FIsInView();
                pactr->GetCenter(&xpC, &ypC);
                pactr->GetRcBounds(&rc);
                BRS xr, yr, zr;
                pactr->GetXyzWorld(&xr, &yr, &zr);
                if (i > 0)
                    js << ",";
                // center/bounds are in movie-view coordinates; add "view" left/top for screen coordinates
                js << "{\"arid\":" << pactr->Arid() << ",\"name\":" << JsonStr(StnToStd(&stnName))
                   << ",\"selected\":" << (pactr == pactrSel ? "true" : "false")
                   << ",\"inview\":" << (fInView ? "true" : "false") << ",\"center\":[" << xpC << "," << ypC << "]"
                   << ",\"bounds\":[" << rc.xpLeft << "," << rc.ypTop << "," << rc.xpRight << "," << rc.ypBottom
                   << "],\"xyz\":[" << BrScalarToFloat(xr) << "," << BrScalarToFloat(yr) << "," << BrScalarToFloat(zr)
                   << "]}";
            }
            js << "]";
        }
    }
    js << "}";
    return js.str();
}

void Finish(void)
{
    std::ostringstream js;
    js << "{\"id\":" << g_cur.id << ",\"name\":" << JsonStr(g_cur.name) << ",\"chars\":" << g_cur.cchText << ",\"steps\":" << g_cur.csteps
       << ",\"ok\":" << (g_cur.errors.empty() ? "true" : "false")
       << ",\"errors\":[";
    for (size_t i = 0; i < g_cur.errors.size(); i++)
        js << (i ? "," : "") << JsonStr(g_cur.errors[i]);
    js << "],\"results\":[";
    for (size_t i = 0; i < g_cur.results.size(); i++)
        js << (i ? "," : "") << g_cur.results[i];
    js << "]}";

    if (!g_dir.empty())
    {
        std::error_code ec;
        fs::path tmp = fs::path(g_dir) / "out" / (g_cur.name + ".tmp");
        {
            std::ofstream out(tmp, std::ios::binary);
            out << js.str();
        }
        fs::rename(tmp, fs::path(g_dir) / "out" / (g_cur.name + ".json"), ec);
    }
    else
    {
        g_done.emplace_back(g_cur.id, js.str());
        if (g_done.size() > 32)
            g_done.erase(g_done.begin());
    }
    g_fBusy = false;
}

void PollInbox(void)
{
    std::error_code ec;
    fs::path in = fs::path(g_dir) / "in";
    fs::path best;
    for (fs::directory_iterator it(in, ec), end; !ec && it != end; it.increment(ec))
    {
        const fs::path &p = it->path();
        if (p.extension() == ".txt" && (best.empty() || p.filename() < best.filename()))
            best = p;
    }
    if (best.empty())
        return;

    // Read the whole file; if it comes back short (e.g. a virus scanner still has it open), retry next tick
    uintmax_t cbFile = fs::file_size(best, ec);
    if (ec)
        return;
    std::ifstream f(best, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    f.close();
    if (ss.str().size() < cbFile)
        return;
    fs::remove(best, ec);

    Batch batch;
    batch.id = g_idNext++;
    batch.name = best.stem().string();
    std::string text = ss.str();
    batch.cchText = text.size();
    Parse(text, &batch);
    batch.csteps = batch.steps.size();
    g_queue.push_back(std::move(batch));
}

std::string ExpandPath(std::string path)
{
    const std::string key = "$APPDIR";
    size_t pos = path.find(key);
    if (pos == std::string::npos)
        return path;
    FNI fni;
    STN stn;
    std::string dir = ".";
    if (fni.FGetExe() && fni.FSetLeaf(pvNil, kftgDir))
    {
        fni.GetStnPath(&stn);
        dir = StnToStd(&stn);
        while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\'))
            dir.pop_back();
    }
    return path.replace(pos, key.size(), dir);
}

void RunStep(const Step &st, uint32_t ts)
{
    g_tsNext = ts + 16; // let the app process each injected event
    switch (st.sk)
    {
    case SK::MouseTo:
        TestSetInputActive(fTrue);
        TestMouseTo(st.a, st.b);
        break;
    case SK::MouseBy:
        TestSetInputActive(fTrue);
        TestMouseBy(st.a, st.b);
        break;
    case SK::Down:
    case SK::Up:
        TestSetInputActive(fTrue);
        TestMouseButton(st.sk == SK::Down);
        break;
    case SK::Wait:
        g_tsNext = ts + (uint32_t)st.a;
        break;
    case SK::Mods:
        TestSetMods((uint32_t)st.a);
        break;
    case SK::KeyDown:
    case SK::KeyUp:
        if (!FTestKey(st.str.c_str(), st.sk == SK::KeyDown) && st.sk == SK::KeyDown)
            g_cur.errors.push_back("unknown key: " + st.str);
        break;
    case SK::Shot: {
        std::string path = g_dirShots + "/" + st.str + ".bmp";
        bool fOk = FTestDumpScreen(path.c_str());
        g_cur.results.push_back("{\"shot\":" + JsonStr(st.str) + ",\"path\":" + JsonStr(path) +
                                ",\"ok\":" + (fOk ? "true" : "false") + "}");
        break;
    }
    case SK::State:
        g_cur.results.push_back("{\"state\":" + StateJson() + "}");
        break;
    case SK::Open: {
        bool fOk = fFalse;
        PSTDIO pstdio = vapp.Pstdio();
        FNI fni;
        STN stn;
        std::string path = ExpandPath(st.str);
        stn.SetUtf8Sz((PU8SZ)path.c_str());
        if (pstdio != pvNil && fni.FBuildFromPath(&stn))
            fOk = pstdio->FLoadMovie(&fni);
        g_cur.results.push_back("{\"open\":" + JsonStr(path) + ",\"ok\":" + (fOk ? "true" : "false") + "}");
        if (!fOk)
            g_cur.errors.push_back("open failed: " + path);
        g_tsNext = ts + 500;
        break;
    }
    case SK::Scene:
    case SK::Frame: {
        bool fOk = fFalse;
        PSTDIO pstdio = vapp.Pstdio();
        PMVIE pmvie = pstdio != pvNil ? pstdio->Pmvie() : pvNil;
        if (pmvie != pvNil)
        {
            if (st.sk == SK::Scene)
                fOk = pmvie->FSwitchScen(st.a);
            else if (pmvie->Pscen() != pvNil)
                fOk = pmvie->Pscen()->FGotoFrm(st.a);
        }
        if (!fOk)
            g_cur.errors.push_back(std::string(st.sk == SK::Scene ? "scene " : "frame ") + std::to_string(st.a) +
                                   " failed");
        g_tsNext = ts + 300;
        break;
    }
    case SK::MidiScan:
        g_cur.results.push_back("{\"midiscan\":" + MidiScanJson(ExpandPath(st.str)) + "}");
        break;
    case SK::Release:
        TestSetInputActive(fFalse);
        break;
    case SK::Echo:
        g_cur.results.push_back("{\"echo\":" + JsonStr(st.str) + "}");
        break;
    case SK::WaitFor:
        break;
    case SK::Error:
        g_cur.errors.push_back("line " + std::to_string(st.a) + ": cannot parse: " + st.str);
        break;
    }
}

void Tick(void)
{
    uint32_t ts = SDL_GetTicks();
    if (!g_fReady)
    {
        g_fReady = true;
        if (!g_dir.empty())
            std::ofstream(fs::path(g_dir) / "ready") << ts;
    }
    if (!g_fBusy)
    {
        if (!g_dir.empty())
            PollInbox();
        if (g_queue.empty())
            return;
        g_cur = std::move(g_queue.front());
        g_queue.pop_front();
        g_fBusy = true;
        g_tsNext = ts;
    }
    if (!SDL_TICKS_PASSED(ts, g_tsNext))
        return;
    if (g_cur.steps.empty())
    {
        Finish();
        return;
    }
    Step &stFront = g_cur.steps.front();
    if (stFront.sk == SK::WaitFor)
    {
        if (stFront.b == 0)
            stFront.b = (int32_t)(ts + (uint32_t)stFront.a); // deadline, set on first check
        bool fAt = FAtPlace(stFront.str);
        if (!fAt && !SDL_TICKS_PASSED(ts, (uint32_t)stFront.b))
        {
            g_tsNext = ts + 100;
            return;
        }
        g_cur.results.push_back("{\"waitfor\":" + JsonStr(stFront.str) + ",\"ok\":" + (fAt ? "true" : "false") +
                                ",\"ticks\":" + std::to_string(ts) + "}");
        if (!fAt)
            g_cur.errors.push_back("waitfor " + stFront.str + " timed out");
        g_cur.steps.pop_front();
        g_tsNext = ts + 300; // let the screen settle
        return;
    }
    Step st = stFront;
    g_cur.steps.pop_front();
    RunStep(st, ts);
}
} // namespace

/***************************************************************************
    Attach the test driver. Desktop: only when LPP_TEST_DIR is set.
    Browser: always attached (idle until a batch is submitted).
***************************************************************************/
void LppTestInit(void)
{
#ifdef __EMSCRIPTEN__
    g_dirShots = "/lpptest/shots";
    std::error_code ec;
    fs::create_directories(g_dirShots, ec);
    vpfnTestTick = Tick;
#else
    const char *pszDir = getenv("LPP_TEST_DIR");
    if (pszDir == nullptr || *pszDir == 0)
        return;
    g_dir = pszDir;
    g_dirShots = g_dir + "/shots";
    std::error_code ec;
    fs::create_directories(fs::path(g_dir) / "in", ec);
    fs::create_directories(fs::path(g_dir) / "out", ec);
    fs::create_directories(g_dirShots, ec);
    vpfnTestTick = Tick;
#endif
}

bool FLppTestDesktopMode(void)
{
    const char *pszDir = getenv("LPP_TEST_DIR");
    return pszDir != nullptr && *pszDir != 0;
}

#ifdef __EMSCRIPTEN__
extern "C"
{
    EMSCRIPTEN_KEEPALIVE int lpp_test_ready(void)
    {
        return g_fReady ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int lpp_test_submit(const char *pszCommands)
    {
        Batch batch;
        batch.id = g_idNext++;
        batch.name = "web-" + std::to_string(batch.id);
        Parse(pszCommands, &batch);
        int id = batch.id;
        g_queue.push_back(std::move(batch));
        return id;
    }

    EMSCRIPTEN_KEEPALIVE const char *lpp_test_result(int id)
    {
        static std::string s;
        s.clear();
        for (auto &done : g_done)
            if (done.first == id)
                s = done.second;
        return s.c_str();
    }
}
#endif
