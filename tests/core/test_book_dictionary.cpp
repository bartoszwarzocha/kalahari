// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_book_dictionary.cpp
/// @brief The dictionary of a book: its names, rules and words, its file, the words it takes
/// for right, the words that go against its rules, and the forms of names and rules

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/affix_expander.h>
#include <kalahari/core/book_dictionary.h>
#include <kalahari/core/name_declension.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <initializer_list>
#include <string>

using kalahari::core::AffixExpander;
using kalahari::core::BookDictionary;
using kalahari::core::DictionaryName;
using kalahari::core::fillRuleForms;
using kalahari::core::GrammaticalCase;
using kalahari::core::makeName;
using kalahari::core::NameDeclension;
using kalahari::core::NameKind;
using kalahari::core::NameVariant;
using kalahari::core::refreshForms;
using kalahari::core::RuleDeviation;
using kalahari::core::SpellingRule;
using kalahari::core::suggestedWrongSpellings;

namespace {

QString text(const char* utf8) {
    return QString::fromUtf8(utf8);
}

/// A rule without forms of the dictionary: its wrong spellings with the right one
SpellingRule rule(const char* right, std::initializer_list<const char*> wrong,
                  bool allForms = true) {
    SpellingRule made;
    made.right = text(right);
    for (const char* spelling : wrong) {
        made.wrong.append(text(spelling));
    }
    made.allForms = allForms;
    fillRuleForms(made, nullptr);
    return made;
}

/// The deviations of a text: "start+length text>right" each, joined with "; "
std::string deviationsOf(const BookDictionary& dictionary, const char* paragraph) {
    QStringList found;
    for (const RuleDeviation& deviation : dictionary.deviations(text(paragraph))) {
        found.append(QStringLiteral("%1+%2 %3>%4")
                         .arg(deviation.start)
                         .arg(deviation.length)
                         .arg(deviation.text, deviation.right));
    }
    return found.join(QStringLiteral("; ")).toStdString();
}

/// The Polish dictionary of the program
const AffixExpander& polishDictionary() {
    static const AffixExpander expander = [] {
        const QString folder = QStringLiteral(KALAHARI_SOURCE_DIR "/resources/dictionaries/");
        AffixExpander loaded;
        REQUIRE(loaded.load(folder + QStringLiteral("pl_PL.aff"),
                            folder + QStringLiteral("pl_PL.dic")));
        return loaded;
    }();
    return expander;
}

/// The forms of the rule's wrong spellings: "wrong>right" each, joined with ", "
std::string formsOf(const SpellingRule& made) {
    QStringList forms;
    for (auto form = made.forms.constBegin(); form != made.forms.constEnd(); ++form) {
        forms.append(form.key() + QLatin1Char('>') + form.value());
    }
    return forms.join(QStringLiteral(", ")).toStdString();
}

/// A dictionary with Anna Wolska, the rules Smok, e-mail and okej, and the word Kirran
BookDictionary sampleDictionary() {
    BookDictionary dictionary;
    const NameDeclension polish(QStringLiteral("pl"));
    dictionary.putName(makeName(text("Anna Wolska"), NameKind::Character, polish));
    dictionary.putName(makeName(text("Księga Popiołów"), NameKind::Item, polish));
    SpellingRule smok = rule("Smok", {"smok"});
    smok.forms.insert(QStringLiteral("smoka"), QStringLiteral("Smoka"));
    dictionary.putRule(smok);
    dictionary.putRule(rule("e-mail", {"email", "e mail"}));
    dictionary.putRule(rule("okej", {"OK", "okey"}, false));
    dictionary.addWord(QStringLiteral("Kirran"));
    dictionary.addWord(text("eldryjski"));
    return dictionary;
}

}  // namespace

TEST_CASE("BookDictionary: names, rules and words", "[core][dictionary][book]") {
    BookDictionary dictionary;
    CHECK(dictionary.isEmpty());
    CHECK(BookDictionary::fileName() == "dictionary.json");

    const NameDeclension polish(QStringLiteral("pl"));
    DictionaryName anna = makeName(QStringLiteral("  Anna   Wolska "), NameKind::Character, polish);
    anna.card = QStringLiteral("e4");
    const QString annaId = dictionary.putName(anna);
    CHECK_FALSE(annaId.isEmpty());
    CHECK_FALSE(dictionary.isEmpty());
    REQUIRE(dictionary.findName(annaId) != nullptr);
    CHECK(dictionary.findName(annaId)->text() == "Anna Wolska");
    CHECK(dictionary.findNameOfCard(QStringLiteral("e4")) == dictionary.findName(annaId));
    CHECK(dictionary.findNameOfCard(QString()) == nullptr);
    CHECK(dictionary.findNameOfCard(QStringLiteral("e5")) == nullptr);
    CHECK(dictionary.findNameWithText(QStringLiteral("anna wolska")) ==
          dictionary.findName(annaId));
    CHECK(dictionary.findNameWithText(QStringLiteral("Anna")) == nullptr);
    CHECK(dictionary.findName(QStringLiteral("none")) == nullptr);

    SECTION("a name with its id takes the place of the one before") {
        DictionaryName changed = *dictionary.findName(annaId);
        changed.plural = true;
        CHECK(dictionary.putName(changed) == annaId);
        CHECK(dictionary.names().size() == 1);
        CHECK(dictionary.findName(annaId)->plural);
        CHECK(dictionary.removeName(annaId));
        CHECK_FALSE(dictionary.removeName(annaId));
        CHECK(dictionary.isEmpty());
    }

    SECTION("rules") {
        const QString ruleId = dictionary.putRule(rule("Smok", {"smok"}));
        REQUIRE(dictionary.findRule(ruleId) != nullptr);
        SpellingRule changed = *dictionary.findRule(ruleId);
        changed.note = QStringLiteral("Smoki to ród.");
        CHECK(dictionary.putRule(changed) == ruleId);
        CHECK(dictionary.rules().size() == 1);
        CHECK(dictionary.findRule(ruleId)->note == text("Smoki to ród."));
        CHECK(dictionary.findRule(QStringLiteral("none")) == nullptr);
        CHECK(dictionary.removeRule(ruleId));
        CHECK_FALSE(dictionary.removeRule(ruleId));
    }

    SECTION("words, sorted, each once") {
        CHECK(dictionary.addWord(QStringLiteral("Kirran")));
        CHECK(dictionary.addWord(QStringLiteral(" eldryjski ")));
        CHECK_FALSE(dictionary.addWord(QStringLiteral("Kirran")));
        CHECK_FALSE(dictionary.addWord(QStringLiteral("  ")));
        CHECK(dictionary.words() ==
              QStringList{QStringLiteral("eldryjski"), QStringLiteral("Kirran")});
        CHECK(dictionary.hasWord(QStringLiteral("Kirran")));
        CHECK_FALSE(dictionary.hasWord(QStringLiteral("kirran")));
        CHECK(dictionary.removeWord(QStringLiteral("Kirran")));
        CHECK_FALSE(dictionary.removeWord(QStringLiteral("Kirran")));
    }

    SECTION("clear") {
        dictionary.addWord(QStringLiteral("Kirran"));
        dictionary.clear();
        CHECK(dictionary.isEmpty());
        CHECK_FALSE(dictionary.isRight(QStringLiteral("Annie")));
    }
}

TEST_CASE("BookDictionary: its file", "[core][dictionary][book]") {
    const QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString path = folder.filePath(BookDictionary::fileName());

    SECTION("a book without the file has an empty dictionary, and an empty one makes no file") {
        BookDictionary dictionary = sampleDictionary();
        CHECK(dictionary.load(path));
        CHECK(dictionary.isEmpty());
        CHECK(dictionary.save(path));
        CHECK_FALSE(QFile::exists(path));
    }

    SECTION("what is saved is read back") {
        BookDictionary saved = sampleDictionary();
        DictionaryName anna = saved.names().front();
        anna.card = QStringLiteral("e4");
        anna.plural = true;
        anna.quickInsert = false;
        anna.variants.front().edited = QStringList{QStringLiteral("vocative")};
        saved.putName(anna);
        SpellingRule smok = saved.rules().front();
        smok.note = QStringLiteral("Smoki to ród.");
        smok.pattern = QStringLiteral("smok");
        saved.putRule(smok);
        QString error;
        REQUIRE(saved.save(path, &error));
        CHECK(error.isEmpty());

        BookDictionary read;
        REQUIRE(read.load(path, &error));
        CHECK(read.names() == saved.names());
        CHECK(read.rules() == saved.rules());
        CHECK(read.words() == saved.words());
        CHECK(read.isRight(text("Annie")));

        SECTION("an empty dictionary is saved over a file that has one") {
            read.clear();
            REQUIRE(read.save(path));
            BookDictionary empty = sampleDictionary();
            REQUIRE(empty.load(path));
            CHECK(empty.isEmpty());
        }
    }

    SECTION("the default values are left out of the file") {
        const QJsonObject json = sampleDictionary().toJson();
        CHECK(json.value(QStringLiteral("format")).toInt() == BookDictionary::FORMAT);
        const QJsonObject name = json.value(QStringLiteral("names")).toArray().first().toObject();
        CHECK(name.value(QStringLiteral("kind")).toString() == "character");
        CHECK_FALSE(name.contains(QStringLiteral("card")));
        CHECK_FALSE(name.contains(QStringLiteral("plural")));
        CHECK_FALSE(name.contains(QStringLiteral("quickInsert")));
        const QJsonObject variant =
            name.value(QStringLiteral("variants")).toArray().first().toObject();
        CHECK_FALSE(variant.contains(QStringLiteral("edited")));
        CHECK_FALSE(variant.contains(QStringLiteral("automatic")));
        const QJsonObject other = name.value(QStringLiteral("variants")).toArray().at(1).toObject();
        CHECK(other.value(QStringLiteral("automatic")).toBool());
        const QJsonObject okej = json.value(QStringLiteral("rules")).toArray().at(2).toObject();
        CHECK(okej.value(QStringLiteral("allForms")).toBool(true) == false);
        CHECK_FALSE(okej.contains(QStringLiteral("note")));
        CHECK_FALSE(okej.contains(QStringLiteral("pattern")));
    }

    SECTION("what the file lacks or has wrong is left out or given") {
        const QJsonObject json = QJsonDocument::fromJson(R"({
            "names": [
                { "kind": "place", "variants": [ { "text": "Gdańsk", "models": ["gdansk"],
                  "forms": { "nominative": ["Gdańsk"], "genitive": ["Gdańska", 3],
                             "ablative": ["Gdańskiem"] } }, { "text": "" } ] },
                { "kind": "ship", "variants": [] },
                { "kind": "ship", "variants": [ { "text": "Orzeł" } ] }
            ],
            "rules": [ { "right": "" }, { "right": "Smok", "forms": { "smoka": "Smoka" } } ],
            "words": ["Kirran", "", "Kirran", 5]
        })")
                                     .object();
        BookDictionary dictionary;
        REQUIRE(dictionary.fromJson(json));
        REQUIRE(dictionary.names().size() == 2);
        const DictionaryName& gdansk = dictionary.names().front();
        CHECK_FALSE(gdansk.id.isEmpty());
        CHECK(gdansk.kind == NameKind::Place);
        CHECK(gdansk.quickInsert);
        CHECK(gdansk.variants.size() == 1);
        CHECK(gdansk.variants.front().forms.cell(GrammaticalCase::Genitive, false) ==
              QStringList{text("Gdańska")});
        CHECK(dictionary.names().at(1).kind == NameKind::Character);
        REQUIRE(dictionary.rules().size() == 1);
        CHECK_FALSE(dictionary.rules().front().id.isEmpty());
        CHECK(dictionary.rules().front().allForms);
        CHECK(dictionary.words() == QStringList{QStringLiteral("Kirran")});
        CHECK(dictionary.isRight(text("Gdańska")));
        CHECK(dictionary.isRight(text("Orzeł")));
    }

    SECTION("a file of a later format is not read, so it is not saved over") {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write(R"({ "format": 2, "words": ["Kirran"] })");
        file.close();
        BookDictionary dictionary = sampleDictionary();
        QString error;
        CHECK_FALSE(dictionary.load(path, &error));
        CHECK(error.contains(QStringLiteral("format 2")));
        CHECK(dictionary.isEmpty());
        CHECK_FALSE(dictionary.fromJson(QJsonObject{{QStringLiteral("format"), 3}}));
    }

    SECTION("a file that is not a dictionary") {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("[1, 2");
        file.close();
        BookDictionary dictionary;
        QString error;
        CHECK_FALSE(dictionary.load(path, &error));
        CHECK_FALSE(error.isEmpty());
        CHECK_FALSE(dictionary.load(path));
    }

    SECTION("a file that cannot be read or written") {
        BookDictionary dictionary = sampleDictionary();
        QString error;
        CHECK_FALSE(dictionary.load(folder.path(), &error));  // a folder
        CHECK_FALSE(error.isEmpty());
        CHECK_FALSE(dictionary.load(folder.path()));
        dictionary = sampleDictionary();
        error.clear();
        const QString inMissingFolder = folder.filePath(QStringLiteral("missing/dictionary.json"));
        CHECK_FALSE(dictionary.save(inMissingFolder, &error));
        CHECK_FALSE(error.isEmpty());
        CHECK_FALSE(dictionary.save(inMissingFolder));
    }
}

TEST_CASE("BookDictionary: the words it takes for right", "[core][dictionary][book]") {
    const BookDictionary dictionary = sampleDictionary();

    SECTION("every form of a name, only as it is written") {
        for (const char* word : {"Anna", "Anny", "Annie", "Annę", "Anną", "Anno", "Wolskiej",
                                 "Wolską", "Księdze", "Popiołów"}) {
            CHECK(dictionary.isRight(text(word)));
        }
        CHECK_FALSE(dictionary.isRight(QStringLiteral("annie")));
        CHECK(dictionary.nameSpelling(QStringLiteral("annie")) == "Annie");
        CHECK(dictionary.nameSpelling(text("WOLSKĄ")) == text("Wolską"));
        CHECK(dictionary.nameSpelling(QStringLiteral("Annie")).isEmpty());
        CHECK(dictionary.nameSpelling(QStringLiteral("kot")).isEmpty());
        CHECK_FALSE(dictionary.isRight(QStringLiteral("Wolskich")));  // the plural is not used
    }

    SECTION("an ending after an apostrophe") {
        CHECK(dictionary.isRight(QStringLiteral("Anna's")));
        CHECK(dictionary.isRight(text("Anna’s")));
        CHECK_FALSE(dictionary.isRight(QStringLiteral("annie's")));
        CHECK_FALSE(dictionary.isRight(QStringLiteral("'s")));
    }

    SECTION("the words of the book: in lower case in any case, with capitals only so") {
        CHECK(dictionary.isRight(QStringLiteral("Kirran")));
        CHECK_FALSE(dictionary.isRight(QStringLiteral("kirran")));
        CHECK(dictionary.isRight(text("eldryjski")));
        CHECK(dictionary.isRight(text("Eldryjski")));
        CHECK(dictionary.isRight(text("ELDRYJSKI")));
    }

    SECTION("the right spellings of the rules, also at the start of a sentence") {
        CHECK(dictionary.isRight(QStringLiteral("Smoka")));
        CHECK(dictionary.isRight(QStringLiteral("e-mail")));
        CHECK(dictionary.isRight(QStringLiteral("E-mail")));
        CHECK(dictionary.isRight(QStringLiteral("okej")));
        CHECK(dictionary.isRight(QStringLiteral("Okej")));
        CHECK_FALSE(dictionary.isRight(QStringLiteral("smoka")));
        CHECK_FALSE(dictionary.isRight(QString()));
    }

    SECTION("hyphenated names and their parts") {
        BookDictionary withHyphen;
        withHyphen.putName(makeName(text("Maria Curie-Skłodowska"), NameKind::Character,
                                    NameDeclension(QStringLiteral("pl"))));
        CHECK(withHyphen.isRight(text("Curie-Skłodowskiej")));
        CHECK(withHyphen.isRight(text("Skłodowskiej")));
        CHECK(withHyphen.isRight(QStringLiteral("Curie")));
    }
}

TEST_CASE("BookDictionary: the words that go against its rules", "[core][dictionary][book]") {
    const BookDictionary dictionary = sampleDictionary();

    CHECK(deviationsOf(dictionary, "– Widziałaś smoka? – zapytał.") == "12+5 smoka>Smoka");
    CHECK(deviationsOf(dictionary, "Widział smoka, a nie Smoka.") == "8+5 smoka>Smoka");
    CHECK(deviationsOf(dictionary, "Ostatni email od Jana.") == "8+5 email>e-mail");
    CHECK(deviationsOf(dictionary, "Przyszedł e mail.") == "10+6 e mail>e-mail");
    CHECK(deviationsOf(dictionary, "Litera e, mail nie.").empty());
    CHECK(deviationsOf(dictionary, "Na końcu e").empty());
    CHECK(deviationsOf(dictionary, "– Ale potem już OK?") == "16+2 OK>okej");
    CHECK(deviationsOf(dictionary, "smoka") == "0+5 smoka>Smoka");
    CHECK(deviationsOf(dictionary, "").empty());
    CHECK(BookDictionary().deviations(QStringLiteral("smoka")).isEmpty());

    SECTION("a capital letter at the start of a sentence") {
        CHECK(deviationsOf(dictionary, "– Okej – szepnęła.").empty());
        CHECK(deviationsOf(dictionary, "Tak. Okej.").empty());
        CHECK(deviationsOf(dictionary, "Tak, Okej.").empty());  // Okej is not a wrong spelling
        CHECK(deviationsOf(dictionary, "Email przyszedł.") == "0+5 Email>E-mail");
        CHECK(deviationsOf(dictionary, "Tak.” Email przyszedł.") == "6+5 Email>E-mail");
        CHECK(deviationsOf(dictionary, "Tak, Email przyszedł.").empty());
        CHECK(deviationsOf(dictionary, "„Smoka nie było.").empty());
        CHECK(deviationsOf(dictionary, "E mail przyszedł.") == "0+6 E mail>E-mail");
    }

    SECTION("a wrong spelling that is also the start of a longer one") {
        BookDictionary withPrefix;
        withPrefix.putRule(rule("e-mail", {"e mail"}));
        withPrefix.putRule(rule("E", {"e"}));
        CHECK(deviationsOf(withPrefix, "Tak, e mail.") == "5+6 e mail>e-mail");
        CHECK(deviationsOf(withPrefix, "Tak, e dole.") == "5+1 e>E");
    }
}

TEST_CASE("BookDictionary: every form of a name to search for", "[core][dictionary][book]") {
    BookDictionary dictionary = sampleDictionary();
    const QStringList anna = dictionary.formsOf(text("annę"));
    CHECK(anna == QStringList{QStringLiteral("Anna"), QStringLiteral("Anny"),
                              QStringLiteral("Annie"), text("Annę"), text("Anną"),
                              QStringLiteral("Anno")});
    CHECK(dictionary.formsOf(QStringLiteral("Anny Wolskiej")).contains(text("Annę Wolską")));
    CHECK(dictionary.formsOf(QStringLiteral("Ania")).isEmpty());
    CHECK(dictionary.formsOf(QStringLiteral("  ")).isEmpty());

    DictionaryName withPlural = dictionary.names().front();
    withPlural.plural = true;
    dictionary.putName(withPlural);
    CHECK(dictionary.formsOf(QStringLiteral("Wolska")).contains(QStringLiteral("Wolskich")));
}

TEST_CASE("BookDictionary: new names and their forms", "[core][dictionary][book]") {
    const NameDeclension polish(QStringLiteral("pl"));

    SECTION("a character of two words gets the first name and the surname alone") {
        const DictionaryName anna =
            makeName(QStringLiteral("Anna Wolska"), NameKind::Character, polish);
        REQUIRE(anna.variants.size() == 3);
        CHECK(anna.text() == "Anna Wolska");
        CHECK(anna.variants.at(1).text == "Anna");
        CHECK(anna.variants.at(1).automatic);
        CHECK(anna.variants.at(1).models == QStringList{QStringLiteral("anna")});
        CHECK(anna.variants.at(2).text == "Wolska");
        CHECK(anna.variants.at(2).models == QStringList{QStringLiteral("kowalska")});
        CHECK(anna.allForms().contains(text("Wolską")));
        CHECK_FALSE(anna.allForms().contains(QStringLiteral("Wolskich")));
    }

    SECTION("a place, or a name of one word, gets only itself") {
        CHECK(makeName(QStringLiteral("Czarny Las"), NameKind::Place, polish).variants.size() == 1);
        CHECK(makeName(QStringLiteral("Eldrion"), NameKind::Character, polish).variants.size() ==
              1);
        CHECK(makeName(QStringLiteral("  "), NameKind::Character, polish).variants.isEmpty());
        CHECK(DictionaryName().text().isEmpty());
    }

    SECTION("the cells the author wrote keep their forms when the model changes") {
        NameVariant variant =
            makeName(QStringLiteral("Ruth"), NameKind::Character, polish).variants.front();
        CHECK(variant.models == QStringList{QStringLiteral("jan")});
        variant.forms.cell(GrammaticalCase::Vocative, false) = QStringList{QStringLiteral("Ruth!")};
        variant.edited = QStringList{QStringLiteral("vocative")};
        variant.models = QStringList{NameDeclension::unchanged()};
        refreshForms(variant, NameKind::Character, polish);
        CHECK(variant.forms.cell(GrammaticalCase::Genitive, false) ==
              QStringList{QStringLiteral("Ruth")});
        CHECK(variant.forms.cell(GrammaticalCase::Vocative, false) ==
              QStringList{QStringLiteral("Ruth!")});

        variant.text = QStringLiteral("Ruth Wolska");  // a word more: the models are picked anew
        refreshForms(variant, NameKind::Character, polish);
        CHECK(variant.models == QStringList{QStringLiteral("none"), QStringLiteral("kowalska")});
    }
}

TEST_CASE("BookDictionary: the wrong spellings a rule usually has", "[core][dictionary][book]") {
    const auto isWord = [](const QString& word) {
        return word == text("pół") || word == QStringLiteral("elf") ||
               word == QStringLiteral("ok") || word == QStringLiteral("ej");
    };
    CHECK(suggestedWrongSpellings(QStringLiteral("Smok")) == QStringList{QStringLiteral("smok")});
    CHECK(suggestedWrongSpellings(QStringLiteral("NATO")).isEmpty());
    CHECK(suggestedWrongSpellings(QStringLiteral("e-mail")) ==
          QStringList{QStringLiteral("email"), QStringLiteral("e mail")});
    CHECK(suggestedWrongSpellings(QStringLiteral("na pewno")) ==
          QStringList{QStringLiteral("napewno"), QStringLiteral("na-pewno")});
    CHECK(suggestedWrongSpellings(text("półelf"), isWord) ==
          QStringList{text("pół-elf"), text("pół elf")});
    CHECK(suggestedWrongSpellings(text("półelf")).isEmpty());
    CHECK(suggestedWrongSpellings(QStringLiteral("okej"), isWord).isEmpty());
    CHECK(suggestedWrongSpellings(QStringLiteral(" ")).isEmpty());
    CHECK(suggestedWrongSpellings(QStringLiteral("Wielka-Brytania")) ==
          QStringList{QStringLiteral("wielka-Brytania"), QStringLiteral("WielkaBrytania"),
                      QStringLiteral("Wielka Brytania")});
}

TEST_CASE("BookDictionary: the forms of a rule", "[core][dictionary][book]") {
    SECTION("without forms: each wrong spelling with the right one") {
        SpellingRule okej = rule("okej", {"OK", "okey", "okej", ""}, false);
        fillRuleForms(okej, &polishDictionary());
        CHECK(formsOf(okej) == "OK>okej, okey>okej");
        CHECK(okej.pattern.isEmpty());
    }

    SECTION("the forms of the right spelling, made the same way") {
        SpellingRule smok = rule("Smok", {"smok"});
        fillRuleForms(smok, &polishDictionary());
        CHECK(smok.forms.value(QStringLiteral("smoka")) == "Smoka");
        CHECK(smok.forms.value(QStringLiteral("smokiem")) == "Smokiem");
        CHECK(smok.forms.value(text("smoków")) == text("Smoków"));
        CHECK(smok.forms.value(QStringLiteral("smok")) == "Smok");
        CHECK(smok.pattern.isEmpty());
    }

    SECTION("the forms of a wrong spelling the dictionary knows") {
        SpellingRule email = rule("e-mail", {"email", "e mail", "mail"});
        fillRuleForms(email, &polishDictionary());
        CHECK(email.forms.value(QStringLiteral("emaila")) == "e-maila");
        CHECK(email.forms.value(QStringLiteral("e mailem")) == "e-mailem");
        CHECK(email.forms.value(QStringLiteral("mailu")) == "e-mailu");
        CHECK(email.pattern.isEmpty());
    }

    SECTION("a word the dictionary does not know: the forms of the word it ends with") {
        SpellingRule halfElf = rule("półelf", {"pół-elf", "pół elf"});
        fillRuleForms(halfElf, &polishDictionary());
        CHECK(halfElf.pattern == "elf");
        CHECK(halfElf.forms.value(text("pół elfem")) == text("półelfem"));
        CHECK(halfElf.forms.value(text("pół-elfie")) == text("półelfie"));

        SpellingRule email = rule("e-mail", {"email"});
        fillRuleForms(email, &polishDictionary());
        CHECK(email.pattern == "mail");
        CHECK(email.forms.value(QStringLiteral("emailem")) == "e-mailem");
    }

    SECTION("a word whose end differs from its pattern's has only its own spellings") {
        SpellingRule invented = rule("Qwxz", {"qwxz"});
        fillRuleForms(invented, &polishDictionary());
        CHECK(formsOf(invented) == "qwxz>Qwxz");
        SpellingRule empty;
        fillRuleForms(empty, &polishDictionary());
        CHECK(empty.forms.isEmpty());
        SpellingRule noDictionary = rule("Smok", {"smok"});
        const AffixExpander notLoaded;
        fillRuleForms(noDictionary, &notLoaded);
        CHECK(formsOf(noDictionary) == "smok>Smok");
    }

    SECTION("a change of the stem: the same characters are dropped from the spellings") {
        const QTemporaryDir folder;
        REQUIRE(folder.isValid());
        QFile affix(folder.filePath(QStringLiteral("t.aff")));
        REQUIRE(affix.open(QIODevice::WriteOnly));
        affix.write("SET UTF-8\nSFX A Y 1\nSFX A óg oga óg\n");
        affix.close();
        QFile words(folder.filePath(QStringLiteral("t.dic")));
        REQUIRE(words.open(QIODevice::WriteOnly));
        words.write("1\nbóg/A\n");
        words.close();
        AffixExpander expander;
        REQUIRE(expander.load(affix.fileName(), words.fileName()));
        SpellingRule god = rule("Bóg", {"bóg", "Bog"});
        fillRuleForms(god, &expander);
        CHECK(formsOf(god) == text("Bog>Bóg, boga>Boga, bóg>Bóg").toStdString());
    }
}
