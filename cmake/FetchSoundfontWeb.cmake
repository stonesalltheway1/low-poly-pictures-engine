include(FetchContent)

# SoundFont for the Emscripten build. FluidSynth is built without libsndfile there, so it can't read the
# compressed .sf3 used by the desktop builds: use GeneralUser GS (SF2, ~31 MB) by S. Christian Collins,
# licensed for use in software projects (see its documentation/LICENSE.txt).
FetchContent_Declare(
    SoundFontWeb
    URL "https://raw.githubusercontent.com/mrbumpy409/GeneralUser-GS/684543d5e5efaef08d02be50dcda8d552478fa60/GeneralUser-GS.sf2"
    URL_HASH SHA256=9575028C7A1F589F5770FCCC8CFF2734566AF40CD26ED836944E9A5152688CFE
    DOWNLOAD_NAME "soundfont.sf2"
    DOWNLOAD_NO_EXTRACT ON
)
FetchContent_MakeAvailable(SoundFontWeb)
set(3DMM_SOUNDFONT_PATH "${soundfontweb_SOURCE_DIR}/soundfont.sf2")
