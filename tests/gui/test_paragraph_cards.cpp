// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_paragraph_cards.cpp
/// @brief A paragraph of a chapter linked with a card of a character, a location or an item:
/// the window Link Paragraph with Card, the signs beside the paragraph, Book > Link Paragraph
/// with Card... and Book > Open Card at Cursor

#include "../editor/editor_test_utils.h"
#include "document_window.h"
#include "test_support/window_parts.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_names.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/cards_in_text_coordinator.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/paragraph_card_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/link_names.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/paragraph_marks_coordinator.h"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTimer>
#include <QTreeWidgetItem>

#include <cstdio>
#include <functional>

using namespace kalahari;
using gui::CardsInTextCoordinator;
using gui::dialogs::ParagraphCardDialog;

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

/// A boolean setting changed for one test
class ChangedSetting {
public:
    ChangedSetting(const char* key, bool value)
        : m_key(key), m_saved(core::SettingsManager::getInstance().get<bool>(key, true)) {
        set(value);
    }
    ~ChangedSetting() {
        try {
            set(m_saved);
        } catch (...) {
            // A destructor may not throw: the later tests then find the setting of this one
            static_cast<void>(std::fputs("ChangedSetting: the setting of the test stays\n",
                                         stderr));
        }
    }
    ChangedSetting(const ChangedSetting&) = delete;
    ChangedSetting& operator=(const ChangedSetting&) = delete;
    ChangedSetting(ChangedSetting&&) = delete;
    ChangedSetting& operator=(ChangedSetting&&) = delete;

    /// @brief Give the setting @p value
    void set(bool value) const { core::SettingsManager::getInstance().set<bool>(m_key, value); }

private:
    const char* m_key;
    bool m_saved;
};

/// The paragraphs of the chapter of the tests
const QStringList PARAGRAPHS{text("Spotkał Annę Wolską w Czarnym Lesie."),
                             text("Deszcz padał całą noc, a rano wszyscy przy ognisku milczeli."), QString(),
                             text("Eldrion milczał.")};

/// The book of the tests: a chapter with PARAGRAPHS, saved, and three cards
struct Cast {
    test::OpenNovel novel{QStringLiteral("Cards")};
    QString anna;
    QString eldrion;
    QString forest;

    Cast() {
        novel.pm.setChapterContent(novel.chapterId, test::kmlOf(PARAGRAPHS));
        REQUIRE(novel.pm.saveChapterContent(novel.chapterId));
        anna = addCard("character", "Anna Wolska");
        eldrion = addCard("character", "Eldrion");
        forest = addCard("location", "Czarny Las");
    }
};

/// Run the events a few times, past the coordinators' waits of 0 ms
void settle() {
    for (int i = 0; i < 3; ++i) {
        QCoreApplication::processEvents();
    }
}

/// Open the chapter of @p cast in a tab of @p window: its editor
editor::BookEditor* openChapter(test::Window& window, const Cast& cast) {
    window.navigator.onElementSelected(cast.novel.chapterId, QStringLiteral("The Storm"));
    REQUIRE(window.tabs->count() == 1);
    editor::BookEditor* editor = window.editor(0)->getBookEditor();
    REQUIRE(editor != nullptr);
    settle();
    return editor;
}

/// Run @p command, answering the window Link Paragraph with Card it shows with @p answer;
/// another window is closed with Cancel
/// @return Whether the window showed
bool answeringLink(const std::function<void()>& command,
                   const std::function<void(ParagraphCardDialog&)>& answer) {
    bool shown = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &answer]() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr || !dialog->isVisible()) {
            return;
        }
        auto* link = qobject_cast<ParagraphCardDialog*>(dialog);
        if (link == nullptr || shown) {
            dialog->reject();
            return;
        }
        shown = true;
        answer(*link);
    });
    timer.start();
    command();
    return shown;
}

/// The titles of the shown rows of the list of the window
QStringList shownCards(const ParagraphCardDialog& dialog) {
    QStringList titles;
    for (int i = 0; i < dialog.cardList()->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* item = dialog.cardList()->topLevelItem(i);
        if (!item->isHidden()) {
            titles << item->text(0);
        }
    }
    return titles;
}

/// The roles the window offers, by their data
QStringList offeredRoles(const ParagraphCardDialog& dialog) {
    QStringList roles;
    for (int i = 0; i < dialog.roleCombo()->count(); ++i) {
        roles << dialog.roleCombo()->itemData(i).toString();
    }
    return roles;
}

}  // namespace

TEST_CASE("Paragraph cards: the window Link Paragraph with Card", "[gui][cards][links]") {
    gui::registerAllIcons();
    Cast cast;
    auto& pm = cast.novel.pm;
    const core::KindRef character = pm.kindOf(*pm.findElement(cast.anna));
    const core::KindRef location = pm.kindOf(*pm.findElement(cast.forest));
    REQUIRE(character);
    REQUIRE(location);

    ParagraphCardDialog dialog(pm, cast.novel.chapterId, 2, PARAGRAPHS.at(1), QString());
    // The chapter, the paragraph and its first words in the heading
    const QString texts = test::textsOf(dialog);
    INFO(texts.toStdString());
    CHECK(texts.contains(QStringLiteral("The Storm, paragraph 2: \"") +
                         core::LinkRoles::excerptOf(PARAGRAPHS.at(1)) + QStringLiteral("…\"")));
    CHECK(dialog.noteLabel()->isHidden());

    // The cards by their names, the first one chosen, with the roles of its kind
    CHECK(shownCards(dialog) == QStringList{QStringLiteral("Anna Wolska"),
                                            QStringLiteral("Czarny Las"),
                                            QStringLiteral("Eldrion")});
    REQUIRE(dialog.choice().cardId == cast.anna);
    CHECK(offeredRoles(dialog) == core::LinkRoles::ofCard(*character.kind));
    CHECK(dialog.choice().role == core::LinkRoles::suggestedForCard(*character.kind));
    CHECK(dialog.roleCombo()->currentText() ==
          gui::cardRoleName(core::LinkRoles::suggestedForCard(*character.kind)));

    // A form of a name filters the cards; a location has its own roles
    dialog.filterEdit()->setText(text("Eldriona"));
    CHECK(shownCards(dialog) == QStringList{QStringLiteral("Eldrion")});
    CHECK(dialog.choice().cardId == cast.eldrion);
    dialog.filterEdit()->setText(text("Lesie"));
    REQUIRE(dialog.choice().cardId == cast.forest);
    CHECK(offeredRoles(dialog) == core::LinkRoles::ofCard(*location.kind));
    CHECK(offeredRoles(dialog).contains(QStringLiteral("setting")));
    dialog.filterEdit()->setText(QStringLiteral("xyz"));
    CHECK(shownCards(dialog).isEmpty());
    CHECK(dialog.choice().cardId.isEmpty());
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    dialog.filterEdit()->clear();

    // The card whose name the cursor is in is chosen first
    ParagraphCardDialog atName(pm, cast.novel.chapterId, 1, PARAGRAPHS.at(0), cast.forest);
    CHECK(atName.choice().cardId == cast.forest);

    SECTION("the link is saved with the paragraph, its first words and the role") {
        QString linked;
        CHECK(answeringLink(
            [&]() {
                linked = gui::dialogs::linkParagraphWithCard(pm, cast.novel.chapterId, 2,
                                                             PARAGRAPHS.at(1), cast.eldrion,
                                                             nullptr);
            },
            [](ParagraphCardDialog& window) {
                window.roleCombo()->setCurrentIndex(
                    window.roleCombo()->findData(core::LinkRoles::MENTIONED));
                window.acceptButton()->click();
            }));
        CHECK(linked == cast.eldrion);
        const core::ElementLink* link = pm.findElement(cast.eldrion)->linkTo(cast.novel.chapterId);
        REQUIRE(link != nullptr);
        CHECK(link->paragraph == 2);
        CHECK(link->excerpt == core::LinkRoles::excerptOf(PARAGRAPHS.at(1)));
        CHECK(link->role == core::LinkRoles::MENTIONED);

        // The card keeps its role in the window again, and says that its link moves to
        // another paragraph
        ParagraphCardDialog again(pm, cast.novel.chapterId, 1, PARAGRAPHS.at(0), cast.eldrion);
        CHECK(again.choice().role == core::LinkRoles::MENTIONED);
        CHECK(again.noteLabel()->isVisibleTo(&again));
        CHECK(again.noteLabel()->text().contains(QStringLiteral("paragraph 2")));
        ParagraphCardDialog same(pm, cast.novel.chapterId, 2, PARAGRAPHS.at(1), cast.eldrion);
        CHECK(same.noteLabel()->text().contains(QStringLiteral("already")));

        // Closed with Cancel: nothing changes
        CHECK(answeringLink(
            [&]() {
                linked = gui::dialogs::linkParagraphWithCard(pm, cast.novel.chapterId, 1,
                                                             PARAGRAPHS.at(0), cast.eldrion,
                                                             nullptr);
            },
            [](ParagraphCardDialog& window) { window.reject(); }));
        CHECK(linked.isEmpty());
        CHECK(pm.findElement(cast.eldrion)->linkTo(cast.novel.chapterId)->paragraph == 2);
    }

    SECTION("the arrows of the filter choose in the list") {
        dialog.show();
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QCoreApplication::sendEvent(dialog.filterEdit(), &down);
        CHECK(dialog.choice().cardId == cast.forest);
    }
}

TEST_CASE("Paragraph cards: the signs of the kinds of the cards beside the paragraph",
          "[gui][cards][marks]") {
    gui::registerAllIcons();
    const ChangedSetting paragraphs(core::LinkRoles::PARAGRAPHS_SETTING, true);
    const ChangedSetting signs(gui::ParagraphMarksCoordinator::CARD_MARKS_SETTING, true);
    Cast cast;
    auto& pm = cast.novel.pm;
    REQUIRE(pm.linkParagraph(cast.anna, cast.novel.chapterId, 1, PARAGRAPHS.at(0),
                             core::LinkRoles::POINT_OF_VIEW));
    REQUIRE(pm.linkParagraph(cast.forest, cast.novel.chapterId, 1, PARAGRAPHS.at(0),
                             core::LinkRoles::SETTING));

    test::Window window;
    QStringList opened;
    QStringList shown;
    gui::ParagraphMarksCoordinator coordinator(
        test::book(), window.tabs, [&shown](const QString& id) { shown << id; },
        [&opened](const QString& id) { opened << id; });
    editor::BookEditor* editor = openChapter(window, cast);

    // One sign for each kind, in the order of the Workshop
    const QString person = gui::NavigatorPanel::iconIdOf(pm.bookTypes(), *pm.findElement(cast.anna));
    const QString place =
        gui::NavigatorPanel::iconIdOf(pm.bookTypes(), *pm.findElement(cast.forest));
    REQUIRE(person != place);
    QList<editor::ParagraphMark> marks = editor->paragraphMarks();
    REQUIRE(marks.size() == 2);
    CHECK(marks.at(0).paragraph == 0);
    CHECK(marks.at(0).ids == QStringList{cast.anna});
    CHECK(marks.at(0).icon == person);
    CHECK(marks.at(1).ids == QStringList{cast.forest});
    CHECK(marks.at(1).icon == place);

    // A sign chosen opens its card, not a material
    emit editor->paragraphMarkClicked(cast.forest);
    CHECK(opened == QStringList{cast.forest});
    CHECK(shown.isEmpty());

    // A card linked with the paragraph joins the sign of its kind
    REQUIRE(pm.linkParagraph(cast.eldrion, cast.novel.chapterId, 1, PARAGRAPHS.at(0),
                             core::LinkRoles::APPEARS));
    settle();
    marks = editor->paragraphMarks();
    REQUIRE(marks.size() == 2);
    CHECK(marks.at(0).ids == QStringList{cast.anna, cast.eldrion});

    // Off in Settings > Cards: no signs, until they are on again
    signs.set(false);
    settle();
    CHECK(editor->paragraphMarks().isEmpty());
    signs.set(true);
    settle();
    CHECK(editor->paragraphMarks().size() == 2);

    // Taken off the paragraph, the card keeps its link with the chapter
    REQUIRE(pm.unlinkParagraph(cast.forest, cast.novel.chapterId));
    settle();
    CHECK(editor->paragraphMarks().size() == 1);
    const core::ElementLink* link = pm.findElement(cast.forest)->linkTo(cast.novel.chapterId);
    REQUIRE(link != nullptr);
    CHECK(link->paragraph == 0);
}

TEST_CASE("Paragraph cards: Book > Link Paragraph with Card... and Open Card at Cursor",
          "[gui][cards][command]") {
    gui::registerAllIcons();
    const ChangedSetting paragraphs(core::LinkRoles::PARAGRAPHS_SETTING, true);
    gui::registerAllCommands(test::commands(), gui::CommandCallbacks{});
    gui::CommandRegistry& registry = test::commands();
    test::OpenNovel novel(QStringLiteral("Cards"));
    novel.pm.setChapterContent(novel.chapterId, test::kmlOf(PARAGRAPHS));
    REQUIRE(novel.pm.saveChapterContent(novel.chapterId));

    test::Window window;
    editor::BookEditor* keys = nullptr;
    QStringList opened;
    CardsInTextCoordinator coordinator(
        test::book(), registry, window.tabs, [&keys]() { return keys; },
        [&opened](const QString& id) { opened << id; }, &window.window);
    coordinator.connectCommands();
    gui::Command* link = registry.getCommand(CardsInTextCoordinator::LINK_COMMAND);
    gui::Command* open = registry.getCommand(CardsInTextCoordinator::OPEN_COMMAND);
    REQUIRE(link != nullptr);
    REQUIRE(open != nullptr);

    // Without a text of the book with the keys
    CHECK(coordinator.whyNotLink(nullptr) == QStringLiteral("The cursor is not in a text of the book"));
    CHECK(link->disabledReason() == coordinator.whyNotLink(nullptr));
    CHECK(coordinator.whyNotOpen(nullptr) == QStringLiteral("The cursor is not in a text of the book"));

    // A book without cards
    window.navigator.onElementSelected(novel.chapterId, QStringLiteral("The Storm"));
    REQUIRE(window.tabs->count() == 1);
    keys = window.editor(0)->getBookEditor();
    settle();
    keys->setCursorPosition({0, 3});
    CHECK(coordinator.whyNotLink(keys) == QStringLiteral("The book has no cards yet"));

    // With them: the paragraph at the cursor, not an empty one, while links name paragraphs
    const QString anna = addCard("character", "Anna Wolska");
    const QString eldrion = addCard("character", "Eldrion");
    CHECK(coordinator.canLink(*keys));
    keys->setCursorPosition({2, 0});
    CHECK(coordinator.whyNotLink(keys) == QStringLiteral("The paragraph at the cursor is empty"));
    keys->setCursorPosition({0, 3});
    {
        const ChangedSetting off(core::LinkRoles::PARAGRAPHS_SETTING, false);
        CHECK(coordinator.whyNotLink(keys) ==
              QStringLiteral("Links name no paragraphs (Settings > Navigator)"));
    }
    keys->setReadOnly(true);
    CHECK_FALSE(coordinator.canLink(*keys));
    keys->setReadOnly(false);

    // The card whose name the cursor is in, in any of its forms
    const auto putCursor = [keys](int paragraph, const char* word) {
        const auto at = static_cast<int>(PARAGRAPHS.at(paragraph).indexOf(text(word)));
        REQUIRE(at >= 0);
        keys->setCursorPosition({paragraph, at + 2});
    };
    putCursor(0, "Wolską");
    CHECK(coordinator.cardNamedAtCursor(*keys) == anna);
    CHECK(coordinator.cardAtCursor(*keys) == anna);
    CHECK(coordinator.canOpen(*keys));
    putCursor(0, "Spotkał");
    CHECK(coordinator.cardAtCursor(*keys).isEmpty());
    CHECK(coordinator.whyNotOpen(keys) ==
          QStringLiteral("The cursor is in no name of a card, and its paragraph has no cards"));
    CHECK_FALSE(coordinator.openCardAtCursor(keys));

    SECTION("Link Paragraph with Card... with the card at the cursor chosen first") {
        putCursor(3, "Eldrion");
        QString chosen;
        CHECK(answeringLink([&]() { link->execute(); },
                            [&chosen](ParagraphCardDialog& window) {
                                chosen = window.choice().cardId;
                                window.acceptButton()->click();
                            }));
        CHECK(chosen == eldrion);
        const core::ElementLink* saved =
            test::book().findElement(eldrion)->linkTo(novel.chapterId);
        REQUIRE(saved != nullptr);
        CHECK(saved->paragraph == 4);
        CHECK(saved->excerpt == core::LinkRoles::excerptOf(PARAGRAPHS.at(3)));

        // Open Card at Cursor: outside a name, the first card of the paragraph
        putCursor(3, "milczał");
        CHECK(coordinator.cardNamedAtCursor(*keys).isEmpty());
        CHECK(coordinator.cardAtCursor(*keys) == eldrion);
        open->execute();
        CHECK(opened == QStringList{eldrion});
    }

    SECTION("the context menu has them after Create Card, Open Card where it can open one") {
        const QString linkText = QCoreApplication::translate("CommandRegistrar",
                                                             "Link Paragraph &with Card...");
        const QString openText =
            QCoreApplication::translate("CommandRegistrar", "&Open Card at Cursor");
        putCursor(0, "Wolską");
        QStringList items = test::runPopupMenu([keys]() { keys->showContextMenuAtCursor(); });
        CHECK(items.contains(linkText));
        CHECK(items.contains(openText));
        CHECK(items.indexOf(linkText) + 1 == items.indexOf(openText));
        putCursor(0, "Spotkał");
        items = test::runPopupMenu([keys]() { keys->showContextMenuAtCursor(); });
        CHECK(items.contains(linkText));
        CHECK_FALSE(items.contains(openText));

        // The item opens the card
        putCursor(0, "Annę");
        test::runPopupMenu([keys]() { keys->showContextMenuAtCursor(); }, openText);
        CHECK(opened == QStringList{anna});
    }
}
