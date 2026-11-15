// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_affix_expander.cpp
/// @brief The forms a Hunspell dictionary gives its words: suffixes, their conditions and
/// continuations, the kinds of flags, the encodings, and the Polish dictionary of the program

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/affix_expander.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <initializer_list>

using namespace kalahari::core;

namespace {

/// A dictionary written in a temporary folder
class Dictionary {
public:
    Dictionary(const QByteArray& affix, const QByteArray& words) {
        write(QStringLiteral("test.aff"), affix);
        write(QStringLiteral("test.dic"), words);
    }

    [[nodiscard]] QString affixFile() const {
        return m_folder.filePath(QStringLiteral("test.aff"));
    }
    [[nodiscard]] QString wordsFile() const {
        return m_folder.filePath(QStringLiteral("test.dic"));
    }

    /// An expander with the dictionary read
    [[nodiscard]] AffixExpander expander() const {
        AffixExpander expander;
        REQUIRE(expander.load(affixFile(), wordsFile()));
        return expander;
    }

private:
    void write(const QString& name, const QByteArray& content) const {
        QFile file(m_folder.filePath(name));
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write(content);
    }

    QTemporaryDir m_folder;
};

QStringList list(std::initializer_list<const char*> texts) {
    QStringList result;
    for (const char* text : texts) {
        result.append(QString::fromUtf8(text));
    }
    return result;
}

QString text(const char* utf8) {
    return QString::fromUtf8(utf8);
}

/// The .aff file of a dictionary of the tests
QByteArray basicAffix() {
    return R"(# A dictionary for the tests
SET UTF-8
TRY abc

PFX n Y 1
PFX n 0 nie .

SFX A Y 4
SFX A 0 a [^aeo]
SFX A 0 owi [^aeo]
SFX A a y a
SFX A a ę a

SFX B N 1
SFX B 0 em k

SFX C Y 2
SFX C 0 ek/D .
SFX C 0 ik/E .

SFX D Y 1
SFX D k ka k

SFX E Y 1
SFX E k ki k

NEEDAFFIX E
FORBIDDENWORD X
ONLYINCOMPOUND O

SFX F Y 1
SFX F 0 ny/O .
)";
}

/// The .dic file of a dictionary of the tests
QByteArray basicWords() {
    return "9\r\n"
           "smok/AB\r\n"
           "Anna/A\r\n"
           "dom/C\n"
           "kot\tpo:noun\n"
           "zły/X\n"
           "kotka/A\n"
           "kotka/n\n"
           "pies/F\n"
           "stół/E\n";
}

}  // namespace

TEST_CASE("AffixExpander: a word with its suffixes", "[core][dictionary][affixes]") {
    const Dictionary dictionary(basicAffix(), basicWords());
    const AffixExpander expander = dictionary.expander();
    REQUIRE(expander.isLoaded());

    SECTION("the word first, then each suffix whose condition its end matches") {
        CHECK(expander.formsOf(QStringLiteral("smok")) ==
              list({"smok", "smoka", "smokowi", "smokem"}));
        CHECK(expander.formsOf(QStringLiteral("Anna")) == list({"Anna", "Anny", "Annę"}));
    }

    SECTION("an entry without flags, with other fields after it") {
        CHECK(expander.formsOf(QStringLiteral("kot")) == list({"kot"}));
        CHECK(expander.hasEntry(QStringLiteral("kot")));
    }

    SECTION("a word written otherwise, or a part of a word, has no entry") {
        CHECK(expander.formsOf(QStringLiteral("Smok")).isEmpty());
        CHECK_FALSE(expander.hasEntry(QStringLiteral("smo")));
        CHECK_FALSE(expander.hasEntry(QStringLiteral("ko")));
        CHECK(expander.formsOf(QString()).isEmpty());
    }

    SECTION("two entries of one word: the forms of both, each once; prefixes are left out") {
        CHECK(expander.formsOf(QStringLiteral("kotka")) == list({"kotka", "kotky", "kotkę"}));
    }

    SECTION(
        "the suffixes a suffix allows (one level); a form that is a word only with a "
        "further suffix is left out (NEEDAFFIX)") {
        CHECK(expander.formsOf(QStringLiteral("dom")) ==
              list({"dom", "domek", "domeka", "domiki"}));
        CHECK(expander.formsOf(text("stół")).isEmpty());
        CHECK(expander.hasEntry(text("stół")));
    }

    SECTION("forms only inside compounds and forbidden words are left out") {
        CHECK(expander.formsOf(QStringLiteral("pies")) == list({"pies"}));
        CHECK_FALSE(expander.hasEntry(text("zły")));
        CHECK(expander.formsOf(text("zły")).isEmpty());
    }
}

TEST_CASE("AffixExpander: the entries a form is a form of", "[core][dictionary][affixes]") {
    const Dictionary dictionary(basicAffix(), basicWords());
    const AffixExpander expander = dictionary.expander();

    CHECK(expander.entriesOf(QStringLiteral("smokowi")) == list({"smok"}));
    CHECK(expander.entriesOf(QStringLiteral("smok")) == list({"smok"}));
    CHECK(expander.entriesOf(text("Annę")) == list({"Anna"}));
    CHECK(expander.entriesOf(QStringLiteral("kota")).isEmpty());  // kot has no flag A
    CHECK(expander.entriesOf(QStringLiteral("nic")).isEmpty());
    CHECK(expander.entriesOf(QString()).isEmpty());

    CHECK(expander.relatedForms(QStringLiteral("smokem")) ==
          list({"smok", "smoka", "smokowi", "smokem"}));
    CHECK(expander.relatedForms(QStringLiteral("nic")).isEmpty());
}

TEST_CASE("AffixExpander: flags of two characters, numbers and aliases",
          "[core][dictionary][affixes]") {
    SECTION("FLAG long") {
        const Dictionary dictionary(
            "SET UTF-8\nFLAG long\nSFX Aa Y 1\nSFX Aa 0 s .\n"
            "SFX Bb Y 1\nSFX Bb 0 ed .\n",
            "1\nwalk/AaBb\n");
        CHECK(dictionary.expander().formsOf(QStringLiteral("walk")) ==
              list({"walk", "walks", "walked"}));
    }

    SECTION("FLAG num") {
        const Dictionary dictionary(
            "SET UTF-8\nFLAG num\nSFX 12 Y 1\nSFX 12 0 s .\n"
            "SFX 7 Y 1\nSFX 7 0 ing .\n",
            "1\nwalk/12,7\n");
        CHECK(dictionary.expander().formsOf(QStringLiteral("walk")) ==
              list({"walk", "walks", "walking"}));
    }

    SECTION("aliases of flags (AF)") {
        const Dictionary dictionary(
            "SET UTF-8\nAF 2\nAF A\nAF AB\nSFX A Y 1\nSFX A 0 s .\n"
            "SFX B Y 1\nSFX B 0 ed/1 .\n",
            "2\nwalk/2\njump/1\n");
        const AffixExpander expander = dictionary.expander();
        CHECK(expander.formsOf(QStringLiteral("walk")) ==
              list({"walk", "walks", "walked", "walkeds"}));
        CHECK(expander.formsOf(QStringLiteral("jump")) == list({"jump", "jumps"}));
    }

    SECTION("a suffix without a condition, and a suffix line that is not whole") {
        const Dictionary dictionary("SET UTF-8\nSFX A Y 2\nSFX A 0 s\nSFX A 0\n", "1\nwalk/A\n");
        CHECK(dictionary.expander().formsOf(QStringLiteral("walk")) == list({"walk", "walks"}));
    }

    SECTION("conditions: any character, a set, a set not closed") {
        const Dictionary dictionary(
            "SET UTF-8\nSFX A Y 3\nSFX A 0 x .k\nSFX A 0 y [ok]\n"
            "SFX A 0 z [ab\n",
            "2\nok/A\nk/A\n");
        const AffixExpander expander = dictionary.expander();
        CHECK(expander.formsOf(QStringLiteral("ok")) == list({"ok", "okx", "oky"}));
        CHECK(expander.formsOf(QStringLiteral("k")) == list({"k", "ky"}));
    }
}

TEST_CASE("AffixExpander: encodings and files", "[core][dictionary][affixes]") {
    SECTION("ISO 8859-1, and Hunspell's default without a SET line") {
        const QByteArray word = text("café").toLatin1();
        const Dictionary latin1("SET ISO8859-1\nSFX A Y 1\nSFX A 0 s .\n", "1\n" + word + "/A\n");
        CHECK(latin1.expander().formsOf(text("café")) == QStringList{text("café"), text("cafés")});

        const Dictionary noSet("SFX A Y 1\nSFX A 0 s .\n", "1\n" + word + "/A\n");
        CHECK(noSet.expander().hasEntry(text("café")));
    }

    SECTION("an encoding the expander does not read") {
        const Dictionary dictionary("SET ISO8859-2\n", "1\nkot\n");
        AffixExpander expander;
        CHECK_FALSE(expander.load(dictionary.affixFile(), dictionary.wordsFile()));
        CHECK_FALSE(expander.isLoaded());
        CHECK(expander.formsOf(QStringLiteral("kot")).isEmpty());
    }

    SECTION("files that are missing; a dictionary read before is dropped") {
        const Dictionary dictionary(basicAffix(), basicWords());
        AffixExpander expander = dictionary.expander();
        CHECK_FALSE(
            expander.load(dictionary.affixFile(), dictionary.wordsFile() + QStringLiteral("x")));
        CHECK_FALSE(expander.isLoaded());
        CHECK_FALSE(expander.hasEntry(QStringLiteral("smok")));
        CHECK_FALSE(
            expander.load(dictionary.affixFile() + QStringLiteral("x"), dictionary.wordsFile()));
    }
}

TEST_CASE("AffixExpander: the Polish dictionary of the program", "[core][dictionary][affixes]") {
    const QString folder = QStringLiteral(KALAHARI_SOURCE_DIR "/resources/dictionaries/");
    AffixExpander expander;
    REQUIRE(
        expander.load(folder + QStringLiteral("pl_PL.aff"), folder + QStringLiteral("pl_PL.dic")));

    const QStringList smok = expander.formsOf(QStringLiteral("smok"));
    for (const char* form :
         {"smoka", "smokowi", "smokiem", "smoku", "smoki", "smoków", "smokami"}) {
        CHECK(smok.contains(text(form)));
    }
    CHECK(expander.formsOf(text("księga")).contains(text("ksiąg")));
    CHECK(expander.entriesOf(QStringLiteral("Wolskiej")) == QStringList{QStringLiteral("Wolski")});
    CHECK(expander.relatedForms(QStringLiteral("Annie")).contains(QStringLiteral("Anny")));
    CHECK(expander.entriesOf(text("popiołów")).contains(text("popiół")));
}
