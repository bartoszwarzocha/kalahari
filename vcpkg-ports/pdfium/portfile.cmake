# PDFium for the preview of PDF files: the archive of pdfium-binaries for the platform of the
# triplet (release.cmake), checked by its SHA-512. Nothing is compiled: the archives hold the
# shared library built from the official sources of PDFium, without JavaScript and XFA.

include("${CMAKE_CURRENT_LIST_DIR}/release.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/install.cmake")

# One build of the library serves Debug and Release: it has a C interface only, so the
# program of either links the same release library (CMakeLists.txt copies the DLL beside it)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_POLICY_MISMATCHED_NUMBER_OF_BINARIES enabled)
# The library carries its own C runtime and needs only libraries of the system (pdfium.dll
# imports KERNEL32, USER32, GDI32 and ADVAPI32 only), so the runtime of the triplet does not
# matter; and it is a shared library in every triplet
set(VCPKG_POLICY_SKIP_CRT_LINKAGE_CHECK enabled)
set(VCPKG_POLICY_DLLS_IN_STATIC_LIBRARY enabled)

if(VCPKG_TARGET_IS_WINDOWS)
    set(_os "Windows")
elseif(VCPKG_TARGET_IS_OSX)
    set(_os "Darwin")
elseif(VCPKG_TARGET_IS_LINUX)
    set(_os "Linux")
else()
    set(_os "${VCPKG_CMAKE_SYSTEM_NAME}")
endif()
pdfium_platform(_platform "${_os}" "${VCPKG_TARGET_ARCHITECTURE}")
if(_platform STREQUAL "")
    message(FATAL_ERROR "pdfium-binaries has no PDFium for ${TARGET_TRIPLET}")
endif()

vcpkg_download_distfile(_archive
    URLS "${PDFIUM_DOWNLOAD_URL}/pdfium-${_platform}.tgz"
    FILENAME "pdfium-${PDFIUM_VERSION}-${_platform}.tgz"
    SHA512 "${PDFIUM_SHA512_${_platform}}"
)
vcpkg_extract_source_archive(_source
    ARCHIVE "${_archive}"
    NO_REMOVE_ONE_LEVEL
)

pdfium_install_files("${_source}" "${CURRENT_PACKAGES_DIR}")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

# The licenses: of the archive, of PDFium and of the libraries built into it
file(GLOB _licenses "${_source}/licenses/*")
list(SORT _licenses)
vcpkg_install_copyright(FILE_LIST "${_source}/LICENSE" ${_licenses})
