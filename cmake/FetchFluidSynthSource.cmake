include(FetchContent)

# Build FluidSynth from source (used by the Emscripten build, where no binaries are available).
# The "embedded" OS abstraction needs no threads or GLib: the synth is driven from the audio callback.
FetchContent_Declare(
    FluidSynth
    URL https://github.com/FluidSynth/fluidsynth/archive/refs/tags/v2.5.2.tar.gz
    URL_HASH SHA256=1DCB13308B3AA383DB658C60A7B63D73D0FF4601CCEE589582BA7A816475410E
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    EXCLUDE_FROM_ALL
)

block()
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS OFF)
    set(osal "embedded" CACHE STRING "FluidSynth OS abstraction" FORCE)
    foreach(feature
            alsa aufile dbus ipv6 jack ladspa libinstpatch libsndfile midishare network oss dsound wasapi waveout
            winmidi sdl3 pulseaudio pipewire readline threads openmp native-dls systemd coreaudio coremidi framework)
        set(enable-${feature} OFF)
    endforeach()

    FetchContent_MakeAvailable(FluidSynth)
endblock()

if (NOT TARGET FluidSynth::libfluidsynth)
    add_library(FluidSynth::libfluidsynth ALIAS libfluidsynth)
endif()
set(FluidSynth_FOUND TRUE)
