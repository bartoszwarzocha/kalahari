/// @file test_text_diff.cpp
/// @brief Differences between two texts, paragraph by paragraph and word by word

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>
#include <kalahari/core/text_diff.h>

#include <algorithm>
#include <string>

using namespace kalahari::core;

namespace Catch {
template <>
struct StringMaker<DiffChange> {
    static std::string convert(const DiffChange& change) {
        return "{" + std::to_string(change.paragraph) + ", " + std::to_string(change.piece) + "}";
    }
};
}  // namespace Catch

namespace {

/// The pieces of @p paragraph: same as they are, deleted in [-…], added in {+…}
std::string marked(const DiffParagraph& paragraph) {
    QString text;
    for (const DiffPiece& piece : paragraph.pieces) {
        switch (piece.kind) {
        case DiffPiece::Kind::Same:
            text += piece.text;
            break;
        case DiffPiece::Kind::Deleted:
            text += QStringLiteral("[-") + piece.text + QLatin1Char(']');
            break;
        case DiffPiece::Kind::Added:
            text += QStringLiteral("{+") + piece.text + QLatin1Char('}');
            break;
        }
    }
    return text.toStdString();
}

/// All paragraphs of @p diff, marked, one a line
std::string marked(const TextDiff& diff) {
    std::string text;
    for (const DiffParagraph& paragraph : diff.paragraphs) {
        text += marked(paragraph) + "\n";
    }
    return text;
}

/// The text that the pieces of @p diff other than @p left out make
QStringList textOf(const TextDiff& diff, DiffPiece::Kind leftOut) {
    QStringList paragraphs;
    for (const DiffParagraph& paragraph : diff.paragraphs) {
        if ((leftOut == DiffPiece::Kind::Added ? paragraph.older : paragraph.newer) < 0) {
            continue;
        }
        QString text;
        for (const DiffPiece& piece : paragraph.pieces) {
            if (piece.kind != leftOut) {
                text += piece.text;
            }
        }
        paragraphs << text;
    }
    return paragraphs;
}

/// Both texts come back from the comparison as they were
void checkBothTexts(const QStringList& older, const QStringList& newer) {
    const TextDiff diff = diffTexts(older, newer);
    CHECK(textOf(diff, DiffPiece::Kind::Added) == older);
    CHECK(textOf(diff, DiffPiece::Kind::Deleted) == newer);
}

}  // namespace

TEST_CASE("The same texts have no changes", "[core][text_diff]") {
    const QStringList text{"Burza", "Burza przyszła od zachodu.", "", "Tau milczał."};
    const TextDiff diff = diffTexts(text, text);
    CHECK(diff.isSame());
    CHECK(diff.deletedWords == 0);
    CHECK(diff.addedWords == 0);
    REQUIRE(diff.paragraphs.size() == 4);
    CHECK(marked(diff) == "Burza\nBurza przyszła od zachodu.\n\nTau milczał.\n");
    for (int i = 0; i < 4; ++i) {
        CHECK(diff.paragraphs.at(i).isSame());
        CHECK(diff.paragraphs.at(i).older == i);
        CHECK(diff.paragraphs.at(i).newer == i);
    }
    CHECK(diffTexts({}, {}).paragraphs.isEmpty());
}

TEST_CASE("A word replaced by others reads as one change", "[core][text_diff]") {
    const TextDiff diff = diffTexts(
        {"Niebo pociemniało w ciągu kilku minut, a piasek uniósł się jak ściana."},
        {"Niebo pociemniało w kilka minut, a piasek uniósł się jak ściana."});
    CHECK(marked(diff) ==
          "Niebo pociemniało w [-ciągu kilku]{+kilka} minut, a piasek uniósł się jak ściana.\n");
    CHECK(diff.deletedWords == 2);
    CHECK(diff.addedWords == 1);
    REQUIRE(diff.changes.size() == 1);
    CHECK(diff.changes.first() == DiffChange{0, 1});
    CHECK(diff.paragraphs.first().older == 0);
    CHECK(diff.paragraphs.first().newer == 0);
    CHECK_FALSE(diff.paragraphs.first().isSame());
}

TEST_CASE("Words deleted and added in several places of a paragraph", "[core][text_diff]") {
    const QStringList older{"Tau milczał długo. W końcu zsiadł z wielbłąda."};
    const QStringList newer{
        "Tau zsiadł z wielbłąda i zaczął rozwiązywać juki. Nikt nie odważył się zapytać."};
    const TextDiff diff = diffTexts(older, newer);
    CHECK(marked(diff) == "Tau [-milczał długo. W końcu ]zsiadł z wielbłąda{+ i zaczął "
                          "rozwiązywać juki. Nikt nie odważył się zapytać}.\n");
    CHECK(diff.deletedWords == 4);
    CHECK(diff.addedWords == 9);
    CHECK(diff.changes.size() == 2);
    checkBothTexts(older, newer);
}

TEST_CASE("Paragraphs added, deleted and changed between the same ones", "[core][text_diff]") {
    const QStringList older{
        "Burza",
        "Burza przyszła od zachodu, zanim karawana dotarła do studni.",
        "– Zawracamy – powiedział Tau, nie odwracając głowy.",
        "– A woda? – zapytała Nadia.",
        "Tau milczał długo.",
    };
    const QStringList newer{
        "Burza",
        "Burza przyszła od zachodu, zanim karawana dotarła do studni.",
        "– Zawracamy – rzucił Tau.",
        "Tau milczał długo.",
        "Nikt nie odważył się zapytać o wodę.",
    };
    const TextDiff diff = diffTexts(older, newer);
    CHECK(marked(diff) == "Burza\n"
                          "Burza przyszła od zachodu, zanim karawana dotarła do studni.\n"
                          "– Zawracamy – [-powiedział]{+rzucił} Tau[-, nie odwracając głowy].\n"
                          "[-– A woda? – zapytała Nadia.]\n"
                          "Tau milczał długo.\n"
                          "{+Nikt nie odważył się zapytać o wodę.}\n");
    REQUIRE(diff.paragraphs.size() == 6);
    CHECK(diff.paragraphs.at(2).older == 2);
    CHECK(diff.paragraphs.at(2).newer == 2);
    CHECK(diff.paragraphs.at(3).older == 3);
    CHECK(diff.paragraphs.at(3).newer == -1);
    CHECK(diff.paragraphs.at(4).older == 4);
    CHECK(diff.paragraphs.at(4).newer == 3);
    CHECK(diff.paragraphs.at(5).older == -1);
    CHECK(diff.paragraphs.at(5).newer == 4);
    CHECK(diff.changes == QList<DiffChange>{{2, 1}, {2, 4}, {3, 0}, {5, 0}});
    CHECK(diff.deletedWords == 1 + 3 + 4);
    CHECK(diff.addedWords == 1 + 7);
    checkBothTexts(older, newer);
}

TEST_CASE("Paragraphs that share few words are deleted and added whole", "[core][text_diff]") {
    const QStringList older{"Początek.", "Zupełnie inny akapit o burzy.", "Koniec."};
    const QStringList newer{"Początek.", "Wielbłądy piły wodę w ciszy.", "Koniec."};
    const TextDiff diff = diffTexts(older, newer);
    CHECK(marked(diff) == "Początek.\n"
                          "[-Zupełnie inny akapit o burzy.]\n"
                          "{+Wielbłądy piły wodę w ciszy.}\n"
                          "Koniec.\n");
    // Two paragraphs changed whole next to each other: one change
    CHECK(diff.changes == QList<DiffChange>{{1, 0}});
    checkBothTexts(older, newer);
}

TEST_CASE("A paragraph added among changed ones pairs the others", "[core][text_diff]") {
    const QStringList older{"Karawana szła na wschód przez wydmy.",
                            "Nadia liczyła wielbłądy przed nocą."};
    const QStringList newer{"Karawana szła na wschód przez wysokie wydmy.",
                            "Nowy akapit bez wspólnych słów.",
                            "Nadia liczyła wszystkie wielbłądy przed nocą."};
    const TextDiff diff = diffTexts(older, newer);
    CHECK(marked(diff) == "Karawana szła na wschód przez {+wysokie }wydmy.\n"
                          "{+Nowy akapit bez wspólnych słów.}\n"
                          "Nadia liczyła {+wszystkie }wielbłądy przed nocą.\n");
    checkBothTexts(older, newer);
}

TEST_CASE("Empty paragraphs and whole texts added or deleted", "[core][text_diff]") {
    SECTION("An empty paragraph added") {
        const TextDiff diff = diffTexts({"A.", "B."}, {"A.", "", "B."});
        REQUIRE(diff.paragraphs.size() == 3);
        CHECK(diff.paragraphs.at(1).pieces.isEmpty());
        CHECK(diff.paragraphs.at(1).older == -1);
        CHECK(diff.paragraphs.at(1).newer == 1);
        CHECK(diff.changes == QList<DiffChange>{{1, 0}});
        checkBothTexts({"A.", "B."}, {"A.", "", "B."});
    }

    SECTION("All of a text") {
        const TextDiff added = diffTexts({}, {"Jeden akapit.", "Drugi."});
        CHECK(marked(added) == "{+Jeden akapit.}\n{+Drugi.}\n");
        CHECK(added.addedWords == 3);
        CHECK(added.changes.size() == 1);
        const TextDiff deleted = diffTexts({"Jeden akapit."}, {});
        CHECK(marked(deleted) == "[-Jeden akapit.]\n");
        CHECK(deleted.deletedWords == 2);
    }
}

TEST_CASE("Spaces, punctuation and letters outside the basic plane are pieces of their own",
          "[core][text_diff]") {
    checkBothTexts({"Słowo  z  dwiema spacjami 𝔸 i znak."}, {"Słowo z dwiema spacjami 𝔹 i znak!"});
    const TextDiff diff = diffTexts({"Tak."}, {"Tak!"});
    CHECK(marked(diff) == "Tak[-.]{+!}\n");
    CHECK(diff.deletedWords == 0);
    CHECK(diff.addedWords == 0);
    CHECK(diff.changes.size() == 1);
}

TEST_CASE("A long text is compared in reasonable time", "[core][text_diff]") {
    // 3000 paragraphs of 60 words: the comparison takes the parts between the same paragraphs
    QStringList older;
    for (int i = 0; i < 3000; ++i) {
        QString paragraph;
        for (int w = 0; w < 60; ++w) {
            paragraph += QStringLiteral("słowo%1_%2 ").arg(i).arg(w);
        }
        older << paragraph.trimmed();
    }
    QStringList newer = older;
    newer[10].replace(QStringLiteral("słowo10_5 "), QStringLiteral("zmienione "));
    newer.removeAt(1500);
    newer.insert(2500, QStringLiteral("Nowy akapit."));
    std::reverse(newer.begin() + 2800, newer.end());

    const TextDiff diff = diffTexts(older, newer);
    CHECK(diff.addedWords >= 3);
    checkBothTexts(older, newer);
}
