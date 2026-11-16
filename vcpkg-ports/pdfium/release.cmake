# The PDFium the program is built with: a release of pdfium-binaries
# (https://github.com/bblanchon/pdfium-binaries), built weekly by GitHub Actions from the
# official sources of PDFium (https://pdfium.googlesource.com/pdfium), without JavaScript
# (V8) and XFA, with a signed record of how each archive was built (pdfium-attestation.json).
#
# Read by the port (portfile.cmake), by cmake/Pdfium.cmake (builds without vcpkg) and by
# scripts/verify_pdfium.cmake (CI: the archives are the ones pdfium-binaries built).
#
# A newer release: set PDFIUM_RELEASE and PDFIUM_VERSION (the VERSION file of an archive,
# MAJOR.MINOR.BUILD.PATCH), the version in vcpkg.json of the port, and the SHA-512 of each
# archive below (cmake -E sha512sum pdfium-<platform>.tgz). The CI check then verifies them.

set(PDFIUM_RELEASE "chromium/8152")
set(PDFIUM_VERSION "159.0.8152.0")

# The archive of each platform and its SHA-512
set(PDFIUM_PLATFORMS win-x64 win-arm64 mac-x64 mac-arm64 linux-x64 linux-arm64)
set(PDFIUM_SHA512_win-x64
    "12e63b680c9a8c9ab899ced2a4be464e64224b3e7b5cb034822bb4fa9fab7e2e03d03ca403aac2db52f8c7e9d43e6f3c82007eb097da007f17d8c856ca0b9533")
set(PDFIUM_SHA512_win-arm64
    "09b435f0ce4855ea59a5e247833d10530ca11bb3152b036ee4f0d57cf3e072ba3363dc59d271b477f677e72a89317d49acd94d29bf9b31ff8463aec90d5e1c1d")
set(PDFIUM_SHA512_mac-x64
    "e7978bc67a1a0df0a2bcb87cf3250f95c0eac9e49dc68af3c177295711a01c2758bbd7a2cb9d098bfcd7c21a0037917693d78e09d1ecbd338300e8850bee75ed")
set(PDFIUM_SHA512_mac-arm64
    "c6d9ca0963445871ff89a341da13544d9c239735266cae82bbf3a667978cb8b4a68f1de974f8c0f8e349c1fa58e121fdb26ecb3894a717a8c8c07182f1136122")
set(PDFIUM_SHA512_linux-x64
    "ff68257c24d52118810fe77b88f6b7f8f9803a83595cab843b6fa63076b79ac7a9b0dcde8be243458e56b5d6647071ca3df7182937a02e1be488312854684f47")
set(PDFIUM_SHA512_linux-arm64
    "d4ccb9df884eeff9fda9fb5d6c6a1a6a53d05fa9be7164c5d23248bacfcf34b1e48ccf3fa8153c19a3f0f866d362b2b3d6b0dba39c0b62684245b1ba0e1b8cad")

# Where the archives of the release are
string(REPLACE "/" "%2F" _pdfium_tag "${PDFIUM_RELEASE}")
set(PDFIUM_DOWNLOAD_URL "https://github.com/bblanchon/pdfium-binaries/releases/download/${_pdfium_tag}")
unset(_pdfium_tag)

# The platform of the archive (one of PDFIUM_PLATFORMS) for system @p os ("Windows", "Darwin"
# or "Linux", as CMAKE_SYSTEM_NAME) and processor @p cpu (x64, AMD64, x86_64, arm64, aarch64);
# empty when pdfium-binaries has no archive for them
function(pdfium_platform out os cpu)
    string(TOLOWER "${cpu}" _cpu)
    if(_cpu MATCHES "^(x64|amd64|x86_64)$")
        set(_cpu "x64")
    elseif(_cpu MATCHES "^(arm64|aarch64)$")
        set(_cpu "arm64")
    else()
        set(_cpu "")
    endif()
    if(os STREQUAL "Windows")
        set(_os "win")
    elseif(os STREQUAL "Darwin")
        set(_os "mac")
    elseif(os STREQUAL "Linux")
        set(_os "linux")
    else()
        set(_os "")
    endif()
    if(_os STREQUAL "" OR _cpu STREQUAL "")
        set(${out} "" PARENT_SCOPE)
    else()
        set(${out} "${_os}-${_cpu}" PARENT_SCOPE)
    endif()
endfunction()
