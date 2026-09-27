execute_process(
    COMMAND git log -1 --format=%H
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND git log -1 --format=%cI
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_COMMITTER_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND git status --porcelain
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_STATUS_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(GIT_STATUS_OUTPUT)
    set(GIT_HAS_UNCOMMITTED_CHANGES "true")
else()
    set(GIT_HAS_UNCOMMITTED_CHANGES "false")
endif()

# write git hash to file so it can be picked up by the executable

configure_file(${CMAKE_CURRENT_LIST_DIR}/src/git_version_info.cpp.in
               ${CMAKE_BINARY_DIR}/git_version_info.cpp
               @ONLY)