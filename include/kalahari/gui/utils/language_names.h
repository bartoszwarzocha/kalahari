/// @file language_names.h
/// @brief The names of languages in the language of the user interface

#pragma once

#include <QString>

namespace kalahari::gui::utils {

/// @brief The name of a language in the language of the user interface
///
/// For the languages of the books and of the spelling dictionaries: the name as written in
/// a sentence of the program's language (French; in the Polish interface "francuski"). A
/// language the program has no name for gets its name in that language, as Qt knows it,
/// else its code as given.
/// @param language A language (fr) or a language with its country or variant (fr_FR, en-GB,
///        ca-valencia), in any case
QString languageName(const QString& language);

}  // namespace kalahari::gui::utils
