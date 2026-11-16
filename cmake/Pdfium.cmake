# The preview of PDF files: PDFium, the PDF library of Chromium, as pdfium-binaries builds it
# from the official sources (vcpkg-ports/pdfium/release.cmake pins the release and the SHA-512
# of each archive).
#
# With vcpkg the port vcpkg-ports/pdfium installs it (vcpkg.json). Without vcpkg
# (USE_SYSTEM_PACKAGES) the same archive is downloaded into the build folder and checked by
# its SHA-512; -DPDFium_DIR=<folder of PDFiumConfig.cmake> uses another copy instead.
# -DKALAHARI_PDF_PREVIEW=OFF builds the program without the preview: PDF files then open in
# the system program.
#
# Sets KALAHARI_PDF_AVAILABLE and, with it, the target PDFium::pdfium.

option(KALAHARI_PDF_PREVIEW "Show PDF files in the program (PDFium)" ON)
set(KALAHARI_PDF_AVAILABLE FALSE)

if(NOT KALAHARI_PDF_PREVIEW)
    message(STATUS "PDF preview: Off (KALAHARI_PDF_PREVIEW=OFF; PDF files open in the system program)")
    return()
endif()

include("${CMAKE_SOURCE_DIR}/vcpkg-ports/pdfium/release.cmake")
include("${CMAKE_SOURCE_DIR}/vcpkg-ports/pdfium/install.cmake")

find_package(PDFium ${PDFIUM_VERSION} CONFIG QUIET)
if(NOT PDFium_FOUND)
    # The processor the program is built for: on macOS the one architecture asked for
    set(_pdfium_cpu "${CMAKE_SYSTEM_PROCESSOR}")
    if(APPLE AND CMAKE_OSX_ARCHITECTURES)
        list(LENGTH CMAKE_OSX_ARCHITECTURES _pdfium_architectures)
        if(_pdfium_architectures GREATER 1)
            message(FATAL_ERROR "PDFium: one architecture at a time (CMAKE_OSX_ARCHITECTURES="
                                "${CMAKE_OSX_ARCHITECTURES}), or -DKALAHARI_PDF_PREVIEW=OFF")
        endif()
        set(_pdfium_cpu "${CMAKE_OSX_ARCHITECTURES}")
    endif()
    pdfium_platform(_pdfium_platform "${CMAKE_SYSTEM_NAME}" "${_pdfium_cpu}")
    if(_pdfium_platform STREQUAL "")
        message(FATAL_ERROR "PDFium: pdfium-binaries has no archive for ${CMAKE_SYSTEM_NAME} "
                            "${_pdfium_cpu}. Give -DPDFium_DIR=<folder of PDFiumConfig.cmake>, "
                            "or build without the preview: -DKALAHARI_PDF_PREVIEW=OFF")
    endif()

    set(_pdfium_prefix "${CMAKE_BINARY_DIR}/_deps/pdfium-${PDFIUM_VERSION}-${_pdfium_platform}")
    if(NOT EXISTS "${_pdfium_prefix}/share/pdfium/PDFiumConfig.cmake")
        set(_pdfium_archive "${CMAKE_BINARY_DIR}/_deps/pdfium-${PDFIUM_VERSION}-${_pdfium_platform}.tgz")
        set(_pdfium_url "${PDFIUM_DOWNLOAD_URL}/pdfium-${_pdfium_platform}.tgz")
        message(STATUS "PDFium: downloading ${_pdfium_url}")
        file(DOWNLOAD "${_pdfium_url}" "${_pdfium_archive}"
             EXPECTED_HASH "SHA512=${PDFIUM_SHA512_${_pdfium_platform}}"
             TLS_VERIFY ON
             STATUS _pdfium_status)
        list(GET _pdfium_status 0 _pdfium_code)
        if(NOT _pdfium_code EQUAL 0)
            list(GET _pdfium_status 1 _pdfium_reason)
            file(REMOVE "${_pdfium_archive}")
            message(FATAL_ERROR "PDFium: the download failed (${_pdfium_reason}). Give "
                                "-DPDFium_DIR=<folder of PDFiumConfig.cmake>, or build without "
                                "the preview: -DKALAHARI_PDF_PREVIEW=OFF")
        endif()
        set(_pdfium_unpacked "${CMAKE_BINARY_DIR}/_deps/pdfium-unpacked")
        file(REMOVE_RECURSE "${_pdfium_unpacked}" "${_pdfium_prefix}")
        file(ARCHIVE_EXTRACT INPUT "${_pdfium_archive}" DESTINATION "${_pdfium_unpacked}")
        pdfium_install_files("${_pdfium_unpacked}" "${_pdfium_prefix}")
        file(INSTALL "${_pdfium_unpacked}/LICENSE" "${_pdfium_unpacked}/licenses"
             DESTINATION "${_pdfium_prefix}/share/pdfium" MESSAGE_NEVER)
        file(REMOVE_RECURSE "${_pdfium_unpacked}")
        file(REMOVE "${_pdfium_archive}")
    endif()
    find_package(PDFium ${PDFIUM_VERSION} CONFIG REQUIRED
                 PATHS "${_pdfium_prefix}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
endif()

set(KALAHARI_PDF_AVAILABLE TRUE)
message(STATUS "PDFium ${PDFium_VERSION}: ${PDFium_DIR} (PDF preview enabled)")
