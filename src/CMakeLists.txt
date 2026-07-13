# 3D Movie Maker

add_library(bren)
target_sources(bren
    PRIVATE
    "${PROJECT_SOURCE_DIR}/bren/bwld.cpp"
    "${PROJECT_SOURCE_DIR}/bren/stderr.c"
    "${PROJECT_SOURCE_DIR}/bren/stdfile.c"
    "${PROJECT_SOURCE_DIR}/bren/tmap.cpp"
    "${PROJECT_SOURCE_DIR}/bren/zbmp.cpp"
)

if (WIN32)
    target_sources(
        bren
        PRIVATE
        "${PROJECT_SOURCE_DIR}/bren/stdmemwin.c"
    )
endif()

target_include_directories(bren PUBLIC "${PROJECT_SOURCE_DIR}/bren/inc")
target_link_libraries(bren
    PUBLIC
    KauaiBase
    KauaiGroup
    BRender::Libraries
)

add_library(engine)
target_sources(engine
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src/engine/actor.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/actredit.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/actrsave.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/actrsnd.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/bkgd.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/body.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/modl.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/movie.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/msnd.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/mtrl.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/scene.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/tagl.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/tagman.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/tbox.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/tdf.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/tdt.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/tmpl.cpp"
)
target_link_libraries(
    engine
    PRIVATE
    miniaudio
)
if (3DMM_GUI STREQUAL "Win32" AND NOT 3DMM_AUDIOMAN_LIBRARY STREQUAL "None")
    target_sources(engine
        PRIVATE
        "${PROJECT_SOURCE_DIR}/src/engine/srecwin.cpp"
    )
elseif(3DMM_GUI STREQUAL "SDL")
    target_sources(engine
        PRIVATE
        "${PROJECT_SOURCE_DIR}/src/engine/srecma.cpp"
    )
else()
    target_sources(engine
        PRIVATE
        "${PROJECT_SOURCE_DIR}/src/engine/srecstub.cpp"
    )
endif()

target_include_directories(engine PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/inc>)
target_link_libraries(engine
    PUBLIC
    KauaiBase
    KauaiGui
    KauaiDoc
    KauaiRichText
    bren
)
if (NOT "${3DMM_AUDIOMAN_LIBRARY}" STREQUAL "None")
    target_link_libraries(engine
        PUBLIC
        3DMMForever::AudioMan
    )
endif()

# On non-windows WIN32 is a no-op
add_executable(studio WIN32)
target_sources(studio
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src/studio/ape.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/appicon.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/browser.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/configini.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/esl.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/popup.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/scnsort.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/splot.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/stdiobrw.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/stdioscb.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/studio.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/tatr.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/tgob.cpp"
    "${PROJECT_SOURCE_DIR}/src/studio/utest.cpp"
)
if (WIN32)
    target_sources(
        studio
        PRIVATE
        "${PROJECT_SOURCE_DIR}/src/studio/mminstal.cpp"
        "${PROJECT_SOURCE_DIR}/src/studio/utest.rc"
        "${PROJECT_SOURCE_DIR}/src/studio/utest.rc2"
    )
endif()

if(3DMM_GUI STREQUAL "SDL")
    target_sources(
        studio
        PRIVATE
        "${PROJECT_SOURCE_DIR}/src/studio/dlgsdl.cpp"
    )

    # Select portfolio
    if (WIN32 OR APPLE OR (UNIX AND NOT APPLE))
        # Use Native File Dialog Extended for file dialogs
        include(FetchNativeFileDialogExtended)
        target_sources(
            studio
            PRIVATE
            "${PROJECT_SOURCE_DIR}/src/studio/portfnative.cpp"
        )
        target_link_libraries(
            studio
            PRIVATE
            nfd::nfd
        )
    else()
        # Portfolio not supported
        target_sources(
            studio
            PRIVATE
            "${PROJECT_SOURCE_DIR}/src/studio/portfstub.cpp"
        )
    endif()
elseif(3DMM_GUI STREQUAL "Win32")
    # Use original Win32 portfolio
    target_sources(
        studio
        PRIVATE
        "${PROJECT_SOURCE_DIR}/src/studio/portfwin.cpp"
    )
endif()

target_include_directories(studio
  PUBLIC $<TARGET_PROPERTY:engine,INCLUDE_DIRECTORIES>
  "${PROJECT_BINARY_DIR}/inc"
  "${PROJECT_SOURCE_DIR}/src")

target_compile_definitions(
    studio
    PRIVATE
    NAMES # Adds names to chunks in Chunky files
)

# These are only here to ensure configure is rerun when dependencies are generated.
file(GLOB building-chunk-sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/building/*.cht")
file(GLOB studio-chunk-sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/studio/*.cht")

if (NOT 3DMM_PRECOMPILED_CHUNKY_PATH)
    target_chomp_sources(studio
        "${PROJECT_SOURCE_DIR}/src/studio/utest.cht"
        "${PROJECT_SOURCE_DIR}/src/studio/studio.cht"
        "${PROJECT_SOURCE_DIR}/src/building/bldghd.cht"
        "${PROJECT_SOURCE_DIR}/src/building/building.cht"
        "${PROJECT_SOURCE_DIR}/src/helpaud/helpaud.cht"
        "${PROJECT_SOURCE_DIR}/src/help/help.cht"
        "${PROJECT_SOURCE_DIR}/src/shared/shared.cht"
        "${PROJECT_SOURCE_DIR}/src/shared/sharecd.cht"
    )
endif()

target_link_libraries(studio
    PUBLIC
    KauaiBase
    KauaiGui
    KauaiGuiMain
    KauaiKid
    KauaiDlg
    engine

    iniparser-static
)
if (WIN32)
    target_link_libraries(studio
        PUBLIC
        mpr     # Required for WNetGetUser()
        vfw32   # Required for ICInfo in _wHaveICMCodec
        msacm32 # Required for acmFormatTagDetailsA in _wHaveACMCodec
    )
endif()

set_property(TARGET studio PROPERTY OUTPUT_NAME 3dmovie)

if(MSVC)
    target_link_options(studio
        BEFORE PRIVATE
        "/MANIFESTUAC:NO"
    )
    target_link_options(studio
        BEFORE PRIVATE
        "/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
    )

    if ("${3DMM_AUDIOMAN_LIBRARY}" STREQUAL "Original")
        # Avoid LNK4098 warning when linking original AudioMan libraries
        target_link_options(studio
            BEFORE PRIVATE
            $<$<CONFIG:DEBUG>:/NODEFAULTLIB:libcmt.lib>
        )
    endif()
endif()

set_property(
    TARGET studio
    PROPERTY 3DMM_SAMPLES
    ${studio-samples}
    "${PROJECT_SOURCE_DIR}/samples/bongo.3mm"
    "${PROJECT_SOURCE_DIR}/samples/boooooo.3mm"
    "${PROJECT_SOURCE_DIR}/samples/citytour.3mm"
    "${PROJECT_SOURCE_DIR}/samples/graveyrd.3mm"
    "${PROJECT_SOURCE_DIR}/samples/haunted.3mm"
    "${PROJECT_SOURCE_DIR}/samples/hospital.3mm"
    "${PROJECT_SOURCE_DIR}/samples/jungle.3mm"
    "${PROJECT_SOURCE_DIR}/samples/message.3mm"
    "${PROJECT_SOURCE_DIR}/samples/space.3mm"
    "${PROJECT_SOURCE_DIR}/samples/sprog.3mm"
    "${PROJECT_SOURCE_DIR}/samples/terror.3mm"
    "${PROJECT_SOURCE_DIR}/samples/thebook.3mm"
    "${PROJECT_SOURCE_DIR}/samples/thelodge.3mm"
    "${PROJECT_SOURCE_DIR}/samples/thethief.3mm"
    "${PROJECT_SOURCE_DIR}/samples/venus31.3mm"
    "${PROJECT_SOURCE_DIR}/samples/where.3mm"
)
set_property(
    TARGET studio
    PROPERTY 3DMM_PROJECTS
    "${PROJECT_SOURCE_DIR}/projects/bongo.3mm"
    "${PROJECT_SOURCE_DIR}/projects/bridge.3mm"
    "${PROJECT_SOURCE_DIR}/projects/dialogue.3mm"
    "${PROJECT_SOURCE_DIR}/projects/jog.3mm"
    "${PROJECT_SOURCE_DIR}/projects/mummy.3mm"
    "${PROJECT_SOURCE_DIR}/projects/surprise.3mm"
)
set_property(
    TARGET studio
    PROPERTY 3DMM_CONTENT
    "${PROJECT_SOURCE_DIR}/content-files/3dmovie.ms"
    "${PROJECT_SOURCE_DIR}/content-files/actor.3th"
    "${PROJECT_SOURCE_DIR}/content-files/actresl.3th"
    "${PROJECT_SOURCE_DIR}/content-files/bkgds.3th"
    "${PROJECT_SOURCE_DIR}/content-files/mtrl.3th"
    "${PROJECT_SOURCE_DIR}/content-files/bkgds.3cn"
    "${PROJECT_SOURCE_DIR}/content-files/mtrls.3cn"
    "${PROJECT_SOURCE_DIR}/content-files/prop.3th"
    "${PROJECT_SOURCE_DIR}/content-files/snds.3cn"
    "${PROJECT_SOURCE_DIR}/content-files/sound.3th"
    "${PROJECT_SOURCE_DIR}/content-files/tbox.3th"
    "${PROJECT_SOURCE_DIR}/content-files/tdfs.3cn"
    "${PROJECT_SOURCE_DIR}/content-files/tmpls.3cn"
)
set_property(
    TARGET studio
    PROPERTY 3DMM_CUTSCENES
    "${PROJECT_SOURCE_DIR}/content-files/04logn3.avi"
    "${PROJECT_SOURCE_DIR}/content-files/05logn4.avi"
    "${PROJECT_SOURCE_DIR}/content-files/backstag.avi"
    "${PROJECT_SOURCE_DIR}/content-files/cntdwn5.avi"
    "${PROJECT_SOURCE_DIR}/content-files/goida.avi"
    "${PROJECT_SOURCE_DIR}/content-files/goprj.avi"
    "${PROJECT_SOURCE_DIR}/content-files/gostu.avi"
    "${PROJECT_SOURCE_DIR}/content-files/idea2.avi"
    "${PROJECT_SOURCE_DIR}/content-files/idea3.avi"
    "${PROJECT_SOURCE_DIR}/content-files/idea4.avi"
    "${PROJECT_SOURCE_DIR}/content-files/logn1.avi"
    "${PROJECT_SOURCE_DIR}/content-files/logn5.avi"
    "${PROJECT_SOURCE_DIR}/content-files/logn06.avi"
    "${PROJECT_SOURCE_DIR}/content-files/mhole2.avi"
    "${PROJECT_SOURCE_DIR}/content-files/proj1.avi"
    "${PROJECT_SOURCE_DIR}/content-files/proj2.avi"
    "${PROJECT_SOURCE_DIR}/content-files/proj3.avi"
    "${PROJECT_SOURCE_DIR}/content-files/proj4.avi"
    "${PROJECT_SOURCE_DIR}/content-files/proj5.avi"
    "${PROJECT_SOURCE_DIR}/content-files/proj6.avi"
    "${PROJECT_SOURCE_DIR}/content-files/projt2.avi"
    "${PROJECT_SOURCE_DIR}/content-files/ticket1.avi"
    "${PROJECT_SOURCE_DIR}/content-files/ticket2.avi"
    "${PROJECT_SOURCE_DIR}/content-files/tools1.avi"
)

# Please pay attention to the ending / in each DIRECTORY declaration. It
# affects the destination!
install(TARGETS studio RUNTIME DESTINATION .)

if (3DMM_PRECOMPILED_CHUNKY_PATH)
    set(chunky_files
        3dmovie.chk
        bldghd.chk
        building.chk
        help.chk
        helpaud.chk
        sharecd.chk
        shared.chk
        studio.chk
    )
    foreach(chunky_file ${chunky_files})
        install(
            FILES "${3DMM_PRECOMPILED_CHUNKY_PATH}/${chunky_file}"
            DESTINATION "Microsoft Kids/3D Movie Maker"
        )
    endforeach()
else()
    # HACK(bruxisma): This is due to UTEST.CHT not generating 3DMOVIE.CHK
    #                 Renaming it to 3DMOVIE.CHT would resolve this.
    install(FILES $<FILTER:$<TARGET_PROPERTY:studio,CHOMPED_CHUNKS>,EXCLUDE,.+utest[.]chk$>
      DESTINATION "Microsoft Kids/3D Movie Maker")
    install(FILES $<FILTER:$<TARGET_PROPERTY:studio,CHOMPED_CHUNKS>,INCLUDE,.+utest[.]chk$>
      RENAME 3dmovie.chk
      DESTINATION "Microsoft Kids/3D Movie Maker")
endif()
install(FILES $<TARGET_PROPERTY:studio,3DMM_PROJECTS>
  DESTINATION "Microsoft Kids/Users/Melanie")
install(FILES $<TARGET_PROPERTY:studio,3DMM_SAMPLES>
  DESTINATION "Microsoft Kids/Users/McZee")
install(FILES $<TARGET_PROPERTY:studio,3DMM_CONTENT>
  DESTINATION "Microsoft Kids/3D Movie Maker")

if(KAUAI_HAS_VIDEO)
    install(
        FILES $<TARGET_PROPERTY:studio,3DMM_CUTSCENES>
        DESTINATION "Microsoft Kids/3D Movie Maker"
    )
else()
    # Generate placeholder files to avoid "I can't find this CD" errors
    set(placeholder_avi "${CMAKE_CURRENT_BINARY_DIR}/placeholder.avi")
    file(TOUCH "${placeholder_avi}")

    get_property(cutscene_paths
        TARGET studio
        PROPERTY 3DMM_CUTSCENES
    )

    foreach(cutscene_path ${cutscene_paths})
        cmake_path(GET cutscene_path FILENAME cutscene_name)
        install(
            FILES "${placeholder_avi}"
            RENAME "${cutscene_name}"
            DESTINATION "Microsoft Kids/3D Movie Maker"
        )
    endforeach()

endif()

if (WIN32 AND 3DMM_GUI STREQUAL "SDL")
    # Install SDL DLLs
    install(FILES $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE:SDL2_ttf::SDL2_ttf>
        DESTINATION .)

    if (FluidSynth_FOUND)
        # Install FluidSynth DLLs
        install(DIRECTORY "${FluidSynth_ROOT}/bin/" DESTINATION .
            PATTERN "*.exe" EXCLUDE)
    endif()
endif()

# Copy soundfont if required
if(NOT "${3DMM_SOUNDFONT_PATH}" STREQUAL "")
    install(FILES "${3DMM_SOUNDFONT_PATH}" DESTINATION .)
endif()
