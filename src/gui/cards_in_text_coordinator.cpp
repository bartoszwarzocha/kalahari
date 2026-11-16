// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cards_in_text_coordinator.cpp
/// @brief The cards of characters, locations and items in the texts of the book

#include "kalahari/gui/cards_in_text_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/card_name_index.h"
#include "kalahari/core/card_relations.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/paragraph_card_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/paragraph_cards_box.h"
#include "kalahari/gui/read_only_book.h"

#include <QTabWidget>

#include <optional>

namespace kalahari::gui {

CardsInTextCoordinator::CardsInTextCoordinator(core::ProjectManager& book,
                                               CommandRegistry& commands,
                                               QTabWidget* centralTabs,
                                               TextWithKeys textWithKeys, CardOpener openCard,
                                               QWidget* window, QObject* parent)
    : QObject(parent),
      m_book(book),
      m_commands(commands),
      m_centralTabs(centralTabs),
      m_textWithKeys(std::move(textWithKeys)),
      m_openCard(std::move(openCard)),
      m_window(window) {
    // The cards, their names and their links change what the commands can do
    for (const auto signal : {&core::ProjectManager::linksChanged,
                              &core::ProjectManager::bookDictionaryChanged}) {
        connect(&m_book, signal, this, &CardsInTextCoordinator::updateCommands);
    }
    connect(&m_book, &core::ProjectManager::cardAdded, this,
            &CardsInTextCoordinator::updateCommands);
    if (m_centralTabs != nullptr) {
        connect(&DocumentWindows::of(*m_centralTabs), &DocumentWindows::frontChanged, this,
                &CardsInTextCoordinator::offerCommands);
    }
}

void CardsInTextCoordinator::connectCommands() {
    auto& registry = m_commands;
    const auto text = [this]() { return m_textWithKeys ? m_textWithKeys() : nullptr; };
    if (Command* command = registry.getCommand(LINK_COMMAND)) {
        command->execute = [this, text]() { linkParagraph(text()); };
        command->isEnabled = [this, text]() { return whyNotLink(text()).isEmpty(); };
        command->disabledReason = [this, text]() { return whyNotLink(text()); };
    }
    if (Command* command = registry.getCommand(OPEN_COMMAND)) {
        command->execute = [this, text]() { openCardAtCursor(text()); };
        command->isEnabled = [this, text]() { return whyNotOpen(text()).isEmpty(); };
        command->disabledReason = [this, text]() { return whyNotOpen(text()); };
    }
    updateCommands();
}

// =============================================================================
// The commands
// =============================================================================

QString CardsInTextCoordinator::whyNotLink(const editor::BookEditor* text) const {
    if (!m_book.isProjectOpen()) {
        //: Why Book > Link Paragraph with Card... is greyed out. In Polish: Żadna książka nie
        //: jest otwarta
        return tr("No book is open");
    }
    if (m_book.isReadOnly() || (text != nullptr && text->isReadOnly())) {
        return readOnlyReason();
    }
    if (text == nullptr || !isBookText(elementOf(text))) {
        //: In Polish: Kursor nie jest w tekście książki
        return tr("The cursor is not in a text of the book");
    }
    if (!core::LinkRoles::paragraphsShown()) {
        //: The setting of Settings > Navigator > Materials is off. In Polish: Powiązania nie
        //: wskazują akapitów (Ustawienia > Nawigator)
        return tr("Links name no paragraphs (Settings > Navigator)");
    }
    if (m_book.cardRelations().cards().isEmpty()) {
        //: The book has no cards of characters, locations or items. In Polish: Książka nie ma
        //: jeszcze kart
        return tr("The book has no cards yet");
    }
    const int paragraph = text->cursorPosition().paragraph;
    if (text->paragraphPlainText(static_cast<size_t>(paragraph)).simplified().isEmpty()) {
        //: In Polish: Akapit przy kursorze jest pusty
        return tr("The paragraph at the cursor is empty");
    }
    return {};
}

QString CardsInTextCoordinator::whyNotOpen(const editor::BookEditor* text) const {
    if (!m_book.isProjectOpen()) {
        //: Why Book > Open Card at Cursor is greyed out. In Polish: Żadna książka nie jest
        //: otwarta
        return tr("No book is open");
    }
    if (text == nullptr || elementOf(text).isEmpty()) {
        return tr("The cursor is not in a text of the book");
    }
    if (cardAtCursor(*text).isEmpty()) {
        //: In Polish: Kursor nie jest w imieniu ani nazwie z karty, a jego akapit nie ma kart
        return tr("The cursor is in no name of a card, and its paragraph has no cards");
    }
    return {};
}

QString CardsInTextCoordinator::cardNamedAtCursor(const editor::BookEditor& text) const {
    if (elementOf(&text).isEmpty()) {
        return {};
    }
    const editor::CursorPosition cursor = text.cursorPosition();
    const QString card =
        m_book.cardNames()
            .cardAt(text.paragraphPlainText(static_cast<size_t>(cursor.paragraph)),
                    cursor.offset)
            .card;
    return m_book.findElement(card) != nullptr ? card : QString();  // Not a card deleted
}

QString CardsInTextCoordinator::cardAtCursor(const editor::BookEditor& text) const {
    if (QString named = cardNamedAtCursor(text); !named.isEmpty()) {
        return named;
    }
    const QList<const core::ProjectElement*> cards =
        cardsOfParagraph(m_book, elementOf(&text), text, text.cursorPosition().paragraph);
    return cards.isEmpty() ? QString() : cards.first()->id;
}

QString CardsInTextCoordinator::linkParagraph(editor::BookEditor* text) {
    if (!whyNotLink(text).isEmpty()) {
        return {};
    }
    const int paragraph = text->cursorPosition().paragraph;
    const QString elementId = elementOf(text);
    const QString linked = dialogs::linkParagraphWithCard(
        m_book, elementId, paragraph + 1,
        text->paragraphPlainText(static_cast<size_t>(paragraph)), cardAtCursor(*text),
        m_window);
    if (!linked.isEmpty()) {
        core::Logger::getInstance().info("Cards in the text: paragraph {} of {} linked with {}",
                                         paragraph + 1, elementId.toStdString(),
                                         linked.toStdString());
    }
    return linked;
}

bool CardsInTextCoordinator::openCardAtCursor(const editor::BookEditor* text) const {
    const QString card = text != nullptr ? cardAtCursor(*text) : QString();
    if (card.isEmpty() || !m_openCard) {
        return false;
    }
    m_openCard(card);
    return true;
}

// =============================================================================
// The text in front
// =============================================================================

void CardsInTextCoordinator::offerCommands(QWidget* page) {
    for (const QMetaObject::Connection& connection : std::as_const(m_followed)) {
        disconnect(connection);
    }
    m_followed.clear();
    auto* panel = qobject_cast<EditorPanel*>(page);
    editor::BookEditor* text = panel != nullptr ? panel->getBookEditor() : nullptr;
    if (text == nullptr) {
        return;
    }
    // A text of the book has them; a text of the Workshop opens its cards only
    editor::BookEditor::ContextMenuCardActions actions;
    const QString elementId = elementOf(text);
    if (isBookText(elementId)) {
        actions.link = m_commands.getAction(std::string(LINK_COMMAND));
    }
    if (!elementId.isEmpty()) {
        actions.open = m_commands.getAction(std::string(OPEN_COMMAND));
    }
    text->setContextMenuCardActions(actions);
    // A right click puts the cursor where the menu opens: the commands are as there at once
    m_followed << connect(text, &editor::BookEditor::cursorPositionChanged, this,
                          &CardsInTextCoordinator::updateCommands);
    m_followed << connect(text, &editor::BookEditor::documentChanged, this,
                          &CardsInTextCoordinator::updateCommands);
    updateCommands();
}

void CardsInTextCoordinator::updateCommands() {
    m_commands.updateActionState(LINK_COMMAND);
    m_commands.updateActionState(OPEN_COMMAND);
}

EditorPanel* CardsInTextCoordinator::panelOf(const editor::BookEditor* text) const {
    if (m_centralTabs == nullptr || text == nullptr) {
        return nullptr;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        if (auto* panel = qobject_cast<EditorPanel*>(page);
            panel != nullptr && panel->getBookEditor() == text) {
            return panel;
        }
    }
    return nullptr;
}

QString CardsInTextCoordinator::elementOf(const editor::BookEditor* text) const {
    const EditorPanel* panel = panelOf(text);
    const QString elementId =
        panel != nullptr ? panel->property("elementId").toString() : QString();
    return m_book.isProjectOpen() && m_book.findElement(elementId) != nullptr ? elementId
                                                                              : QString();
}

bool CardsInTextCoordinator::isBookText(const QString& elementId) const {
    const core::BookProject* project = m_book.project();
    const std::optional<core::ElementPlace> at =
        project != nullptr && !elementId.isEmpty() ? project->placeOf(elementId) : std::nullopt;
    return at.has_value() && at->place != core::BookPlace::Workshop;
}

}  // namespace kalahari::gui
