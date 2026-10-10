/// @file test_language_names.cpp
/// @brief Names of languages in the language of the user interface

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/utils/language_names.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

#include <utility>

using kalahari::gui::utils::languageName;

namespace {

/// The Polish names of two languages, as the Polish translation of the program has them
class PolishNames : public QTranslator {
public:
    bool isEmpty() const override { return false; }

    QString translate(const char* context, const char* sourceText, const char* /*disambiguation*/,
                      int /*n*/) const override {
        if (qstrcmp(context, "LanguageNames") != 0) {
            return QString();
        }
        if (qstrcmp(sourceText, "French") == 0) {
            return QStringLiteral("francuski");
        }
        if (qstrcmp(sourceText, "Polish") == 0) {
            return QStringLiteral("polski");
        }
        return QString();
    }
};

}  // namespace

TEST_CASE("Language names: the language of a code, with its country or without it",
          "[gui][spelling]") {
    CHECK(languageName(QStringLiteral("fr")) == QStringLiteral("French"));
    CHECK(languageName(QStringLiteral("fr_FR")) == QStringLiteral("French"));
    CHECK(languageName(QStringLiteral("en-GB")) == QStringLiteral("English"));
    CHECK(languageName(QStringLiteral(" PT_br ")) == QStringLiteral("Portuguese"));
    CHECK(languageName(QStringLiteral("ca-valencia")) == QStringLiteral("Catalan"));
    CHECK(languageName(QStringLiteral("kmr_Latn")) == QStringLiteral("Kurdish (Kurmanji)"));
    CHECK(languageName(QStringLiteral("nb_NO")) == QStringLiteral("Norwegian"));
    CHECK(languageName(QStringLiteral("nn_NO")) == QStringLiteral("Norwegian Nynorsk"));

    // Every language a book can be in (its Properties) has the name the Properties give it
    const std::pair<const char*, const char*> bookLanguages[] = {
        {"en", "English"}, {"pl", "Polish"},  {"de", "German"},     {"fr", "French"},
        {"es", "Spanish"}, {"it", "Italian"}, {"pt", "Portuguese"}, {"ru", "Russian"},
        {"zh", "Chinese"}, {"ja", "Japanese"}};
    for (const auto& [code, name] : bookLanguages) {
        INFO(code);
        CHECK(languageName(QString::fromLatin1(code)) == QString::fromLatin1(name));
    }
}

TEST_CASE("Language names: a language without a name in the program", "[gui][spelling]") {
    // Its name in that language, as Qt knows it
    const QLocale yoruba(QStringLiteral("yo"));
    REQUIRE(yoruba.language() == QLocale::Yoruba);
    CHECK(languageName(QStringLiteral("yo_NG")) == yoruba.nativeLanguageName());

    // Else the code as given
    CHECK(languageName(QStringLiteral("xx_YY")) == QStringLiteral("xx_YY"));
    CHECK(languageName(QStringLiteral(" xx ")) == QStringLiteral("xx"));
    CHECK(languageName(QString()).isEmpty());
}

TEST_CASE("Language names: in the language of the program", "[gui][spelling]") {
    PolishNames polish;
    REQUIRE(QCoreApplication::installTranslator(&polish));
    CHECK(languageName(QStringLiteral("fr_FR")) == QStringLiteral("francuski"));
    CHECK(languageName(QStringLiteral("pl")) == QStringLiteral("polski"));

    // A name the translation does not have stays in English
    CHECK(languageName(QStringLiteral("de")) == QStringLiteral("German"));

    QCoreApplication::removeTranslator(&polish);
    CHECK(languageName(QStringLiteral("fr")) == QStringLiteral("French"));
}
