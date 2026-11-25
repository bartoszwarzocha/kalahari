// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file paragraph_card_dialog.h
/// @brief Link Paragraph with Card: a card of a character, a location or an item linked with a
/// paragraph of a chapter, in a role

#ifndef KALAHARI_GUI_DIALOGS_PARAGRAPH_CARD_DIALOG_H
#define KALAHARI_GUI_DIALOGS_PARAGRAPH_CARD_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QTreeWidgetItem;

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::gui::dialogs {

class RowList;

/// @brief What the writer chose in the window Link Paragraph with Card
struct ParagraphCardChoice {
    QString cardId;  ///< The card; empty: none
    QString role;    ///< Its role in the paragraph (core::LinkRoles::ofCard())
};

/// @brief Link Paragraph with Card: the card and its role in the paragraph
///
/// The heading names the chapter and the paragraph, with its first words. The cards of the
/// book are listed by their names, with their kinds; typing filters them by their names, the
/// forms of the names and their other names in the book's dictionary ("Eldriona" finds
/// Eldrion). The roles are those of the kind of the card chosen in the chapters ("point of
/// view", "appears", "mentioned"; "setting" for a location), the one it has in the chapter or
/// the one its kind suggests first. A card links with one paragraph of a chapter: a sentence
/// says so when its link names another paragraph, which it leaves for this one.
///
/// The filter has the keys: the letters filter, the arrows choose in the list, Tab goes to the
/// role, Enter links and Esc closes. On a small screen only the list scrolls.
class ParagraphCardDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param elementId The chapter
    /// @param paragraph The paragraph, from 1
    /// @param paragraphText Its text
    /// @param cardId The card chosen first (the card whose name the cursor is in); empty: the
    ///        first one
    /// @param parent Parent widget
    ParagraphCardDialog(const core::ProjectManager& book, const QString& elementId,
                        int paragraph, const QString& paragraphText, const QString& cardId,
                        QWidget* parent = nullptr);

    /// @brief What the writer chose
    [[nodiscard]] ParagraphCardChoice choice() const;

    /// @brief Make the item of card @p cardId the current one
    void setCurrentCard(const QString& cardId);

    /// @brief The filter above the list of the cards
    [[nodiscard]] QLineEdit* filterEdit() const { return m_filter; }
    /// @brief The cards
    [[nodiscard]] RowList* cardList() const { return m_cards; }
    /// @brief The roles of the card chosen
    [[nodiscard]] QComboBox* roleCombo() const { return m_role; }
    /// @brief The sentence on the paragraph the card's link names now; hidden when there is
    /// nothing to say
    [[nodiscard]] QLabel* noteLabel() const { return m_note; }

protected:
    /// @brief The arrows of the filter choose in the list; the letters typed in the list go to
    /// the filter
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The cards of the book, by their names
    void fillCards();

    /// @brief Show the cards whose names have @p text
    void filter(const QString& text);

    /// @brief Whether card item @p item has @p wanted in its title, or in a form of its name
    ///        or of its other names in the book's dictionary
    [[nodiscard]] bool matches(const QTreeWidgetItem* item, const QString& wanted) const;

    /// @brief The roles of the kind of the card chosen
    void fillRoles();

    /// @brief The sentence on the paragraph the link of the card chosen names now
    void showNote();

    /// @brief The card chosen; nullptr for none
    [[nodiscard]] const core::ProjectElement* currentCard() const;

    /// @brief The first item of the list that is shown
    [[nodiscard]] QTreeWidgetItem* firstShown() const;

    const core::ProjectManager& m_book;  ///< The book of its window
    QString m_elementId;
    int m_paragraph = 0;
    QLineEdit* m_filter = nullptr;
    RowList* m_cards = nullptr;
    QComboBox* m_role = nullptr;
    QLabel* m_note = nullptr;
};

/// @brief Open the window Link Paragraph with Card for paragraph @p paragraph (from 1) of
/// chapter @p elementId of @p book, whose text is @p paragraphText, with card @p cardId chosen
/// first, and save the link (core::ProjectManager::linkParagraph()); a link that cannot be
/// saved is said so
/// @return The card linked; empty when the window was closed or nothing was saved
QString linkParagraphWithCard(core::ProjectManager& book, const QString& elementId,
                              int paragraph, const QString& paragraphText,
                              const QString& cardId, QWidget* parent);

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_PARAGRAPH_CARD_DIALOG_H
