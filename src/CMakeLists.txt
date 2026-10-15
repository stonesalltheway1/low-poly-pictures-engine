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

add_executable(studio)
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
    set_target_properties(
        studio
        PROPERTIES WIN32_EXECUTABLE ON
    )
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
        "${PROJECT_SOURCE_DIR}/src/studio/lpptest.cpp"
    )

    # Select portfolio
    if ((WIN32 OR APPLE OR (UNIX AND NOT APPLE)) AND NOT EMSCRIPTEN)
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
    elseif (EMSCRIPTEN)
        # The web page shows the open/save dialog
        target_sources(
            studio
            PRIVATE
            "${PROJECT_SOURCE_DIR}/src/studio/portfweb.cpp"
            "${PROJECT_SOURCE_DIR}/src/studio/webrecovery.cpp"
            "${PROJECT_SOURCE_DIR}/src/studio/webexport.cpp"
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
else()
    # target_chomp_sources normally adds each .cht file's directory to the include path (studio.h needs
    # stdiocrs.h from src/studio); add them explicitly when using precompiled chunky files.
    target_include_directories(studio PRIVATE
        "${PROJECT_SOURCE_DIR}/src/studio"
        "${PROJECT_SOURCE_DIR}/src/building"
        "${PROJECT_SOURCE_DIR}/src/helpaud"
        "${PROJECT_SOURCE_DIR}/src/help"
        "${PROJECT_SOURCE_DIR}/src/shared"
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

if (EMSCRIPTEN)
    # Web build: 3dmovie.html/.js/.wasm/.data. Content and fonts are preloaded into the virtual filesystem
    # under /app (see kauai/src/platemscripten.cpp).
    set(3DMM_WEB_CONTENT_DIR "" CACHE PATH "Directory containing the installed 'Microsoft Kids' content to preload")
    set(3DMM_WEB_FONTS_DIR "" CACHE PATH "Directory containing .ttf fonts to bundle (e.g. Comic Neue)")
    set_target_properties(studio PROPERTIES SUFFIX ".html")
    target_link_options(studio PRIVATE
        "-sASYNCIFY"
        "-sASYNCIFY_STACK_SIZE=1048576"
        "-sSTACK_SIZE=8388608"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=268435456"
        "-sFORCE_FILESYSTEM=1"
        "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,FS,callMain,addRunDependency,removeRunDependency"
        "-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$stringToNewUTF8"
        "-lidbfs.js"
        # Keep the preloaded files (fonts, soundfont: ~10 MB) in IndexedDB, keyed by a hash of their contents, so
        # return visits don't download them again (browsers with small or in-memory HTTP caches don't keep them)
        "--use-preload-cache"
    )
    set(3DMM_WEB_SHELL_FILE "" CACHE FILEPATH "HTML page template for the web build (default: Emscripten's)")
    if (3DMM_WEB_SHELL_FILE)
        target_link_options(studio PRIVATE "SHELL:--shell-file \"${3DMM_WEB_SHELL_FILE}\"")
        set_property(TARGET studio APPEND PROPERTY LINK_DEPENDS "${3DMM_WEB_SHELL_FILE}")
    endif()
    option(3DMM_WEB_STREAM_CONTENT "Read content files from the web server on demand instead of preloading them" ON)
    if (3DMM_WEB_CONTENT_DIR AND 3DMM_WEB_STREAM_CONTENT)
        # Content stays on the server, under content/Microsoft Kids/... next to the page; the build only preloads
        # a list of the files and their sizes (see kauai/src/fileremote.h)
        file(GLOB_RECURSE web_content_files LIST_DIRECTORIES false RELATIVE "${3DMM_WEB_CONTENT_DIR}"
            "${3DMM_WEB_CONTENT_DIR}/*")
        list(SORT web_content_files)
        set(web_manifest "")
        foreach(web_file IN LISTS web_content_files)
            file(SIZE "${3DMM_WEB_CONTENT_DIR}/${web_file}" web_file_size)
            string(APPEND web_manifest "${web_file_size}\tMicrosoft Kids/${web_file}\n")
        endforeach()
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/remote-files.txt.new" "${web_manifest}")
        configure_file("${CMAKE_CURRENT_BINARY_DIR}/remote-files.txt.new" "${CMAKE_CURRENT_BINARY_DIR}/remote-files.txt"
            COPYONLY)
        target_link_options(studio PRIVATE
            "SHELL:--preload-file \"${CMAKE_CURRENT_BINARY_DIR}/remote-files.txt@/app/remote-files.txt\""
        )
        set_property(TARGET studio APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/remote-files.txt")
    elseif (3DMM_WEB_CONTENT_DIR)
        # Preload all content (the .avi intro/help videos must be present: the app looks them up even though video
        # playback is not implemented on the web yet)
        target_link_options(studio PRIVATE
            "SHELL:--preload-file \"${3DMM_WEB_CONTENT_DIR}@/app/Microsoft Kids\""
        )
    endif()
    if (3DMM_WEB_FONTS_DIR)
        target_link_options(studio PRIVATE "SHELL:--preload-file \"${3DMM_WEB_FONTS_DIR}@/app/fonts\"")
    endif()
    if (3DMM_SOUNDFONT_PATH)
        # MIDI soundfont, found by FMS in the resources directory (/app/soundfont.sf3 or .sf2)
        get_filename_component(web_soundfont_ext "${3DMM_SOUNDFONT_PATH}" LAST_EXT)
        string(TOLOWER "${web_soundfont_ext}" web_soundfont_ext)
        target_link_options(studio PRIVATE
            "SHELL:--preload-file \"${3DMM_SOUNDFONT_PATH}@/app/soundfont${web_soundfont_ext}\""
        )
        set_property(TARGET studio APPEND PROPERTY LINK_DEPENDS "${3DMM_SOUNDFONT_PATH}")
    endif()
endif()

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

if (APPLE)
    set(icon_name "app.icns")
    set(icon_path "${PROJECT_SOURCE_DIR}/src/studio/macos/${icon_name}")
    target_sources(
        studio
        PRIVATE
        ${icon_path}
    )

    set_target_properties(
        studio
        PROPERTIES
        OUTPUT_NAME "3D Movie Maker"
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/src/studio/macos/Info.plist.in"
        MACOSX_BUNDLE_EXECUTABLE_NAME "3D Movie Maker"
        MACOSX_BUNDLE_BUNDLE_NAME "3DMMEx"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.3dmmex.3dmovie"
        MACOSX_BUNDLE_BUNDLE_VERSION "${GIT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE ${icon_name}
        RESOURCE ${icon_path}
    )

    add_custom_command(
        TARGET studio
        POST_BUILD
        COMMAND plutil -lint "$<TARGET_BUNDLE_CONTENT_DIR:studio>/Info.plist"
        COMMENT "Validating generated plist"
        VERBATIM
    )
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

if(APPLE)
    set(resources_path "./3D Movie Maker.app/Contents/Resources")
else()
    set(resources_path ".")
endif()
set(resources_path "$<PATH:NORMAL_PATH,${resources_path}>")

set(mskids_path "${resources_path}/Microsoft Kids")
set(mskids_3dmm_path "${mskids_path}/3D Movie Maker")
set(mczee_path "${mskids_path}/Users/McZee")
set(melanie_path "${mskids_path}/Users/Melanie")

install(TARGETS studio
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)

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
            DESTINATION "${mskids_3dmm_path}"
        )
    endforeach()
else()
    # HACK(bruxisma): This is due to UTEST.CHT not generating 3DMOVIE.CHK
    #                 Renaming it to 3DMOVIE.CHT would resolve this.
    install(FILES $<FILTER:$<TARGET_PROPERTY:studio,CHOMPED_CHUNKS>,EXCLUDE,.+utest[.]chk$>
      DESTINATION "${mskids_3dmm_path}")
    install(FILES $<FILTER:$<TARGET_PROPERTY:studio,CHOMPED_CHUNKS>,INCLUDE,.+utest[.]chk$>
      RENAME 3dmovie.chk
      DESTINATION "${mskids_3dmm_path}")
endif()
install(FILES $<TARGET_PROPERTY:studio,3DMM_PROJECTS>
  DESTINATION "${melanie_path}")
install(FILES $<TARGET_PROPERTY:studio,3DMM_SAMPLES>
  DESTINATION "${mczee_path}")
install(FILES $<TARGET_PROPERTY:studio,3DMM_CONTENT>
  DESTINATION "${mskids_3dmm_path}")

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
            DESTINATION "${mskids_3dmm_path}"
        )
    endforeach()

endif()

if (WIN32 AND 3DMM_GUI STREQUAL "SDL")
    # Install SDL DLLs
    install(FILES $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE:SDL2_ttf::SDL2_ttf>
        DESTINATION ${resources_path})

    if (FluidSynth_FOUND)
        # Install FluidSynth DLLs
        install(DIRECTORY "${FluidSynth_ROOT}/bin/" DESTINATION ${resources_path}
            PATTERN "*.exe" EXCLUDE)
    endif()
endif()

# Copy soundfont if required (FMS looks for soundfont.sf3, then soundfont.sf2)
if(NOT "${3DMM_SOUNDFONT_PATH}" STREQUAL "" AND NOT EMSCRIPTEN)
    get_filename_component(soundfont_ext "${3DMM_SOUNDFONT_PATH}" LAST_EXT)
    string(TOLOWER "${soundfont_ext}" soundfont_ext)
    install(FILES "${3DMM_SOUNDFONT_PATH}" DESTINATION ${resources_path} RENAME "soundfont${soundfont_ext}")
endif()

if (APPLE)

    set(MACOSX_CODESIGNING_IDENTITY "-" CACHE STRING "Code signing identity used for signing bundle")
    if (MACOSX_CODESIGNING_IDENTITY STREQUAL "-")
        message("Using ad-hoc signing for bundle")
    endif()

    install(CODE "
        include(BundleUtilities)

        get_filename_component(install_prefix \"\${CMAKE_INSTALL_PREFIX}\" ABSOLUTE)
        fixup_bundle(
            \"\${install_prefix}/3D Movie Maker.app\"
            \"\"
            \"${DOWNLOADED_FRAMEWORKS_PATH}\"
        )

        execute_process(
            COMMAND codesign --force --deep --sign \"${MACOSX_CODESIGNING_IDENTITY}\" \"\${install_prefix}/3D Movie Maker.app\"
            COMMAND_ERROR_IS_FATAL ANY
        )
    ")
endif()
