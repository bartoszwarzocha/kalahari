// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_cursor_card.cpp
/// @brief The card at the cursor in the Properties panel: the card whose name the cursor of a
/// text of the book is in, in any of its forms

#include "kalahari_tests/editor/editor_test_utils.h"
#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_names.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/link_names.h"
#include "kalahari/gui/panels/card_page.h"
#include "kalahari/gui/panels/cursor_card_box.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/paragraph_cards_box.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/widgets/relations_box.h"

#include <catch2/catch_test_macros.hpp>

#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QStringList>
#include <QToolButton>
#include <QTreeWidget>

#include <cstdio>
#include <string>

namespace kalahari {
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

TEST_CASE("Card at the cursor: the Chapter page of the chapter in front has the cards too",
          "[gui][cards][names]") {
    gui::registerAllIcons();
    const AtCursor names(CursorCardBox::NAMES);
    Cast cast;
    auto& pm = cast.novel.pm;
    const QString first = text("Spotkał Annę Wolską i Eldriona w Czarnym Lesie.");
    cast.editor->fromKml(test::kmlOf({first}));
    REQUIRE(pm.linkParagraph(cast.forest, cast.novel.chapterId, 1, first,
                             core::LinkRoles::SETTING));
    const QString other = pm.addElement(pm.chapterKindFor(core::BookPlace::Main),
                                        QStringLiteral("The Calm"), core::BookPlace::Main);
    REQUIRE_FALSE(other.isEmpty());
    // As a chapter opened from the Navigator: its tab in front, then its page
    gui::PropertiesPanel panel(test::book());
    panel.setActiveEditor(&cast.editorPanel);
    panel.showChapterProperties(cast.novel.chapterId);
    REQUIRE(panel.currentPage() == gui::PropertiesPanel::Page::Chapter);
    const gui::ParagraphCardsBox* box = panel.paragraphCards();
    REQUIRE(box != nullptr);

    // Under the versions of the chapter: the card at the cursor and the cards of its paragraph
    putCursor(*cast.editor, "Wolską");
    waitForPanel();
    REQUIRE(showsCard(panel));
    CHECK(panel.cursorCard()->cardId() == cast.anna);
    REQUIRE(box->isVisibleTo(&panel));
    CHECK(box->list()->topLevelItemCount() == 1);

    SECTION("the page of another chapter has none") {
        panel.showChapterProperties(other);
        CHECK_FALSE(showsCard(panel));
        CHECK_FALSE(box->isVisibleTo(&panel));
        // Its cursor moving shows none either
        putCursor(*cast.editor, "Eldriona");
        waitForPanel();
        CHECK_FALSE(showsCard(panel));
        // Its own page again
        panel.showChapterProperties(cast.novel.chapterId);
        CHECK(showsCard(panel));
        CHECK(panel.cursorCard()->cardId() == cast.eldrion);
    }

    SECTION("they leave with the text") {
        panel.setActiveEditor(nullptr);
        CHECK(panel.currentPage() == gui::PropertiesPanel::Page::Chapter);
        CHECK_FALSE(showsCard(panel));
        CHECK_FALSE(box->isVisibleTo(&panel));
    }

    SECTION("the Editor page has them again, in the same order") {
        panel.setActiveEditor(&cast.editorPanel);
        REQUIRE(panel.currentPage() == gui::PropertiesPanel::Page::Editor);
        CHECK(showsCard(panel));
        CHECK(box->isVisibleTo(&panel));
        const auto* versions = panel.findChild<QGroupBox*>(QStringLiteral("chapterVersionsGroup"));
        REQUIRE(versions != nullptr);
        const QLayout* layout = panel.cursorCard()->parentWidget()->layout();
        REQUIRE(layout != nullptr);
        CHECK(layout->indexOf(versions) >= 0);
        CHECK(layout->indexOf(versions) < layout->indexOf(panel.cursorCard()));
        CHECK(layout->indexOf(panel.cursorCard()) < layout->indexOf(panel.paragraphCards()));
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

TEST_CASE("Card at the cursor: the cards of the paragraph, with their roles",
          "[gui][cards][links]") {
    gui::registerAllIcons();
    const AtCursor names(CursorCardBox::NAMES);
    Cast cast;
    auto& pm = cast.novel.pm;
    const QString first = text("Spotkał Annę Wolską i Eldriona w Czarnym Lesie.");
    const QString second = text("Deszcz padał całą noc.");
    cast.editor->fromKml(test::kmlOf({first, second}));
    REQUIRE(pm.linkParagraph(cast.anna, cast.novel.chapterId, 1, first,
                             core::LinkRoles::POINT_OF_VIEW));
    REQUIRE(pm.linkParagraph(cast.forest, cast.novel.chapterId, 1, first,
                             core::LinkRoles::SETTING));
    gui::PropertiesPanel panel(test::book());
    panel.setActiveEditor(&cast.editorPanel);
    gui::ParagraphCardsBox* box = panel.paragraphCards();
    REQUIRE(box != nullptr);
    const auto rows = [box]() {
        QStringList texts;
        for (int i = 0; i < box->list()->topLevelItemCount(); ++i) {
            const QTreeWidgetItem* item = box->list()->topLevelItem(i);
            texts << item->text(0) + QStringLiteral(" – ") + item->text(1);
        }
        return texts;
    };

    // The cards of the paragraph at the cursor: their roles and names
    putCursor(*cast.editor, "Spotkał");
    waitForPanel();
    REQUIRE(box->isVisibleTo(&panel));
    CHECK(box->paragraph() == 0);
    CHECK(rows() == QStringList{gui::cardRoleName(core::LinkRoles::POINT_OF_VIEW) +
                                    QStringLiteral(" – Anna Wolska"),
                                gui::cardRoleName(core::LinkRoles::SETTING) +
                                    QStringLiteral(" – Czarny Las")});
    CHECK_FALSE(box->emptyLabel()->isVisibleTo(box));
    CHECK(box->addButton()->isEnabled());
    CHECK_FALSE(box->removeButton()->isEnabled());

    // Another paragraph has none yet
    cast.editor->setCursorPosition({1, 2});
    waitForPanel();
    CHECK(box->paragraph() == 1);
    CHECK(box->list()->topLevelItemCount() == 0);
    CHECK(box->emptyLabel()->isVisibleTo(box));

    SECTION("\"+\" asks for the window, Enter opens the card, \"-\" takes it off the paragraph") {
        putCursor(*cast.editor, "Spotkał");
        waitForPanel();
        int asked = 0;
        QStringList opened;
        QObject::connect(&panel, &gui::PropertiesPanel::requestLinkParagraphCard,
                         [&asked]() { ++asked; });
        QObject::connect(&panel, &gui::PropertiesPanel::requestOpenCard,
                         [&opened](const QString& id) { opened << id; });
        box->addButton()->click();
        CHECK(asked == 1);
        box->list()->setCurrentItem(box->list()->topLevelItem(1));
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QCoreApplication::sendEvent(box->list(), &enter);
        CHECK(opened == QStringList{cast.forest});
        REQUIRE(box->removeButton()->isEnabled());
        box->removeButton()->click();
        CHECK(rows() == QStringList{gui::cardRoleName(core::LinkRoles::POINT_OF_VIEW) +
                                    QStringLiteral(" – Anna Wolska")});
        // The card stays linked with the chapter
        const core::ElementLink* link = pm.findElement(cast.forest)->linkTo(cast.novel.chapterId);
        REQUIRE(link != nullptr);
        CHECK(link->paragraph == 0);
    }

    SECTION("a card linked meanwhile, a new role and a new name show at once") {
        putCursor(*cast.editor, "Spotkał");
        waitForPanel();
        REQUIRE(pm.linkParagraph(cast.eldrion, cast.novel.chapterId, 1, first,
                                 core::LinkRoles::MENTIONED));
        CHECK(rows().size() == 3);
        REQUIRE(pm.renameElement(cast.forest, QStringLiteral("Las")));
        CHECK(rows().contains(gui::cardRoleName(core::LinkRoles::SETTING) +
                              QStringLiteral(" – Las")));
    }

    SECTION("Settings > Cards: also with only the linked paragraphs; none with no cards") {
        putCursor(*cast.editor, "Wolską");
        waitForPanel();
        AtCursor::choose(CursorCardBox::PARAGRAPHS);
        waitForPanel();
        CHECK_FALSE(showsCard(panel));
        CHECK(box->isVisibleTo(&panel));
        AtCursor::choose(CursorCardBox::NONE);
        waitForPanel();
        CHECK_FALSE(box->isVisibleTo(&panel));
    }

    SECTION("a text of the Workshop has no cards of its paragraphs") {
        cast.editorPanel.setProperty("elementId", cast.anna);
        putCursor(*cast.editor, "Spotkał");
        waitForPanel();
        CHECK_FALSE(box->isVisibleTo(&panel));
    }
}

}  // namespace kalahari
