# Part of `cmake --install` on Windows and macOS (cmake/Packaging.cmake): the licence of each
# package of vcpkg the program is built with, share/<package>/copyright, as
# <prefix>/<KALAHARI_LICENSES_TO>/<package>.txt.
#
# Needs KALAHARI_LICENSES_FROM (the share folder of vcpkg) and KALAHARI_LICENSES_TO (relative
# to the prefix of the installation).

if(NOT KALAHARI_LICENSES_FROM OR NOT KALAHARI_LICENSES_TO)
    message(FATAL_ERROR "InstallLicenses.cmake needs KALAHARI_LICENSES_FROM and KALAHARI_LICENSES_TO")
endif()

set(_licenses_to "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${KALAHARI_LICENSES_TO}")
file(GLOB _copyrights "${KALAHARI_LICENSES_FROM}/*/copyright")
file(MAKE_DIRECTORY "${_licenses_to}")
foreach(_copyright IN LISTS _copyrights)
    get_filename_component(_package "${_copyright}" DIRECTORY)
    get_filename_component(_package "${_package}" NAME)
    file(COPY_FILE "${_copyright}" "${_licenses_to}/${_package}.txt" ONLY_IF_DIFFERENT)
endforeach()
list(LENGTH _copyrights _count)
message(STATUS "Installing: the licences of ${_count} packages of vcpkg in ${_licenses_to}")
