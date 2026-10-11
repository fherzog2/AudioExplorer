cmake_minimum_required(VERSION 3.21)

project(AudioExplorer)

option(SANITIZE_ADDRESS "Add address sanitizer flags" OFF)
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Linguist)
#find_package(TagLib REQUIRED)
find_path(TAGLIB_INCLUDE_DIR
  NAMES taglib/tag.h
)
find_library(TAGLIB_LIBRARY
  NAMES tag
)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(GTest CONFIG REQUIRED)

if(WIN32)
    set(WIN_RESOURCE_FILE res/appicon.rc
                          src/project_version.rc)
endif()

set_source_files_properties(${CMAKE_BINARY_DIR}/git_version_info.cpp
                            PROPERTIES GENERATED TRUE
                                       SKIP_AUTOMOC ON)

add_custom_target(EmbedGitHash
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_LIST_DIR}/EmbedGitHash.cmake
    BYPRODUCTS ${CMAKE_BINARY_DIR}/git_version_info.cpp
    VERBATIM
)

# add our own translations

#set(RECREATE_TRANSLATIONS ON)

set(TS_FILES ${CMAKE_SOURCE_DIR}/translations/AudioExplorer_de.ts)
if(RECREATE_TRANSLATIONS)
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR}/src ${TS_FILES} OPTIONS -no-obsolete -locations none)
else()
    qt_add_translation(QM_FILES ${TS_FILES})
endif()

# use an object library to avoid redundant compilation for the app and the unit test runner

add_library(AudioExplorer_objects OBJECT
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
    src/git_version_info.h
    ${CMAKE_BINARY_DIR}/git_version_info.cpp)
target_link_libraries(AudioExplorer_objects PUBLIC Threads::Threads Qt6::Widgets Qt::Svg ${TAGLIB_LIBRARY}) # TagLib::tag
target_include_directories(AudioExplorer_objects PUBLIC ${TAGLIB_INCLUDE_DIR})
set_property(TARGET AudioExplorer_objects PROPERTY CXX_STANDARD 23)
set_property(TARGET AudioExplorer_objects PROPERTY AUTOMOC ON)
# execute EmbedGitHash before each build
add_dependencies(AudioExplorer_objects EmbedGitHash)
if(MSVC)
  target_precompile_headers(AudioExplorer_objects PRIVATE
    <QtCore/QtCore>
    <QtGui/QtGui>
    <QtWidgets/QtWidgets>)
endif(MSVC)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND SANITIZE_ADDRESS)
    target_compile_options(AudioExplorer_objects PUBLIC -fno-omit-frame-pointer -fsanitize=address)
    target_link_options(AudioExplorer_objects PUBLIC -fno-omit-frame-pointer -fsanitize=address)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND ENABLE_COVERAGE)
    target_compile_options(AudioExplorer_objects PUBLIC -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(AudioExplorer_objects PUBLIC -fprofile-instr-generate)
endif()

if(MSVC)
  target_compile_options(AudioExplorer_objects PUBLIC /W4 /WX)
else(MSVC)
  target_compile_options(AudioExplorer_objects PUBLIC -Wall -Wextra -pedantic -Werror)
endif(MSVC)

# the app

add_executable(AudioExplorer WIN32 src/main.cpp
                                   ${TS_FILES}
                                   ${WIN_RESOURCE_FILE})
target_link_libraries(AudioExplorer AudioExplorer_objects)
set_property(TARGET AudioExplorer PROPERTY CXX_STANDARD 23)

set_source_files_properties(${CMAKE_BINARY_DIR}/AudioExplorer_de.qm PROPERTIES QT_RESOURCE_ALIAS res/AudioExplorer_de.qm)

qt_add_resources(AudioExplorer "appresources"
    PREFIX "/"
    FILES res/view_menu.svg ${QM_FILES})

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
               test/test_AudioLibrarySaveAndLoad.cpp
               test/test_AudioLibraryTrackCleanup.cpp
               test/test_AudioLibraryViews.cpp
               test/test_ThreadSafeAudioLibrary.cpp
               test/test_TrackInfo.cpp
               test/test_VisualIndexRestoration.cpp
               test/tools.h)
target_link_libraries(tests GTest::gtest GTest::gtest_main AudioExplorer_objects)
target_include_directories(tests PRIVATE "src")
set_property(TARGET tests PROPERTY CXX_STANDARD 23)
if(MSVC)
  target_precompile_headers(tests PRIVATE
    <QtCore/QtCore>
    <QtGui/QtGui>
    <QtWidgets/QtWidgets>
    <gtest/gtest.h>)
endif(MSVC)

gtest_discover_tests(tests)
enable_testing()

# install

if (WIN32)
    # install the executable

    install(TARGETS AudioExplorer)

    # install selected dependencies, which the qt deploy script does not find automatically

    install(CODE [[
        function(is_windows_dll INPUT_PATH OUTPUT_BOOL)
            # check if the path starts with C:\Windows

            file(TO_CMAKE_PATH "$ENV{SystemRoot}" WIN_NORMALIZED)
            string(TOLOWER "${WIN_NORMALIZED}" WIN_LOWER)

            file(TO_CMAKE_PATH "${INPUT_PATH}" INPUT_NORMALIZED)
            string(TOLOWER "${INPUT_NORMALIZED}" INPUT_LOWER)

            string(FIND "${INPUT_LOWER}" "${WIN_LOWER}" SUBSTRING_INDEX)

            if(SUBSTRING_INDEX EQUAL 0)
                set(${OUTPUT_BOOL} TRUE PARENT_SCOPE)
            else()
                set(${OUTPUT_BOOL} FALSE PARENT_SCOPE)
            endif()
        endfunction()

        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES $<TARGET_FILE:AudioExplorer>
            RESOLVED_DEPENDENCIES_VAR _r_deps
            UNRESOLVED_DEPENDENCIES_VAR _u_deps
        )

        foreach(_file ${_r_deps})
            is_windows_dll(${_file} FILE_IS_WINDOWS_DLL)
            if(NOT FILE_IS_WINDOWS_DLL)
                file(INSTALL
                    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                    TYPE SHARED_LIBRARY
                    FOLLOW_SYMLINK_CHAIN
                    FILES "${_file}"
                )
            endif()
        endforeach()
    ]])

    # install qt dependencies

    qt_generate_deploy_app_script(
        TARGET AudioExplorer
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
else()
    # "install" by creating an AppImage

    set(APP_DIR_DESTINATION_START "AppDir/usr")

    install(TARGETS AudioExplorer DESTINATION ${APP_DIR_DESTINATION_START}/bin)

    install(FILES ${CMAKE_SOURCE_DIR}/res/AudioExplorer.desktop DESTINATION ${APP_DIR_DESTINATION_START}/share/applications)
    install(FILES ${CMAKE_SOURCE_DIR}/res/appicon.svg           DESTINATION ${APP_DIR_DESTINATION_START}/share/icons/hicolor/scalable/apps)

    install(CODE "execute_process(COMMAND \"${CMAKE_SOURCE_DIR}/deploy_as_appimage.sh\" WORKING_DIRECTORY \"${CMAKE_INSTALL_PREFIX}\")")
endif()
