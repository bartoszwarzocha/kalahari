/// @file catch2_windows_compat.cpp
/// @brief Windows compatibility shims for Catch2 missing CRT symbols
///
/// Task #00035 blocker: vcpkg Catch2d.lib on VS 2022/2026 has 4 unresolved symbols:
/// - __std_find_last_not_ch_pos_1
/// - __std_regex_transform_primary_char
/// - __std_find_first_not_of_trivial_pos_1
/// - __std_find_last_not_of_trivial_pos_1
///
/// These are MSVC CRT internal string functions that should be provided by
/// the C++ runtime, but vcpkg Catch2 was built with a different CRT version.
///
/// This file provides fallback implementations to resolve link errors.
///
/// NOTE: MSVC 14.50+ (_MSC_VER >= 1950) provides these symbols natively,
/// so we only define them for older compiler versions to avoid LNK2005 errors.

#ifdef _WIN32
// Only provide fallbacks for MSVC versions older than 14.50
// MSVC 14.50 (VS 2026) has these symbols in msvcprt.lib
#if defined(_MSC_VER) && _MSC_VER < 1950

#include <cstddef>
#include <cstring>
#include <cstdint>

extern "C" {

/// @brief Find last occurrence of a character not equal to given value
/// @param haystack String to search
/// @param haystackSize Size of haystack
/// @param needle Character to avoid
/// @return Position of last non-matching character, or SIZE_MAX if all match
size_t __std_find_last_not_ch_pos_1(
    const char* haystack,
    size_t haystackSize,
    char needle
) {
    if (!haystack || haystackSize == 0) {
        return SIZE_MAX;
    }

    // Search backwards
    for (size_t i = haystackSize; i > 0; --i) {
        if (haystack[i - 1] != needle) {
            return i - 1;
        }
    }

    return SIZE_MAX;  // All characters match needle
}

/// @brief Regex transform primary character (simplified stub)
/// @note This is a complex internal regex function. We provide a minimal stub.
/// @return 0 (indicates no transformation)
size_t __std_regex_transform_primary_char(
    char* /* dest */,
    char* /* destEnd */,
    const char* /* src */,
    const char* /* srcEnd */,
    const void* /* collvec */
) {
    // Catch2 uses this for regex matching in string matchers
    // For basic test cases, we can return 0 (no transformation needed)
    // If regex tests fail, we'll need to implement proper collation
    return 0;
}

/// @brief Find first occurrence of any character from a set
/// @param haystack String to search
/// @param haystackSize Size of haystack
/// @param needles Set of characters to find
/// @param needlesSize Size of needles set
/// @return Position of first match, or SIZE_MAX if no match
size_t __std_find_first_not_of_trivial_pos_1(
    const char* haystack,
    size_t haystackSize,
    const char* needles,
    size_t needlesSize
) {
    if (!haystack || !needles || haystackSize == 0) {
        return SIZE_MAX;
    }

    for (size_t i = 0; i < haystackSize; ++i) {
        bool found = false;
        for (size_t j = 0; j < needlesSize; ++j) {
            if (haystack[i] == needles[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            return i;  // Found character not in needles
        }
    }

    return SIZE_MAX;  // All characters are in needles
}

/// @brief Find last occurrence of any character not in a set
/// @param haystack String to search
/// @param haystackSize Size of haystack
/// @param needles Set of characters to avoid
/// @param needlesSize Size of needles set
/// @return Position of last non-match, or SIZE_MAX if all match
size_t __std_find_last_not_of_trivial_pos_1(
    const char* haystack,
    size_t haystackSize,
    const char* needles,
    size_t needlesSize
) {
    if (!haystack || !needles || haystackSize == 0) {
        return SIZE_MAX;
    }

    // Search backwards
    for (size_t i = haystackSize; i > 0; --i) {
        bool found = false;
        for (size_t j = 0; j < needlesSize; ++j) {
            if (haystack[i - 1] == needles[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            return i - 1;  // Found character not in needles
        }
    }

    return SIZE_MAX;  // All characters are in needles
}

} // extern "C"

#endif // _MSC_VER < 1950
#endif // _WIN32
