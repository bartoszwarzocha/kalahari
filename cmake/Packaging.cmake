# The program as it is installed and packaged: the icon and the version of the program file
# (Windows), the bundle of macOS, and where `cmake --install` puts each part. scripts/package.py
# makes the packages of the three systems from this layout (BUILDING.md, "Packages").
#
#   Windows  <prefix>/kalahari.exe, kalahari_core.dll and the DLLs of vcpkg, the Qt plugins
#            (platforms, styles, imageformats, tls, sqldrivers), resources/, the Python of the
#            plugins (Lib/, DLLs/), LICENSE.txt, NOTICE.md, THIRD-PARTY.md, licenses/
#   macOS    <prefix>/kalahari.app: Contents/MacOS/kalahari, Contents/Frameworks (the libraries
#            of the program), Contents/Resources (resources/, python3/, kalahari.icns,
#            licenses/)
#   Linux    <prefix>/bin/kalahari, lib/ (kalahari_core, PDFium), share/kalahari/resources,
#            share/applications, share/icons, share/mime, share/doc/kalahari. The Python
#            standard library only with --component python: the packages carry the Python the
#            program is built with, an installation into /usr/local uses the system's
#
# Included by the root CMakeLists.txt after the targets kalahari and kalahari_core.

set(KALAHARI_PACKAGING_DIR "${CMAKE_SOURCE_DIR}/packaging")
set(KALAHARI_COPYRIGHT "Copyright (c) 2025-2026 Bartosz W. Warzocha")
# Who makes the program: the publisher of the installer and the company of the program file
set(KALAHARI_PUBLISHER "BW Digital Systems & Publishing")
# The application id of the desktop: kalahari.desktop on Linux, the bundle id on macOS
set(KALAHARI_BUNDLE_ID "io.github.bartoszwarzocha.kalahari")
# The sizes of resources/images/app_icon (scripts/make_app_icons.py)
set(KALAHARI_ICON_SIZES 16 24 32 48 64 128 256 512)
set(KALAHARI_PYTHON_VERSION "${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")

# The parts of the standard library of Python the plugins do not need: its tests and test
# modules, the editor IDLE, Tk, pip's installer and the libraries Python was built from, and
# the modules of a terminal and of dbm files, whose libraries (GNU readline, gdbm) the packages
# would otherwise carry
set(KALAHARI_PYTHON_EXCLUDES
    PATTERN "readline.cpython-*" EXCLUDE
    PATTERN "_curses*" EXCLUDE
    PATTERN "_dbm.*" EXCLUDE
    PATTERN "_gdbm.*" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
    PATTERN "test" EXCLUDE
    PATTERN "tests" EXCLUDE
    PATTERN "idle_test" EXCLUDE
    PATTERN "idlelib" EXCLUDE
    PATTERN "tkinter" EXCLUDE
    PATTERN "turtledemo" EXCLUDE
    PATTERN "ensurepip" EXCLUDE
    PATTERN "lib2to3" EXCLUDE
    PATTERN "site-packages" EXCLUDE
    PATTERN "config-*" EXCLUDE
    PATTERN "_tkinter*" EXCLUDE
    PATTERN "_test*" EXCLUDE
    PATTERN "_ctypes_test*" EXCLUDE
    PATTERN "xxlimited*" EXCLUDE
    PATTERN "*.pdb" EXCLUDE
)

# The licence of PDFium and of the libraries it is built with, next to its CMake files
set(_kalahari_pdfium_licenses "")
if(KALAHARI_PDF_AVAILABLE AND PDFium_DIR AND EXISTS "${PDFium_DIR}/LICENSE")
    set(_kalahari_pdfium_licenses "${PDFium_DIR}/LICENSE")
endif()

if(WIN32 OR APPLE)
    # The prefix vcpkg installs the libraries into
    if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
        set(_kalahari_vcpkg_prefix "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    elseif(WIN32)
        set(_kalahari_vcpkg_prefix "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
    else()
        set(_kalahari_vcpkg_prefix "")
    endif()
endif()

if(WIN32)
    # ------------------------------------------------------------------------------------------
    # Windows: everything in one folder, as the build runs the program
    # ------------------------------------------------------------------------------------------
    set(KALAHARI_ICO_PATH "${KALAHARI_PACKAGING_DIR}/icons/kalahari.ico")
    if(KALAHARI_VERSION_SUFFIX)
        set(KALAHARI_RC_FILEFLAGS "VS_FF_PRERELEASE")
    else()
        set(KALAHARI_RC_FILEFLAGS "0x0L")
    endif()
    configure_file("${KALAHARI_PACKAGING_DIR}/windows/kalahari.rc.in"
                   "${CMAKE_BINARY_DIR}/packaging/kalahari.rc" @ONLY)
    target_sources(kalahari PRIVATE "${CMAKE_BINARY_DIR}/packaging/kalahari.rc")

    install(TARGETS kalahari RUNTIME DESTINATION .)
    install(TARGETS kalahari_core
            RUNTIME DESTINATION .
            ARCHIVE DESTINATION lib COMPONENT development EXCLUDE_FROM_ALL)

    # The DLLs of vcpkg: the program's libraries and those the Qt plugins load (as
    # cmake/CopyDlls.cmake copies them next to the program after the build)
    set(_kalahari_vcpkg_config "${_kalahari_vcpkg_prefix}/$<$<CONFIG:Debug>:debug/>")
    install(DIRECTORY "${_kalahari_vcpkg_config}bin/"
            DESTINATION .
            FILES_MATCHING PATTERN "*.dll")
    if(KALAHARI_PDF_AVAILABLE)
        install(FILES "$<TARGET_FILE:PDFium::pdfium>" DESTINATION .)
    endif()
    # The runtime of Visual C++ (vcruntime140.dll, msvcp140.dll, ...) next to the program: a
    # Windows without the Visual C++ Redistributable has none, while a build machine has it in
    # System32 with Visual Studio. The universal C runtime (ucrtbase.dll) is part of Windows 10
    # and newer. scripts/package.py ends the package when a DLL is missing from it
    if(MSVC)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS TRUE)
        set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)
        include(InstallRequiredSystemLibraries)
        if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS AND DEFINED ENV{VCToolsRedistDir})
            # The environment of the compiler (vcvars) names the folder of its runtime
            file(GLOB CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS
                 "$ENV{VCToolsRedistDir}/x64/Microsoft.VC*.CRT/*.dll")
        endif()
        if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
            install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION .)
        else()
            message(STATUS "No runtime of Visual C++ ${MSVC_VERSION} found for the packages")
        endif()
    endif()
    # The Qt plugins the build copies next to the program (src/CMakeLists.txt)
    foreach(_kalahari_plugins IN ITEMS platforms styles imageformats tls sqldrivers)
        install(DIRECTORY "${_kalahari_vcpkg_config}Qt6/plugins/${_kalahari_plugins}"
                DESTINATION .
                FILES_MATCHING PATTERN "*.dll")
    endforeach()

    install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources" DESTINATION .)

    # The Python of the plugins: PythonInterpreter takes the folder of the program as its home
    # when Lib is there (python_interpreter.cpp)
    install(DIRECTORY "${_kalahari_vcpkg_prefix}/tools/python3/Lib"
            DESTINATION .
            ${KALAHARI_PYTHON_EXCLUDES})
    install(DIRECTORY "${_kalahari_vcpkg_prefix}/tools/python3/DLLs"
            DESTINATION .
            OPTIONAL
            ${KALAHARI_PYTHON_EXCLUDES})

    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION . RENAME LICENSE.txt)
    install(FILES "${CMAKE_SOURCE_DIR}/NOTICE.md" "${KALAHARI_PACKAGING_DIR}/THIRD-PARTY.md"
            DESTINATION .)
    set(_kalahari_licenses_destination "licenses")
elseif(APPLE)
    # ------------------------------------------------------------------------------------------
    # macOS: the bundle with its own libraries and Python
    # ------------------------------------------------------------------------------------------
    set(_kalahari_icns "${KALAHARI_PACKAGING_DIR}/icons/kalahari.icns")
    target_sources(kalahari PRIVATE "${_kalahari_icns}")
    set_source_files_properties("${_kalahari_icns}" TARGET_DIRECTORY kalahari
                                PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    # The values go into the XML of Info.plist as they are: "&" of the copyright would make it
    # a file no one reads (codesign: "invalid Info.plist")
    string(REPLACE "&" "&amp;" _kalahari_plist_copyright "${KALAHARI_COPYRIGHT}")
    string(REPLACE "<" "&lt;" _kalahari_plist_copyright "${_kalahari_plist_copyright}")
    set_target_properties(kalahari PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${KALAHARI_PACKAGING_DIR}/macos/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Kalahari"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${KALAHARI_BUNDLE_ID}"
        MACOSX_BUNDLE_ICON_FILE "kalahari.icns"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "${_kalahari_plist_copyright}"
        # The installed bundle finds its libraries in Contents/Frameworks
        INSTALL_RPATH "@executable_path/../Frameworks")
    set_target_properties(kalahari_core PROPERTIES INSTALL_RPATH "@loader_path")

    set(_kalahari_bundle "kalahari.app/Contents")
    install(TARGETS kalahari BUNDLE DESTINATION .)
    install(TARGETS kalahari_core
            LIBRARY DESTINATION "${_kalahari_bundle}/Frameworks"
            ARCHIVE DESTINATION lib COMPONENT development EXCLUDE_FROM_ALL)
    if(KALAHARI_PDF_AVAILABLE)
        install(FILES "$<TARGET_FILE:PDFium::pdfium>" DESTINATION "${_kalahari_bundle}/Frameworks")
    endif()

    # The Python of the plugins: PythonInterpreter looks for Contents/Resources/python3
    # (python_interpreter.cpp); vcpkg installs the standard library under its prefix
    set(_kalahari_python_stdlib "")
    foreach(_kalahari_candidate IN ITEMS
            "${_kalahari_vcpkg_prefix}/lib/python${KALAHARI_PYTHON_VERSION}"
            "${_kalahari_vcpkg_prefix}/tools/python3/lib/python${KALAHARI_PYTHON_VERSION}"
            "${Python3_STDLIB}")
        if(_kalahari_candidate AND EXISTS "${_kalahari_candidate}/os.py")
            set(_kalahari_python_stdlib "${_kalahari_candidate}")
            break()
        endif()
    endforeach()
    if(_kalahari_python_stdlib)
        install(DIRECTORY "${_kalahari_python_stdlib}"
                DESTINATION "${_kalahari_bundle}/Resources/python3/lib"
                ${KALAHARI_PYTHON_EXCLUDES})
    else()
        message(WARNING "No standard library of Python ${KALAHARI_PYTHON_VERSION}: the "
                        "installed bundle has no Python for the plugins")
    endif()

    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/NOTICE.md"
                  "${KALAHARI_PACKAGING_DIR}/THIRD-PARTY.md"
            DESTINATION "${_kalahari_bundle}/Resources/licenses")
    set(_kalahari_licenses_destination "${_kalahari_bundle}/Resources/licenses")
else()
    # ------------------------------------------------------------------------------------------
    # Linux and other Unix systems: the program under any prefix, its libraries next to it
    # ------------------------------------------------------------------------------------------
    set_target_properties(kalahari PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
    set_target_properties(kalahari_core PROPERTIES INSTALL_RPATH "$ORIGIN")

    install(TARGETS kalahari RUNTIME DESTINATION bin)
    install(TARGETS kalahari_core
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib COMPONENT development EXCLUDE_FROM_ALL)
    if(KALAHARI_PDF_AVAILABLE)
        install(FILES "$<TARGET_FILE:PDFium::pdfium>" DESTINATION lib)
    endif()

    # ResourcePaths looks for <prefix>/share/kalahari/resources next to <prefix>/bin
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources" DESTINATION share/kalahari)

    # The menu entry, its icon, the types of the books (.klh) and the documents (.kchapter) and
    # their icon in the file managers: the logo of the program, named after each MIME type as
    # shared-mime-info looks it up (src/gui/utils/file_associations.cpp installs the same for
    # an AppImage)
    install(FILES "${KALAHARI_PACKAGING_DIR}/linux/kalahari.desktop" DESTINATION share/applications)
    install(FILES "${KALAHARI_PACKAGING_DIR}/linux/kalahari-mime.xml"
            DESTINATION share/mime/packages RENAME kalahari.xml)
    foreach(_kalahari_size IN LISTS KALAHARI_ICON_SIZES)
        set(_kalahari_icon "${CMAKE_SOURCE_DIR}/resources/images/app_icon/kalahari_${_kalahari_size}.png")
        set(_kalahari_icons "share/icons/hicolor/${_kalahari_size}x${_kalahari_size}")
        install(FILES "${_kalahari_icon}" DESTINATION "${_kalahari_icons}/apps" RENAME kalahari.png)
        foreach(_kalahari_type IN ITEMS application-x-kalahari-book application-x-kalahari-document)
            install(FILES "${_kalahari_icon}" DESTINATION "${_kalahari_icons}/mimetypes"
                    RENAME "${_kalahari_type}.png")
        endforeach()
    endforeach()

    # The Python of the packages: PythonInterpreter takes as its home the folder above the
    # libpython the program loads, when lib/python3.X is there (python_interpreter.cpp)
    if(Python3_STDLIB AND EXISTS "${Python3_STDLIB}/os.py")
        install(DIRECTORY "${Python3_STDLIB}/"
                DESTINATION "lib/python${KALAHARI_PYTHON_VERSION}"
                COMPONENT python EXCLUDE_FROM_ALL
                ${KALAHARI_PYTHON_EXCLUDES})
    endif()

    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/NOTICE.md"
                  "${KALAHARI_PACKAGING_DIR}/THIRD-PARTY.md"
            DESTINATION share/doc/kalahari)
    set(_kalahari_licenses_destination "share/doc/kalahari/licenses")
endif()

# The licences of the libraries: PDFium's, and on Windows and macOS those vcpkg keeps for each
# of its packages (share/<package>/copyright), as licenses/<package>.txt
if(_kalahari_pdfium_licenses)
    install(FILES ${_kalahari_pdfium_licenses}
            DESTINATION "${_kalahari_licenses_destination}" RENAME pdfium.txt)
    if(IS_DIRECTORY "${PDFium_DIR}/licenses")
        install(DIRECTORY "${PDFium_DIR}/licenses/"
                DESTINATION "${_kalahari_licenses_destination}/pdfium")
    endif()
endif()
if((WIN32 OR APPLE) AND _kalahari_vcpkg_prefix)
    install(CODE "set(KALAHARI_LICENSES_FROM \"${_kalahari_vcpkg_prefix}/share\")
                  set(KALAHARI_LICENSES_TO \"${_kalahari_licenses_destination}\")")
    install(SCRIPT "${CMAKE_SOURCE_DIR}/cmake/InstallLicenses.cmake")
endif()
