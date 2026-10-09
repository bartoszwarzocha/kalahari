/// @file test_spell_check_service.cpp
/// @brief The spelling dictionary: loading (also in the background), what is checked and
///        the writer's own words

#include <catch2/catch_test_macros.hpp>

#include "kalahari/editor/spell_check_service.h"
#include "editor_test_utils.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <memory>

using namespace kalahari::editor;
using kalahari::test::runEventLoop;
using kalahari::test::waitUntil;

namespace {

/// What a service tells: changes of the words, dictionaries loaded and errors
class ServiceSignals {
public:
    explicit ServiceSignals(const SpellCheckService& service) {
        QObject::connect(&service, &SpellCheckService::wordsChanged, &m_context,
                         [this]() { ++changed; });
        QObject::connect(&service, &SpellCheckService::dictionaryLoaded, &m_context,
                         [this](const QString& dictionary) { loaded.append(dictionary); });
        QObject::connect(&service, &SpellCheckService::dictionaryError, &m_context,
                         [this](const QString& error) { errors.append(error); });
    }

    int changed = 0;
    QStringList loaded;
    QStringList errors;

private:
    QObject m_context;  ///< Gone first: no signal reaches the counts after them
};

/// The misspelled words of a text, as written
QStringList misspelled(const SpellCheckService& service, const QString& text) {
    QStringList words;
    for (const SpellErrorInfo& error : service.checkParagraph(text)) {
        CHECK(text.mid(error.startPos, error.length) == error.word);
        words.append(error.word);
    }
    return words;
}

/// A service checking with the shipped English dictionary
std::unique_ptr<SpellCheckService> english() {
    auto service = std::make_unique<SpellCheckService>();
    REQUIRE(service->loadDictionary(QStringLiteral("en_US")));
    return service;
}

/// The lines of a text file, as UTF-8
QStringList linesOf(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    return lines;
}

}  // namespace

// ============================================================================
// Dictionaries
// ============================================================================

TEST_CASE("SpellCheckService: without a dictionary every word is right", "[editor][spell_check]") {
    SpellCheckService service;
    CHECK(service.isEnabled());
    CHECK_FALSE(service.isDictionaryLoaded());
    CHECK_FALSE(service.isActive());
    CHECK_FALSE(service.isLoading());
    CHECK(service.currentDictionary().isEmpty());

    CHECK(service.isCorrect(QStringLiteral("errrors")));
    CHECK(service.suggestions(QStringLiteral("errrors")).isEmpty());
    CHECK(service.checkParagraph(QStringLiteral("This is a tset with errrors.")).isEmpty());
}

TEST_CASE("SpellCheckService: the shipped dictionaries", "[editor][spell_check]") {
    const QStringList dictionaries = SpellCheckService::availableDictionaries();
    REQUIRE(dictionaries.contains(QStringLiteral("pl_PL")));
    REQUIRE(dictionaries.contains(QStringLiteral("en_US")));

    // A language picks its main dictionary
    CHECK(SpellCheckService::dictionaryFor(QStringLiteral("pl")) == QStringLiteral("pl_PL"));
    CHECK(SpellCheckService::dictionaryFor(QStringLiteral("en")) == QStringLiteral("en_US"));
    CHECK(SpellCheckService::dictionaryFor(QStringLiteral("pl-PL")) == QStringLiteral("pl_PL"));
    CHECK(SpellCheckService::dictionaryFor(QStringLiteral(" en_US ")) == QStringLiteral("en_US"));

    // A language without a dictionary has none
    CHECK(SpellCheckService::dictionaryFor(QStringLiteral("xx")).isEmpty());
    CHECK(SpellCheckService::dictionaryFor(QString()).isEmpty());
}

TEST_CASE("SpellCheckService: a dictionary loaded now", "[editor][spell_check]") {
    SpellCheckService service;
    const ServiceSignals told(service);

    REQUIRE(service.loadDictionary(QStringLiteral("en_US")));
    CHECK(service.isDictionaryLoaded());
    CHECK(service.isActive());
    CHECK(service.currentDictionary() == QStringLiteral("en_US"));
    CHECK(told.loaded == QStringList{"en_US"});
    CHECK(told.changed == 1);

    // A dictionary that is not there: the one loaded before stays
    CHECK_FALSE(service.loadDictionary(QStringLiteral("xx_YY")));
    CHECK(told.errors.size() == 1);
    CHECK(service.currentDictionary() == QStringLiteral("en_US"));
    CHECK(told.changed == 1);
}

TEST_CASE("SpellCheckService: a dictionary loaded in the background", "[editor][spell_check]") {
    SpellCheckService service;
    const ServiceSignals told(service);

    service.loadDictionaryInBackground(QStringLiteral("pl_PL"));
    CHECK(service.isLoading());
    CHECK_FALSE(service.isDictionaryLoaded());
    REQUIRE(waitUntil([&told]() { return !told.loaded.isEmpty(); }, 20000));
    CHECK_FALSE(service.isLoading());
    CHECK(service.currentDictionary() == QStringLiteral("pl_PL"));
    CHECK(told.loaded == QStringList{"pl_PL"});
    CHECK(told.changed == 1);
    CHECK(misspelled(service, QStringLiteral("Ta ksi\u0105\u017cka ma b\u0142\u0105d: "
                                             "ksi\u0105rzka.")) ==
          QStringList{QStringLiteral("ksi\u0105rzka")});

    SECTION("the last dictionary asked for is the one loaded") {
        // The Polish one checks the words until another is ready; asked for again while
        // the English one loads, it stays and the English one is dropped
        service.loadDictionaryInBackground(QStringLiteral("en_US"));
        CHECK(service.currentDictionary() == QStringLiteral("pl_PL"));
        service.loadDictionaryInBackground(QStringLiteral("pl_PL"));
        REQUIRE(waitUntil([&service]() { return !service.isLoading(); }, 20000));
        CHECK(service.currentDictionary() == QStringLiteral("pl_PL"));
        CHECK(told.loaded == QStringList{"pl_PL"});
        CHECK(told.changed == 1);

        service.loadDictionaryInBackground(QStringLiteral("en_US"));
        REQUIRE(waitUntil([&told]() { return told.loaded.size() == 2; }, 20000));
        CHECK(service.currentDictionary() == QStringLiteral("en_US"));
        CHECK(told.changed == 2);
    }

    SECTION("a language without a dictionary checks nothing") {
        service.unloadDictionary();
        CHECK_FALSE(service.isDictionaryLoaded());
        CHECK(service.currentDictionary().isEmpty());
        CHECK(told.changed == 2);
        CHECK(service.checkParagraph(QStringLiteral("ksi\u0105rzka")).isEmpty());

        // The dictionary kept is checked with again at once
        service.loadDictionaryInBackground(QStringLiteral("pl_PL"));
        CHECK_FALSE(service.isLoading());
        CHECK(service.currentDictionary() == QStringLiteral("pl_PL"));
        CHECK(told.changed == 3);
    }
}

TEST_CASE("SpellCheckService: closed while a dictionary loads", "[editor][spell_check]") {
    // The loading thread is waited for: nothing is left behind (sanitizers)
    auto service = std::make_unique<SpellCheckService>();
    service->loadDictionaryInBackground(QStringLiteral("pl_PL"));
    CHECK(service->isLoading());
    service.reset();

    // And a dictionary loaded now drops the one loading
    SpellCheckService other;
    other.loadDictionaryInBackground(QStringLiteral("pl_PL"));
    REQUIRE(other.loadDictionary(QStringLiteral("en_US")));
    CHECK_FALSE(other.isLoading());
    runEventLoop(50);
    CHECK(other.currentDictionary() == QStringLiteral("en_US"));
}

TEST_CASE("SpellCheckService: turning the checking off", "[editor][spell_check]") {
    auto service = english();
    const ServiceSignals told(*service);

    service->setEnabled(false);
    CHECK_FALSE(service->isActive());
    CHECK(service->checkParagraph(QStringLiteral("errrors")).isEmpty());
    CHECK(told.changed == 1);
    service->setEnabled(false);
    CHECK(told.changed == 1);

    service->setEnabled(true);
    CHECK(service->isActive());
    CHECK(misspelled(*service, QStringLiteral("errrors")) == QStringList{"errrors"});
    CHECK(told.changed == 2);
}

// ============================================================================
// What is checked
// ============================================================================

TEST_CASE("SpellCheckService: the words checked", "[editor][spell_check]") {
    auto service = english();

    SECTION("misspelled words where they are") {
        const QString text = QStringLiteral("The chaptre is reddy.");
        const QList<SpellErrorInfo> errors = service->checkParagraph(text);
        REQUIRE(errors.size() == 2);
        CHECK(errors[0] == SpellErrorInfo(4, 7, QStringLiteral("chaptre")));
        CHECK(errors[1] == SpellErrorInfo(15, 5, QStringLiteral("reddy")));
        CHECK(service->suggestions(QStringLiteral("chaptre")).contains("chapter"));
    }

    SECTION("apostrophes, also the typographic one") {
        CHECK(misspelled(*service, QStringLiteral("Don't stop, it isn\u2019t over.")).isEmpty());
        CHECK(misspelled(*service, QStringLiteral("It isn\u2019tt over.")) ==
              QStringList{QStringLiteral("isn\u2019tt")});
    }

    SECTION("a hyphenated word by its wrong parts") {
        CHECK(misspelled(*service, QStringLiteral("A well-known self-aware writer.")).isEmpty());
        const QString text = QStringLiteral("A well-knwon writer.");
        const QList<SpellErrorInfo> errors = service->checkParagraph(text);
        REQUIRE(errors.size() == 1);
        CHECK(errors[0] == SpellErrorInfo(7, 5, QStringLiteral("knwon")));
    }

    SECTION("words left out") {
        // One letter, capitals, words touching digits or underscores, addresses
        CHECK(misspelled(*service, QStringLiteral("x y z q")).isEmpty());
        CHECK(misspelled(*service, QStringLiteral("NATO XYZZY QWRT")).isEmpty());
        CHECK(misspelled(*service, QStringLiteral("abc123 x2y 3rd v1_beta snake_cse")).isEmpty());
        CHECK(misspelled(*service, QStringLiteral("See https://exampel.com/pathh, "
                                                  "www.exampel.org or jhon@exampel.com."))
                  .isEmpty());
        // But not the words around them
        CHECK(misspelled(*service, QStringLiteral("Errror at www.exampel.org")) ==
              QStringList{"Errror"});
    }

    SECTION("Polish") {
        SpellCheckService polish;
        REQUIRE(polish.loadDictionary(QStringLiteral("pl_PL")));
        // Words with Polish letters: the dictionary is UTF-8, like the text
        CHECK(polish.isCorrect(QStringLiteral("ksi\u0105\u017cka")));
        CHECK(polish.isCorrect(QStringLiteral("\u017b\u00f3\u0142w")));
        CHECK_FALSE(polish.isCorrect(QStringLiteral("ksi\u0105rzka")));
        CHECK(polish.suggestions(QStringLiteral("ksi\u0105rzka"))
                  .contains(QStringLiteral("ksi\u0105\u017cka")));
        CHECK(misspelled(polish, QStringLiteral("Bia\u0142o-czerwona flaga, \u017c\u00f3\u0142w i "
                                                "\u017c\u00f3\u0142f.")) ==
              QStringList{QStringLiteral("\u017c\u00f3\u0142f")});
    }
}

// ============================================================================
// The writer's own words
// ============================================================================

TEST_CASE("SpellCheckService: the writer's own words", "[editor][spell_check]") {
    auto service = english();
    const ServiceSignals told(*service);

    SECTION("a word in lower case is right in any case, also with an ending") {
        REQUIRE(misspelled(*service, QStringLiteral("glorptik")) == QStringList{"glorptik"});
        service->addToUserDictionary(QStringLiteral(" glorptik "));
        CHECK(service->isInUserDictionary(QStringLiteral("glorptik")));
        CHECK(told.changed == 1);
        CHECK(misspelled(*service, QStringLiteral("glorptik Glorptik Glorptik's "
                                                  "Glorptik\u2019s")).isEmpty());

        // Added twice: nothing changes
        service->addToUserDictionary(QStringLiteral("glorptik"));
        CHECK(told.changed == 1);
        CHECK(service->userDictionaryWords() == QStringList{"glorptik"});

        service->removeFromUserDictionary(QStringLiteral("glorptik"));
        CHECK(told.changed == 2);
        CHECK(misspelled(*service, QStringLiteral("Glorptik")) == QStringList{"Glorptik"});
    }

    SECTION("a word with capitals is right only with them") {
        REQUIRE(misspelled(*service, QStringLiteral("McGlorp")) == QStringList{"McGlorp"});
        service->addToUserDictionary(QStringLiteral("McGlorp"));
        CHECK(misspelled(*service, QStringLiteral("McGlorp McGlorp's mcglorp")) ==
              QStringList{"mcglorp"});
    }

    SECTION("ignored words, until the application closes") {
        service->ignoreWord(QStringLiteral("Zorblax"));
        CHECK(service->isIgnored(QStringLiteral("Zorblax")));
        CHECK_FALSE(service->isInUserDictionary(QStringLiteral("Zorblax")));
        CHECK(told.changed == 1);
        CHECK(misspelled(*service, QStringLiteral("Zorblax zorblax")) == QStringList{"zorblax"});
        service->ignoreWord(QStringLiteral("Zorblax"));
        CHECK(told.changed == 1);
    }

    SECTION("all the words at once") {
        service->setUserDictionaryWords({QStringLiteral("zorblax"), QStringLiteral(" "),
                                         QStringLiteral("Quuxly")});
        CHECK(service->userDictionaryWords() == QStringList({"Quuxly", "zorblax"}));
        CHECK(told.changed == 1);
        service->setUserDictionaryWords({QStringLiteral("Quuxly"), QStringLiteral("zorblax")});
        CHECK(told.changed == 1);
        service->setUserDictionaryWords({});
        CHECK(service->userDictionaryWords().isEmpty());
        CHECK(told.changed == 2);
    }
}

TEST_CASE("SpellCheckService: the user dictionary is kept in a file", "[editor][spell_check]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    // In a folder that is not there yet
    const QString path = folder.filePath(QStringLiteral("settings/user_dictionary.txt"));

    {
        SpellCheckService service;
        const ServiceSignals told(service);
        service.setUserDictionaryFile(path);
        CHECK(service.userDictionaryFile() == path);
        CHECK(told.changed == 0);

        service.addToUserDictionary(QStringLiteral("\u017b\u00f3\u0142wik"));
        service.addToUserDictionary(QStringLiteral("kalahari"));
        const QStringList lines = linesOf(path);
        REQUIRE(lines.size() == 3);
        CHECK(lines[0].startsWith(QLatin1Char('#')));
        CHECK(lines.mid(1) == QStringList({"kalahari", QStringLiteral("\u017b\u00f3\u0142wik")}));
    }

    // Read by the next session, comments and empty lines left out
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::Append | QIODevice::Text));
        file.write("\n# a comment\n  Quuxly  \n");
    }
    SpellCheckService service;
    const ServiceSignals told(service);
    service.setUserDictionaryFile(path);
    CHECK(told.changed == 1);
    CHECK(service.userDictionaryWords() ==
          QStringList({"Quuxly", "kalahari", QStringLiteral("\u017b\u00f3\u0142wik")}));

    // Without a file the words are kept only while the application runs
    service.setUserDictionaryFile(QString());
    CHECK(service.userDictionaryWords().isEmpty());
    service.addToUserDictionary(QStringLiteral("zorblax"));
    CHECK(linesOf(path).size() == 6);
}

TEST_CASE("SpellErrorInfo", "[editor][spell_check]") {
    const SpellErrorInfo empty;
    CHECK(empty.startPos == 0);
    CHECK(empty.length == 0);
    CHECK(empty.word.isEmpty());

    const SpellErrorInfo info(5, 7, QStringLiteral("misspel"));
    CHECK(info.startPos == 5);
    CHECK(info.length == 7);
    CHECK(info.word == QStringLiteral("misspel"));
    CHECK(info == SpellErrorInfo(5, 7, QStringLiteral("misspel")));
    CHECK_FALSE(info == SpellErrorInfo(6, 7, QStringLiteral("misspel")));
}
