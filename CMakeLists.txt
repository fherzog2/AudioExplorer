cmake_minimum_required(VERSION 3.21)

project(AudioExplorer)

option(SANITIZE_ADDRESS "Add address sanitizer flags" OFF)

find_path(TAGLIB_INCLUDE_DIR
  NAMES taglib/tag.h
)
find_library(TAGLIB_LIBRARY
  NAMES tag
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Linguist)
find_package(Threads REQUIRED)
find_package(GTest CONFIG REQUIRED)

if(WIN32)
    set(WIN_RESOURCE_FILE res/appicon.rc
                          src/project_version.rc)
endif()

# add our own translations

#set(RECREATE_TRANSLATIONS ON)

set(TS_FILES ${CMAKE_SOURCE_DIR}/translations/AudioExplorer_de.ts)
if(RECREATE_TRANSLATIONS)
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR}/src ${TS_FILES} OPTIONS -no-obsolete -locations none)
else()
    qt_add_translation(QM_FILES ${TS_FILES})
endif()

add_executable(AudioExplorer WIN32 src/main.cpp
                                   src/MainWindow.cpp
                                   src/MainWindow.h
                                   src/AudioLibrary.cpp
                                   src/AudioLibrary.h
                                   src/AudioLibraryModel.cpp
                                   src/AudioLibraryModel.h
                                   src/AudioLibraryView.cpp
                                   src/AudioLibraryView.h
                                   src/DetailsPane.cpp
                                   src/DetailsPane.h
                                   src/ImageViewWindow.cpp
                                   src/ImageViewWindow.h
                                   src/project_version.h
                                   src/Settings.h
                                   src/Settings.cpp
                                   src/SettingsEditorWindow.cpp
                                   src/SettingsEditorWindow.h
                                   src/ThreadSafeAudioLibrary.cpp
                                   src/ThreadSafeAudioLibrary.h
                                   src/TrackInfoReader.cpp
                                   src/TrackInfoReader.h
                                   ${TS_FILES}
                                   ${WIN_RESOURCE_FILE})
target_link_libraries(AudioExplorer ${CMAKE_THREAD_LIBS_INIT} Qt::Widgets Qt::Svg ${TAGLIB_LIBRARY})
target_include_directories(AudioExplorer PRIVATE ${TAGLIB_INCLUDE_DIR})
set_property(TARGET AudioExplorer PROPERTY CXX_STANDARD 20)
set_property(TARGET AudioExplorer PROPERTY AUTOMOC ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND SANITIZE_ADDRESS)
    target_compile_options(AudioExplorer PRIVATE -fno-omit-frame-pointer -fsanitize=address)
    target_link_options(AudioExplorer PRIVATE -fno-omit-frame-pointer -fsanitize=address)
endif()

if(MSVC)
  target_compile_options(AudioExplorer PRIVATE /W4 /WX)
else(MSVC)
  target_compile_options(AudioExplorer PRIVATE -Wall -Wextra -pedantic -Werror)
endif(MSVC)

set_source_files_properties(${CMAKE_BINARY_DIR}/AudioExplorer_de.qm PROPERTIES QT_RESOURCE_ALIAS res/AudioExplorer_de.qm)

qt_add_resources(AudioExplorer "appresources"
    PREFIX "/"
    FILES res/view_menu.svg ${QM_FILES})

if (WIN32)
    # copy Qt plugins and translations to bin directory

    find_program(WINDEPLOYQT_DEBUG NAMES windeployqt.debug.bat)
    find_program(WINDEPLOYQT_RELEASE NAMES windeployqt)

    # write a "deployed" marker after qtwindeploy is done, to avoid unnecessary reruns

    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/$<CONFIG>/deployed.txt"
        COMMAND $<IF:$<CONFIG:Debug>,${WINDEPLOYQT_DEBUG},${WINDEPLOYQT_RELEASE}> $<TARGET_FILE:AudioExplorer>
        COMMAND ${CMAKE_COMMAND} -E echo "done" > "${CMAKE_BINARY_DIR}/$<CONFIG>/deployed.txt"
        COMMENT "Running windeployqt..."
        VERBATIM
    )

    add_custom_target(QT_WIN_DEPLOY ALL
        DEPENDS "${CMAKE_BINARY_DIR}/$<CONFIG>/deployed.txt"
    )

    # add qt.conf, to get correct qt translation paths when starting the app from the build directory

    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/$<CONFIG>/qt.conf"
        COMMAND ${CMAKE_COMMAND} -E echo "[Paths] Prefix = ." > "${CMAKE_BINARY_DIR}/$<CONFIG>/qt.conf"
        COMMENT "Adding qt.conf"
        VERBATIM
    )

    add_custom_target(QT_CONF_DEPLOY ALL
        DEPENDS "${CMAKE_BINARY_DIR}/$<CONFIG>/qt.conf"
    )

    # vcpkg automatically copies DLLs of all dependencies into the build dir
    # unfortunately this doesn't work for libjpeg, because Qt technically can run without it (plugin system)
    # manually copy the libjpeg DLL here to fix this

    find_package(JPEG REQUIRED)

    get_target_property(MY_JPEG_LIB_PATH JPEG::JPEG IMPORTED_LOCATION_RELEASE)

    cmake_path(GET MY_JPEG_LIB_PATH PARENT_PATH MY_JPEG_LIB_DIR)

    find_file(MY_JPEG_DLL
        NAMES "jpeg${JPEG_VERSION}.dll"
        PATHS "${MY_JPEG_LIB_DIR}\\..\\bin"
        REQUIRED NO_DEFAULT_PATH)

    add_custom_command(TARGET AudioExplorer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${MY_JPEG_DLL}
        "$<TARGET_FILE_DIR:AudioExplorer>\\jpeg${JPEG_VERSION}.dll"
        COMMENT "Copying libjpeg to build dir..."
    )
endif()

# test

configure_file(${CMAKE_SOURCE_DIR}/test/AudioLibraryViews.txt ${CMAKE_CURRENT_BINARY_DIR}/test_data/AudioLibraryViews.txt COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/test/data/gradient.jpg ${CMAKE_CURRENT_BINARY_DIR}/test_data/gradient.jpg COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/test/data/noise.ape ${CMAKE_CURRENT_BINARY_DIR}/test_data/noise.ape COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/test/data/noise.m4a ${CMAKE_CURRENT_BINARY_DIR}/test_data/noise.m4a COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/test/data/noise.mp3 ${CMAKE_CURRENT_BINARY_DIR}/test_data/noise.mp3 COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/test/data/noise.ogg ${CMAKE_CURRENT_BINARY_DIR}/test_data/noise.ogg COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/test/data/noise.wma ${CMAKE_CURRENT_BINARY_DIR}/test_data/noise.wma COPYONLY)

include(GoogleTest)

add_executable(tests
               src/AudioLibrary.cpp
               src/AudioLibrary.h
               src/AudioLibraryModel.cpp
               src/AudioLibraryModel.h
               src/AudioLibraryView.cpp
               src/AudioLibraryView.h
               src/ThreadSafeAudioLibrary.cpp
               src/ThreadSafeAudioLibrary.h
               src/TrackInfoReader.h
               src/TrackInfoReader.cpp
               test/AudioLibrarySaveAndLoad.cpp
               test/AudioLibraryTrackCleanup.cpp
               test/AudioLibraryViews.cpp
               test/ThreadSafeAudioLibrary.cpp
               test/TrackInfo.cpp
               test/VisualIndexRestoration.cpp
               test/tools.h)
target_link_libraries(tests GTest::gtest GTest::gtest_main ${CMAKE_THREAD_LIBS_INIT} Qt::Widgets ${TAGLIB_LIBRARY})
target_include_directories(tests PRIVATE ${TAGLIB_INCLUDE_DIR})
set_property(TARGET tests PROPERTY CXX_STANDARD 20)
set_property(TARGET tests PROPERTY AUTOMOC ON)
target_include_directories(tests PRIVATE "src")
gtest_discover_tests(tests)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND SANITIZE_ADDRESS)
    target_compile_options(tests PRIVATE -fno-omit-frame-pointer -fsanitize=address)
    target_link_options(tests PRIVATE -fno-omit-frame-pointer -fsanitize=address)
endif()

if(MSVC)
  target_compile_options(tests PRIVATE /W4 /WX)
else(MSVC)
  target_compile_options(tests PRIVATE -Wall -Wextra -pedantic -Werror)
endif(MSVC)

enable_testing()

# install

if (WIN32)
    # install the executable

    install(TARGETS AudioExplorer)

    # install selected dependencies, which the qt deploy script does not find automatically

    install(CODE [[
      file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES $<TARGET_FILE:AudioExplorer>
        RESOLVED_DEPENDENCIES_VAR _r_deps
        UNRESOLVED_DEPENDENCIES_VAR _u_deps
      )
      foreach(_file ${_r_deps})
        # whitelist, only these dlls are required

        string(FIND ${_file} pcre2 pcre2_found)
        string(FIND ${_file} jpeg jpeg_found)
        string(FIND ${_file} libpng libpng_found)
        string(FIND ${_file} zlib zlib_found)
        string(FIND ${_file} double-conversion double-conversion_found)
        string(FIND ${_file} libcrypto libcrypto_found)
        string(FIND ${_file} harfbuzz harfbuzz_found)
        string(FIND ${_file} zstd zstd_found)
        string(FIND ${_file} freetype freetype_found)
        string(FIND ${_file} bz2 bz2_found)
        string(FIND ${_file} brotli brotli_found)
        string(FIND ${_file} tag tag_found)

        if(NOT pcre2_found EQUAL -1 OR
           NOT jpeg_found EQUAL -1 OR
           NOT libpng_found EQUAL -1 OR
           NOT zlib_found EQUAL -1 OR
           NOT double-conversion_found EQUAL -1 OR
           NOT libcrypto_found EQUAL -1 OR
           NOT harfbuzz_found EQUAL -1 OR
           NOT zstd_found EQUAL -1 OR
           NOT freetype_found EQUAL -1 OR
           NOT bz2_found EQUAL -1 OR
           NOT brotli_found EQUAL -1 OR
           NOT tag_found EQUAL -1)
            file(INSTALL
              DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
              TYPE SHARED_LIBRARY
              FOLLOW_SYMLINK_CHAIN
              FILES "${_file}"
            )
        endif()
      endforeach()
    ]])

    # extra step for jpeg, which cannot be found through the dependencies

    install(FILES ${MY_JPEG_DLL}
            DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

    # install qt dependencies

    qt_generate_deploy_app_script(
        TARGET AudioExplorer
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()