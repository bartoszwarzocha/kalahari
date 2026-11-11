/// @file book_styles.h
/// @brief The paragraph styles of the open book: those of its type and its own

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/editor/paragraph_styles.h"

#include <QList>
#include <QString>

namespace kalahari::gui {

/// @brief The paragraph styles of the type of the open book (with the packages it uses); the
///        styles of the package Base, which every book has, without a book or for a type the
///        program does not have
QList<core::PackageParagraphStyle> bookTypeStyles();

/// @brief The styles of the package Base: those of a text outside the book
QList<core::PackageParagraphStyle> baseStyles();

/// @brief The styles of the open book: its type's with the book's entries over them
///        (editor::ParagraphStyleSet::forBook()), the names in the program's language
editor::ParagraphStyleSet bookParagraphStyles();

/// @brief The styles the open book would have with @p entries in place of its own
editor::ParagraphStyleSet bookParagraphStyles(const QList<core::BookParagraphStyle>& entries);

}  // namespace kalahari::gui
