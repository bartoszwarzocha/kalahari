// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_chapters_box.h
/// @brief The chapters a card of a character, a location or an item is in

#ifndef KALAHARI_GUI_WIDGETS_CARD_CHAPTERS_BOX_H
#define KALAHARI_GUI_WIDGETS_CARD_CHAPTERS_BOX_H

#include <QGroupBox>
#include <QString>

class QLabel;
class QMenu;
class QToolButton;
class QTreeWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The group "In Chapters" of the full card: the chapters the card is in, in the order
/// of the book
///
/// A row has the chapter with its icon, the role of the card in it with the paragraph
/// ("appears · paragraph 4", core::LinkRoles::ofCard()) and the note of the link. The link is
/// saved with the card, beside its relations, which stay as they are.
///
/// "+" or Insert opens Link with the chapters of the book (dialogs::chooseChapters()), F2 opens
/// it at the current chapter, and "-" or Delete takes the chapter out. Enter or a double-click
/// opens the chapter (requestOpenChapter()); the menu of a row has it all too. A book open
/// read-only shows the chapters and changes nothing.
class CardChaptersBox : public QGroupBox {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param parent Parent widget
    explicit CardChaptersBox(core::ProjectManager& book, QWidget* parent = nullptr);
    /// @note What the list does while the box is destroyed does not reach the box
    ~CardChaptersBox() override;

    CardChaptersBox(const CardChaptersBox&) = delete;
    CardChaptersBox& operator=(const CardChaptersBox&) = delete;
    CardChaptersBox(CardChaptersBox&&) = delete;
    CardChaptersBox& operator=(CardChaptersBox&&) = delete;

    /// @brief Show the chapters of card @p cardId
    void showCard(const QString& cardId);

    /// @brief Show the chapters again as the book has them; the current row stays
    void refresh();

    /// @brief The chapter of the current row; empty for none
    [[nodiscard]] QString currentChapter() const;

    /// @brief The chapters
    [[nodiscard]] QTreeWidget* list() const { return m_list; }
    /// @brief "+": the window Link with the chapters
    [[nodiscard]] QToolButton* addButton() const { return m_addButton; }
    /// @brief "-": the current chapter goes
    [[nodiscard]] QToolButton* removeButton() const { return m_removeButton; }
    /// @brief The sentence shown without chapters
    [[nodiscard]] QLabel* emptyLabel() const { return m_emptyLabel; }
    /// @brief The menu of a row, filled when it opens
    [[nodiscard]] QMenu* rowMenu() const { return m_menu; }

public slots:
    /// @brief Open Link with the chapters of the book, at the current chapter
    void editChapters();

    /// @brief Take the current chapter out of the card
    void removeChapter();

signals:
    /// @brief Open chapter @p elementId
    void requestOpenChapter(const QString& elementId);

protected:
    /// @brief Insert, Delete, F2 and Enter of the list
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Fill the menu of the current row as it opens
    void fillMenu();

    /// @brief The list as high as its rows; the sentence without them
    void fitList();

    /// @brief The buttons follow the current row and a book open read-only
    void updateButtons();

    /// @brief Make the row of chapter @p chapterId the current one
    void selectChapter(const QString& chapterId);

    QString m_cardId;
    QTreeWidget* m_list = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
    QMenu* m_menu = nullptr;
    int m_settingsListener = 0;    ///< core::SettingsManager::subscribe() id
    core::ProjectManager& m_book;  ///< The book of its window
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_CARD_CHAPTERS_BOX_H
