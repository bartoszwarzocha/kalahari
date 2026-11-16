# PDFium of pdfium-binaries (vcpkg-ports/pdfium): find_package(PDFium CONFIG) gives the
# shared library PDFium::pdfium. Kept in <prefix>/share/pdfium, with the headers in
# <prefix>/include/pdfium and the library in <prefix>/lib (the DLL in <prefix>/bin); its
# version is in PDFiumConfigVersion.cmake beside it.

get_filename_component(_pdfium_prefix "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(NOT TARGET PDFium::pdfium)
    add_library(PDFium::pdfium SHARED IMPORTED)
    set_target_properties(PDFium::pdfium PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_pdfium_prefix}/include/pdfium"
    )
    if(WIN32)
        set_target_properties(PDFium::pdfium PROPERTIES
            IMPORTED_LOCATION "${_pdfium_prefix}/bin/pdfium.dll"
            IMPORTED_IMPLIB "${_pdfium_prefix}/lib/pdfium.dll.lib"
        )
    elseif(APPLE)
        # The library names itself @rpath/libpdfium.dylib: the program finds it by its rpath
        set_target_properties(PDFium::pdfium PROPERTIES
            IMPORTED_LOCATION "${_pdfium_prefix}/lib/libpdfium.dylib"
            IMPORTED_SONAME "@rpath/libpdfium.dylib"
        )
    else()
        set_target_properties(PDFium::pdfium PROPERTIES
            IMPORTED_LOCATION "${_pdfium_prefix}/lib/libpdfium.so"
            IMPORTED_SONAME "libpdfium.so"
        )
    endif()
endif()

get_target_property(_pdfium_library PDFium::pdfium IMPORTED_LOCATION)
if(EXISTS "${_pdfium_library}" AND EXISTS "${_pdfium_prefix}/include/pdfium/fpdfview.h")
    set(PDFium_FOUND TRUE)
else()
    set(PDFium_FOUND FALSE)
    set(PDFium_NOT_FOUND_MESSAGE "PDFium is not complete in ${_pdfium_prefix}")
endif()
unset(_pdfium_library)
unset(_pdfium_prefix)
