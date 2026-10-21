/// @file test_quick_character_palette.cpp
/// @brief Insert > Quick Character: the favorites and the characters used last at the cursor,
///        the search, the keys of the palette and its place on the screen

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/special_character_choices.h"
#include "kalahari/gui/special_character_coordinator.h"
#include "kalahari/gui/widgets/quick_character_palette.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMouseEvent>
#include <QStatusBar>

#include <memory>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// A palette with the em dash given keys and "(c)" replaced with ©; the test deletes it
std::unique_ptr<QuickCharacterPalette> newPalette()
{
    auto palette = std::make_unique<QuickCharacterPalette>(
        QString(),
        [](char32_t code) { return code == U'—' ? QStringLiteral("Alt+_") : QString(); },
        [](char32_t code) {
            return code == U'©' ? QStringList{QStringLiteral("(c)")} : QStringList();
        });
    palette->setAttribute(Qt::WA_DeleteOnClose, false);
    palette->showAt(QRect(100, 100, 2, 20));
    return palette;
}

/// Press keys in the search of the palette
void press(QuickCharacterPalette& palette, Qt::Key key,
           Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    auto* search = palette.findChild<QLineEdit*>();
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(search, &event);
}

/// The text under the rows: the character chosen
QString detailsOf(const QuickCharacterPalette& palette)
{
    QString details;
    for (const QLabel* label : palette.findChildren<QLabel*>()) {
        if (label->textFormat() == Qt::RichText) {
            details = label->text();
        }
    }
    return details;
}

/// A row of the palette by its name, shown
QListWidget* rowOf(const QuickCharacterPalette& palette, const QString& name)
{
    for (QListWidget* row : palette.findChildren<QListWidget*>()) {
        if (row->accessibleName() == name && row->isVisibleTo(&palette)) {
            return row;
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("Quick character palette: the favorites and those used last, Enter inserts",
          "[gui][special-characters][palette]") {
    addRecentCharacter(U'§');
    addRecentCharacter(U'→');
    const std::unique_ptr<QuickCharacterPalette> palette = newPalette();
    QList<char32_t> inserted;
    QObject::connect(palette.get(), &QuickCharacterPalette::insertRequested,
                     [&inserted](char32_t code) { inserted.append(code); });
    REQUIRE(palette->isVisible());

    // A row of the favorites, a row of those used last; the first favorite chosen
    QList<char32_t> shown = favoriteCharacters();
    shown.append({U'→', U'§'});
    CHECK(palette->shownCharacters() == shown);
    CHECK(palette->currentCharacter() == U'—');
    CHECK(detailsOf(*palette) == QStringLiteral("<b>Em dash</b> · U+2014 · Alt+_"));

    // Left and Right while nothing is typed, Up and Down always
    press(*palette, Qt::Key_Right);
    CHECK(palette->currentCharacter() == U'–');
    press(*palette, Qt::Key_Left);
    press(*palette, Qt::Key_Left);
    CHECK(palette->currentCharacter() == U'—');
    for (int step = 0; step < 10; ++step) {
        press(*palette, Qt::Key_Down);
    }
    CHECK(palette->currentCharacter() == U'→');
    QListWidget* recent = rowOf(*palette, QStringLiteral("Recently used"));
    REQUIRE(recent != nullptr);
    CHECK(recent->currentRow() == 0);

    press(*palette, Qt::Key_Return);
    CHECK(inserted == QList<char32_t>{U'→'});
    CHECK_FALSE(palette->isVisible());
}

TEST_CASE("Quick character palette: the search", "[gui][special-characters][palette]") {
    const std::unique_ptr<QuickCharacterPalette> palette = newPalette();
    QList<char32_t> inserted;
    QObject::connect(palette.get(), &QuickCharacterPalette::insertRequested,
                     [&inserted](char32_t code) { inserted.append(code); });

    SECTION("A name finds the characters; Left and Right move in the text") {
        palette->setSearchText(QStringLiteral("arrow right"));
        CHECK(palette->currentCharacter() == U'→');
        press(*palette, Qt::Key_Left);
        CHECK(palette->currentCharacter() == U'→');
        press(*palette, Qt::Key_Down);
        CHECK(palette->currentCharacter() != U'→');
        palette->setSearchText(QStringLiteral("U+00A9"));
        CHECK(palette->shownCharacters() == QList<char32_t>{U'©'});
        CHECK(detailsOf(*palette) ==
              QStringLiteral("<b>Copyright sign</b> · U+00A9 · auto-correct <code>(c)</code>"));
        press(*palette, Qt::Key_Enter);
        CHECK(inserted == QList<char32_t>{U'©'});
    }

    SECTION("Many found: the first three rows") {
        palette->setSearchText(QStringLiteral("letter"));
        CHECK(palette->shownCharacters().size() ==
              QuickCharacterPalette::COLUMNS * QuickCharacterPalette::FOUND_ROWS);
        bool said = false;
        for (const QLabel* label : palette->findChildren<QLabel*>()) {
            said = said || (label->isVisibleTo(palette.get()) &&
                            label->text().startsWith(QStringLiteral("Found: the first 36 of ")));
        }
        CHECK(said);
    }

    SECTION("Nothing found: Enter inserts nothing") {
        palette->setSearchText(QStringLiteral("nothing like this"));
        CHECK(palette->shownCharacters().isEmpty());
        CHECK(palette->currentCharacter() == 0);
        press(*palette, Qt::Key_Return);
        CHECK(inserted.isEmpty());
        CHECK(palette->isVisible());
    }
}

TEST_CASE("Quick character palette: Esc, Ctrl+Enter, a click",
          "[gui][special-characters][palette]") {
    const std::unique_ptr<QuickCharacterPalette> palette = newPalette();
    QList<char32_t> inserted;
    int all = 0;
    QObject::connect(palette.get(), &QuickCharacterPalette::insertRequested,
                     [&inserted](char32_t code) { inserted.append(code); });
    QObject::connect(palette.get(), &QuickCharacterPalette::allCharactersRequested,
                     [&all]() { ++all; });

    SECTION("Esc closes it without inserting") {
        press(*palette, Qt::Key_Escape);
        CHECK_FALSE(palette->isVisible());
        CHECK(inserted.isEmpty());
        CHECK(all == 0);
    }

    SECTION("Ctrl+Enter opens the window of all the characters") {
        press(*palette, Qt::Key_Return, Qt::ControlModifier);
        CHECK_FALSE(palette->isVisible());
        CHECK(all == 1);
        CHECK(inserted.isEmpty());
    }

    SECTION("A click on a favorite inserts it") {
        QListWidget* favorites = rowOf(*palette, QStringLiteral("Favorites"));
        REQUIRE(favorites != nullptr);
        const QPoint at = favorites->visualItemRect(favorites->item(2)).center();
        const QPointF global = favorites->viewport()->mapToGlobal(QPointF(at));
        QMouseEvent pressed(QEvent::MouseButtonPress, QPointF(at), global, Qt::LeftButton,
                            Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(favorites->viewport(), &pressed);
        QMouseEvent released(QEvent::MouseButtonRelease, QPointF(at), global, Qt::LeftButton,
                             Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(favorites->viewport(), &released);
        CHECK(inserted == QList<char32_t>{favoriteCharacters().at(2)});
        CHECK_FALSE(palette->isVisible());
    }
}

TEST_CASE("Quick character palette: under the cursor, within the screen",
          "[gui][special-characters][palette]") {
    const QRect screen(0, 0, 1000, 800);
    const QSize size(400, 300);

    // Under the cursor, a little to its left
    CHECK(QuickCharacterPalette::placeFor(QRect(100, 100, 2, 20), size, screen) ==
          QRect(84, 126, 400, 300));
    // Over it where the screen ends below
    CHECK(QuickCharacterPalette::placeFor(QRect(100, 700, 2, 20), size, screen) ==
          QRect(84, 394, 400, 300));
    // At the right edge and the left edge of the screen
    CHECK(QuickCharacterPalette::placeFor(QRect(950, 100, 2, 20), size, screen).right() == 999);
    CHECK(QuickCharacterPalette::placeFor(QRect(5, 100, 2, 20), size, screen).left() == 0);
}

TEST_CASE("Quick character palette: Insert > Quick Character in the text in front",
          "[gui][special-characters][palette][editor]") {
    registerAllCommands(CommandCallbacks{});
    const Command* command = CommandRegistry::getInstance().getCommand("insert.quickCharacter");
    REQUIRE(command != nullptr);
    CHECK(command->shortcut ==
          KeyboardShortcut(Qt::Key_I, Qt::ControlModifier | Qt::ShiftModifier));

    QMainWindow window;
    auto* text = new editor::BookEditor();
    window.setCentralWidget(text);
    window.show();
    text->fromKml(test::kmlOf({QStringLiteral("Ala")}));
    text->setCursorPosition({0, 3});
    SpecialCharacterCoordinator coordinator(
        &window, window.statusBar(), [text]() { return text; }, {}, {});
    coordinator.connectCommands();

    CommandRegistry::getInstance().getCommand("insert.quickCharacter")->execute();
    QuickCharacterPalette* palette = coordinator.quickPalette();
    REQUIRE(palette != nullptr);
    CHECK(palette->isVisible());
    palette->insertCurrent();
    CHECK(text->plainText() == QStringLiteral("Ala—"));
    CHECK(window.statusBar()->currentMessage() == QStringLiteral("Inserted: Em dash (—)"));

    // All Characters...: the window
    coordinator.openPalette();
    REQUIRE(coordinator.quickPalette() != nullptr);
    coordinator.quickPalette()->showAllCharacters();
    REQUIRE(coordinator.dialog() != nullptr);
    CHECK(coordinator.dialog()->isVisible());
    coordinator.dialog()->close();

    // In a book open read-only: no palette, and the status bar says why
    text->setReadOnly(true);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    coordinator.openPalette();
    CHECK(coordinator.quickPalette() == nullptr);
    CHECK(window.statusBar()->currentMessage() == QStringLiteral("The book is open read-only"));
}
