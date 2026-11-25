// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file paragraph_cards_box.h
/// @brief The cards of characters, locations and items linked with the paragraph at the cursor
/// of a chapter, with their roles in it

#ifndef KALAHARI_GUI_PANELS_PARAGRAPH_CARDS_BOX_H
#define KALAHARI_GUI_PANELS_PARAGRAPH_CARDS_BOX_H

#include <QGroupBox>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

class QKeyEvent;
class QLabel;
class QToolButton;
class QTreeWidget;

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui {

/// @brief The cards linked with paragraph @p paragraph (from 0) of @p text, the text of element
///        @p elementId of @p book, in the order of their links: the paragraph of a link is found
///        in the text as it is now (core::LinkRoles::paragraphOf()); none while links name no
///        paragraphs (core::LinkRoles::PARAGRAPHS_SETTING)
[[nodiscard]] QList<const core::ProjectElement*> cardsOfParagraph(
    const core::ProjectManager& book, const QString& elementId, const editor::BookEditor& text,
    int paragraph);

/// @brief The group "Cards of the Paragraph" of the Properties panel
///
/// A row has the role of the card in the paragraph ("point of view") and the card with its
/// color. "+" or Insert asks for the window Link Paragraph with Card (requestLink()), "-" or
/// Delete takes the card of the row off the paragraph, and the card keeps its link with the
/// chapter (core::ProjectManager::unlinkParagraph()); Enter or a double-click opens the card
/// (requestOpenCard()). A book open read-only shows the cards and changes nothing.
class ParagraphCardsBox : public QGroupBox {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param parent Parent widget
    explicit ParagraphCardsBox(core::ProjectManager& book, QWidget* parent = nullptr);
    /// @note What the list does while the box is destroyed does not reach the box
    ~ParagraphCardsBox() override;

    ParagraphCardsBox(const ParagraphCardsBox&) = delete;
    ParagraphCardsBox& operator=(const ParagraphCardsBox&) = delete;
    ParagraphCardsBox(ParagraphCardsBox&&) = delete;
    ParagraphCardsBox& operator=(ParagraphCardsBox&&) = delete;

    /// @brief Show cards @p cardIds, linked with paragraph @p paragraph (from 0) of element
    ///        @p elementId; the current row stays with its card
    void showParagraph(const QString& elementId, int paragraph, const QStringList& cardIds);

    /// @brief Show the rows again as the book has the cards and their roles
    void refresh();

    /// @brief The card of the current row; empty for none
    [[nodiscard]] QString currentCard() const;

    /// @brief The element whose paragraph it shows
    [[nodiscard]] const QString& elementId() const { return m_elementId; }
    /// @brief The paragraph it shows, from 0
    [[nodiscard]] int paragraph() const { return m_paragraph; }
    /// @brief The cards it shows, in their order
    [[nodiscard]] const QStringList& cards() const { return m_cards; }

    /// @brief The cards with their roles
    [[nodiscard]] QTreeWidget* list() const { return m_list; }
    /// @brief "+": the window Link Paragraph with Card
    [[nodiscard]] QToolButton* addButton() const { return m_addButton; }
    /// @brief "-": the card of the current row off the paragraph
    [[nodiscard]] QToolButton* removeButton() const { return m_removeButton; }
    /// @brief The sentence shown without cards
    [[nodiscard]] QLabel* emptyLabel() const { return m_emptyLabel; }

public slots:
    /// @brief Take the card of the current row off the paragraph
    void unlinkCurrent();

signals:
    /// @brief "+" or Insert: the window Link Paragraph with Card for the paragraph
    void requestLink();

    /// @brief Open card @p cardId
    void requestOpenCard(const QString& cardId);

protected:
    /// @brief Insert, Delete and Enter of the list
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief What @p key does in the list: Insert links a card, Delete takes one off, Enter
    ///        opens it; nothing for another key
    [[nodiscard]] std::function<void()> keyAction(const QKeyEvent* key);

    /// @brief The list as high as its rows; the sentence without them
    void fitList();

    /// @brief The buttons follow the current row and a book open read-only
    void updateButtons();

    QString m_elementId;
    int m_paragraph = -1;
    QStringList m_cards;
    QTreeWidget* m_list = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
    core::ProjectManager& m_book;  ///< The book of its window
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PARAGRAPH_CARDS_BOX_H
