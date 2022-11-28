cmake_minimum_required(VERSION 3.0)

project(AudioExplorer)

option(ENABLE_STATIC_RUNTIME "Use static runtime library" OFF)
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

if(MSVC AND ENABLE_STATIC_RUNTIME)
    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endforeach(flag_var)
endif()

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

set_source_files_properties(${CMAKE_BINARY_DIR}/compiled_resources.cpp
                            ${CMAKE_BINARY_DIR}/compiled_resources.h
                            PROPERTIES GENERATED TRUE
                                       SKIP_AUTOMOC ON)

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
                                   src/resource_helpers.cpp
                                   src/resource_helpers.h
                                   src/Settings.h
                                   src/Settings.cpp
                                   src/SettingsEditorWindow.cpp
                                   src/SettingsEditorWindow.h
                                   src/ThreadSafeAudioLibrary.cpp
                                   src/ThreadSafeAudioLibrary.h
                                   src/TrackInfoReader.cpp
                                   src/TrackInfoReader.h
                                   ${TS_FILES}
                                   ${CMAKE_BINARY_DIR}/compiled_resources.cpp
                                   ${CMAKE_BINARY_DIR}/compiled_resources.h
                                   ${WIN_RESOURCE_FILE})
target_link_libraries(AudioExplorer ${CMAKE_THREAD_LIBS_INIT} Qt::Widgets Qt::Svg ${TAGLIB_LIBRARY} ${STATIC_LINK_DEPENDENCIES})

target_include_directories(AudioExplorer PRIVATE ${TAGLIB_INCLUDE_DIR})
target_compile_definitions(AudioExplorer PRIVATE TAGLIB_STATIC)
set_property(TARGET AudioExplorer PROPERTY CXX_STANDARD 14)
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

add_executable(ResCompiler ResCompiler.cpp)
set_property(TARGET ResCompiler PROPERTY CXX_STANDARD 14)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/compiled_resources.cpp ${CMAKE_BINARY_DIR}/compiled_resources.h
                   COMMAND ResCompiler -i ${CMAKE_CURRENT_LIST_DIR}/res/view_menu.svg ${QM_FILES} -o ${CMAKE_BINARY_DIR}/compiled_resources.cpp
                   DEPENDS ${CMAKE_CURRENT_LIST_DIR}/res/view_menu.svg ${QM_FILES}
                   VERBATIM)

if (WIN32)
    # copy Qt plugins and translations to bin directory

    find_program(WINDEPLOYQT_DEBUG NAMES windeployqt.debug.bat)
    find_program(WINDEPLOYQT_RELEASE NAMES windeployqt)

    add_custom_command(TARGET AudioExplorer POST_BUILD
        COMMAND $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:${WINDEPLOYQT_DEBUG}>$<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:${WINDEPLOYQT_RELEASE}>
                $<TARGET_FILE:AudioExplorer>
        COMMENT "Running windeployqt..."
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

create_test_sourcelist (TEST_SOURCES
    TestDriver.cpp
    test/AudioLibrarySaveAndLoad.cpp
    test/AudioLibraryTrackCleanup.cpp
    test/AudioLibraryViews.cpp
    test/ThreadSafeAudioLibrary.cpp
    test/TrackInfo.cpp
    test/VisualIndexRestoration.cpp)

set(TESTS_TO_RUN
    test/AudioLibrarySaveAndLoad
    test/AudioLibraryTrackCleanup
    test/AudioLibraryViews
    test/ThreadSafeAudioLibrary
    test/TrackInfo
    test/VisualIndexRestoration)

add_executable(TestDriver ${TEST_SOURCES}
                          test/tools.h
                          src/AudioLibrary.cpp
                          src/AudioLibrary.h
                          src/AudioLibraryModel.cpp
                          src/AudioLibraryModel.h
                          src/AudioLibraryView.cpp
                          src/AudioLibraryView.h
                          src/ThreadSafeAudioLibrary.cpp
                          src/ThreadSafeAudioLibrary.h
                          src/TrackInfoReader.h
                          src/TrackInfoReader.cpp)
target_link_libraries(TestDriver ${CMAKE_THREAD_LIBS_INIT} Qt::Widgets ${TAGLIB_LIBRARY} ${STATIC_LINK_DEPENDENCIES})

target_include_directories(TestDriver PRIVATE ${TAGLIB_INCLUDE_DIR})
target_compile_definitions(TestDriver PRIVATE TAGLIB_STATIC)
set_property(TARGET TestDriver PROPERTY CXX_STANDARD 14)
set_property(TARGET TestDriver PROPERTY AUTOMOC ON)
target_include_directories(TestDriver PRIVATE "src")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND SANITIZE_ADDRESS)
    target_compile_options(TestDriver PRIVATE -fno-omit-frame-pointer -fsanitize=address)
    target_link_options(TestDriver PRIVATE -fno-omit-frame-pointer -fsanitize=address)
endif()

if(MSVC)
  target_compile_options(TestDriver PRIVATE /W4 /WX)
else(MSVC)
  target_compile_options(TestDriver PRIVATE -Wall -Wextra -pedantic -Werror)
endif(MSVC)

enable_testing()
foreach (test ${TESTS_TO_RUN})
  add_test(NAME ${test} COMMAND TestDriver ${test} ${CMAKE_SOURCE_DIR}/test ${CMAKE_BINARY_DIR})
endforeach ()
