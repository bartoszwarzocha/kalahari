// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cards_in_text_coordinator.h
/// @brief The cards of characters, locations and items in the texts of the book: Book > Link
///        Paragraph with Card... and Book > Open Card at Cursor, also on the context menu

#ifndef KALAHARI_GUI_CARDS_IN_TEXT_COORDINATOR_H
#define KALAHARI_GUI_CARDS_IN_TEXT_COORDINATOR_H

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QString>

#include <functional>

class QTabWidget;
class QWidget;

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui {

class CommandRegistry;
class EditorPanel;

/// @brief Book > Link Paragraph with Card... and Book > Open Card at Cursor
///
/// Link Paragraph with Card... opens the window of the paragraph at the cursor of a text of the
/// book (dialogs::ParagraphCardDialog), with the card whose name the cursor is in chosen first;
/// the card links with the paragraph in a role (core::ProjectManager::linkParagraph()). Open
/// Card at Cursor opens the card whose name the cursor is in, in any of its forms
/// (core::ProjectManager::cardNames()), else the first card linked with the paragraph. The
/// context menu of the text in front has them after Create Card
/// (editor::BookEditor::setContextMenuCardActions()); their state follows the cursor.
class CardsInTextCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Id of Book > Link Paragraph with Card... in the registry
    static constexpr const char* LINK_COMMAND = "book.linkParagraphCard";
    /// @brief Id of Book > Open Card at Cursor in the registry
    static constexpr const char* OPEN_COMMAND = "book.openCardAtCursor";

    /// @brief The text that has the keys (none: the keys are elsewhere)
    using TextWithKeys = std::function<editor::BookEditor*()>;
    /// @brief Opens the full card of a character, a location or an item
    using CardOpener = std::function<void(const QString& cardId)>;

    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param centralTabs The tabs of the book window (DocumentWindows gives all the texts)
    /// @param textWithKeys The text that has the keys
    /// @param openCard What a card found does
    /// @param window Over which the window of the link opens
    /// @param parent Parent object
    CardsInTextCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                           QTabWidget* centralTabs, TextWithKeys textWithKeys,
                           CardOpener openCard, QWidget* window, QObject* parent = nullptr);
    ~CardsInTextCoordinator() override = default;
    CardsInTextCoordinator(const CardsInTextCoordinator&) = delete;
    CardsInTextCoordinator& operator=(const CardsInTextCoordinator&) = delete;
    CardsInTextCoordinator(CardsInTextCoordinator&&) = delete;
    CardsInTextCoordinator& operator=(CardsInTextCoordinator&&) = delete;

    /// @brief Give the commands their actions, their states and the reasons why they are
    ///        greyed out (the main window has their states follow the keys too:
    ///        EditCoordinator::addTextCommands() with canLink() and canOpen())
    void connectCommands();

    /// @brief Why the paragraph at the cursor of @p text cannot be linked with a card now
    ///        (empty: it can)
    [[nodiscard]] QString whyNotLink(const editor::BookEditor* text) const;

    /// @brief Whether the paragraph at the cursor of @p text can be linked with a card
    [[nodiscard]] bool canLink(const editor::BookEditor& text) const {
        return whyNotLink(&text).isEmpty();
    }

    /// @brief Why no card can be opened at the cursor of @p text now (empty: one can)
    [[nodiscard]] QString whyNotOpen(const editor::BookEditor* text) const;

    /// @brief Whether a card can be opened at the cursor of @p text
    [[nodiscard]] bool canOpen(const editor::BookEditor& text) const {
        return whyNotOpen(&text).isEmpty();
    }

    /// @brief The card whose name the cursor of @p text is in, in any of its forms; empty for
    ///        none, or for a text outside the book
    [[nodiscard]] QString cardNamedAtCursor(const editor::BookEditor& text) const;

    /// @brief The card Open Card at Cursor opens in @p text: the one whose name the cursor is
    ///        in, else the first card linked with its paragraph (cardsOfParagraph()); empty
    ///        for none
    [[nodiscard]] QString cardAtCursor(const editor::BookEditor& text) const;

    /// @brief Link Paragraph with Card... in @p text at its cursor
    /// @return The card linked; empty when the paragraph cannot be linked, the window was
    ///         closed or nothing was saved (a warning says so)
    QString linkParagraph(editor::BookEditor* text);

    /// @brief Open Card at Cursor in @p text
    /// @return Whether a card was opened
    bool openCardAtCursor(const editor::BookEditor* text) const;

private:
    /// @brief The text of @p page has the commands on its context menu, and they follow its
    ///        cursor
    void offerCommands(QWidget* page);

    /// @brief The commands enabled again, as the text with the keys allows
    void updateCommands();

    /// @brief The tab of @p text, or nullptr
    [[nodiscard]] EditorPanel* panelOf(const editor::BookEditor* text) const;

    /// @brief The element of the book whose text @p text is; empty for a text outside the book
    [[nodiscard]] QString elementOf(const editor::BookEditor* text) const;

    /// @brief Whether @p elementId is a text of a book, not of the Workshop
    [[nodiscard]] bool isBookText(const QString& elementId) const;

    core::ProjectManager& m_book;      ///< The book of its window
    CommandRegistry& m_commands;       ///< The commands of its window
    QTabWidget* m_centralTabs;         ///< The tabs of the documents
    TextWithKeys m_textWithKeys;       ///< The text that has the keys
    CardOpener m_openCard;             ///< What a card found does
    QWidget* m_window;                 ///< Over which the window of the link opens
    QList<QMetaObject::Connection> m_followed;  ///< The cursor of the text in front
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_CARDS_IN_TEXT_COORDINATOR_H
