/// @file kind_names.h
/// @brief The names of kinds of elements in the program's sentences, in the program's language
///
/// The sentences have markers that the forms of the names fill (core/kind_words.h):
/// @code
/// const QString heading = core::fillWords(tr("Add {Kind}"), {{"kind", wordsOf(kind)}});
/// // "Add Story"; in Polish, "Dodaj {kind:accusative}": "Dodaj opowiadanie"
/// @endcode

#pragma once

#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/kind_words.h"

#include <QString>

namespace kalahari {
namespace core {
struct ProjectElement;
} // namespace core

namespace gui {

/// @brief Language of the program's texts: the language of the translation loaded at start
/// ("pl"), or English ("en")
///
/// A change of the language in the settings takes effect after a restart, so the names follow
/// the translation, not the setting.
QString programLanguage();

/// @brief Set the language of the program's texts; main() sets the language of the translation
/// it loads
void setProgramLanguage(const QString& language);

/// @brief Forms of the name of @p kind in the program's language; none without a kind
core::KindWords wordsOf(const core::KindRef& kind);

/// @brief Forms of the name of the kind of @p element, a kind of @p registry; when the registry
/// does not have the kind (its package is not installed), the id of the kind stands for its name
core::KindWords wordsOf(const core::BookTypeRegistry& registry,
                        const core::ProjectElement& element);

/// @brief Forms of the name of the kind of @p element of the open book (wordsOf() with the
/// book types of core::ProjectManager)
core::KindWords wordsOf(const core::ProjectElement& element);

/// @brief Forms of the name of the main text kind of the open book: chapter, story, poem...
/// (core::ProjectManager::mainTextKind()); none without a book
core::KindWords mainWords();

} // namespace gui
} // namespace kalahari
