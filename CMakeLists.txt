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
find_package(Qt5Widgets CONFIG REQUIRED)
find_package(Qt5Svg CONFIG REQUIRED)
find_package(Qt5LinguistTools CONFIG REQUIRED)
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
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR}/src ${TS_FILES} OPTIONS -no-obsolete -locations none)
else()
    qt5_add_translation(QM_FILES ${TS_FILES})
endif()

# add Qt translations

get_target_property (QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
execute_process(COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_TRANSLATIONS
                OUTPUT_VARIABLE QT_INSTALL_TRANSLATIONS
                RESULT_VARIABLE QT_INSTALL_TRANSLATIONS_RESULT)

if (NOT QT_INSTALL_TRANSLATIONS_RESULT EQUAL 0)
    message(FATAL_ERROR "Cannot find Qt translations")
endif()

# remove trailing newline
string(REGEX REPLACE "\n$" "" QT_INSTALL_TRANSLATIONS "${QT_INSTALL_TRANSLATIONS}")

list(APPEND QM_FILES ${QT_INSTALL_TRANSLATIONS}/qt_en.qm
                     ${QT_INSTALL_TRANSLATIONS}/qt_de.qm
                     ${QT_INSTALL_TRANSLATIONS}/qtbase_en.qm
                     ${QT_INSTALL_TRANSLATIONS}/qtbase_de.qm)

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
target_link_libraries(AudioExplorer ${CMAKE_THREAD_LIBS_INIT} Qt5::Widgets Qt5::Svg ${TAGLIB_LIBRARY} ${STATIC_LINK_DEPENDENCIES})

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
                   DEPENDS ${CMAKE_CURRENT_LIST_DIR}/res/view_menu.svg ${QM_FILES})

# test

add_executable(test1 test/test.cpp
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
target_link_libraries(test1 ${CMAKE_THREAD_LIBS_INIT} Qt5::Widgets ${TAGLIB_LIBRARY} ${STATIC_LINK_DEPENDENCIES})

target_include_directories(test1 PRIVATE ${TAGLIB_INCLUDE_DIR})
target_compile_definitions(test1 PRIVATE TAGLIB_STATIC)
set_property(TARGET test1 PROPERTY CXX_STANDARD 14)
set_property(TARGET test1 PROPERTY AUTOMOC ON)
target_include_directories(test1 PRIVATE "src")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND SANITIZE_ADDRESS)
    target_compile_options(test1 PRIVATE -fno-omit-frame-pointer -fsanitize=address)
    target_link_options(test1 PRIVATE -fno-omit-frame-pointer -fsanitize=address)
endif()

if(MSVC)
  target_compile_options(test1 PRIVATE /W4 /WX)
else(MSVC)
  target_compile_options(test1 PRIVATE -Wall -Wextra -pedantic -Werror)
endif(MSVC)

enable_testing()
add_test(NAME test1 COMMAND test1 ${CMAKE_SOURCE_DIR}/test/data ${CMAKE_BINARY_DIR})