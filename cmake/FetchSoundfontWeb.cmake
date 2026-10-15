include(FetchContent)

# SoundFont for the Emscripten build (SF2 or SF3; FluidSynth reads SF3 through the sndfile shim).
# 3DMM_WEB_SOUNDFONT picks a file, e.g. a compressed SF3 made for the web; otherwise the build downloads
# GeneralUser GS (SF2, ~31 MB) by S. Christian Collins, licensed for use in software projects (see its
# documentation/LICENSE.txt).
set(3DMM_WEB_SOUNDFONT "" CACHE FILEPATH "SoundFont (.sf2/.sf3) for the web build; empty: download GeneralUser GS")

if (3DMM_WEB_SOUNDFONT)
    set(3DMM_SOUNDFONT_PATH "${3DMM_WEB_SOUNDFONT}")
else()
    FetchContent_Declare(
        SoundFontWeb
        URL "https://raw.githubusercontent.com/mrbumpy409/GeneralUser-GS/684543d5e5efaef08d02be50dcda8d552478fa60/GeneralUser-GS.sf2"
        URL_HASH SHA256=9575028C7A1F589F5770FCCC8CFF2734566AF40CD26ED836944E9A5152688CFE
        DOWNLOAD_NAME "soundfont.sf2"
        DOWNLOAD_NO_EXTRACT ON
    )
    FetchContent_MakeAvailable(SoundFontWeb)
    set(3DMM_SOUNDFONT_PATH "${soundfontweb_SOURCE_DIR}/soundfont.sf2")
endif()
