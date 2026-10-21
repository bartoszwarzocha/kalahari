/// @file test_special_characters.cpp
/// @brief The characters of Insert > Special Character...: their groups, names and finding them

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/special_characters.h"

#include <QChar>
#include <QCoreApplication>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTranslator>

using namespace kalahari::gui;

namespace {

/// @brief The ids of the groups, in their order
QStringList idsOf(const SpecialCharacters& characters) {
    QStringList ids;
    for (const SpecialCharacterGroup& group : characters.groups()) {
        ids.append(group.id);
    }
    return ids;
}

/// @brief The first character a text finds; 0 for none
char32_t firstFound(const SpecialCharacters& characters, const QString& text) {
    const QList<char32_t> found = characters.find(text);
    return found.isEmpty() ? 0 : found.first();
}

/// @brief The program in Polish while it lives, with the translation it ships
class PolishTranslation {
public:
    PolishTranslation() {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
    }
    ~PolishTranslation() { QCoreApplication::removeTranslator(&m_translator); }
    PolishTranslation(const PolishTranslation&) = delete;
    PolishTranslation& operator=(const PolishTranslation&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    bool m_loaded = false;
};

} // namespace

TEST_CASE("SpecialCharacters: the groups and their characters", "[gui][special-characters]") {
    const SpecialCharacters characters;

    REQUIRE(idsOf(characters) == QStringList{"punctuation", "spaces", "accents", "greek",
                                             "arrows", "math", "currencies", "fractions",
                                             "ornaments"});

    QSet<char32_t> all;
    for (const SpecialCharacterGroup& group : characters.groups()) {
        INFO(group.id.toStdString());
        CHECK_FALSE(group.name.isEmpty());
        CHECK_FALSE(group.characters.isEmpty());
        for (const char32_t code : group.characters) {
            INFO(SpecialCharacters::codeText(code).toStdString());
            // Each in one group, with its names, and one Unicode assigns
            CHECK_FALSE(all.contains(code));
            all.insert(code);
            CHECK(characters.contains(code));
            const SpecialCharacter character = characters.character(code);
            CHECK(character.code == code);
            CHECK(character.group == group.id);
            CHECK_FALSE(character.name.isEmpty());
            CHECK_FALSE(character.englishName.isEmpty());
            CHECK(QChar::category(code) != QChar::Other_NotAssigned);
            // A character that cannot be seen is shown by its mark
            if (const QChar::Category category = QChar::category(code);
                category == QChar::Separator_Space || category == QChar::Other_Format) {
                CHECK_FALSE(character.mark.isEmpty());
            }
        }
    }

    // The characters of the plan of the editor
    for (const char32_t code : {U'—', U'–', U'…', U'„', U'”', U'«', U'»', U'\u00A0', U'\u202F',
                                U'\u2011', U'\u00AD', U'§', U'©', U'→', U'±', U'€', U'½', U'²'}) {
        INFO(SpecialCharacters::codeText(code).toStdString());
        CHECK(characters.contains(code));
    }
    CHECK(characters.character(U'—').group == QStringLiteral("punctuation"));
    CHECK(characters.character(U'\u00A0').group == QStringLiteral("spaces"));
}

TEST_CASE("SpecialCharacters: the names", "[gui][special-characters]") {
    const SpecialCharacters characters;

    SECTION("A character with a name of its own") {
        const SpecialCharacter dash = characters.character(U'—');
        CHECK(dash.name == QStringLiteral("Em dash"));
        CHECK(dash.englishName == QStringLiteral("Em dash"));
        CHECK(dash.text() == QStringLiteral("—"));
        CHECK(dash.mark.isEmpty());

        CHECK(characters.character(U'\u00A0').name == QStringLiteral("No-break space"));
        CHECK(characters.character(U'\u00A0').mark == QStringLiteral("°"));
        CHECK(characters.character(U'ł').name == QStringLiteral("l with stroke"));
        CHECK(characters.character(U'Ł').name == QStringLiteral("L with stroke"));
    }

    SECTION("A letter with an accent: its letter and its mark, small and capital") {
        CHECK(characters.character(U'ą').name == QStringLiteral("a with ogonek"));
        CHECK(characters.character(U'Ą').name == QStringLiteral("A with ogonek"));
        CHECK(characters.character(U'ż').name == QStringLiteral("z with dot above"));
        CHECK(characters.character(U'ș').name == QStringLiteral("s with comma below"));
        CHECK(characters.character(U'ő').englishName == QStringLiteral("o with double acute"));
        CHECK(characters.character(U'ą').group == QStringLiteral("accents"));
    }

    SECTION("Greek letters, superscripts and subscripts") {
        CHECK(characters.character(U'α').name == QStringLiteral("alpha (small letter)"));
        CHECK(characters.character(U'Ω').name == QStringLiteral("Omega (capital letter)"));
        CHECK(characters.character(U'²').name == QStringLiteral("Superscript 2"));
        CHECK(characters.character(U'₂').name == QStringLiteral("Subscript 2"));
    }

    SECTION("A character of no group: named by its code") {
        const SpecialCharacter snowman = characters.character(U'☃');
        CHECK_FALSE(characters.contains(U'☃'));
        CHECK(snowman.code == U'☃');
        CHECK(snowman.name == QStringLiteral("Character U+2603"));
        CHECK(snowman.englishName == QStringLiteral("Character U+2603"));
        CHECK(snowman.group.isEmpty());
    }
}

TEST_CASE("SpecialCharacters: finding by a name, a code or the character",
          "[gui][special-characters]") {
    const SpecialCharacters characters;

    SECTION("Every word of the text, in any case and without accents") {
        CHECK(firstFound(characters, QStringLiteral("em dash")) == U'—');
        CHECK(firstFound(characters, QStringLiteral("DASH EM")) == U'—');
        CHECK(firstFound(characters, QStringLiteral("  Ém   dásh ")) == U'—');

        const QList<char32_t> dashes = characters.find(QStringLiteral("dash"));
        CHECK(dashes.contains(U'—'));
        CHECK(dashes.contains(U'–'));
        CHECK(dashes.indexOf(U'—') < dashes.indexOf(U'–'));  // in the order of the groups

        const QList<char32_t> ogonek = characters.find(QStringLiteral("ogonek"));
        CHECK(ogonek.contains(U'ą'));
        CHECK(ogonek.contains(U'Ę'));
        CHECK_FALSE(ogonek.contains(U'ł'));

        CHECK(characters.find(QStringLiteral("dash nothing")).isEmpty());
        CHECK(characters.find(QStringLiteral("   ")).isEmpty());
    }

    SECTION("A code finds its character first, also one of no group") {
        CHECK(firstFound(characters, QStringLiteral("2014")) == U'—');
        CHECK(firstFound(characters, QStringLiteral("U+2014")) == U'—');
        CHECK(firstFound(characters, QStringLiteral("u+1f600")) == U'😀');
        CHECK(firstFound(characters, QStringLiteral("0x2603")) == U'☃');
        CHECK(characters.find(QStringLiteral("U+2014")).count(U'—') == 1);
    }

    SECTION("The character typed finds itself") {
        CHECK(firstFound(characters, QStringLiteral("—")) == U'—');
        CHECK(firstFound(characters, QStringLiteral("ą")) == U'ą');
    }
}

TEST_CASE("SpecialCharacters: in Polish, found by the Polish and the English names",
          "[gui][special-characters]") {
    const PolishTranslation polish;
    REQUIRE(polish.loaded());
    const SpecialCharacters characters;

    CHECK(characters.groups().first().name == QStringLiteral("Interpunkcja i typografia"));
    CHECK(characters.character(U'–').name == QStringLiteral("Półpauza"));
    CHECK(characters.character(U'–').englishName == QStringLiteral("En dash"));
    CHECK(characters.character(U'ą').name == QStringLiteral("a z ogonkiem"));
    CHECK(characters.character(U'Ω').name == QStringLiteral("Omega (wielka litera)"));
    CHECK(characters.character(U'☃').name == QStringLiteral("Znak U+2603"));

    // Without the Polish letters, and in English
    CHECK(firstFound(characters, QStringLiteral("polpauza")) == U'–');
    CHECK(firstFound(characters, QStringLiteral("en dash")) == U'–');
    CHECK(characters.find(QStringLiteral("lacznik")).contains(U'\u2011'));
    CHECK(firstFound(characters, QStringLiteral("spacja nierozdzielajaca")) == U'\u00A0');
    CHECK(characters.find(QStringLiteral("ogonkiem")).contains(U'ę'));
    CHECK(characters.find(QStringLiteral("ogonek")).contains(U'ę'));
}

TEST_CASE("SpecialCharacters: codes", "[gui][special-characters]") {
    CHECK(SpecialCharacters::codeText(U'—') == QStringLiteral("U+2014"));
    CHECK(SpecialCharacters::codeText(U'é') == QStringLiteral("U+00E9"));
    CHECK(SpecialCharacters::codeText(U'😀') == QStringLiteral("U+1F600"));

    CHECK(SpecialCharacters::codeOf(QStringLiteral("U+2014")) == U'—');
    CHECK(SpecialCharacters::codeOf(QStringLiteral(" u+00e9 ")) == U'é');
    CHECK(SpecialCharacters::codeOf(QStringLiteral("0x41")) == U'A');
    CHECK(SpecialCharacters::codeOf(QStringLiteral("00E9")) == U'é');
    CHECK(SpecialCharacters::codeOf(QStringLiteral("1F600")) == U'😀');

    // Without "U+": four digits or more, one of them decimal
    CHECK_FALSE(SpecialCharacters::codeOf(QStringLiteral("41")));
    CHECK_FALSE(SpecialCharacters::codeOf(QStringLiteral("face")));
    CHECK(SpecialCharacters::codeOf(QStringLiteral("U+face")) == U'龜');

    // Not a code, or one no character can be inserted for
    CHECK_FALSE(SpecialCharacters::codeOf(QString()));
    CHECK_FALSE(SpecialCharacters::codeOf(QStringLiteral("U+")));
    CHECK_FALSE(SpecialCharacters::codeOf(QStringLiteral("U+12G4")));
    CHECK_FALSE(SpecialCharacters::codeOf(QStringLiteral("U+1234567")));
    CHECK_FALSE(SpecialCharacters::codeOf(QStringLiteral("U+110000")));
    CHECK_FALSE(SpecialCharacters::codeOf(QStringLiteral("U+D800")));  // a surrogate
    CHECK_FALSE(SpecialCharacters::codeOf(QStringLiteral("U+0007")));  // a control character
    CHECK_FALSE(SpecialCharacters::codeOf(QStringLiteral("U+0378")));  // not assigned
    CHECK_FALSE(SpecialCharacters::codeOf(QStringLiteral("٢٠١٤")));    // Arabic digits
}
