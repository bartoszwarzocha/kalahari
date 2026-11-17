// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_styles.h
/// @brief The paragraph styles of the open book: those of its type and its own

#ifndef KALAHARI_GUI_BOOK_STYLES_H
#define KALAHARI_GUI_BOOK_STYLES_H

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/editor/paragraph_styles.h"

#include <QList>
#include <QString>

namespace kalahari::core {
class ProjectManager;
} // namespace kalahari::core

namespace kalahari::gui {

/// @brief The paragraph styles of the type of the book open in @p book (with the packages it
///        uses); the styles of the package Base, which every book has, without a book or for a
///        type the program does not have
QList<core::PackageParagraphStyle> bookTypeStyles(const core::ProjectManager& book);

/// @brief The styles of the package Base: those of a text outside the book
QList<core::PackageParagraphStyle> baseStyles();

/// @brief The styles a new book of type @p typeId starts with, the names in the program's
///        language; those of the package Base for a type the program does not have
editor::ParagraphStyleSet newBookParagraphStyles(const QString& typeId);

/// @brief The styles of the book open in @p book: its type's with the book's entries over them
///        (editor::ParagraphStyleSet::forBook()), the names in the program's language
editor::ParagraphStyleSet bookParagraphStyles(const core::ProjectManager& book);

/// @brief The styles the book open in @p book would have with @p entries in place of its own
editor::ParagraphStyleSet bookParagraphStyles(const core::ProjectManager& book,
                                              const QList<core::BookParagraphStyle>& entries);

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_BOOK_STYLES_H
