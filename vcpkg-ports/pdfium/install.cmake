# Puts an unpacked archive of pdfium-binaries (@p source) into @p prefix as
# find_package(PDFium CONFIG) finds it: the headers in include/pdfium, the library in lib
# (the DLL in bin), PDFiumConfig.cmake and its version in share/pdfium. Used by the port
# (portfile.cmake) and by cmake/Pdfium.cmake; needs release.cmake (PDFIUM_VERSION).

function(pdfium_install_files source prefix)
    if(NOT EXISTS "${source}/include/fpdfview.h")
        message(FATAL_ERROR "No PDFium headers in ${source}")
    endif()
    # The headers, without the copies the patches of pdfium-binaries leave (*.h.orig)
    file(INSTALL "${source}/include/"
         DESTINATION "${prefix}/include/pdfium"
         MESSAGE_NEVER
         FILES_MATCHING PATTERN "*.h")
    if(EXISTS "${source}/bin/pdfium.dll")
        file(INSTALL "${source}/bin/pdfium.dll" DESTINATION "${prefix}/bin" MESSAGE_NEVER)
        file(INSTALL "${source}/lib/pdfium.dll.lib" DESTINATION "${prefix}/lib" MESSAGE_NEVER)
    elseif(EXISTS "${source}/lib/libpdfium.dylib")
        file(INSTALL "${source}/lib/libpdfium.dylib" DESTINATION "${prefix}/lib" MESSAGE_NEVER)
    elseif(EXISTS "${source}/lib/libpdfium.so")
        file(INSTALL "${source}/lib/libpdfium.so" DESTINATION "${prefix}/lib" MESSAGE_NEVER)
    else()
        message(FATAL_ERROR "No PDFium library in ${source}")
    endif()
    file(INSTALL "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/PDFiumConfig.cmake"
         DESTINATION "${prefix}/share/pdfium" MESSAGE_NEVER)
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/PDFiumConfigVersion.cmake.in"
                   "${prefix}/share/pdfium/PDFiumConfigVersion.cmake" @ONLY)
endfunction()
