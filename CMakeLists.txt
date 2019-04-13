cmake_minimum_required(VERSION 3.0)

project(AudioExplorer)

option(ENABLE_STATIC_RUNTIME "Use static runtime library" OFF)

find_path(TAGLIB_INCLUDE_DIR
  NAMES taglib/tag.h
)
find_library(TAGLIB_LIBRARY
  NAMES tag
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(Qt5Widgets CONFIG REQUIRED)
find_package(Qt5Svg CONFIG REQUIRED)
find_package(Threads REQUIRED)

if(MSVC AND ENABLE_STATIC_RUNTIME)
    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endforeach(flag_var)

    # for static linking, some additional dependencies have to be spelled out

    find_package(harfbuzz CONFIG REQUIRED)
    find_package(double-conversion CONFIG REQUIRED)
    find_package(JPEG REQUIRED)
    find_package(PNG REQUIRED)
    find_package(ZLIB REQUIRED)

    # some Qt dependencies have to be searched by name

    find_library(PCRE2_LIBRARY NAMES pcre2-16)
    find_library(Qt5AccessibilitySupport_LIBRARY NAMES Qt5AccessibilitySupport)
    find_library(Qt5EventDispatcherSupport_LIBRARY NAMES Qt5EventDispatcherSupport)
    find_library(Qt5FontDatabaseSupport_LIBRARY NAMES Qt5FontDatabaseSupport)
    find_library(Qt5ThemeSupport_LIBRARY NAMES Qt5ThemeSupport)
    find_library(Qt5WindowsUIAutomationSupport_LIBRARY NAMES Qt5WindowsUIAutomationSupport)

    # windows libraries

    find_library(DWMAPI_LIBRARY NAMES dwmapi)
    find_library(IMM32_LIBRARY NAMES imm32)
    find_library(NETAPI32_LIBRARY NAMES netapi32)
    find_library(USERENV_LIBRARY NAMES userenv)
    find_library(VERSION_LIBRARY NAMES version)
    find_library(WINMM_LIBRARY NAMES winmm)
    find_library(WS2_32_LIBRARY NAMES ws2_32)
    find_library(UXTHEME_LIBRARY NAMES uxtheme)
    find_library(WTSAPI32_LIBRARY NAMES Wtsapi32)

    if(CMAKE_BUILD_TYPE MATCHES "^Debug$" OR NOT DEFINED CMAKE_BUILD_TYPE)

        # cannot use the harfbuzz package for debug builds, because it pulls in the release version of freetype
        # so search for harfbuzz by name and use the freetype package directly

        find_library(HARFBUZZ_LIBRARY_DEBUG NAMES harfbuzz)
        find_package(freetype CONFIG REQUIRED)

        # some Qt dependencies have to be searched by name

        find_library(PCRE2_LIBRARY_DEBUG NAMES pcre2-16d)
        find_library(Qt5AccessibilitySupport_LIBRARY_DEBUG NAMES Qt5AccessibilitySupportd)
        find_library(Qt5EventDispatcherSupport_LIBRARY_DEBUG NAMES Qt5EventDispatcherSupportd)
        find_library(Qt5FontDatabaseSupport_LIBRARY_DEBUG NAMES Qt5FontDatabaseSupportd)
        find_library(Qt5ThemeSupport_LIBRARY_DEBUG NAMES Qt5ThemeSupportd)
        find_library(Qt5WindowsUIAutomationSupport_LIBRARY_DEBUG NAMES Qt5WindowsUIAutomationSupportd)

        find_library(QWindows_DEBUG NAMES qwindowsd PATH_SUFFIXES plugins/platforms)
        find_library(QWindowsVistaStyle_DEBUG NAMES qwindowsvistastyled PATH_SUFFIXES plugins/styles)
        find_library(QJpegPlugin_DEBUG NAMES qjpegd PATH_SUFFIXES plugins/imageformats)

        set(STATIC_LINK_DEPENDENCIES_DEBUG debug freetype
                                           debug ${HARFBUZZ_LIBRARY_DEBUG}
                                           debug ${PCRE2_LIBRARY_DEBUG}
                                           debug ${Qt5AccessibilitySupport_LIBRARY_DEBUG}
                                           debug ${Qt5EventDispatcherSupport_LIBRARY_DEBUG}
                                           debug ${Qt5FontDatabaseSupport_LIBRARY_DEBUG}
                                           debug ${Qt5ThemeSupport_LIBRARY_DEBUG}
                                           debug ${Qt5WindowsUIAutomationSupport_LIBRARY_DEBUG}
                                           debug ${QWindows_DEBUG}
                                           debug ${QWindowsVistaStyle_DEBUG}
                                           debug ${QJpegPlugin_DEBUG})
    endif()

    set(STATIC_LINK_DEPENDENCIES double-conversion::double-conversion
                                 ${JPEG_LIBRARIES}
                                 PNG::PNG
                                 ZLIB::ZLIB
                                 ${DWMAPI_LIBRARY}
                                 ${IMM32_LIBRARY}
                                 ${NETAPI32_LIBRARY}
                                 ${USERENV_LIBRARY}
                                 ${VERSION_LIBRARY}
                                 ${WINMM_LIBRARY}
                                 ${WS2_32_LIBRARY}
                                 ${UXTHEME_LIBRARY}
                                 ${WTSAPI32_LIBRARY}
                                 optimized harfbuzz::harfbuzz
                                 optimized ${PCRE2_LIBRARY}
                                 optimized ${Qt5AccessibilitySupport_LIBRARY}
                                 optimized ${Qt5EventDispatcherSupport_LIBRARY}
                                 optimized ${Qt5FontDatabaseSupport_LIBRARY}
                                 optimized ${Qt5ThemeSupport_LIBRARY}
                                 optimized ${Qt5WindowsUIAutomationSupport_LIBRARY}
                                 optimized Qt5::QWindowsIntegrationPlugin
                                 optimized Qt5::QWindowsVistaStylePlugin
                                 optimized Qt5::QJpegPlugin
                                 ${STATIC_LINK_DEPENDENCIES_DEBUG})

    set(STATIC_PLUGIN_INCLUDE src/static_plugin_include.cpp)
endif()

if(WIN32)
    set(WIN_RESOURCE_FILE res/appicon.rc
                          src/project_version.rc)
endif()

add_executable(AudioExplorer WIN32 src/AdvancedSearchDialog.cpp
                                   src/AdvancedSearchDialog.h
                                   src/main.cpp
                                   src/MainWindow.cpp
                                   src/MainWindow.h
                                   src/AudioLibrary.cpp
                                   src/AudioLibrary.h
                                   src/AudioLibraryModel.cpp
                                   src/AudioLibraryModel.h
                                   src/AudioLibraryView.cpp
                                   src/AudioLibraryView.h
                                   src/project_version.h
                                   src/Settings.h
                                   src/Settings.cpp
                                   src/SettingsEditorWindow.cpp
                                   src/SettingsEditorWindow.h
                                   src/TrackInfoReader.cpp
                                   src/TrackInfoReader.h
                                   ${WIN_RESOURCE_FILE}
                                   ${STATIC_PLUGIN_INCLUDE})
target_link_libraries(AudioExplorer ${CMAKE_THREAD_LIBS_INIT} Qt5::Widgets Qt5::Svg ${TAGLIB_LIBRARY} ${STATIC_LINK_DEPENDENCIES})

target_include_directories(AudioExplorer PRIVATE ${TAGLIB_INCLUDE_DIR})
target_compile_definitions(AudioExplorer PRIVATE TAGLIB_STATIC)
set_property(TARGET AudioExplorer PROPERTY CXX_STANDARD 11)

if(MSVC)
  target_compile_options(AudioExplorer PRIVATE /W4 /WX)
else(MSVC)
  target_compile_options(AudioExplorer PRIVATE -Wall -Wextra -pedantic -Werror)
endif(MSVC)

# test

add_executable(test1 test/test.cpp
                     src/AudioLibrary.cpp
                     src/AudioLibrary.h
                     src/TrackInfoReader.h
                     src/TrackInfoReader.cpp
                     ${STATIC_PLUGIN_INCLUDE})
target_link_libraries(test1 Qt5::Widgets ${TAGLIB_LIBRARY} ${STATIC_LINK_DEPENDENCIES})

target_include_directories(test1 PRIVATE ${TAGLIB_INCLUDE_DIR})
target_compile_definitions(test1 PRIVATE TAGLIB_STATIC)
set_property(TARGET test1 PROPERTY CXX_STANDARD 11)
target_include_directories(test1 PRIVATE "src")

if(MSVC)
  target_compile_options(test1 PRIVATE /W4 /WX)
else(MSVC)
  target_compile_options(test1 PRIVATE -Wall -Wextra -pedantic -Werror)
endif(MSVC)

# test

enable_testing()
add_test(NAME test1 COMMAND test1 ${CMAKE_SOURCE_DIR}/test/data ${CMAKE_BINARY_DIR})