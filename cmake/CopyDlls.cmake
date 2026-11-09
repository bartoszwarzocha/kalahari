# Copies every DLL of FROM_DIR into TO_DIR:
#   cmake -DFROM_DIR=<folder> -DTO_DIR=<folder> -P CopyDlls.cmake
#
# Run after the build on Windows. The Qt plugins load libraries of their own (libjpeg of the
# JPEG plugin, libtiff and libwebp of the TIFF and WebP ones) that nothing of the program
# links, so vcpkg's copy of what the program links leaves them out, and the plugins do not load.

if(NOT FROM_DIR OR NOT TO_DIR)
    message(FATAL_ERROR "CopyDlls.cmake needs FROM_DIR and TO_DIR")
endif()

file(GLOB _dlls "${FROM_DIR}/*.dll")
if(_dlls)
    # A file with the same time already there is not copied again
    file(COPY ${_dlls} DESTINATION "${TO_DIR}")
endif()
