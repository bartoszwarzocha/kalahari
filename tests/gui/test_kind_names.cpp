/// @file test_kind_names.cpp
/// @brief The names of kinds in the texts of menu commands, and of types and kinds that the
/// program does not have

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/kind_words.h>
#include <kalahari/core/package_card.h>
#include <kalahari/gui/kind_names.h>

using namespace kalahari;

namespace {

/// A kind whose name has a '&', as an add-on package may name it
core::KindWords questions() {
    core::KindWords words;
    words.gender = core::Gender::Plural;
    words.forms.insert(QStringLiteral("singular"), QStringLiteral("questions & answers"));
    words.forms.insert(QStringLiteral("accusative"), QStringLiteral("questions & answers"));
    return words;
}

} // namespace

TEST_CASE("Kind names: in a menu a '&' of the name is doubled, and the text keeps its access key",
          "[gui][kindwords]") {
    const core::KindWords menu = gui::menuWords(questions());
    CHECK(menu.forms.value(QStringLiteral("singular")) == QStringLiteral("questions && answers"));
    CHECK(menu.forms.value(QStringLiteral("accusative")) ==
          QStringLiteral("questions && answers"));
    CHECK(menu.gender == core::Gender::Plural);

    // The text's own '&' marks the access key, in English and in a choice of a translation
    CHECK(core::fillWords(QStringLiteral("&New {Kind}..."), {{QStringLiteral("kind"), menu}}) ==
          QStringLiteral("&New Questions && answers..."));
    CHECK(core::fillWords(QStringLiteral("{kind:m=&Nowy|f=&Nowa|n=&Nowe|p=&Nowe} {kind}..."),
                          {{QStringLiteral("kind"), menu}}) ==
          QStringLiteral("&Nowe questions && answers..."));

    // Outside a menu the name keeps its '&'
    CHECK(core::fillWords(QStringLiteral("New {Kind}..."),
                          {{QStringLiteral("kind"), questions()}}) ==
          QStringLiteral("New Questions & answers..."));
}

TEST_CASE("Kind names: a type and a kind that the program does not have are named after the "
          "book's card of their package",
          "[gui][kindwords]") {
    core::BookTypeRegistry registry;
    registry.load({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(registry.problems().isEmpty());

    core::BookProject project;
    core::ProjectType saga;
    saga.id = QStringLiteral("acme.saga");
    saga.version = QStringLiteral("2.0");
    project.type = saga;
    core::ProjectElement element;
    element.kind = core::KindReference{QStringLiteral("acme.saga"), QStringLiteral("generation")};

    core::PackageCard card;
    card.id = QStringLiteral("acme.saga");
    card.version = QStringLiteral("2.0");
    card.name.values = {{QStringLiteral("en"), QStringLiteral("Family Saga")}};
    core::ElementKind generation;
    generation.id = QStringLiteral("generation");
    generation.name.values = {{QStringLiteral("en"), QStringLiteral("Generation")}};
    generation.plural.values = {{QStringLiteral("en"), QStringLiteral("Generations")}};
    card.kinds.append(generation);

    // After the card
    CHECK(gui::typeNameOf(registry, project, &card) ==
          QStringLiteral("Family Saga (not in the program)"));
    const core::KindWords words = gui::wordsOf(registry, element, &card);
    CHECK(words.forms.value(QStringLiteral("singular")) == QStringLiteral("generation"));
    CHECK(words.forms.value(QStringLiteral("plural")) == QStringLiteral("generations"));

    // Without a card, or a card without the kind: the ids
    CHECK(gui::typeNameOf(registry, project) == QStringLiteral("acme.saga (not in the program)"));
    card.kinds.clear();
    CHECK(gui::wordsOf(registry, element, &card).forms.value(QStringLiteral("singular")) ==
          QStringLiteral("generation"));
    CHECK(gui::wordsOf(registry, element).forms.value(QStringLiteral("plural")) ==
          QStringLiteral("generation"));

    // A type the program has is named after its package, not after a card
    project.type->id = QStringLiteral("kalahari.novel");
    card.name.values = {{QStringLiteral("en"), QStringLiteral("Old Novel")}};
    CHECK(gui::typeNameOf(registry, project, &card) == QStringLiteral("Novel"));
}
