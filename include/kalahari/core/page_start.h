// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file page_start.h
/// @brief Which page of a printed book a text starts on: a right one, a left one or the next
///
/// With mirror margins the pages of a book face each other, as in an open book: a right page
/// (odd: 1, 3, 5) and a left one (even). Each kind of text has its place: the title page, the
/// dedication and a part's title on a right page, the copyright page on a left one (the back
/// of the title page). The book type says it of each kind ("firstPage" of a kind in its
/// booktype.json, ElementKind::firstPage); a kind without it, as the chapter, starts as
/// Settings > Editor > Pages and Margins say ("editor.page.textsStart"): on a right page, with
/// a left page left empty before it when needed, or on the next page, left or right. A text
/// of the book can say otherwise in the Properties panel ("firstPage" of its element in the
/// .klh file).

#ifndef KALAHARI_CORE_PAGE_START_H
#define KALAHARI_CORE_PAGE_START_H

#include <QLatin1String>
#include <QString>

#include <optional>

namespace kalahari::core {

/// @brief Which page a text starts on
enum class PageStart {
    Auto,   ///< As its kind says, and a kind that says nothing as the settings say
    Right,  ///< A right page: a left page before it is left empty when needed
    Left,   ///< A left page: a right page before it is left empty when needed
    Next,   ///< The next page, left or right
};

/// @brief The names of the files and of the settings ("firstPage", "editor.page.textsStart")
inline constexpr const char* PAGE_START_RIGHT = "right";  ///< PageStart::Right
inline constexpr const char* PAGE_START_LEFT = "left";    ///< PageStart::Left
inline constexpr const char* PAGE_START_NEXT = "next";    ///< PageStart::Next

/// @brief The setting of the texts whose kind says nothing ("right" or "next")
inline constexpr const char* TEXTS_START_KEY = "editor.page.textsStart";

/// @brief The name of @p start in the files; empty for PageStart::Auto (no field)
[[nodiscard]] inline QString pageStartName(PageStart start) {
    switch (start) {
        case PageStart::Right:
            return QString::fromLatin1(PAGE_START_RIGHT);
        case PageStart::Left:
            return QString::fromLatin1(PAGE_START_LEFT);
        case PageStart::Next:
            return QString::fromLatin1(PAGE_START_NEXT);
        case PageStart::Auto:
            break;
    }
    return {};
}

/// @brief The page of a name of the files; nullopt for another name (empty: PageStart::Auto)
[[nodiscard]] inline std::optional<PageStart> pageStartFromName(const QString& name) {
    if (name.isEmpty()) {
        return PageStart::Auto;
    }
    if (name == QLatin1String(PAGE_START_RIGHT)) {
        return PageStart::Right;
    }
    if (name == QLatin1String(PAGE_START_LEFT)) {
        return PageStart::Left;
    }
    if (name == QLatin1String(PAGE_START_NEXT)) {
        return PageStart::Next;
    }
    return std::nullopt;
}

/// @brief The page the texts whose kind says nothing start on, as the setting names it
///        ("next": the next page; any other name, "right" among them: a right page)
[[nodiscard]] inline PageStart textsStartFromName(const QString& name) {
    return name == QLatin1String(PAGE_START_NEXT) ? PageStart::Next : PageStart::Right;
}

/// @brief The page a text starts on: its own choice, else its kind's, else the setting's
/// @param own The text's choice (the Properties panel)
/// @param kind Its kind's (the book type); PageStart::Auto: the kind says nothing, or the
///             program does not have it
/// @param texts The setting's (textsStartFromName()); PageStart::Auto is a right page
/// @return PageStart::Right, PageStart::Left or PageStart::Next
[[nodiscard]] inline PageStart resolvedPageStart(PageStart own, PageStart kind, PageStart texts) {
    if (own != PageStart::Auto) {
        return own;
    }
    if (kind != PageStart::Auto) {
        return kind;
    }
    return texts == PageStart::Auto ? PageStart::Right : texts;
}

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_PAGE_START_H
