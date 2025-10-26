# Kalahari Precompiled Headers Configuration
# Speeds up compilation by precompiling frequently-used headers

# Function to setup precompiled headers for a target
function(kalahari_add_pch target_name)
    # Only enable PCH for GCC/Clang/MSVC (CMake 3.16+)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
        target_precompile_headers(${target_name} PRIVATE
            # C++ Standard Library (most frequently used)
            <string>
            <vector>
            <memory>
            <map>
            <unordered_map>
            <algorithm>
            <functional>
            <iostream>
            <fstream>
            <sstream>
            <stdexcept>
            <optional>

            # wxWidgets (Phase 1+)
            # Uncomment when wxWidgets is used in Phase 1:
            # <wx/wx.h>
            # <wx/aui/aui.h>
            # <wx/richtext/richtextctrl.h>

            # Third-party libraries
            <spdlog/spdlog.h>
            <nlohmann/json.hpp>
        )
        message(STATUS "Precompiled headers enabled for ${target_name}")
    else()
        message(WARNING "CMake ${CMAKE_VERSION} does not support precompiled headers (requires 3.16+)")
    endif()
endfunction()
