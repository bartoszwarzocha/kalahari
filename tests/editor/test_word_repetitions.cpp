/// @file test_word_repetitions.cpp
/// @brief The repetitions of words: those too close, the most frequent, the base forms

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/word_repetitions.h>

#include <algorithm>

using namespace kalahari::editor;

namespace {

RepetitionOptions polish() {
    RepetitionOptions options;
    options.language = QStringLiteral("pl");
    return options;
}

/// The base forms a spelling dictionary gives for the words of the tests
QString baseFormOf(const QString& word) {
    const QString lower = word.toLower();
    if (lower == QStringLiteral("powiedziała") || lower == QStringLiteral("powiedział")) {
        return QStringLiteral("powiedzieć");
    }
    return {};
}

}  // namespace

TEST_CASE("Repetitions: a word that comes back within the distance", "[editor][repetitions]") {
    const QStringList chapter = {
        QStringLiteral("Anna weszła do kuchni i spojrzała na stół. Spojrzała na kopertę jeszcze "
                       "raz, potem spojrzała w okno."),
        QStringLiteral("Za oknem padał deszcz.")};
    const RepetitionAnalysis analysis = analyzeRepetitions({chapter}, polish());

    REQUIRE(analysis.clusters.size() == 1);
    const RepetitionCluster& cluster = analysis.clusters.first();
    CHECK(cluster.key == QStringLiteral("spojrzała"));
    CHECK(cluster.forms == QStringList{QStringLiteral("spojrzała")});
    REQUIRE(cluster.words.size() == 3);
    // Where its words are: the first "spojrzała" of the first paragraph
    const TextWord& first = analysis.words.at(cluster.words.first());
    CHECK(first.paragraph == 0);
    CHECK(chapter.first().mid(first.start, first.length) == QStringLiteral("spojrzała"));
    // From the 6th word to the 15th
    CHECK(cluster.span == 10);
    CHECK(analysis.totalWords == 21);
}

TEST_CASE("Repetitions: farther than the distance is not a repetition", "[editor][repetitions]") {
    RepetitionOptions options = polish();
    options.distance = 3;
    const RepetitionAnalysis analysis = analyzeRepetitions(
        {{QStringLiteral("deszcz padał nad miastem, deszcz"),
          QStringLiteral("deszcz deszcz")}},
        options);
    // deszcz(0) ... deszcz(4): 4 words apart, more than 3; the next ones are within 3
    REQUIRE(analysis.clusters.size() == 1);
    CHECK(analysis.clusters.first().words.size() == 3);
    CHECK(analysis.clusters.first().span == 3);
}

TEST_CASE("Repetitions: words of different texts are never too close", "[editor][repetitions]") {
    const RepetitionAnalysis analysis = analyzeRepetitions(
        {{QStringLiteral("Koniec rozdziału: deszcz")}, {QStringLiteral("deszcz na początku")}},
        polish());
    CHECK(analysis.clusters.isEmpty());
    REQUIRE_FALSE(analysis.counts.isEmpty());
    CHECK(analysis.counts.first().key == QStringLiteral("deszcz"));
    CHECK(analysis.counts.first().count == 2);
    CHECK(analysis.words.at(analysis.counts.first().words.at(1)).text == 1);
}

TEST_CASE("Repetitions: short and common words do not count, and the writer's skipped ones",
          "[editor][repetitions]") {
    RepetitionOptions options = polish();
    const QStringList text = {QStringLiteral("Nie wiem, czy to się uda. Nie wiem, czy ona się zgodzi. "
                                             "Ona mówi, że kot i kot.")};
    RepetitionAnalysis analysis = analyzeRepetitions({text}, options);
    QStringList keys;
    for (const RepetitionCluster& cluster : analysis.clusters) {
        keys.append(cluster.key);
    }
    // "nie", "czy", "się", "ona" are common, "to" and "i" too short
    CHECK(keys == QStringList{QStringLiteral("wiem"), QStringLiteral("kot")});

    options.skipped.insert(QStringLiteral("wiem"));
    analysis = analyzeRepetitions({text}, options);
    REQUIRE(analysis.clusters.size() == 1);
    CHECK(analysis.clusters.first().key == QStringLiteral("kot"));

    // Without skipping the common words, they repeat too
    options.skipCommon = false;
    analysis = analyzeRepetitions({text}, options);
    CHECK(analysis.clusters.size() > 2);
}

TEST_CASE("Repetitions: the forms of a word count as one by its base form",
          "[editor][repetitions]") {
    const QStringList text = {
        QStringLiteral("– Nie wiem – powiedziała cicho. Jan powiedział coś jeszcze.")};
    RepetitionAnalysis analysis = analyzeRepetitions({text}, polish(), baseFormOf);
    REQUIRE(analysis.clusters.size() == 1);
    CHECK(analysis.clusters.first().key == QStringLiteral("powiedzieć"));
    CHECK(analysis.clusters.first().forms ==
          QStringList{QStringLiteral("powiedziała"), QStringLiteral("powiedział")});

    // Without merging the forms they are two words
    RepetitionOptions options = polish();
    options.mergeForms = false;
    analysis = analyzeRepetitions({text}, options, baseFormOf);
    CHECK(analysis.clusters.isEmpty());
}

TEST_CASE("Repetitions: the most frequent words with their forms", "[editor][repetitions]") {
    const QStringList text = {QStringLiteral("Anna powiedziała. Anna powiedział. Anna list."),
                              QStringLiteral("Powiedziała Anna.")};
    const RepetitionAnalysis analysis = analyzeRepetitions({text}, polish(), baseFormOf);
    REQUIRE(analysis.counts.size() == 3);
    CHECK(analysis.counts.at(0).key == QStringLiteral("anna"));
    CHECK(analysis.counts.at(0).count == 4);
    CHECK(analysis.counts.at(1).key == QStringLiteral("powiedzieć"));
    CHECK(analysis.counts.at(1).count == 3);
    REQUIRE(analysis.counts.at(1).forms.size() == 2);
    CHECK(analysis.counts.at(1).forms.at(0) == std::pair{QStringLiteral("powiedziała"), 2});
    CHECK(analysis.counts.at(1).forms.at(1) == std::pair{QStringLiteral("powiedział"), 1});
    CHECK(analysis.counts.at(2).key == QStringLiteral("list"));
}

TEST_CASE("Repetitions: words of letters, joined by a hyphen or an apostrophe",
          "[editor][repetitions]") {
    RepetitionOptions options;
    options.minLength = 1;
    options.skipCommon = false;
    const RepetitionAnalysis analysis = analyzeRepetitions(
        {{QStringLiteral("e-mail don’t B12 3D biało-czerwony - koniec")}}, options);
    QStringList forms;
    for (const TextWord& word : analysis.words) {
        forms.append(word.form);
    }
    CHECK(forms == QStringList{QStringLiteral("e-mail"), QStringLiteral("don’t"),
                               QStringLiteral("biało-czerwony"), QStringLiteral("koniec")});
}

TEST_CASE("Repetitions: the base form chosen from the stems of the dictionary",
          "[editor][repetitions]") {
    const QString pl = QStringLiteral("pl");
    CHECK(chooseBaseForm(QStringLiteral("powiedziała"), {QStringLiteral("powiedzieć")}, pl) ==
          QStringLiteral("powiedzieć"));
    CHECK(chooseBaseForm(QStringLiteral("była"),
                         {QStringLiteral("była"), QStringLiteral("były"), QStringLiteral("być")},
                         pl) == QStringLiteral("być"));
    CHECK(chooseBaseForm(QStringLiteral("Anny"), {QStringLiteral("Anny"), QStringLiteral("Anna")},
                         pl) == QStringLiteral("anna"));
    // A stem of another word does not join it
    CHECK(chooseBaseForm(QStringLiteral("idzie"), {QStringLiteral("idzie"), QStringLiteral("id")},
                         pl) == QStringLiteral("idzie"));
    CHECK(chooseBaseForm(QStringLiteral("domu"), {}, pl) == QStringLiteral("domu"));

    const QString en = QStringLiteral("en_US");
    CHECK(chooseBaseForm(QStringLiteral("walking"),
                         {QStringLiteral("walking"), QStringLiteral("walk")},
                         en) == QStringLiteral("walk"));
    CHECK(chooseBaseForm(QStringLiteral("says"), {QStringLiteral("say")}, en) ==
          QStringLiteral("say"));
    CHECK(chooseBaseForm(QStringLiteral("went"), {QStringLiteral("went")}, en) ==
          QStringLiteral("went"));
}

TEST_CASE("Repetitions: the common words of Polish and English", "[editor][repetitions]") {
    CHECK(commonWords(QStringLiteral("pl_PL")).contains(QStringLiteral("się")));
    CHECK(commonWords(QStringLiteral("pl")).contains(QStringLiteral("już")));
    CHECK(commonWords(QStringLiteral("en_GB")).contains(QStringLiteral("the")));
    CHECK(commonWords(QStringLiteral("de")).isEmpty());
}

TEST_CASE("Repetitions: the text of a card, its words marked", "[editor][repetitions]") {
    const QStringList chapter = {
        QStringLiteral("Anna weszła do kuchni i spojrzała na stół. Spojrzała na kopertę jeszcze "
                       "raz, potem spojrzała w okno."),
        QStringLiteral("Za oknem padał deszcz.")};
    const RepetitionAnalysis analysis = analyzeRepetitions({chapter}, polish());
    REQUIRE(analysis.clusters.size() == 1);
    QList<TextWord> words;
    for (const int i : analysis.clusters.first().words) {
        words.append(analysis.words.at(i));
    }
    const RepetitionContext context = repetitionContext(chapter, words);
    // The whole paragraph: the words are close, and the text around them short
    CHECK(context.text == chapter.first());
    REQUIRE(context.marks.size() == 3);
    for (const auto& [start, length] : context.marks) {
        CHECK(context.text.mid(start, length).toLower() == QStringLiteral("spojrzała"));
    }

    // Far apart, and in another paragraph: shortened
    const QString filler = QStringLiteral(
        "potem długo nic się nie działo, a wieczór ciągnął się bez końca i bez słowa ");
    const QStringList far = {QStringLiteral("Na początku był deszcz, ") + filler + filler +
                                 QStringLiteral("i znów deszcz nad miastem."),
                             QStringLiteral("Rano deszcz.")};
    RepetitionOptions options = polish();
    options.distance = 100;
    const RepetitionAnalysis farAnalysis = analyzeRepetitions({far}, options);
    // The words of the filler repeat too
    const auto rain = std::find_if(
        farAnalysis.clusters.cbegin(), farAnalysis.clusters.cend(),
        [](const RepetitionCluster& cluster) { return cluster.key == QStringLiteral("deszcz"); });
    REQUIRE(rain != farAnalysis.clusters.cend());
    QList<TextWord> farWords;
    for (const int i : rain->words) {
        farWords.append(farAnalysis.words.at(i));
    }
    const RepetitionContext shortened = repetitionContext(far, farWords);
    REQUIRE(shortened.marks.size() == 3);
    for (const auto& [start, length] : shortened.marks) {
        CHECK(shortened.text.mid(start, length) == QStringLiteral("deszcz"));
    }
    CHECK(shortened.text.contains(QStringLiteral("… …")));
    CHECK(shortened.text.contains(QStringLiteral("i znów deszcz nad miastem. Rano deszcz.")));
    CHECK(shortened.text.startsWith(QStringLiteral("Na początku był deszcz, potem")));
    CHECK(shortened.text.endsWith(QStringLiteral(" Rano deszcz.")));
    CHECK(shortened.text.size() < far.first().size());
}
