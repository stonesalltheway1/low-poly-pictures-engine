include(FetchContent)

# SoundFont for FluidSynth (desktop builds). 3DMM_SOUNDFONT picks a file (.sf2 or .sf3), e.g. the same one the web
# build uses so both play identical music; otherwise MuseScore's "MS Basic" is downloaded.
set(3DMM_SOUNDFONT "" CACHE FILEPATH "SoundFont (.sf2/.sf3) for FluidSynth; empty: download MuseScore's MS Basic")

if (3DMM_SOUNDFONT)
    file(TO_CMAKE_PATH "${3DMM_SOUNDFONT}" 3DMM_SOUNDFONT_PATH)
    if (NOT EXISTS "${3DMM_SOUNDFONT_PATH}")
        message(FATAL_ERROR "3DMM_SOUNDFONT not found: ${3DMM_SOUNDFONT_PATH}")
    endif()
else()
    FetchContent_Declare(
        SoundFont
        URL "https://github.com/musescore/MuseScore/raw/refs/tags/v4.7.1/share/sound/MS%20Basic.sf3"
        URL_HASH SHA256=5EA2375E8BD7D8E71DEF1036978C1621E85B66934169B6A2744B27B9B3C2D99C
        DOWNLOAD_NAME "soundfont.sf3"
        DOWNLOAD_NO_EXTRACT ON
    )
    FetchContent_MakeAvailable(SoundFont)
    set(3DMM_SOUNDFONT_PATH "${soundfont_SOURCE_DIR}/soundfont.sf3")
endif()
