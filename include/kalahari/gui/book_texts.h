// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_texts.h
/// @brief The texts of the open book in reading order, and the saved text of one not open

#ifndef KALAHARI_GUI_BOOK_TEXTS_H
#define KALAHARI_GUI_BOOK_TEXTS_H

#include "kalahari/core/book_project.h"

#include <QString>

#include <algorithm>
#include <optional>
#include <vector>

namespace kalahari::core {
class ProjectManager;
struct ProjectBook;
} // namespace kalahari::core

namespace kalahari::gui {

/// @brief A text of the open book at its place
struct BookTextElement {
    const core::ProjectElement* element = nullptr;
    core::BookPlace place = core::BookPlace::Main;
    /// A chapter: of the main kind (core::ProjectManager::mainTextKind()) in the main part
    bool chapter = false;
};

/// @brief The texts of the book open in @p book in reading order: the front part, the main part
///        and the back part, the texts of their groups among them; none without a book
/// @param volume The volume of a series whose texts they are; nullptr: the current book
///        (core::ProjectManager::book())
/// @note The elements are the book's: valid while it stays as it is
std::vector<BookTextElement> bookTextElements(const core::ProjectManager& book,
                                              const core::ProjectBook* volume = nullptr);

/// @brief The volume of a series whose texts element @p elementId numbers its notes and
///        captions with: its own book, or the current book for an element of no book
///        (core::ProjectManager::bookOf()); nullptr without a book
const core::ProjectBook* volumeOfText(const core::ProjectManager& book, const QString& elementId);

/// @brief The place of the text of element @p elementId among @p texts, anything with an
///        @c elementId; -1 when it is not among them or the id is empty
template <typename Text>
[[nodiscard]] int textIndexOf(const std::vector<Text>& texts, const QString& elementId) {
    if (elementId.isEmpty()) {
        return -1;
    }
    const auto found = std::find_if(texts.cbegin(), texts.cend(), [&elementId](const Text& text) {
        return text.elementId == elementId;
    });
    return found != texts.cend() ? static_cast<int>(found - texts.cbegin()) : -1;
}

/// @brief The KML of a text of the book that is not open, when it changed: the unsaved text
///        the program keeps, else its file
/// @param book The book the text is in
/// @param elementId The text's element
/// @param signature What it was when it was read last (empty: never); it becomes what it is
///        now: the file's time and size, or the unsaved text's hash
/// @return Its KML when it changed (or was never read); std::nullopt when it is as it was
std::optional<QString> savedTextIfChanged(const core::ProjectManager& book,
                                          const QString& elementId, QString& signature);

/// @brief Before Replace All in a text of the book, the book keeps the text as it is as a
///        version, if the settings say so (ChapterVersions)
/// @param book The book the text is in
/// @param elementId The text's element
/// @param kml The text before the replacement
/// @param found What is replaced: @p count matches of it
/// @param replacement What they become
/// @param count How many are replaced
void keepVersionBeforeReplaceAll(core::ProjectManager& book, const QString& elementId,
                                 const QString& kml, const QString& found,
                                 const QString& replacement, int count);

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_BOOK_TEXTS_H
