/// @file language_names.cpp
/// @brief The names of languages in the language of the user interface

#include "kalahari/gui/utils/language_names.h"

#include <QCoreApplication>
#include <QLatin1String>
#include <QLocale>

#include <algorithm>
#include <iterator>

namespace kalahari::gui::utils {

namespace {

/// @brief The translation context of the names
constexpr const char* CONTEXT = "LanguageNames";

/// @brief A language the program has a name for
struct Language {
    const char* code;  ///< Its ISO 639 code, in lower case
    const char* name;  ///< Its name in English, translated in the context of the names
};

/// @brief The languages of the books (the Properties panel) and of the spelling
///        dictionaries of LibreOffice and of the systems, by their codes
constexpr Language LANGUAGES[] = {
    {"af", QT_TRANSLATE_NOOP("LanguageNames", "Afrikaans")},
    {"an", QT_TRANSLATE_NOOP("LanguageNames", "Aragonese")},
    {"ar", QT_TRANSLATE_NOOP("LanguageNames", "Arabic")},
    {"be", QT_TRANSLATE_NOOP("LanguageNames", "Belarusian")},
    {"bg", QT_TRANSLATE_NOOP("LanguageNames", "Bulgarian")},
    {"bn", QT_TRANSLATE_NOOP("LanguageNames", "Bengali")},
    {"bo", QT_TRANSLATE_NOOP("LanguageNames", "Tibetan")},
    {"br", QT_TRANSLATE_NOOP("LanguageNames", "Breton")},
    {"bs", QT_TRANSLATE_NOOP("LanguageNames", "Bosnian")},
    {"ca", QT_TRANSLATE_NOOP("LanguageNames", "Catalan")},
    {"ckb", QT_TRANSLATE_NOOP("LanguageNames", "Kurdish (Sorani)")},
    {"cs", QT_TRANSLATE_NOOP("LanguageNames", "Czech")},
    {"cy", QT_TRANSLATE_NOOP("LanguageNames", "Welsh")},
    {"da", QT_TRANSLATE_NOOP("LanguageNames", "Danish")},
    {"de", QT_TRANSLATE_NOOP("LanguageNames", "German")},
    {"el", QT_TRANSLATE_NOOP("LanguageNames", "Greek")},
    {"en", QT_TRANSLATE_NOOP("LanguageNames", "English")},
    {"eo", QT_TRANSLATE_NOOP("LanguageNames", "Esperanto")},
    {"es", QT_TRANSLATE_NOOP("LanguageNames", "Spanish")},
    {"et", QT_TRANSLATE_NOOP("LanguageNames", "Estonian")},
    {"eu", QT_TRANSLATE_NOOP("LanguageNames", "Basque")},
    {"fa", QT_TRANSLATE_NOOP("LanguageNames", "Persian")},
    {"fi", QT_TRANSLATE_NOOP("LanguageNames", "Finnish")},
    {"fo", QT_TRANSLATE_NOOP("LanguageNames", "Faroese")},
    {"fr", QT_TRANSLATE_NOOP("LanguageNames", "French")},
    {"fy", QT_TRANSLATE_NOOP("LanguageNames", "Western Frisian")},
    {"ga", QT_TRANSLATE_NOOP("LanguageNames", "Irish")},
    {"gd", QT_TRANSLATE_NOOP("LanguageNames", "Scottish Gaelic")},
    {"gl", QT_TRANSLATE_NOOP("LanguageNames", "Galician")},
    {"gu", QT_TRANSLATE_NOOP("LanguageNames", "Gujarati")},
    {"gug", QT_TRANSLATE_NOOP("LanguageNames", "Guarani")},
    {"he", QT_TRANSLATE_NOOP("LanguageNames", "Hebrew")},
    {"hi", QT_TRANSLATE_NOOP("LanguageNames", "Hindi")},
    {"hr", QT_TRANSLATE_NOOP("LanguageNames", "Croatian")},
    {"hu", QT_TRANSLATE_NOOP("LanguageNames", "Hungarian")},
    {"hy", QT_TRANSLATE_NOOP("LanguageNames", "Armenian")},
    {"id", QT_TRANSLATE_NOOP("LanguageNames", "Indonesian")},
    {"is", QT_TRANSLATE_NOOP("LanguageNames", "Icelandic")},
    {"it", QT_TRANSLATE_NOOP("LanguageNames", "Italian")},
    {"ja", QT_TRANSLATE_NOOP("LanguageNames", "Japanese")},
    {"ka", QT_TRANSLATE_NOOP("LanguageNames", "Georgian")},
    {"kk", QT_TRANSLATE_NOOP("LanguageNames", "Kazakh")},
    {"km", QT_TRANSLATE_NOOP("LanguageNames", "Khmer")},
    {"kmr", QT_TRANSLATE_NOOP("LanguageNames", "Kurdish (Kurmanji)")},
    {"ko", QT_TRANSLATE_NOOP("LanguageNames", "Korean")},
    {"ku", QT_TRANSLATE_NOOP("LanguageNames", "Kurdish")},
    {"la", QT_TRANSLATE_NOOP("LanguageNames", "Latin")},
    {"lb", QT_TRANSLATE_NOOP("LanguageNames", "Luxembourgish")},
    {"lo", QT_TRANSLATE_NOOP("LanguageNames", "Lao")},
    {"lt", QT_TRANSLATE_NOOP("LanguageNames", "Lithuanian")},
    {"lv", QT_TRANSLATE_NOOP("LanguageNames", "Latvian")},
    {"mk", QT_TRANSLATE_NOOP("LanguageNames", "Macedonian")},
    {"mn", QT_TRANSLATE_NOOP("LanguageNames", "Mongolian")},
    {"ms", QT_TRANSLATE_NOOP("LanguageNames", "Malay")},
    {"mt", QT_TRANSLATE_NOOP("LanguageNames", "Maltese")},
    {"nb", QT_TRANSLATE_NOOP("LanguageNames", "Norwegian")},
    {"ne", QT_TRANSLATE_NOOP("LanguageNames", "Nepali")},
    {"nl", QT_TRANSLATE_NOOP("LanguageNames", "Dutch")},
    {"nn", QT_TRANSLATE_NOOP("LanguageNames", "Norwegian Nynorsk")},
    {"no", QT_TRANSLATE_NOOP("LanguageNames", "Norwegian")},
    {"oc", QT_TRANSLATE_NOOP("LanguageNames", "Occitan")},
    {"pl", QT_TRANSLATE_NOOP("LanguageNames", "Polish")},
    {"pt", QT_TRANSLATE_NOOP("LanguageNames", "Portuguese")},
    {"ro", QT_TRANSLATE_NOOP("LanguageNames", "Romanian")},
    {"ru", QT_TRANSLATE_NOOP("LanguageNames", "Russian")},
    {"si", QT_TRANSLATE_NOOP("LanguageNames", "Sinhala")},
    {"sk", QT_TRANSLATE_NOOP("LanguageNames", "Slovak")},
    {"sl", QT_TRANSLATE_NOOP("LanguageNames", "Slovenian")},
    {"sq", QT_TRANSLATE_NOOP("LanguageNames", "Albanian")},
    {"sr", QT_TRANSLATE_NOOP("LanguageNames", "Serbian")},
    {"sv", QT_TRANSLATE_NOOP("LanguageNames", "Swedish")},
    {"sw", QT_TRANSLATE_NOOP("LanguageNames", "Swahili")},
    {"ta", QT_TRANSLATE_NOOP("LanguageNames", "Tamil")},
    {"te", QT_TRANSLATE_NOOP("LanguageNames", "Telugu")},
    {"th", QT_TRANSLATE_NOOP("LanguageNames", "Thai")},
    {"tr", QT_TRANSLATE_NOOP("LanguageNames", "Turkish")},
    {"uk", QT_TRANSLATE_NOOP("LanguageNames", "Ukrainian")},
    {"ur", QT_TRANSLATE_NOOP("LanguageNames", "Urdu")},
    {"uz", QT_TRANSLATE_NOOP("LanguageNames", "Uzbek")},
    {"vi", QT_TRANSLATE_NOOP("LanguageNames", "Vietnamese")},
    {"zh", QT_TRANSLATE_NOOP("LanguageNames", "Chinese")},
};

}  // namespace

QString languageName(const QString& language) {
    const QString given = language.trimmed();
    if (given.isEmpty()) {
        return QString();
    }

    // The language is the first part of the code: fr of fr_FR, en of en-GB
    qsizetype end = 0;
    while (end < given.size() && given.at(end) != QLatin1Char('_') &&
           given.at(end) != QLatin1Char('-')) {
        ++end;
    }
    const QString code = given.left(end).toLower();
    const auto* known =
        std::find_if(std::begin(LANGUAGES), std::end(LANGUAGES),
                     [&code](const Language& entry) { return code == QLatin1String(entry.code); });
    if (known != std::end(LANGUAGES)) {
        return QCoreApplication::translate(CONTEXT, known->name);
    }

    // Another language: its name in that language
    const QLocale locale(given);
    const QString name = locale.language() == QLocale::C ? QString() : locale.nativeLanguageName();
    return name.isEmpty() ? given : name;
}

}  // namespace kalahari::gui::utils
