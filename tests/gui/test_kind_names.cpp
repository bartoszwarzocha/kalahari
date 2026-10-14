/// @file test_kind_names.cpp
/// @brief The names of kinds in the texts of menu commands

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/kind_words.h>
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
