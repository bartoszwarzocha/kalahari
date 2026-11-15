// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_name_declension.cpp
/// @brief The forms of names in the grammatical cases: the Polish models, the model each word
/// of a name gets, the forms the spelling dictionary confirms, English and other languages

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/affix_expander.h>
#include <kalahari/core/name_declension.h>

#include <QHash>
#include <QString>
#include <QStringList>

#include <initializer_list>
#include <string>
#include <utility>

using kalahari::core::AffixExpander;
using kalahari::core::CASE_COUNT;
using kalahari::core::FormTable;
using kalahari::core::GrammaticalCase;
using kalahari::core::NameDeclension;
using kalahari::core::NameKind;

namespace {

QString text(const char* utf8) {
    return QString::fromUtf8(utf8);
}

/// The main form of each case, joined with ", "
std::string row(const FormTable& table, bool plural) {
    QStringList forms;
    for (const GrammaticalCase grammaticalCase : FormTable::allCases()) {
        forms.append(table.cell(grammaticalCase, plural).join(QLatin1Char('/')));
    }
    return forms.join(QStringLiteral(", ")).toStdString();
}

/// The forms of a name with the models the program picks
FormTable formsOf(const char* name, NameKind kind = NameKind::Character,
                  const NameDeclension::DictionaryForms& dictionary = {}) {
    const NameDeclension polish(QStringLiteral("pl"));
    const QString written = text(name);
    return polish.nameForms(written, polish.modelsFor(written, kind, dictionary), kind, dictionary);
}

std::string modelsOf(const char* name, NameKind kind = NameKind::Character,
                     const NameDeclension::DictionaryForms& dictionary = {}) {
    const NameDeclension polish(QStringLiteral("pl"));
    return polish.modelsFor(text(name), kind, dictionary).join(QLatin1Char(',')).toStdString();
}

/// A spelling dictionary of a few words: each word and its forms
NameDeclension::DictionaryForms fakeDictionary(
    std::initializer_list<std::pair<const char*, std::initializer_list<const char*>>> words) {
    QHash<QString, QStringList> known;
    for (const auto& [word, forms] : words) {
        QStringList all;
        for (const char* form : forms) {
            all.append(text(form));
        }
        for (const QString& form : std::as_const(all)) {
            known.insert(form, all);
        }
        known.insert(text(word), all);
    }
    return [known](const QString& word) {
        return known.value(word);
    };
}

}  // namespace

TEST_CASE("NameDeclension: women's names and surnames", "[core][dictionary][declension]") {
    CHECK(modelsOf("Anna Wolska") == "anna,kowalska");
    CHECK(row(formsOf("Anna Wolska"), false) ==
          text("Anna Wolska, Anny Wolskiej, Annie Wolskiej, Annę Wolską, Anną Wolską, "
               "Annie Wolskiej, Anno Wolska")
              .toStdString());
    CHECK(row(formsOf("Anna Wolska"), true) ==
          text("Anny Wolskie, Ann Wolskich, Annom Wolskim, Anny Wolskie, Annami Wolskimi, "
               "Annach Wolskich, Anny Wolskie")
              .toStdString());

    CHECK(row(formsOf("Kasia"), false) ==
          text("Kasia, Kasi, Kasi, Kasię, Kasią, Kasi, Kasiu").toStdString());
    CHECK(row(formsOf("Kasia"), true) ==
          text("Kasie, Kaś, Kasiom, Kasie, Kasiami, Kasiach, Kasie").toStdString());
    CHECK(row(formsOf("Zofia"), false) ==
          text("Zofia, Zofii, Zofii, Zofię, Zofią, Zofii, Zofio").toStdString());
    CHECK(row(formsOf("Zofia"), true) ==
          text("Zofie, Zofii, Zofiom, Zofie, Zofiami, Zofiach, Zofie").toStdString());
    CHECK(row(formsOf("Maja"), false) ==
          text("Maja, Mai, Mai, Maję, Mają, Mai, Maju").toStdString());
    CHECK(
        row(formsOf("Patrycja"), false) ==
        text("Patrycja, Patrycji, Patrycji, Patrycję, Patrycją, Patrycji, Patrycjo").toStdString());
    CHECK(row(formsOf("Ola"), false) == text("Ola, Oli, Oli, Olę, Olą, Oli, Olu").toStdString());
    CHECK(row(formsOf("Ola"), true) == text("Ole, Ol, Olom, Ole, Olami, Olach, Ole").toStdString());
    CHECK(row(formsOf("Urszula"), false) ==
          text("Urszula, Urszuli, Urszuli, Urszulę, Urszulą, Urszuli, Urszulo").toStdString());
    CHECK(row(formsOf("Monika"), false) ==
          text("Monika, Moniki, Monice, Monikę, Moniką, Monice, Moniko").toStdString());
    CHECK(row(formsOf("Monika"), true) ==
          text("Moniki, Monik, Monikom, Moniki, Monikami, Monikach, Moniki").toStdString());
    CHECK(row(formsOf("Olga"), false) ==
          text("Olga, Olgi, Oldze, Olgę, Olgą, Oldze, Olgo").toStdString());
    CHECK(row(formsOf("Marta"), false) ==
          text("Marta, Marty, Marcie, Martę, Martą, Marcie, Marto").toStdString());
    CHECK(row(formsOf("Wiesława"), false) ==
          text("Wiesława, Wiesławy, Wiesławie, Wiesławę, Wiesławą, Wiesławie, Wiesławo")
              .toStdString());
    CHECK(row(formsOf("Elżbieta Nowakowa"), false) ==
          text("Elżbieta Nowakowa, Elżbiety Nowakowej, Elżbiecie Nowakowej, Elżbietę Nowakową, "
               "Elżbietą Nowakową, Elżbiecie Nowakowej, Elżbieto Nowakowa")
              .toStdString());

    SECTION("a woman's surname ending in a consonant, or in -o, -e, -i, -y, does not change") {
        CHECK(modelsOf("Anna Nowak") == "anna,none");
        CHECK(modelsOf("Joanna Fredro") == "anna,none");
        CHECK(modelsOf("Maria Curie") == "anna,none");
        CHECK(modelsOf("Ewa Nelly") == "anna,none");
        CHECK(row(formsOf("Anna Nowak"), false) ==
              text("Anna Nowak, Anny Nowak, Annie Nowak, Annę Nowak, Anną Nowak, Annie Nowak, "
                   "Anno Nowak")
                  .toStdString());
    }

    SECTION("a surname ending in -ska makes a woman of a name without -a") {
        CHECK(modelsOf("Ruth Wolska") == "none,kowalska");
    }
}

TEST_CASE("NameDeclension: men's names and surnames", "[core][dictionary][declension]") {
    CHECK(modelsOf("Piotr Kowalski") == "jan,kowalski");
    CHECK(row(formsOf("Piotr Kowalski"), false) ==
          text("Piotr Kowalski, Piotra Kowalskiego, Piotrowi Kowalskiemu, Piotra Kowalskiego, "
               "Piotrem Kowalskim, Piotrze Kowalskim, Piotrze Kowalski")
              .toStdString());
    CHECK(row(formsOf("Piotr Kowalski"), true) ==
          text("Piotrowie Kowalscy, Piotrów Kowalskich, Piotrom Kowalskim, Piotrów Kowalskich, "
               "Piotrami Kowalskimi, Piotrach Kowalskich, Piotrowie Kowalscy")
              .toStdString());
    CHECK(row(formsOf("Eldrion"), false) ==
          text("Eldrion, Eldriona, Eldrionowi, Eldriona, Eldrionem, Eldrionie, Eldrionie")
              .toStdString());
    CHECK(row(formsOf("Eldrion"), true) ==
          text("Eldrionowie, Eldrionów, Eldrionom, Eldrionów, Eldrionami, Eldrionach, Eldrionowie")
              .toStdString());

    CHECK(modelsOf("Marek") == "marek");
    CHECK(row(formsOf("Marek"), false) ==
          text("Marek, Marka, Markowi, Marka, Markiem, Marku, Marku").toStdString());
    CHECK(row(formsOf("Paweł"), false) ==
          text("Paweł, Pawła, Pawłowi, Pawła, Pawłem, Pawle, Pawle").toStdString());
    CHECK(row(formsOf("Tomasz"), false) ==
          text("Tomasz, Tomasza, Tomaszowi, Tomasza, Tomaszem, Tomaszu, Tomaszu").toStdString());
    CHECK(row(formsOf("Staś"), false) ==
          text("Staś, Stasia, Stasiowi, Stasia, Stasiem, Stasiu, Stasiu").toStdString());
    CHECK(row(formsOf("Robert"), false) ==
          text("Robert, Roberta, Robertowi, Roberta, Robertem, Robercie, Robercie").toStdString());
    CHECK(row(formsOf("Wojciech"), false) ==
          text("Wojciech, Wojciecha, Wojciechowi, Wojciecha, Wojciechem, Wojciechu, Wojciechu")
              .toStdString());
    CHECK(row(formsOf("Jerzy"), false) ==
          text("Jerzy, Jerzego, Jerzemu, Jerzego, Jerzym, Jerzym, Jerzy").toStdString());
    CHECK(modelsOf("Kuba") == "kuba");
    CHECK(row(formsOf("Kuba"), false) ==
          text("Kuba, Kuby, Kubie, Kubę, Kubą, Kubie, Kubo").toStdString());
    CHECK(row(formsOf("Kuba"), true) ==
          text("Kubowie, Kubów, Kubom, Kubów, Kubami, Kubach, Kubowie").toStdString());
    CHECK(row(formsOf("Kościuszko"), false) ==
          text("Kościuszko, Kościuszki, Kościuszce, Kościuszkę, Kościuszką, Kościuszce, Kościuszko")
              .toStdString());

    SECTION("the plural of adjectives of persons") {
        const NameDeclension polish(QStringLiteral("pl"));
        const auto nominativePlural = [&polish](const char* word) {
            return polish.wordForms(text(word), QStringLiteral("kowalski"), NameKind::Character)
                .cell(GrammaticalCase::Nominative, true)
                .join(QLatin1Char('/'))
                .toStdString();
        };
        CHECK(nominativePlural("Długi") == text("Dłudzy").toStdString());
        CHECK(nominativePlural("Czarny") == "Czarni");
        CHECK(nominativePlural("Cichy") == "Cisi");
        CHECK(nominativePlural("Stary") == "Starzy");
        CHECK(nominativePlural("Mały") == "Mali");
        CHECK(nominativePlural("Jerzy") == "Jerzy");
    }
}

TEST_CASE("NameDeclension: places, things and other names", "[core][dictionary][declension]") {
    CHECK(modelsOf("Gdańsk", NameKind::Place) == "gdansk");
    CHECK(row(formsOf("Gdańsk", NameKind::Place), false) ==
          text("Gdańsk, Gdańska, Gdańskowi, Gdańsk, Gdańskiem, Gdańsku, Gdańsku").toStdString());
    CHECK(
        row(formsOf("Gdańsk", NameKind::Place), true) ==
        text("Gdański, Gdańsków, Gdańskom, Gdański, Gdańskami, Gdańskach, Gdański").toStdString());
    CHECK(row(formsOf("Kraków", NameKind::Place), false) ==
          text("Kraków, Krakowa, Krakowowi, Kraków, Krakowem, Krakowie, Krakowie").toStdString());
    CHECK(row(formsOf("Poznań", NameKind::Place), true) ==
          text("Poznanie, Poznani, Poznaniom, Poznanie, Poznaniami, Poznaniach, Poznanie")
              .toStdString());
    CHECK(row(formsOf("Paryż", NameKind::Place), true) ==
          text("Paryże, Paryżów, Paryżom, Paryże, Paryżami, Paryżach, Paryże").toStdString());
    CHECK(row(formsOf("Gniezno", NameKind::Place), false) ==
          text("Gniezno, Gniezna, Gnieznu, Gniezno, Gnieznem, Gnieźnie, Gniezno").toStdString());
    CHECK(row(formsOf("Kłodzko", NameKind::Place), false) ==
          text("Kłodzko, Kłodzka, Kłodzku, Kłodzko, Kłodzkiem, Kłodzku, Kłodzko").toStdString());
    CHECK(row(formsOf("Opole", NameKind::Place), false) ==
          text("Opole, Opola, Opolu, Opole, Opolem, Opolu, Opole").toStdString());
    CHECK(row(formsOf("Opole", NameKind::Place), true) ==
          text("Opola, Opol, Opolom, Opola, Opolami, Opolach, Opola").toStdString());
    CHECK(row(formsOf("Zakopane", NameKind::Place), false) ==
          text("Zakopane, Zakopanego, Zakopanemu, Zakopane, Zakopanem/Zakopanym, "
               "Zakopanem/Zakopanym, Zakopane")
              .toStdString());
    CHECK(row(formsOf("Zakopane", NameKind::Place), true) ==
          text("Zakopane, Zakopanych, Zakopanym, Zakopane, Zakopanymi, Zakopanych, Zakopane")
              .toStdString());
    CHECK(row(formsOf("Wysokie", NameKind::Place), false) ==
          text("Wysokie, Wysokiego, Wysokiemu, Wysokie, Wysokim, Wysokim, Wysokie").toStdString());
    CHECK(row(formsOf("Czarny Las", NameKind::Place), false) ==
          text("Czarny Las, Czarnego Lasa, Czarnemu Lasowi, Czarny Las, Czarnym Lasem, "
               "Czarnym Lasie, Czarny Lasie")
              .toStdString());
    CHECK(row(formsOf("Czarny Las", NameKind::Place), true) ==
          text("Czarne Lasy, Czarnych Lasów, Czarnym Lasom, Czarne Lasy, Czarnymi Lasami, "
               "Czarnych Lasach, Czarne Lasy")
              .toStdString());
    CHECK(row(formsOf("Wieża Ossów", NameKind::Place), false) ==
          text("Wieża Ossów, Wieży Ossów, Wieży Ossów, Wieżę Ossów, Wieżą Ossów, Wieży Ossów, "
               "Wieżo Ossów")
              .toStdString());
    CHECK(row(formsOf("Wieża Ossów", NameKind::Place), true) ==
          text("Wieże Ossów, Wież Ossów, Wieżom Ossów, Wieże Ossów, Wieżami Ossów, "
               "Wieżach Ossów, Wieże Ossów")
              .toStdString());
    CHECK(row(NameDeclension(QStringLiteral("pl"))
                  .nameForms(text("Ostatnia Wieża"),
                             {QStringLiteral("kowalska"), QStringLiteral("anna")}, NameKind::Place),
              true) == text("Ostatnie Wieże, Ostatnich Wież, Ostatnim Wieżom, Ostatnie Wieże, "
                            "Ostatnimi Wieżami, Ostatnich Wieżach, Ostatnie Wieże")
                           .toStdString());
    CHECK(row(formsOf("Wilcha", NameKind::Other), false) ==
          text("Wilcha, Wilchy, Wilsze, Wilchę, Wilchą, Wilsze, Wilcho").toStdString());
    CHECK(row(formsOf("Wilcha", NameKind::Other), true) ==
          text("Wilchy, Wilch, Wilchom, Wilchy, Wilchami, Wilchach, Wilchy").toStdString());

    SECTION("the words after a preposition, and particles, do not change") {
        CHECK(modelsOf("Wieża nad Morzem", NameKind::Place) == "anna,none,none");
        CHECK(modelsOf("Ludwig van Beethoven") == "jan,none,jan");
    }

    SECTION("words that do not decline") {
        CHECK(modelsOf("NATO", NameKind::Other) == "none");
        CHECK(modelsOf("Malibu", NameKind::Place) == "none");
        CHECK(modelsOf("X", NameKind::Other) == "none");
        CHECK(modelsOf("Renee") == "none");
        CHECK(modelsOf("Aleja 3", NameKind::Place) == "anna,none");
    }
}

TEST_CASE("NameDeclension: the spelling dictionary picks and confirms the forms",
          "[core][dictionary][declension]") {
    SECTION("the model whose forms the dictionary knows best") {
        const auto dictionary = fakeDictionary(
            {{"londyn", {"londyn", "londynu", "londynowi", "londynem", "londynie"}}});
        CHECK(modelsOf("Londyn", NameKind::Place) == "gdansk");
        CHECK(modelsOf("Londyn", NameKind::Place, dictionary) == "londyn");
        CHECK(row(formsOf("Londyn", NameKind::Place, dictionary), false) ==
              "Londyn, Londynu, Londynowi, Londyn, Londynem, Londynie, Londynie");
    }

    SECTION("a form the model is not sure of is the one the dictionary knows") {
        const auto dictionary = fakeDictionary(
            {{"księga", {"księga", "księgi", "księdze", "księgę", "księgą", "ksiąg", "księgom"}}});
        CHECK(formsOf("Księga", NameKind::Item).cell(GrammaticalCase::Genitive, true) ==
              QStringList{text("Księg")});
        CHECK(formsOf("Księga", NameKind::Item, dictionary).cell(GrammaticalCase::Genitive, true) ==
              QStringList{text("Ksiąg")});
        CHECK(formsOf("Agnieszka", NameKind::Character,
                      fakeDictionary({{"Agnieszka", {"Agnieszka", "Agnieszek"}}}))
                  .cell(GrammaticalCase::Genitive, true) == QStringList{text("Agnieszek")});
    }

    SECTION("a later word already a form of another word does not change") {
        const auto dictionary = fakeDictionary(
            {{"popiół", {"popiół", "popiołu", "popiołowi", "popiołem", "popiele", "popiołów"}}});
        CHECK(modelsOf("Księga Popiołów", NameKind::Item) == "anna,none");
        CHECK(modelsOf("Wieża Popioła", NameKind::Place) == "anna,anna");
        CHECK(modelsOf("Wieża Popioła", NameKind::Place,
                       fakeDictionary({{"Popioła", {"Popioła"}}})) == "anna,none");
        CHECK(modelsOf("Popioła", NameKind::Place, fakeDictionary({{"Popioła", {"Popioła"}}})) ==
              "anna");
        CHECK(modelsOf("Księga Popiołów", NameKind::Item, dictionary) == "anna,none");
    }

    SECTION("a name the dictionary has, and its common word in lower case") {
        const auto dictionary = fakeDictionary(
            {{"Biała", {"Biała"}}, {"biały", {"biała", "białej", "białą", "białe", "białych"}}});
        CHECK(modelsOf("Biała Wieża", NameKind::Place) == "anna,anna");
        CHECK(modelsOf("Biała Wieża", NameKind::Place, dictionary) == "kowalska,anna");
    }

    SECTION("the Polish dictionary of the program") {
        const QString folder = QStringLiteral(KALAHARI_SOURCE_DIR "/resources/dictionaries/");
        AffixExpander expander;
        REQUIRE(expander.load(folder + QStringLiteral("pl_PL.aff"),
                              folder + QStringLiteral("pl_PL.dic")));
        const NameDeclension::DictionaryForms dictionary = [&expander](const QString& word) {
            return expander.relatedForms(word);
        };
        CHECK(modelsOf("Księga Popiołów", NameKind::Item, dictionary) == "anna,none");
        CHECK(formsOf("Księga Popiołów", NameKind::Item, dictionary)
                  .cell(GrammaticalCase::Genitive, true) == QStringList{text("Ksiąg Popiołów")});
        CHECK(modelsOf("Anna Wolska", NameKind::Character, dictionary) == "anna,kowalska");
        CHECK(modelsOf("Kuba", NameKind::Character, dictionary) == "kuba");
    }
}

TEST_CASE("NameDeclension: models given by hand", "[core][dictionary][declension]") {
    const NameDeclension polish(QStringLiteral("pl_PL"));
    CHECK(polish.language() == "pl");
    CHECK(polish.cases().size() == static_cast<qsizetype>(CASE_COUNT));
    CHECK(polish.models().size() == 12);
    CHECK(polish.models().back() == NameDeclension::unchanged());

    SECTION("a model that does not fit the word gives it unchanged") {
        const FormTable table =
            polish.wordForms(QStringLiteral("Anna"), QStringLiteral("jan"), NameKind::Character);
        CHECK(row(table, false) == "Anna, Anna, Anna, Anna, Anna, Anna, Anna");
        CHECK(row(polish.wordForms(QStringLiteral("Ruth"), QStringLiteral("unknown"),
                                   NameKind::Character),
                  true) == "Ruth, Ruth, Ruth, Ruth, Ruth, Ruth, Ruth");
    }

    SECTION("each model on a word it fits") {
        CHECK(row(polish.wordForms(QStringLiteral("Londyn"), QStringLiteral("londyn"),
                                   NameKind::Place),
                  false) == "Londyn, Londynu, Londynowi, Londyn, Londynem, Londynie, Londynie");
        CHECK(row(polish.wordForms(QStringLiteral("Czarny"), QStringLiteral("kowalski"),
                                   NameKind::Place),
                  false) == "Czarny, Czarnego, Czarnemu, Czarny, Czarnym, Czarnym, Czarny");
        CHECK(row(polish.wordForms(QStringLiteral("Zabrze"), QStringLiteral("opole"),
                                   NameKind::Place),
                  false) == "Zabrze, Zabrza, Zabrzu, Zabrze, Zabrzem, Zabrzu, Zabrze");
        CHECK(row(polish.wordForms(QStringLiteral("Fredro"), QStringLiteral("kuba"),
                                   NameKind::Character),
                  false) == "Fredro, Fredry, Fredrze, Fredrę, Fredrą, Fredrze, Fredro");
    }

    SECTION("models as many as the words, or picked anew") {
        CHECK(row(polish.nameForms(QStringLiteral("Anna Nowak"),
                                   {QStringLiteral("anna"), QStringLiteral("jan")},
                                   NameKind::Character),
                  false) ==
              text("Anna Nowak, Anny Nowaka, Annie Nowakowi, Annę Nowaka, Anną Nowakiem, "
                   "Annie Nowaku, Anno Nowaku")
                  .toStdString());
        CHECK(row(polish.nameForms(QStringLiteral("Anna Nowak"), {QStringLiteral("anna")},
                                   NameKind::Character),
                  false) == text("Anna Nowak, Anny Nowak, Annie Nowak, Annę Nowak, Anną Nowak, "
                                 "Annie Nowak, Anno Nowak")
                                .toStdString());
        CHECK(polish.nameForms(QString(), {}, NameKind::Character).isEmpty());
        CHECK(polish.wordForms(QString(), QStringLiteral("anna"), NameKind::Character).isEmpty());
    }

    SECTION("at most four forms in a cell of a name") {
        const FormTable table =
            polish.nameForms(QStringLiteral("Zakopane Zakopane Zakopane"), {}, NameKind::Place);
        CHECK(table.cell(GrammaticalCase::Locative, false).size() == 4);
        CHECK(table.cell(GrammaticalCase::Locative, false).front() ==
              "Zakopanem Zakopanem Zakopanem");
    }
}

TEST_CASE("NameDeclension: English and other languages", "[core][dictionary][declension]") {
    const NameDeclension english(QStringLiteral("en-GB"));
    CHECK(english.language() == "en");
    CHECK(english.cases() == QList<GrammaticalCase>{GrammaticalCase::Nominative});
    CHECK(english.models() == QStringList{QStringLiteral("regular"), QStringLiteral("none")});
    CHECK(english.modelsFor(QStringLiteral("The Black Tower"), NameKind::Place) ==
          QStringList{QStringLiteral("none"), QStringLiteral("none"), QStringLiteral("regular")});
    CHECK(english.modelsFor(QStringLiteral("NATO"), NameKind::Other) ==
          QStringList{QStringLiteral("none")});

    const auto plural = [&english](const char* name) {
        return english.nameForms(text(name), {}, NameKind::Character)
            .cell(GrammaticalCase::Nominative, true)
            .join(QLatin1Char('/'))
            .toStdString();
    };
    CHECK(plural("Anna Smith") == "Anna Smiths");
    CHECK(plural("Jones") == "Joneses");
    CHECK(plural("Murphy") == "Murphies");
    CHECK(plural("Kennedy Grey") == "Kennedy Greys");
    CHECK(plural("Butch") == "Butches");
    CHECK(english.nameForms(QStringLiteral("Anna"), {}, NameKind::Character)
              .cell(GrammaticalCase::Genitive, false)
              .isEmpty());
    CHECK(english.wordForms(QStringLiteral("Anna"), QStringLiteral("none"), NameKind::Character)
              .cell(GrammaticalCase::Nominative, true) == QStringList{QStringLiteral("Anna")});

    const NameDeclension german(QStringLiteral("de_DE"));
    CHECK(german.models() == QStringList{QStringLiteral("none")});
    CHECK(german.modelsFor(QStringLiteral("Anna Schmidt"), NameKind::Character) ==
          QStringList{QStringLiteral("none"), QStringLiteral("none")});
    const FormTable table =
        german.nameForms(QStringLiteral("Anna Schmidt"), {}, NameKind::Character);
    CHECK(table.allForms(true) == QStringList{QStringLiteral("Anna Schmidt")});
}

TEST_CASE("FormTable: cells, their names and all the forms", "[core][dictionary][declension]") {
    FormTable table;
    CHECK(table.isEmpty());
    table.cell(GrammaticalCase::Nominative, false) = QStringList{QStringLiteral("Anna")};
    table.cell(GrammaticalCase::Genitive, false) = QStringList{QStringLiteral("Anny"), QString()};
    table.cell(GrammaticalCase::Nominative, true) = QStringList{QStringLiteral("Anny")};
    table.cell(GrammaticalCase::Genitive, true) = QStringList{QStringLiteral("Ann")};
    CHECK_FALSE(table.isEmpty());
    CHECK(table.allForms(false) == QStringList{QStringLiteral("Anna"), QStringLiteral("Anny")});
    CHECK(table.allForms(true) ==
          QStringList{QStringLiteral("Anna"), QStringLiteral("Anny"), QStringLiteral("Ann")});

    CHECK(FormTable::cellKey(GrammaticalCase::Instrumental, false) == "instrumental");
    CHECK(FormTable::cellKey(GrammaticalCase::Vocative, true) == "vocativePlural");
    GrammaticalCase grammaticalCase = GrammaticalCase::Nominative;
    bool plural = false;
    REQUIRE(FormTable::cellOfKey(QStringLiteral("locativePlural"), grammaticalCase, plural));
    CHECK(grammaticalCase == GrammaticalCase::Locative);
    CHECK(plural);
    CHECK_FALSE(FormTable::cellOfKey(QStringLiteral("ablative"), grammaticalCase, plural));
}
