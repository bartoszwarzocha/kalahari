# What the program is built from: the commit, its branch, whether the code differs from the
# commit, and when it was built. Run at every build in script mode (cmake -P) by the target
# kalahari_build_info_file (CMakeLists.txt); the About dialog shows what it writes.
#
#   -DGIT_EXECUTABLE=<git, or empty>  -DSOURCE_DIR=<the sources>
#   -DTEMPLATE=<cmake/build_info.cpp.in>  -DOUTPUT=<the generated build_info.cpp>
#
# The file is written again only when the code is another: its time is then the time of the
# build of this code, and a build with nothing to do compiles and links nothing.

# The folders and files of the program's code: a change elsewhere (a document, a test) is not
# another program
set(CODE_PATHS src include resources translations cmake plugins CMakeLists.txt vcpkg.json)

function(run_git out_var)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" ${ARGN}
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE output
        ERROR_QUIET
        RESULT_VARIABLE result
    )
    if(result EQUAL 0)
        set(${out_var} "${output}" PARENT_SCOPE)
    else()
        set(${out_var} "" PARENT_SCOPE)
    endif()
endfunction()

set(COMMIT "")
set(BRANCH "")
set(MODIFIED "false")
set(STATE "no git")

if(GIT_EXECUTABLE)
    run_git(COMMIT rev-parse --short=8 HEAD)
    string(STRIP "${COMMIT}" COMMIT)
endif()

if(COMMIT)
    run_git(BRANCH rev-parse --abbrev-ref HEAD)
    string(STRIP "${BRANCH}" BRANCH)
    if(BRANCH STREQUAL "HEAD")
        # A checkout of the commit alone (git checkout --detach, CI): a branch that points at it,
        # local or of the remote, without the name of the remote
        run_git(names branch --all --points-at HEAD "--format=%(refname:short)")
        string(REPLACE "\n" ";" names "${names}")
        set(BRANCH "")
        foreach(name IN LISTS names)
            if(name STREQUAL "" OR name MATCHES "^\\(" OR name MATCHES "/HEAD$")
                continue()
            endif()
            string(REGEX REPLACE "^[^/]+/" "" short "${name}")
            if(name MATCHES "/")
                # A branch of the remote: only when there is no local one
                if(BRANCH STREQUAL "")
                    set(BRANCH "${short}")
                endif()
            else()
                set(BRANCH "${name}")
                break()
            endif()
        endforeach()
    endif()

    # The code changed and the new files of the code, not yet in the commit
    run_git(status status --porcelain --untracked-files=normal -- ${CODE_PATHS})
    set(diff "")
    if(NOT status STREQUAL "")
        set(MODIFIED "true")
        run_git(diff diff HEAD -- ${CODE_PATHS})
    endif()
    string(SHA1 STATE "${COMMIT}|${BRANCH}|${status}|${diff}")
endif()

# The same code as at the build before: the file stays as it is
if(EXISTS "${OUTPUT}")
    file(STRINGS "${OUTPUT}" previous REGEX "^// State: ")
    if(previous STREQUAL "// State: ${STATE}")
        return()
    endif()
endif()

string(TIMESTAMP BUILT_AT "%Y-%m-%dT%H:%M:%SZ" UTC)
# A branch name may have a quotation mark; git forbids the backslash
string(REPLACE "\"" "\\\"" BRANCH "${BRANCH}")
configure_file("${TEMPLATE}" "${OUTPUT}" @ONLY)
