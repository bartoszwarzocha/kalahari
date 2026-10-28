/// @file test_link_roles.cpp
/// @brief The roles of links, the role a new link of a material gets, and the paragraph a link
/// names

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/link_roles.h>
#include <kalahari/core/material_kinds.h>

using namespace kalahari::core;

namespace {

/// A link with paragraph @p paragraph and excerpt @p excerpt
ElementLink linkTo(int paragraph, const QString& excerpt) {
    ElementLink link;
    link.to = QStringLiteral("e1");
    link.paragraph = paragraph;
    link.excerpt = excerpt;
    return link;
}

} // namespace

TEST_CASE("Link roles: the roles and the role of a new link of each kind of materials",
          "[core][links]") {
    CHECK(LinkRoles::all() == QStringList{"quoted", "source", "inspiration", "illustration",
                                          "to_check"});
    CHECK(LinkRoles::suggestedFor(MaterialKinds::QUOTE) == "quoted");
    CHECK(LinkRoles::suggestedFor(MaterialKinds::PICTURE) == "illustration");
    CHECK(LinkRoles::suggestedFor(MaterialKinds::WEB_PAGE) == "source");
    CHECK(LinkRoles::suggestedFor(MaterialKinds::DOCUMENT) == "source");
    CHECK(LinkRoles::suggestedFor(MaterialKinds::PUBLICATION) == "source");
    // The other kinds suggest none
    CHECK(LinkRoles::suggestedFor(MaterialKinds::WORK_NOTE).isEmpty());
    CHECK(LinkRoles::suggestedFor(MaterialKinds::RECORDING).isEmpty());
    CHECK(LinkRoles::suggestedFor(MaterialKinds::OTHER_FILE).isEmpty());
    CHECK(LinkRoles::suggestedFor(QStringLiteral("chapter")).isEmpty());
}

TEST_CASE("Link roles: the excerpt of a paragraph", "[core][links]") {
    CHECK(LinkRoles::excerptOf(QStringLiteral("  Burza   przyszła\tod zachodu. ")) ==
          "Burza przyszła od zachodu.");
    CHECK(LinkRoles::excerptOf(
              QStringLiteral("One two three four five six seven eight nine ten")) ==
          "One two three four five six seven eight");
    CHECK(LinkRoles::excerptOf(QString()).isEmpty());
}

TEST_CASE("Link roles: the paragraph a link names follows the text", "[core][links]") {
    const QStringList text{QStringLiteral("The storm came from the west."),
                           QStringLiteral("- We turn back - said Tau."),
                           QStringLiteral("- And the water? - asked Nadia."),
                           QStringLiteral("Tau was silent for a long time.")};

    SECTION("a link without a paragraph names none") {
        CHECK(LinkRoles::paragraphOf(text, linkTo(0, QString())) == 0);
        CHECK(LinkRoles::paragraphOf(text, linkTo(0, QStringLiteral("Tau was"))) == 0);
    }

    SECTION("the paragraph of its number while it starts with the excerpt") {
        CHECK(LinkRoles::paragraphOf(text, linkTo(3, LinkRoles::excerptOf(text.at(2)))) == 3);
        // Without an excerpt, the number alone
        CHECK(LinkRoles::paragraphOf(text, linkTo(2, QString())) == 2);
    }

    SECTION("a paragraph that moved is found by its excerpt") {
        QStringList moved = text;
        moved.prepend(QStringLiteral("A new first paragraph."));
        moved.insert(2, QString());
        CHECK(LinkRoles::paragraphOf(moved, linkTo(3, LinkRoles::excerptOf(text.at(2)))) == 5);
        QStringList fewer = text;
        fewer.removeFirst();
        CHECK(LinkRoles::paragraphOf(fewer, linkTo(4, LinkRoles::excerptOf(text.at(3)))) == 3);
    }

    SECTION("of two paragraphs with the excerpt, the one nearer to its number") {
        const QStringList twice{QStringLiteral("Again."), QStringLiteral("Other."),
                                QStringLiteral("Other."), QStringLiteral("Again."),
                                QStringLiteral("Other.")};
        CHECK(LinkRoles::paragraphOf(twice, linkTo(5, QStringLiteral("Again."))) == 4);
        CHECK(LinkRoles::paragraphOf(twice, linkTo(2, QStringLiteral("Again."))) == 1);
    }

    SECTION("a rewritten paragraph keeps its number; a text too short names none") {
        CHECK(LinkRoles::paragraphOf(text, linkTo(2, QStringLiteral("Not in the text"))) == 2);
        CHECK(LinkRoles::paragraphOf(text, linkTo(9, QStringLiteral("Not in the text"))) == 0);
        CHECK(LinkRoles::paragraphOf({}, linkTo(1, QString())) == 0);
    }
}
