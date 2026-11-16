# Checks that the PDFium the program is built with (vcpkg-ports/pdfium/release.cmake) is the
# one pdfium-binaries built from the official sources, without JavaScript and XFA:
#
#   - each pinned archive downloads and has its SHA-512;
#   - its VERSION is PDFIUM_VERSION, and the version of the port (vcpkg-ports/pdfium/vcpkg.json)
#     is the same;
#   - its args.gn builds PDFium without V8 (JavaScript) and XFA;
#   - GitHub's signed record of how the release was built (pdfium-attestation.json, Sigstore)
#     holds for it: built from the branch master by the workflow build-all.yml of
#     bblanchon/pdfium-binaries on a runner of GitHub (gh attestation verify).
#
#   cmake [-DWORK_DIR=<folder>] [-DGH=<gh>] -P scripts/verify_pdfium.cmake
#
# Needs the network and the GitHub CLI (gh, with GH_TOKEN set). Run by the CI (ci-linux.yml).

cmake_minimum_required(VERSION 3.21)

get_filename_component(_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${_root}/vcpkg-ports/pdfium/release.cmake")

set(_repository "bblanchon/pdfium-binaries")
set(_workflow "${_repository}/.github/workflows/build-all.yml")

if(NOT WORK_DIR)
    set(WORK_DIR "${_root}/build-pdfium-verify")
endif()
if(NOT GH)
    find_program(GH gh REQUIRED)
endif()
file(MAKE_DIRECTORY "${WORK_DIR}")

# The port is of the same version
file(READ "${_root}/vcpkg-ports/pdfium/vcpkg.json" _port)
string(JSON _port_version GET "${_port}" version)
if(NOT _port_version STREQUAL PDFIUM_VERSION)
    message(FATAL_ERROR "PDFium: the port is version ${_port_version}, release.cmake "
                        "${PDFIUM_VERSION}")
endif()

# A file of the release into WORK_DIR, checked by its SHA-512 when @p sha512 is given
function(_pdfium_download name sha512)
    set(_hash "")
    if(sha512)
        set(_hash EXPECTED_HASH "SHA512=${sha512}")
    endif()
    file(DOWNLOAD "${PDFIUM_DOWNLOAD_URL}/${name}" "${WORK_DIR}/${name}"
         ${_hash} TLS_VERIFY ON STATUS _status)
    list(GET _status 0 _code)
    if(NOT _code EQUAL 0)
        list(GET _status 1 _reason)
        file(REMOVE "${WORK_DIR}/${name}")
        message(FATAL_ERROR "PDFium: ${name} did not download (${_reason})")
    endif()
endfunction()

_pdfium_download("pdfium-attestation.json" "")

foreach(_platform IN LISTS PDFIUM_PLATFORMS)
    set(_name "pdfium-${_platform}.tgz")
    _pdfium_download("${_name}" "${PDFIUM_SHA512_${_platform}}")

    # Its version and how it was built
    set(_unpacked "${WORK_DIR}/pdfium-${_platform}")
    file(REMOVE_RECURSE "${_unpacked}")
    file(ARCHIVE_EXTRACT INPUT "${WORK_DIR}/${_name}" DESTINATION "${_unpacked}"
         PATTERNS VERSION args.gn)
    file(STRINGS "${_unpacked}/VERSION" _lines)
    set(_version "")
    foreach(_key MAJOR MINOR BUILD PATCH)
        set(_value "?")
        foreach(_line IN LISTS _lines)
            if(_line MATCHES "^${_key}=([0-9]+)$")
                set(_value "${CMAKE_MATCH_1}")
            endif()
        endforeach()
        list(APPEND _version "${_value}")
    endforeach()
    list(JOIN _version "." _version)
    if(NOT _version STREQUAL PDFIUM_VERSION)
        message(FATAL_ERROR "PDFium: ${_name} is version ${_version}, not ${PDFIUM_VERSION}")
    endif()
    file(READ "${_unpacked}/args.gn" _arguments)
    foreach(_argument "pdf_enable_v8 = false" "pdf_enable_xfa = false" "is_debug = false")
        string(FIND "${_arguments}" "${_argument}" _at)
        if(_at LESS 0)
            message(FATAL_ERROR "PDFium: ${_name} is not built with ${_argument}")
        endif()
    endforeach()

    # The signed record of its build
    execute_process(
        COMMAND "${GH}" attestation verify "${WORK_DIR}/${_name}"
                --bundle "${WORK_DIR}/pdfium-attestation.json"
                --repo "${_repository}"
                --signer-workflow "${_workflow}"
                --source-ref refs/heads/master
                --deny-self-hosted-runners
        RESULT_VARIABLE _result)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "PDFium: the attestation of ${_name} does not hold")
    endif()
    message(STATUS "PDFium ${PDFIUM_VERSION} ${_platform}: SHA-512, version, build "
                   "arguments and attestation verified")
endforeach()
