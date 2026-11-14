// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_texts.h
/// @brief The texts of the open book in reading order, and the saved text of one not open

#pragma once

#include "kalahari/core/book_project.h"

#include <QString>

#include <optional>
#include <vector>

namespace kalahari::core {
class ProjectManager;
}

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
/// @note The elements are the book's: valid while it stays as it is
std::vector<BookTextElement> bookTextElements(const core::ProjectManager& book);

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
