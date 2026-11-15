// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_styles.cpp
/// @brief The paragraph styles of the open book

#include "kalahari/gui/book_styles.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/kind_names.h"

namespace kalahari::gui {

QList<core::PackageParagraphStyle> baseStyles() {
    return core::ProjectManager::installedBookTypes().paragraphStyles(
        QString::fromLatin1(core::ProjectManager::BASE_PACKAGE));
}

QList<core::PackageParagraphStyle> bookTypeStyles(const core::ProjectManager& book) {
    const core::BookProject* project = book.project();
    QList<core::PackageParagraphStyle> styles;
    if (project != nullptr && project->type.has_value()) {
        styles = book.bookTypes().paragraphStyles(project->type->id);
    }
    // A type the program does not have gives none: the styles every book has
    return styles.isEmpty() ? baseStyles() : styles;
}

editor::ParagraphStyleSet bookParagraphStyles(const core::ProjectManager& book) {
    const core::BookProject* project = book.project();
    return bookParagraphStyles(book, project != nullptr ? project->paragraphStyles
                                                        : QList<core::BookParagraphStyle>());
}

editor::ParagraphStyleSet bookParagraphStyles(const core::ProjectManager& book,
                                              const QList<core::BookParagraphStyle>& entries) {
    return editor::ParagraphStyleSet::forBook(bookTypeStyles(book), entries, programLanguage());
}

}  // namespace kalahari::gui
