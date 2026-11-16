// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cursor_card_box.h
/// @brief The card at the cursor in the Properties panel: the card whose name the cursor of a
/// text of the book is in

#ifndef KALAHARI_GUI_PANELS_CURSOR_CARD_BOX_H
#define KALAHARI_GUI_PANELS_CURSOR_CARD_BOX_H

#include <QGroupBox>
#include <QString>

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

class RelationsBox;

/// @brief The group "Card at the Cursor" of the Properties panel, under the data of the text
///
/// It shows the card of a character, a location or an item whose name the cursor is in, in
/// any of its forms (core::CardNameIndex): its portrait, its name, its kind with the value of
/// the field beside it ("Character · main"), In Short, its relations with the other cards
/// (Enter shows the other card, requestSelectCard()) and Open Card, which opens the full card
/// (requestOpenCard()). The gear at its title opens Settings > Cards, where Card at the Cursor
/// (cardsAtCursor()) chooses what the Properties panel shows of the cards at the cursor.
class CursorCardBox : public QGroupBox {
    Q_OBJECT

public:
    /// @brief What the Properties panel shows of the cards at the cursor
    enum class Shown {
        NamesAndParagraphs,  ///< The card whose name the cursor is in and the paragraph's cards
        Paragraphs,          ///< Only the cards linked with the paragraph
        None                 ///< None
    };

    /// @brief The setting of what the panel shows: "names", "paragraphs" or "none"
    static constexpr const char* SETTING = "cards.atCursor";
    /// @brief The values of SETTING, in the order of Shown
    static constexpr const char* NAMES = "names";
    static constexpr const char* PARAGRAPHS = "paragraphs";
    static constexpr const char* NONE = "none";
    /// @brief Side of the portrait
    static constexpr int PORTRAIT = 48;

    /// @brief What the panel shows of the cards at the cursor, as the setting has it; an
    /// unknown value: the names and the paragraphs
    [[nodiscard]] static Shown cardsAtCursor();

    /// @param book The book of its window
    /// @param parent Parent widget
    explicit CursorCardBox(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief Show card @p cardId of the open book; hidden for none or another element
    void showCard(const QString& cardId);

    /// @brief The card it shows; empty for none
    [[nodiscard]] QString cardId() const { return m_cardId; }

    /// @brief Show the card again as the book has it
    void refresh();

    /// @brief The portrait
    [[nodiscard]] QLabel* portraitLabel() const { return m_portrait; }
    /// @brief The name of the card
    [[nodiscard]] QLabel* nameLabel() const { return m_name; }
    /// @brief The kind with the value of the field beside it: "Character · main"
    [[nodiscard]] QLabel* subtitleLabel() const { return m_subtitle; }
    /// @brief In Short; hidden when the card has none
    [[nodiscard]] QLabel* shortLabel() const { return m_short; }
    /// @brief The relations with the other cards
    [[nodiscard]] RelationsBox* relationsBox() const { return m_relations; }
    /// @brief Open Card
    [[nodiscard]] QPushButton* openButton() const { return m_openButton; }
    /// @brief The gear: Settings > Cards
    [[nodiscard]] QToolButton* settingsButton() const { return m_settingsButton; }

signals:
    /// @brief Open the full card of @p cardId
    void requestOpenCard(const QString& cardId);

    /// @brief Show card @p cardId in the Navigator: the other card of a relation
    void requestSelectCard(const QString& cardId);

    /// @brief Card @p cardId was made with a relation: the Navigator shows it
    void cardAdded(const QString& cardId);

    /// @brief Open Settings > Cards
    void requestSettings();

protected:
    /// @brief The gear follows the title of the group
    void resizeEvent(QResizeEvent* event) override;

private:
    /// @brief The portrait beside the name and the kind, in @p layout
    void createHeader(QVBoxLayout* layout);

    QString m_cardId;
    QToolButton* m_settingsButton = nullptr;
    QLabel* m_portrait = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_subtitle = nullptr;
    QLabel* m_short = nullptr;
    RelationsBox* m_relations = nullptr;
    QPushButton* m_openButton = nullptr;
    core::ProjectManager& m_book;  ///< The book of its window
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_CURSOR_CARD_BOX_H
