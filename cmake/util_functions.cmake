cmake_minimum_required(VERSION 3.21)

# Function to set output directories for libraries and executables
function(set_output_directories target_name)
    if(WIN32)
        if(BUILD_SHARED_LIBS)
            set(lib_dir "${CMAKE_SOURCE_DIR}/lib/${VCPKG_TARGET_TRIPLET}-dll")
            set(bin_dir "${CMAKE_SOURCE_DIR}/bin/${VCPKG_TARGET_TRIPLET}-dll")
        else()
            set(lib_dir "${CMAKE_SOURCE_DIR}/lib/${VCPKG_TARGET_TRIPLET}")
            set(bin_dir "${CMAKE_SOURCE_DIR}/bin/${VCPKG_TARGET_TRIPLET}")
        endif()
    else()
        set(lib_dir "${CMAKE_SOURCE_DIR}/lib/${VCPKG_TARGET_TRIPLET}/${CMAKE_BUILD_TYPE}")
        set(bin_dir "${CMAKE_SOURCE_DIR}/bin/${VCPKG_TARGET_TRIPLET}/${CMAKE_BUILD_TYPE}")
    endif()

    set_target_properties(${target_name} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${lib_dir}"
        LIBRARY_OUTPUT_DIRECTORY "${lib_dir}"
        RUNTIME_OUTPUT_DIRECTORY "${bin_dir}"
    )
endfunction()