/// @file book_styles.cpp
/// @brief The paragraph styles of the open book

#include "kalahari/gui/book_styles.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/kind_names.h"

namespace kalahari::gui {

QList<core::PackageParagraphStyle> baseStyles() {
    return core::ProjectManager::getInstance().bookTypes().paragraphStyles(
        QString::fromLatin1(core::ProjectManager::BASE_PACKAGE));
}

QList<core::PackageParagraphStyle> bookTypeStyles() {
    const auto& projects = core::ProjectManager::getInstance();
    const core::BookProject* book = projects.project();
    QList<core::PackageParagraphStyle> styles;
    if (book != nullptr && book->type.has_value()) {
        styles = projects.bookTypes().paragraphStyles(book->type->id);
    }
    // A type the program does not have gives none: the styles every book has
    return styles.isEmpty() ? baseStyles() : styles;
}

editor::ParagraphStyleSet bookParagraphStyles() {
    const core::BookProject* book = core::ProjectManager::getInstance().project();
    return bookParagraphStyles(book != nullptr ? book->paragraphStyles
                                               : QList<core::BookParagraphStyle>());
}

editor::ParagraphStyleSet bookParagraphStyles(const QList<core::BookParagraphStyle>& entries) {
    return editor::ParagraphStyleSet::forBook(bookTypeStyles(), entries, programLanguage());
}

}  // namespace kalahari::gui
