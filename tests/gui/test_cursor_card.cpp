// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_cursor_card.cpp
/// @brief The card at the cursor in the Properties panel: the card whose name the cursor of a
/// text of the book is in, in any of its forms

#include "../editor/editor_test_utils.h"
#include "document_window.h"
#include "test_support/window_parts.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_names.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/card_page.h"
#include "kalahari/gui/panels/cursor_card_box.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/widgets/relations_box.h"

#include <catch2/catch_test_macros.hpp>

#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QToolButton>
#include <QTreeWidget>

#include <cstdio>
#include <string>
#include <utility>

using namespace kalahari;
using gui::CursorCardBox;

namespace {

/// @p utf8 as a QString
QString text(const char* utf8) {
    return QString::fromUtf8(utf8);
}

/// A new card @p title of kind @p kindId of the Base, with its name in the book's dictionary,
/// in the forms of Polish
QString addCard(const char* kindId, const char* title) {
    auto& pm = test::book();
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QLatin1String(kindId));
    REQUIRE(kind);
    const QString id = pm.addElement(kind, text(title), core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    const core::NameDeclension polish(QStringLiteral("pl"));
    REQUIRE(pm.changeBookDictionary([&](core::BookDictionary& dictionary) {
        dictionary.putName(core::nameOfCard(dictionary, id, text(title),
                                            core::nameKindOf(*kind.kind), polish));
    }));
    return id;
}

/// What the Properties panel shows of the cards at the cursor, for one test
class AtCursor {
public:
    explicit AtCursor(const char* value)
        : m_saved(core::SettingsManager::getInstance().get<std::string>(
              CursorCardBox::SETTING, CursorCardBox::NAMES)) {
        core::SettingsManager::getInstance().set<std::string>(CursorCardBox::SETTING, value);
    }
    ~AtCursor() {
        try {
            core::SettingsManager::getInstance().set<std::string>(CursorCardBox::SETTING,
                                                                 m_saved);
        } catch (...) {
            // A destructor may not throw: the later tests then find the setting of this one
            static_cast<void>(std::fputs("AtCursor: the setting of the test stays\n", stderr));
        }
    }
    AtCursor(const AtCursor&) = delete;
    AtCursor& operator=(const AtCursor&) = delete;
    AtCursor(AtCursor&&) = delete;
    AtCursor& operator=(AtCursor&&) = delete;

    /// @brief Choose @p value
    static void choose(const char* value) {
        core::SettingsManager::getInstance().set<std::string>(CursorCardBox::SETTING, value);
    }

private:
    std::string m_saved;
};

/// Put the cursor of @p editor into the first @p word of its first paragraph, @p into it
void putCursor(editor::BookEditor& editor, const char* word, int into = 2) {
    const auto at = static_cast<int>(editor.paragraphPlainText(0).indexOf(text(word)));
    REQUIRE(at >= 0);
    editor.setCursorPosition({0, at + into});
}

/// Run the event loop past the panel's wait after the cursor moved
void waitForPanel() {
    test::runEventLoop(300);
}

/// Whether the panel shows the card at the cursor
bool showsCard(const gui::PropertiesPanel& panel) {
    return panel.cursorCard()->isVisibleTo(&panel);
}

/// The cast of the tests: two characters with a relation, in a chapter of the open book
struct Cast {
    test::OpenNovel novel{QStringLiteral("Cursor")};
    QString anna = addCard("character", "Anna Wolska");
    QString eldrion = addCard("character", "Eldrion");
    QString forest = addCard("location", "Czarny Las");
    gui::EditorPanel editorPanel{test::book(), test::commands()};
    editor::BookEditor* editor = editorPanel.getBookEditor();

    Cast() {
        auto& pm = novel.pm;
        REQUIRE(pm.setCardValue(anna, QStringLiteral("role"), QStringLiteral("main")));
        REQUIRE(pm.setDescription(anna, QStringLiteral("A painter who\nlost her sight.")));
        REQUIRE(pm.addRelation(anna, eldrion, QStringLiteral("sibling"), QString()));
        editorPanel.setProperty("elementId", novel.chapterId);
        editor->fromKml(test::kmlOf({text("Spotkał Annę Wolską i Eldriona w Czarnym Lesie.")}));
    }
};

}  // namespace

TEST_CASE("Card at the cursor: the card whose name the cursor is in", "[gui][cards][names]") {
    gui::registerAllIcons();
    const AtCursor names(CursorCardBox::NAMES);
    Cast cast;
    gui::PropertiesPanel panel(test::book());
    panel.setActiveEditor(&cast.editorPanel);
    REQUIRE(panel.currentPage() == gui::PropertiesPanel::Page::Editor);
    CursorCardBox* box = panel.cursorCard();
    REQUIRE(box != nullptr);
    CHECK_FALSE(showsCard(panel));

    SECTION("a form of the name: the card with its kind, In Short and its relations") {
        putCursor(*cast.editor, "Wolską");
        waitForPanel();
        REQUIRE(showsCard(panel));
        CHECK(box->cardId() == cast.anna);
        CHECK(box->nameLabel()->text() == QStringLiteral("Anna Wolska"));
        const core::ProjectElement* anna = test::book().findElement(cast.anna);
        REQUIRE(anna != nullptr);
        CHECK(box->subtitleLabel()->text() == gui::CardPage::subtitleOf(test::book(), *anna));
        CHECK(box->subtitleLabel()->text().contains(QStringLiteral("main")));
        CHECK(box->shortLabel()->isVisibleTo(box));
        CHECK(box->shortLabel()->text() == QStringLiteral("A painter who lost her sight."));
        CHECK_FALSE(box->portraitLabel()->pixmap().isNull());
        CHECK(box->relationsBox()->list()->topLevelItemCount() == 1);

        // Another card: without In Short
        putCursor(*cast.editor, "Eldriona");
        waitForPanel();
        CHECK(box->cardId() == cast.eldrion);
        CHECK_FALSE(box->shortLabel()->isVisibleTo(box));
        putCursor(*cast.editor, "Lesie");
        waitForPanel();
        CHECK(box->cardId() == cast.forest);

        // No name: no card
        putCursor(*cast.editor, "Spotkał");
        waitForPanel();
        CHECK_FALSE(showsCard(panel));
        CHECK(box->cardId().isEmpty());
    }

    SECTION("Open Card, the gear and a relation ask the window") {
        putCursor(*cast.editor, "Annę");
        waitForPanel();
        REQUIRE(showsCard(panel));
        QStringList opened;
        int settings = 0;
        QStringList selected;
        QObject::connect(&panel, &gui::PropertiesPanel::requestOpenCard,
                         [&opened](const QString& id) { opened << id; });
        QObject::connect(&panel, &gui::PropertiesPanel::requestCardSettings,
                         [&settings]() { ++settings; });
        QObject::connect(&panel, &gui::PropertiesPanel::requestSelectCard,
                         [&selected](const QString& id) { selected << id; });
        box->openButton()->click();
        CHECK(opened == QStringList{cast.anna});
        box->settingsButton()->click();
        CHECK(settings == 1);
        QTreeWidget* relations = box->relationsBox()->list();
        relations->setCurrentItem(relations->topLevelItem(0));
        emit relations->itemDoubleClicked(relations->topLevelItem(0), 0);
        CHECK(selected == QStringList{cast.eldrion});
    }

    SECTION("a change of the card shows at once") {
        putCursor(*cast.editor, "Annę");
        waitForPanel();
        REQUIRE(test::book().setDescription(cast.anna, QStringLiteral("A painter.")));
        CHECK(box->shortLabel()->text() == QStringLiteral("A painter."));
        REQUIRE(test::book().renameElement(cast.anna, QStringLiteral("Anna W.")));
        CHECK(box->nameLabel()->text() == QStringLiteral("Anna W."));
    }

    SECTION("a new name in the book's dictionary is found at once") {
        cast.editor->fromKml(test::kmlOf({text("Kirran milczał.")}));
        putCursor(*cast.editor, "Kirran");
        waitForPanel();
        CHECK_FALSE(showsCard(panel));
        const QString kirran = addCard("character", "Kirran");
        waitForPanel();
        CHECK(box->cardId() == kirran);
    }

    SECTION("Settings > Cards: only the linked paragraphs, or no cards") {
        putCursor(*cast.editor, "Annę");
        waitForPanel();
        REQUIRE(showsCard(panel));
        for (const char* value : {CursorCardBox::PARAGRAPHS, CursorCardBox::NONE}) {
            AtCursor::choose(value);
            waitForPanel();
            CHECK_FALSE(showsCard(panel));
            AtCursor::choose(CursorCardBox::NAMES);
            waitForPanel();
            CHECK(showsCard(panel));
        }
    }

    SECTION("a text outside the book has no cards") {
        cast.editorPanel.setProperty("elementId", QString());
        putCursor(*cast.editor, "Annę");
        waitForPanel();
        CHECK_FALSE(showsCard(panel));
    }
}

TEST_CASE("Card at the cursor: the setting", "[gui][cards][settings]") {
    const AtCursor names(CursorCardBox::NAMES);
    CHECK(CursorCardBox::cardsAtCursor() == CursorCardBox::Shown::NamesAndParagraphs);
    AtCursor::choose(CursorCardBox::PARAGRAPHS);
    CHECK(CursorCardBox::cardsAtCursor() == CursorCardBox::Shown::Paragraphs);
    AtCursor::choose(CursorCardBox::NONE);
    CHECK(CursorCardBox::cardsAtCursor() == CursorCardBox::Shown::None);
    // An unknown value: the names and the paragraphs
    AtCursor::choose("other");
    CHECK(CursorCardBox::cardsAtCursor() == CursorCardBox::Shown::NamesAndParagraphs);
}
