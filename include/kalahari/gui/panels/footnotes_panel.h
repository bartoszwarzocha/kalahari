// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file footnotes_panel.h
/// @brief The Footnotes panel: the footnotes and endnotes of the text in front or of the book

#ifndef KALAHARI_GUI_PANELS_FOOTNOTES_PANEL_H
#define KALAHARI_GUI_PANELS_FOOTNOTES_PANEL_H

#include <kalahari/editor/text_footnotes.h>

#include <QColor>
#include <QString>
#include <QWidget>

#include <array>
#include <vector>

class QAbstractButton;
class QButtonGroup;
class QContextMenuEvent;
class QLabel;
class QLineEdit;
class QListView;
class QMenu;
class QStandardItemModel;
class QToolButton;

namespace kalahari::gui {

class CommandRegistry;

/// @brief A note as the Footnotes panel lists it
struct FootnoteEntry {
    QString elementId;   ///< Its text in the book; empty: a document outside it
    QString textTitle;   ///< The title of its text (or document)
    int textOrder = 0;   ///< Where its text is in the book (the scope Book)
    QString id;          ///< Its id in its text
    editor::NoteKind kind = editor::NoteKind::Footnote;
    QString number;      ///< As its text shows it: "3", "ii"
    QString text;        ///< Its text, plain (editor::notePlainText())
    QString before;      ///< The words of its paragraph before its number (editor::lastWords())

    bool operator==(const FootnoteEntry&) const = default;

    /// @brief What tells it from every other note of the book
    [[nodiscard]] QString key() const { return elementId + QLatin1Char('/') + id; }
};

/// @brief Which notes the panel lists
enum class FootnoteScope {
    Text,  ///< Those of the text in front
    Book,  ///< Those of every text of the book, closed ones too, in reading order
};

/// @brief Which kinds of notes the panel shows
enum class FootnoteKinds {
    All,        ///< Footnotes and endnotes
    Footnotes,  ///< Only the footnotes
    Endnotes,   ///< Only the endnotes
};

/// @brief The Footnotes panel
///
/// Lists the notes it is given as cards: the number and the kind (a bar in its color, from
/// the theme: Settings > Editor > Colors), the words before the number in its text and the
/// beginning of its text (Settings > Editor > Writing, group Footnotes and Endnotes); in the
/// scope Book under the title of each text. A search in their texts, the kinds (with their
/// counts) and the scope narrow the list. The panel changes no note itself: it says what the
/// writer asks for, and shows the notes it is given next.
///
/// Keys in the list: Up and Down select a note (and the text goes to its number), Enter or F2
/// edits it (its frame in the text), Delete deletes it, the menu key or Shift+F10 opens its
/// menu (Edit, Convert to Endnote or Footnote, Copy Text, Delete, Footnote Layout...). Esc goes
/// back to the text from anywhere in the panel; Down in the search goes to the list.
class FootnotesPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief The setting: the words before the number on the cards
    static constexpr const char* BEFORE_SETTING = "editor.footnotes.panelBefore";
    /// @brief The setting: the lines of a note's text on its card
    static constexpr const char* LINES_SETTING = "editor.footnotes.panelLines";
    /// @brief The words before the number on a card
    static constexpr int BEFORE_WORDS = 6;
    /// @brief The fewest and the most lines of a note's text on its card
    static constexpr int MIN_LINES = 1;
    static constexpr int MAX_LINES = 6;

    /// @brief The theme colors of the kinds (Settings > Editor > Colors)
    static constexpr const char* FOOTNOTE_COLOR_KEY = "footnotesPanelFootnote";
    static constexpr const char* ENDNOTE_COLOR_KEY = "footnotesPanelEndnote";

    /// @param commands The commands of its window (Footnote Layout... in the menu)
    /// @param parent Parent widget
    explicit FootnotesPanel(CommandRegistry& commands, QWidget* parent = nullptr);
    ~FootnotesPanel() override;
    FootnotesPanel(const FootnotesPanel&) = delete;
    FootnotesPanel& operator=(const FootnotesPanel&) = delete;
    FootnotesPanel(FootnotesPanel&&) = delete;
    FootnotesPanel& operator=(FootnotesPanel&&) = delete;

    /// @brief The notes to list (the panel narrows them as the search and the kinds say)
    void setEntries(const std::vector<FootnoteEntry>& entries);

    /// @brief The notes it was given
    [[nodiscard]] const std::vector<FootnoteEntry>& entries() const { return m_entries; }

    /// @brief The notes it shows, in their order
    [[nodiscard]] std::vector<FootnoteEntry> shownEntries() const;

    /// @brief Whose notes it lists
    [[nodiscard]] FootnoteScope scope() const { return m_scope; }
    void setScope(FootnoteScope scope);

    /// @brief Whether the whole book can be listed (a book is open)
    void setBookScopeAvailable(bool available);

    /// @brief What the panel says without notes: whether a text is in front, and whether it
    ///        keeps notes (a plain text file does not)
    void setDocumentState(bool inFront, bool notesAllowed);

    /// @brief The name of the text in front on the button of the scope: its kind ("Chapter",
    ///        "Story"); empty: "Text"
    void setTextName(const QString& name);

    /// @brief Whether its notes are only read (a book open read-only): Convert and Delete do
    ///        nothing then
    void setReadOnly(bool readOnly);
    [[nodiscard]] bool isReadOnly() const { return m_readOnly; }

    /// @brief The kinds shown
    [[nodiscard]] FootnoteKinds kinds() const { return m_kinds; }
    void setKinds(FootnoteKinds kinds);

    /// @brief The search text
    [[nodiscard]] QString searchText() const;
    void setSearchText(const QString& text);

    /// @brief Select a note's card, scrolled into view (empty: none)
    void selectEntry(const QString& key);

    /// @brief The key of the selected note (empty: none)
    [[nodiscard]] QString selectedKey() const;

    /// @brief Give the keys to the list, the note @p key selected (else the selected one, else
    ///        the first one)
    void focusList(const QString& key = QString());

    /// @brief Whether the keys are in the panel
    [[nodiscard]] bool hasFocusInside() const;

    /// @brief The colors of the kinds from the current theme, and the settings of the cards
    void applySettings();

    /// @brief The color of a kind, as the cards show it
    [[nodiscard]] QColor kindColor(editor::NoteKind kind) const;

    /// @brief The menu of the note @p key, as the menu key opens it (for the tests); nullptr:
    ///        no such note shown. The caller owns it.
    [[nodiscard]] QMenu* createMenu(const QString& key);

    /// @brief The list of the cards
    [[nodiscard]] QListView* list() const { return m_list; }
    [[nodiscard]] QLineEdit* searchEdit() const { return m_search; }
    [[nodiscard]] QLabel* emptyLabel() const { return m_empty; }
    [[nodiscard]] QAbstractButton* kindButton(FootnoteKinds kinds) const;
    [[nodiscard]] QAbstractButton* scopeButton(FootnoteScope scope) const;

signals:
    /// @brief A note was selected (a click, the arrows): go to its number in its text
    void entryActivated(const kalahari::gui::FootnoteEntry& entry);
    /// @brief Editing a note was asked for (Enter, F2, a double click, Edit)
    void editRequested(const kalahari::gui::FootnoteEntry& entry);
    /// @brief Making a footnote an endnote, or an endnote a footnote, was asked for
    void convertRequested(const kalahari::gui::FootnoteEntry& entry);
    /// @brief Deleting a note was asked for (Delete, Delete in the menu)
    void deleteRequested(const kalahari::gui::FootnoteEntry& entry);
    /// @brief The writer asked to go back to the text (Esc)
    void editorFocusRequested();
    /// @brief The writer chose another scope
    void scopeChanged(kalahari::gui::FootnoteScope scope);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// @brief The rows of the notes shown, the selected one kept
    void rebuild();
    /// @brief The rows of the notes shown, the note @p kept selected when it is shown
    void rebuild(const QString& kept);
    /// @brief The counts on the buttons of the kinds
    void updateCounts();
    /// @brief What the panel says without a card
    void updateEmptyText(bool anyShown);
    /// @brief The note of a row (nullptr: a heading, or none)
    [[nodiscard]] const FootnoteEntry* entryAt(int row) const;
    [[nodiscard]] const FootnoteEntry* selectedEntry() const;
    /// @brief The keys of the list (see the class)
    bool listKeyPressed(const QKeyEvent* event);
    /// @brief The menu of the selected note, at the card or under the mouse
    void showMenu(const QPoint& globalPos);
    /// @brief The texts of the buttons of the scope
    void updateScopeText();

    std::vector<FootnoteEntry> m_entries;
    std::vector<int> m_rowEntries;  ///< For each row the index of its entry; -1: a heading
    FootnoteScope m_scope = FootnoteScope::Text;
    FootnoteKinds m_kinds = FootnoteKinds::All;
    bool m_readOnly = false;
    bool m_inFront = false;
    bool m_notesAllowed = true;
    bool m_bookAvailable = false;
    bool m_updating = false;  ///< The list is made again: no note activated
    QString m_textName;
    std::array<QColor, 2> m_kindColors;  ///< Footnote, endnote

    QLineEdit* m_search = nullptr;
    QButtonGroup* m_kindGroup = nullptr;
    std::array<QToolButton*, 3> m_kindButtons{};  ///< All, footnotes, endnotes
    QButtonGroup* m_scopeGroup = nullptr;
    QToolButton* m_textScope = nullptr;
    QToolButton* m_bookScope = nullptr;
    QListView* m_list = nullptr;
    QStandardItemModel* m_model = nullptr;
    QLabel* m_empty = nullptr;
    int m_settingsListener = 0;
    CommandRegistry& m_commands;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_FOOTNOTES_PANEL_H
