/// @file kind_words.h
/// @brief How the program's sentences name kinds of elements: the forms of their names
///
/// A sentence of the program that names a kind has markers, which the forms of the name fill
/// in the language of the program. "Add {Kind}" is "Add Story" in English, and its Polish
/// translation, "Dodaj {kind:accusative}", is "Dodaj opowiadanie". Each language takes the
/// forms its grammar needs:
/// - {kind}: the name as in a sentence, in the singular: "story", "opowiadanie";
/// - {kind:genitive}: another form of the name, one of KindWords::formNames();
/// - {Kind}, {Kind:plural}: the same with a capital letter, at the start of a sentence or in a
///   title: "Story", "Opowiadania";
/// - {kind:m=dodany|f=dodana|n=dodane|p=dodane}: the text for the gender of the name, for the
///   words that agree with it; "p" is for a name that is plural in form ("Podziękowania");
/// - {kind:s=is|p=are}: the text for a name singular or plural in form, in a language whose
///   words agree only in number.
///
/// The word before the colon names a noun of the sentence: the code gives the words of each
/// noun it speaks of ("kind", "group", "main"), and a translation uses only the nouns of its
/// source.

#pragma once

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>

namespace kalahari::core {

/// @brief Grammatical gender of a name
enum class Gender {
    Masculine,  ///< "rozdział", "dział"
    Feminine,   ///< "część", "dedykacja"
    Neuter,     ///< "opowiadanie", "motto"
    Plural      ///< A name plural in form, whose words agree as with a plural: "Podziękowania"
};

/// @brief The name of a kind in the program's sentences, in one language
///
/// The forms are as in the middle of a sentence ("rozdział", "chapter"); a marker with a
/// capital letter makes the first letter capital.
struct KindWords {
    QMap<QString, QString> forms;      ///< Form ("singular", "genitive"...) -> the name in it
    Gender gender = Gender::Masculine;

    /// @brief The name in form @p name: the form given, or the one it comes from when it is
    /// not given (a case of the singular: the singular; a case of the plural: the plural; with
    /// an article: the singular); empty for a form that is not one of formNames()
    QString form(const QString& name) const;

    /// @brief Forms a package can give: "singular", "plural", the cases of each ("genitive",
    /// "accusative", "locative"..., "genitivePlural"...) and "indefinite" (with an indefinite
    /// article: "a story", "an act")
    static const QStringList& formNames();

    /// @brief Gender named @p name in packages ("masculine", "feminine", "neuter" or "plural");
    /// false when it is not one
    static bool genderFromName(const QString& name, Gender& gender);
};

/// @brief @p text with the markers of @p nouns filled with their words
///
/// A marker of a noun that @p nouns does not have, or with a form that is not one, stays as
/// it is.
QString fillWords(const QString& text, const QHash<QString, KindWords>& nouns);

/// @brief Problems of the markers of @p text: a form that is not one, a choice without the
/// text of each gender (m, f, n, p) or of each number (s, p); empty when they have none
/// @param nouns Gets the nouns of the markers, each once, in the order of the text
QStringList wordMarkerProblems(const QString& text, QStringList* nouns = nullptr);

}  // namespace kalahari::core
