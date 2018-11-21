cmake_minimum_required(VERSION 3.0)

project(AudioExplorer)

option(ENABLE_STATIC_RUNTIME "Use static runtime library" OFF)

find_path(TAGLIB_INCLUDE_DIR
  NAMES taglib/tag.h
  PATHS ${TAGLIB_DIR}/include
)
find_library(TAGLIB_LIBRARY
  NAMES tag
  PATHS ${TAGLIB_DIR}/lib
)

# debug library (suffixed with d)
find_library(TAGLIB_LIBRARY_DEBUG
  NAMES tagd
  PATHS ${TAGLIB_DIR}/lib
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(Qt5Widgets)
find_package(Qt5Svg)
find_package(Threads)

function(add_missing_qt_lib libname)
    get_target_property(
        linked_libs
        Qt5::Widgets
        INTERFACE_LINK_LIBRARIES
    )

    set(debug "${_qt5Widgets_install_prefix}/${libname}d.lib")
    set(nondebug "${_qt5Widgets_install_prefix}/${libname}.lib")

    # use generator expressions to support debug and release builds
    set(debug_gen_expr "$<$<CONFIG:Debug>:${debug}>")
    set(nondebug_gen_expr "$<$<NOT:$<CONFIG:Debug>>:${nondebug}>")
    set(gen_expr "${debug_gen_expr};${nondebug_gen_expr}")

    set_target_properties(
        Qt5::Widgets
        PROPERTIES
        INTERFACE_LINK_LIBRARIES "${linked_libs};${gen_expr}"
    )
endfunction()

function(add_missing_system_lib libname)
    get_target_property(
        linked_libs
        Qt5::Widgets
        INTERFACE_LINK_LIBRARIES
    )

    set_target_properties(
        Qt5::Widgets
        PROPERTIES
        INTERFACE_LINK_LIBRARIES "${linked_libs};${libname}"
    )
endfunction()

if(MSVC AND ENABLE_STATIC_RUNTIME)
    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endforeach(flag_var)

    # add missing dependencies manually for static libs
    add_missing_qt_lib("lib/Qt5AccessibilitySupport")
    add_missing_qt_lib("lib/Qt5EventDispatcherSupport")
    add_missing_qt_lib("lib/Qt5FontDatabaseSupport")
    add_missing_qt_lib("lib/Qt5ThemeSupport")
    add_missing_qt_lib("lib/Qt5WindowsUIAutomationSupport")
    add_missing_qt_lib("lib/qtfreetype")
    add_missing_qt_lib("lib/qtharfbuzz")
    add_missing_qt_lib("lib/qtlibpng")
    add_missing_qt_lib("lib/qtpcre2")
    add_missing_qt_lib("plugins/imageformats/qgif")
    add_missing_qt_lib("plugins/imageformats/qicns")
    add_missing_qt_lib("plugins/imageformats/qico")
    add_missing_qt_lib("plugins/imageformats/qjpeg")
    add_missing_qt_lib("plugins/imageformats/qtga")
    add_missing_qt_lib("plugins/imageformats/qtiff")
    add_missing_qt_lib("plugins/imageformats/qwbmp")
    add_missing_qt_lib("plugins/imageformats/qwebp")
    add_missing_qt_lib("plugins/platforms/qwindows")
    add_missing_qt_lib("plugins/styles/qwindowsvistastyle")

    add_missing_system_lib("dwmapi.lib")
    add_missing_system_lib("imm32.lib")
    add_missing_system_lib("netapi32.lib")
    add_missing_system_lib("userenv.lib")
    add_missing_system_lib("version.lib")
    add_missing_system_lib("winmm.lib")
    add_missing_system_lib("ws2_32.lib")
    add_missing_system_lib("uxtheme.lib")

    set(STATIC_PLUGIN_INCLUDE src/static_plugin_include.cpp)
endif()

add_executable(AudioExplorer WIN32 src/AdvancedSearchDialog.cpp
                                   src/AdvancedSearchDialog.h
                                   src/main.cpp
                                   src/MainWindow.cpp
                                   src/MainWindow.h
                                   src/AudioLibrary.cpp
                                   src/AudioLibrary.h
                                   src/AudioLibraryView.cpp
                                   src/AudioLibraryView.h
                                   src/project_version.h
                                   src/Settings.h
                                   src/Settings.cpp
                                   src/SettingsEditorWindow.cpp
                                   src/SettingsEditorWindow.h
                                   src/TrackInfoReader.cpp
                                   src/TrackInfoReader.h
                                   ${STATIC_PLUGIN_INCLUDE})
target_link_libraries(AudioExplorer ${CMAKE_THREAD_LIBS_INIT} Qt5::Widgets Qt5::Svg optimized ${TAGLIB_LIBRARY})
if(DEFINED ${TAGLIB_LIBRARY_DEBUG})
    target_link_libraries(AudioExplorer debug ${TAGLIB_LIBRARY_DEBUG})
endif()

target_include_directories(AudioExplorer PRIVATE ${TAGLIB_INCLUDE_DIR})
target_compile_definitions(AudioExplorer PRIVATE TAGLIB_STATIC)
set_property(TARGET AudioExplorer PROPERTY CXX_STANDARD 11)

# test

add_executable(test1 test/test.cpp
                     src/AudioLibrary.cpp
                     src/AudioLibrary.h
                     src/TrackInfoReader.h
                     src/TrackInfoReader.cpp
                     ${STATIC_PLUGIN_INCLUDE})
target_link_libraries(test1 Qt5::Widgets optimized ${TAGLIB_LIBRARY})
if(DEFINED ${TAGLIB_LIBRARY_DEBUG})
    target_link_libraries(test1 debug ${TAGLIB_LIBRARY_DEBUG})
endif()

target_include_directories(test1 PRIVATE ${TAGLIB_INCLUDE_DIR})
target_compile_definitions(test1 PRIVATE TAGLIB_STATIC)
set_property(TARGET test1 PROPERTY CXX_STANDARD 11)
target_include_directories(test1 PRIVATE "src")

enable_testing()
add_test(NAME test1 COMMAND test1 ${CMAKE_SOURCE_DIR}/test/data ${CMAKE_BINARY_DIR})