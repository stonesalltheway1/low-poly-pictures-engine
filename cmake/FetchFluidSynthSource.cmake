include(FetchContent)

# Build FluidSynth from source (used by the Emscripten build, where no binaries are available).
# The "embedded" OS abstraction needs no threads or GLib: the synth is driven from the audio callback.
# SF2 and SF3 soundfonts are supported (SF3 through the sndfile shim below).
FetchContent_Declare(
    FluidSynth
    URL https://github.com/FluidSynth/fluidsynth/archive/refs/tags/v2.5.2.tar.gz
    URL_HASH SHA256=1DCB13308B3AA383DB658C60A7B63D73D0FF4601CCEE589582BA7A816475410E
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    EXCLUDE_FROM_ALL
)

# SF3 soundfonts store samples as Ogg Vorbis, which FluidSynth decodes through libsndfile. Instead of building
# libsndfile + libogg + libvorbis, provide a tiny libsndfile replacement that decodes with stb_vorbis (public domain).
FetchContent_Declare(
    StbVorbis
    URL "https://raw.githubusercontent.com/nothings/stb/1ee679ca2ef753a528db5ba6801e1067b40481b8/stb_vorbis.c"
    URL_HASH SHA256=4C7CB2FF1F7011E9D67950446B7EB9CA044F2E464D76BFBB0B84DD2E23E65636
    DOWNLOAD_NO_EXTRACT ON
)
FetchContent_MakeAvailable(StbVorbis)
add_library(sndfile_shim STATIC "${CMAKE_CURRENT_LIST_DIR}/sndfile-shim/sndfile_shim.c")
target_include_directories(sndfile_shim
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/sndfile-shim"
    PRIVATE "${stbvorbis_SOURCE_DIR}"
)
# Imported, like a system library, so FluidSynth's install(EXPORT) rules don't require exporting the shim
add_library(SndFile::sndfile INTERFACE IMPORTED GLOBAL)
target_link_libraries(SndFile::sndfile INTERFACE sndfile_shim)
# Package files so FluidSynth's find_package(SndFile) picks up the shim
set(sndfile_shim_package_dir "${CMAKE_BINARY_DIR}/sndfile-shim")
file(WRITE "${sndfile_shim_package_dir}/SndFileConfig.cmake" "set(SndFile_WITH_EXTERNAL_LIBS TRUE)\n")
file(WRITE "${sndfile_shim_package_dir}/SndFileConfigVersion.cmake"
    "set(PACKAGE_VERSION 1.2.2)\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\n")

block()
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS OFF)
    set(osal "embedded" CACHE STRING "FluidSynth OS abstraction" FORCE)
    set(SndFile_DIR "${sndfile_shim_package_dir}")
    set(enable-libsndfile ON)
    foreach(feature
            alsa aufile dbus ipv6 jack ladspa libinstpatch midishare network oss dsound wasapi waveout
            winmidi sdl3 pulseaudio pipewire readline threads openmp native-dls systemd coreaudio coremidi framework)
        set(enable-${feature} OFF)
    endforeach()

    FetchContent_MakeAvailable(FluidSynth)
endblock()

if (NOT TARGET FluidSynth::libfluidsynth)
    add_library(FluidSynth::libfluidsynth ALIAS libfluidsynth)
endif()
set(FluidSynth_FOUND TRUE)
