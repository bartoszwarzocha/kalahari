/// @file book_constants.h
/// @brief Known book element type constants and utilities
///
/// Defines predefined type constants for BookElement instances. These constants
/// represent standard book sections (front matter, chapters, back matter) that
/// receive special handling in the GUI (icons, labels, templates).
///
/// Type System Design:
/// - **Known types:** Defined here, GUI provides icons/labels/templates
/// - **Unknown types:** User-defined or plugin-defined, displayed generically
/// - **Extensibility:** String-based (not enum) allows plugins to add new types
///
/// Example usage:
/// @code{.cpp}
/// using namespace kalahari::core;
///
/// // Create chapter with known type
/// auto chapter = std::make_shared<BookElement>(TYPE_CHAPTER, "ch-001", "Chapter 1");
///
/// // Check if type is known
/// if (isKnownType(chapter->getType())) {
///     std::cout << "Display name: " << getDisplayName(chapter->getType()) << "\n";
/// }
///
/// // Create custom type (plugin-defined or user-defined)
/// auto notes = std::make_shared<BookElement>("character_notes", "notes-001", "Character Notes");
/// // GUI will display as generic "Section" (no special icon/template)
/// @endcode

#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

namespace kalahari {
namespace core {

// =============================================================================
// Known Type Constants
// =============================================================================

/// @name Front Matter Types
/// @{

/// Title page (usually first page with book title, author, publisher)
inline constexpr const char* TYPE_TITLE_PAGE = "title_page";

/// Copyright page (copyright notice, ISBN, publication info)
inline constexpr const char* TYPE_COPYRIGHT = "copyright";

/// Dedication (short dedication to person/group)
inline constexpr const char* TYPE_DEDICATION = "dedication";

/// Preface (author's introduction explaining purpose/background)
inline constexpr const char* TYPE_PREFACE = "preface";

/// @}

/// @name Body Types
/// @{

/// Chapter (main content unit in book body)
inline constexpr const char* TYPE_CHAPTER = "chapter";

/// @}

/// @name Back Matter Types
/// @{

/// Epilogue (concluding section, wraps up story)
inline constexpr const char* TYPE_EPILOGUE = "epilogue";

/// Glossary (definitions of terms used in book)
inline constexpr const char* TYPE_GLOSSARY = "glossary";

/// Bibliography (list of sources/references)
inline constexpr const char* TYPE_BIBLIOGRAPHY = "bibliography";

/// About the Author (author biography, photo, contact)
inline constexpr const char* TYPE_ABOUT_AUTHOR = "about_author";

/// @}

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Get set of all known types
/// @return Unordered set with all predefined type constants
///
/// Used for validation and GUI decisions (e.g., show icon vs generic label).
inline const std::unordered_set<std::string_view>& getKnownTypes() {
    static const std::unordered_set<std::string_view> knownTypes = {
        // Front matter
        TYPE_TITLE_PAGE,
        TYPE_COPYRIGHT,
        TYPE_DEDICATION,
        TYPE_PREFACE,

        // Body
        TYPE_CHAPTER,

        // Back matter
        TYPE_EPILOGUE,
        TYPE_GLOSSARY,
        TYPE_BIBLIOGRAPHY,
        TYPE_ABOUT_AUTHOR
    };
    return knownTypes;
}

/// @brief Check if type is a known predefined type
/// @param type Type string to check
/// @return true if type is in getKnownTypes(), false otherwise
///
/// Example:
/// @code{.cpp}
/// if (isKnownType("chapter")) {
///     // Show chapter icon
/// } else {
///     // Show generic section icon
/// }
/// @endcode
inline bool isKnownType(const std::string& type) {
    return getKnownTypes().count(type) > 0;
}

/// @brief Get user-friendly display name for a type
/// @param type Type string (e.g., "title_page", "chapter", "custom_type")
/// @return Human-readable name for known types, or "Section" for unknown types
///
/// Mapping (Phase 0 - English only, Phase 2+ will use i18n):
/// - "title_page" → "Title Page"
/// - "chapter" → "Chapter"
/// - "custom_type" → "Section" (generic fallback)
///
/// Example:
/// @code{.cpp}
/// std::string displayName = getDisplayName(element->getType());
/// // GUI shows: "Title Page" instead of "title_page"
/// @endcode
inline std::string getDisplayName(const std::string& type) {
    // Phase 0: Simple English mapping (Phase 2+: use wxLocale + gettext)
    if (type == TYPE_TITLE_PAGE)    return "Title Page";
    if (type == TYPE_COPYRIGHT)     return "Copyright";
    if (type == TYPE_DEDICATION)    return "Dedication";
    if (type == TYPE_PREFACE)       return "Preface";
    if (type == TYPE_CHAPTER)       return "Chapter";
    if (type == TYPE_EPILOGUE)      return "Epilogue";
    if (type == TYPE_GLOSSARY)      return "Glossary";
    if (type == TYPE_BIBLIOGRAPHY)  return "Bibliography";
    if (type == TYPE_ABOUT_AUTHOR)  return "About the Author";

    // Unknown type → generic fallback
    return "Section";
}

/// @brief Get category for a type (front matter, body, back matter)
/// @param type Type string
/// @return "front", "body", "back", or "unknown"
///
/// Used for organizing elements in Project Navigator tree structure.
///
/// Example:
/// @code{.cpp}
/// std::string category = getTypeCategory("title_page");  // Returns "front"
/// // GUI places element in "Front Matter" tree node
/// @endcode
inline std::string getTypeCategory(const std::string& type) {
    // Front matter types
    if (type == TYPE_TITLE_PAGE || type == TYPE_COPYRIGHT ||
        type == TYPE_DEDICATION || type == TYPE_PREFACE) {
        return "front";
    }

    // Body types
    if (type == TYPE_CHAPTER) {
        return "body";
    }

    // Back matter types
    if (type == TYPE_EPILOGUE || type == TYPE_GLOSSARY ||
        type == TYPE_BIBLIOGRAPHY || type == TYPE_ABOUT_AUTHOR) {
        return "back";
    }

    // Unknown → no assumption
    return "unknown";
}

} // namespace core
} // namespace kalahari
