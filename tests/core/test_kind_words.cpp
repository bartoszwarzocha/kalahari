/// @file test_kind_words.cpp
/// @brief Names of kinds in the program's sentences: the forms of the packages, the markers of
/// the sentences and of their translations

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/kind_words.h>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <string>

using namespace kalahari::core;

namespace {

KindWords words(Gender gender, std::initializer_list<std::pair<const char*, const char*>> forms) {
    KindWords result;
    result.gender = gender;
    for (const auto& [form, text] : forms) {
        result.forms.insert(QString::fromLatin1(form), QString::fromUtf8(text));
    }
    return result;
}

KindWords story() {
    return words(Gender::Neuter, {{"singular", "opowiadanie"},
                                  {"plural", "opowiadania"},
                                  {"genitive", "opowiadania"},
                                  {"accusative", "opowiadanie"},
                                  {"locative", "opowiadaniu"},
                                  {"genitivePlural", "opowiadań"}});
}

KindWords part() {
    return words(Gender::Feminine, {{"singular", "część"},
                                    {"plural", "części"},
                                    {"genitive", "części"},
                                    {"accusative", "część"}});
}

KindWords acknowledgments() {
    return words(Gender::Plural, {{"singular", "podziękowania"},
                                  {"plural", "podziękowania"},
                                  {"accusative", "podziękowania"}});
}

std::string filled(const char* text, const QHash<QString, KindWords>& nouns) {
    return fillWords(QString::fromUtf8(text), nouns).toStdString();
}

std::string problemsOf(const char* text, QStringList* nouns = nullptr) {
    return wordMarkerProblems(QString::fromUtf8(text), nouns)
        .join(QStringLiteral("; "))
        .toStdString();
}

QString builtInDirectory() {
    return QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes");
}

const ElementKind& builtInKind(const BookTypeRegistry& registry, const char* package,
                               const char* kind) {
    const KindRef found =
        registry.findKind(QString::fromLatin1(package), QString::fromLatin1(kind));
    REQUIRE(found);
    return *found.kind;
}

}  // namespace

// =============================================================================
// Markers
// =============================================================================

TEST_CASE("Kind words: a marker takes the form of the name it asks for", "[core][kindwords]") {
    const QHash<QString, KindWords> nouns{{QStringLiteral("kind"), story()}};

    CHECK(filled("Dodaj {kind:accusative}", nouns) == "Dodaj opowiadanie");
    CHECK(filled("Tytuł {kind:genitive}", nouns) == "Tytuł opowiadania");
    CHECK(filled("Liczba {kind:genitivePlural} w książce", nouns) == "Liczba opowiadań w książce");
    CHECK(filled("{kind}", nouns) == "opowiadanie");

    // A capital letter in the marker: a capital letter in the sentence
    CHECK(filled("{Kind:plural}: 12", nouns) == "Opowiadania: 12");
    CHECK(filled("{Kind}", nouns) == "Opowiadanie");

    // The rest of the sentence stays as it is: numbered arguments, quotation marks
    CHECK(filled("Dodano {kind:accusative}: %1", nouns) == "Dodano opowiadanie: %1");
    CHECK(filled("„%1” – {kind}", nouns) == "„%1” – opowiadanie");
}

TEST_CASE("Kind words: the words that agree take the gender or the number of the name",
          "[core][kindwords]") {
    const char* added = "{kind:m=nowy|f=nowa|n=nowe|p=nowe} {kind} "
                        "{kind:m=zostanie dodany|f=zostanie dodana|n=zostanie dodane|"
                        "p=zostaną dodane}";
    CHECK(filled(added, {{QStringLiteral("kind"), story()}}) ==
          "nowe opowiadanie zostanie dodane");
    CHECK(filled(added, {{QStringLiteral("kind"), part()}}) == "nowa część zostanie dodana");
    CHECK(filled(added, {{QStringLiteral("kind"), acknowledgments()}}) ==
          "nowe podziękowania zostaną dodane");
    KindWords chapter = words(Gender::Masculine, {{"singular", "rozdział"}});
    CHECK(filled(added, {{QStringLiteral("kind"), chapter}}) ==
          "nowy rozdział zostanie dodany");

    // A language whose words agree only in number
    const char* english = "The {kind} {kind:s=is|p=are} added";
    KindWords stories = words(Gender::Masculine, {{"singular", "story"}});
    KindWords thanks = words(Gender::Plural, {{"singular", "acknowledgments"}});
    CHECK(filled(english, {{QStringLiteral("kind"), stories}}) == "The story is added");
    CHECK(filled(english, {{QStringLiteral("kind"), thanks}}) ==
          "The acknowledgments are added");

    // A capital letter takes the first letter of the chosen text
    CHECK(filled("{Kind:m=ten|f=ta|n=to|p=te}", {{QStringLiteral("kind"), part()}}) == "Ta");
}

TEST_CASE("Kind words: a sentence with more nouns", "[core][kindwords]") {
    const QHash<QString, KindWords> nouns{{QStringLiteral("kind"), story()},
                                          {QStringLiteral("group"), part()}};
    CHECK(filled("{Kind} {kind:m=zostanie dodany|f=zostanie dodana|n=zostanie dodane|"
                 "p=zostaną dodane} na końcu {group:genitive} „%1”",
                 nouns) == "Opowiadanie zostanie dodane na końcu części „%1”");
}

TEST_CASE("Kind words: a form that is not given comes from the singular or the plural",
          "[core][kindwords]") {
    const KindWords given = words(Gender::Masculine, {{"singular", "act"}, {"plural", "acts"}});
    CHECK(given.form(QStringLiteral("genitive")) == "act");
    CHECK(given.form(QStringLiteral("indefinite")) == "act");
    CHECK(given.form(QStringLiteral("genitivePlural")) == "acts");
    CHECK(given.form(QStringLiteral("locativePlural")) == "acts");
    CHECK(given.form(QStringLiteral("vocative")).isEmpty());

    const KindWords none;
    CHECK(none.form(QStringLiteral("singular")).isEmpty());
    CHECK(none.form(QStringLiteral("genitive")).isEmpty());
}

TEST_CASE("Kind words: a marker that cannot be filled stays as it is", "[core][kindwords]") {
    const QHash<QString, KindWords> nouns{{QStringLiteral("kind"), story()}};

    // A noun the code does not give, a form that is not one, a choice without the gender
    CHECK(filled("Na końcu {group:genitive}", nouns) == "Na końcu {group:genitive}");
    CHECK(filled("{kind:vocative}", nouns) == "{kind:vocative}");
    CHECK(filled("{kind:m=nowy|f=nowa}", nouns) == "{kind:m=nowy|f=nowa}");

    // Braces that are not a marker
    CHECK(filled("{ kind } i {kind}", nouns) == "{ kind } i opowiadanie");
    CHECK(filled("} {kind} {", nouns) == "} opowiadanie {");
    CHECK(filled("{{kind}}", nouns) == "{opowiadanie}");
    CHECK(filled("Bez znaczników", nouns) == "Bez znaczników");
}

TEST_CASE("Kind words: the problems of markers", "[core][kindwords]") {
    QStringList nouns;
    CHECK(problemsOf("Dodaj {kind:accusative} na końcu {group:genitive} – {Kind} {group}",
                     &nouns)
              .empty());
    CHECK(nouns == QStringList{QStringLiteral("kind"), QStringLiteral("group")});

    CHECK(problemsOf("{kind:m=a|f=b|n=c|p=d} {kind:s=is|p=are}").empty());
    CHECK(problemsOf("Bez znaczników, 100% i „%1”").empty());

    CHECK(problemsOf("{kind:vocative}") == "'{kind:vocative}': 'vocative' is not a form");
    CHECK(problemsOf("{kind:m=a|f=b}") ==
          "'{kind:m=a|f=b}' needs the text of each gender, m=...|f=...|n=...|p=..., "
          "or of each number, s=...|p=...");
    CHECK(problemsOf("{kind:s=is|p=are|x=y}").find("needs the text") != std::string::npos);
    CHECK(problemsOf("{kind:m=a|f=b|n=c|p=d|}").find("needs the text") != std::string::npos);
    CHECK(problemsOf("{ kind}") == "'{ kind}' does not name a noun");
    CHECK(problemsOf("{rodzaj_1}") == "'{rodzaj_1}' does not name a noun");
    CHECK(problemsOf("a } b") == "'}' without '{'");
    CHECK(problemsOf("a { b") == "'{' without '}'");
    CHECK(problemsOf("{ a {kind}") == "'{' without '}'");
}

// =============================================================================
// The forms of a kind in a package
// =============================================================================

TEST_CASE("Kind words: the forms of a kind in a language", "[core][kindwords]") {
    ElementKind kind;
    kind.name.values = {{QStringLiteral("en"), QStringLiteral("Story")},
                        {QStringLiteral("pl"), QStringLiteral("Opowiadanie")},
                        {QStringLiteral("de"), QStringLiteral("Erzählung")}};
    kind.plural.values = {{QStringLiteral("en"), QStringLiteral("Stories")},
                          {QStringLiteral("pl"), QStringLiteral("Opowiadania")},
                          {QStringLiteral("de"), QStringLiteral("Erzählungen")}};
    kind.words.insert(QStringLiteral("pl"), story());

    // The language alone for a language with its country
    const KindWords polish = kind.wordsIn(QStringLiteral("pl_PL"));
    CHECK(polish.gender == Gender::Neuter);
    CHECK(polish.form(QStringLiteral("genitivePlural")) == "opowiadań");

    // Without words: the names with a small letter, a masculine name
    const KindWords german = kind.wordsIn(QStringLiteral("de"));
    CHECK(german.gender == Gender::Masculine);
    CHECK(german.form(QStringLiteral("singular")) == "erzählung");
    CHECK(german.form(QStringLiteral("plural")) == "erzählungen");
    CHECK(german.form(QStringLiteral("genitive")) == "erzählung");

    // A language without names: the English ones
    CHECK(kind.wordsIn(QStringLiteral("fr")).form(QStringLiteral("plural")) == "stories");

    // A name that starts with capitals keeps them
    ElementKind episode;
    episode.name.values = {{QStringLiteral("en"), QStringLiteral("TV episode")}};
    episode.plural.values = {{QStringLiteral("en"), QStringLiteral("TV episodes")}};
    CHECK(episode.wordsIn(QStringLiteral("en")).form(QStringLiteral("singular")) ==
          "TV episode");
}

TEST_CASE("Built-in book types: every kind has the forms of its name in Polish and English",
          "[core][kindwords][booktypes]") {
    BookTypeRegistry registry;
    registry.load({builtInDirectory()});
    REQUIRE(registry.problems().isEmpty());

    const QStringList polishForms{QStringLiteral("singular"),     QStringLiteral("plural"),
                                  QStringLiteral("genitive"),     QStringLiteral("dative"),
                                  QStringLiteral("accusative"),   QStringLiteral("instrumental"),
                                  QStringLiteral("locative"),     QStringLiteral("genitivePlural")};
    const QStringList englishForms{QStringLiteral("singular"), QStringLiteral("plural"),
                                   QStringLiteral("indefinite")};
    for (const BookTypePackage* package : registry.packages()) {
        // The gender is in the package: a kind without it would be masculine
        QFile file(QDir(builtInDirectory())
                       .filePath(package->id + QStringLiteral("/booktype.json")));
        REQUIRE(file.open(QIODevice::ReadOnly));
        const QJsonObject kinds = QJsonDocument::fromJson(file.readAll())
                                      .object()
                                      .value(QStringLiteral("kinds"))
                                      .toObject();
        for (const ElementKind& kind : package->kinds) {
            INFO((package->id + QLatin1Char(':') + kind.id).toStdString());
            const QJsonObject polishJson = kinds.value(kind.id)
                                               .toObject()
                                               .value(QStringLiteral("words"))
                                               .toObject()
                                               .value(QStringLiteral("pl"))
                                               .toObject();
            CHECK(polishJson.value(QStringLiteral("gender")).isString());
            const KindWords polish = kind.words.value(QStringLiteral("pl"));
            for (const QString& form : polishForms) {
                INFO(form.toStdString());
                CHECK_FALSE(polish.forms.value(form).isEmpty());
            }
            const KindWords english = kind.words.value(QStringLiteral("en"));
            for (const QString& form : englishForms) {
                INFO(form.toStdString());
                CHECK_FALSE(english.forms.value(form).isEmpty());
            }
        }
    }
}

TEST_CASE("Built-in book types: the names of kinds in the program's sentences",
          "[core][kindwords][booktypes]") {
    BookTypeRegistry registry;
    registry.load({builtInDirectory()});
    REQUIRE(registry.problems().isEmpty());

    const auto in = [&registry](const char* package, const char* kind, const char* language) {
        return builtInKind(registry, package, kind).wordsIn(QString::fromLatin1(language));
    };
    const auto fill = [](const char* text, const KindWords& kind) {
        return fillWords(QString::fromUtf8(text), {{QStringLiteral("kind"), kind}}).toStdString();
    };

    CHECK(fill("Dodaj {kind:accusative}", in("kalahari.short_stories", "story", "pl")) ==
          "Dodaj opowiadanie");
    CHECK(fill("Dodaj {kind:accusative}", in("kalahari.base", "part", "pl")) == "Dodaj część");
    CHECK(fill("Dodaj {kind:accusative}", in("kalahari.poetry", "poem", "pl")) == "Dodaj wiersz");
    CHECK(fill("Dodaj {kind:accusative}", in("kalahari.screenplay", "act", "pl")) == "Dodaj akt");
    CHECK(fill("{Kind:plural}: 12", in("kalahari.short_stories", "story", "pl")) ==
          "Opowiadania: 12");
    CHECK(fill("Liczba {kind:genitivePlural}", in("kalahari.poetry", "poem", "pl")) ==
          "Liczba wierszy");
    CHECK(fill("na końcu {kind:genitive}", in("kalahari.short_stories", "section", "pl")) ==
          "na końcu działu");
    CHECK(fill("{kind:m=nowy|f=nowa|n=nowe|p=nowe} {kind}", in("kalahari.poetry", "cycle", "pl")) ==
          "nowy cykl");

    // Book > New Chapter... names the main texts of each type
    const char* newText = "{kind:m=Nowy|f=Nowa|n=Nowe|p=Nowe} {kind}...";
    CHECK(fill(newText, in("kalahari.base", "chapter", "pl")) == "Nowy rozdział...");
    CHECK(fill(newText, in("kalahari.short_stories", "story", "pl")) == "Nowe opowiadanie...");
    CHECK(fill(newText, in("kalahari.poetry", "poem", "pl")) == "Nowy wiersz...");
    CHECK(fill(newText, in("kalahari.screenplay", "act", "pl")) == "Nowy akt...");
    CHECK(fill("New {Kind}...", in("kalahari.short_stories", "story", "en")) == "New Story...");

    CHECK(fill("Add {Kind}", in("kalahari.short_stories", "story", "en")) == "Add Story");
    CHECK(fill("Add {Kind}", in("kalahari.short_stories", "section", "en")) == "Add Division");
    CHECK(fill("Open {kind:indefinite}", in("kalahari.screenplay", "act", "en")) ==
          "Open an act");
    CHECK(fill("The {kind} {kind:s=is|p=are} added",
               in("kalahari.base", "acknowledgments", "en")) == "The acknowledgments are added");
}

// =============================================================================
// Translations
// =============================================================================

TEST_CASE("Translations: the markers of the names of kinds", "[core][kindwords][translations]") {
    QFile file(QStringLiteral(KALAHARI_SOURCE_DIR "/translations/kalahari_pl.ts"));
    REQUIRE(file.open(QIODevice::ReadOnly));

    QXmlStreamReader reader(&file);
    QString context;
    QString source;
    int sentences = 0;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }
        if (reader.name() == QLatin1String("name")) {
            context = reader.readElementText();
        } else if (reader.name() == QLatin1String("source")) {
            source = reader.readElementText();
        } else if (reader.name() == QLatin1String("translation")) {
            const QString translation =
                reader.readElementText(QXmlStreamReader::IncludeChildElements);
            if (!source.contains(QLatin1Char('{')) && !translation.contains(QLatin1Char('{'))) {
                continue;
            }
            ++sentences;
            INFO((context + QStringLiteral(": ") + source).toStdString());
            INFO(translation.toStdString());

            // The markers are right in the sentence and in its translation, and the
            // translation names only the nouns the code gives for the sentence
            QStringList sourceNouns;
            CHECK(wordMarkerProblems(source, &sourceNouns).isEmpty());
            QStringList translationNouns;
            CHECK(wordMarkerProblems(translation, &translationNouns).isEmpty());
            for (const QString& noun : translationNouns) {
                INFO(noun.toStdString());
                CHECK(sourceNouns.contains(noun));
            }
        }
    }
    REQUIRE_FALSE(reader.hasError());
    CHECK(sentences > 0);
}
