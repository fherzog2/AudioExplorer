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

    # linking Mincore.lib causes problems on Windows 7
    # because then the program looks for a specific api-ms-win-core-file*.dll which is not present

    get_property(QT5_CORE_DEPENDENCIES TARGET Qt5::Core PROPERTY INTERFACE_LINK_LIBRARIES)
    list(REMOVE_ITEM QT5_CORE_DEPENDENCIES "Mincore.lib")
    set_property(TARGET Qt5::Core PROPERTY INTERFACE_LINK_LIBRARIES ${QT5_CORE_DEPENDENCIES})

    find_library(QWindowsVistaStyle_DEBUG NAMES qwindowsvistastyled PATH_SUFFIXES plugins/styles)
    find_library(QJpegPlugin_DEBUG NAMES qjpegd PATH_SUFFIXES plugins/imageformats)

    set(STATIC_LINK_DEPENDENCIES debug ${QWindowsVistaStyle_DEBUG}
                                 optimized Qt5::QWindowsVistaStylePlugin # without it, the program looks dated
                                 debug ${QJpegPlugin_DEBUG}
                                 optimized Qt5::QJpegPlugin
                                 Uxtheme.lib # prerequisite for Qt5::QWindowsVistaStylePlugin
                                 Userenv.lib # replacement for Mincore.lib
                                 version.lib # replacement for Mincore.lib
                                 )

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

enable_testing()
add_test(NAME test1 COMMAND test1 ${CMAKE_SOURCE_DIR}/test/data ${CMAKE_BINARY_DIR})