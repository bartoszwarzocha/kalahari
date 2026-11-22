// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file figures_panel.h
/// @brief The panel Images and Tables: the images and the tables of the text in front or of
///        the book, with their captions

#ifndef KALAHARI_GUI_PANELS_FIGURES_PANEL_H
#define KALAHARI_GUI_PANELS_FIGURES_PANEL_H

#include <kalahari/editor/text_figures.h>

#include <QColor>
#include <QHash>
#include <QImage>
#include <QString>
#include <QWidget>

#include <array>
#include <functional>
#include <vector>

class QAbstractButton;
class QButtonGroup;
class QLabel;
class QLineEdit;
class QListView;
class QMenu;
class QStandardItemModel;
class QToolButton;

namespace kalahari::gui {

/// @brief An image or a table as the panel lists it
struct FigureEntry {
    QString elementId;   ///< Its text in the book; empty: a document outside it
    QString textTitle;   ///< The title of its text (or document)
    int textOrder = 0;   ///< Where its text is in the book (the scope Book)
    editor::FigureInText figure;  ///< What it is, where, and its caption

    bool operator==(const FigureEntry&) const = default;

    /// @brief What tells it from every other figure of the book
    [[nodiscard]] QString key() const;
};

/// @brief Which figures the panel lists
enum class FigureScope {
    Text,  ///< Those of the text in front
    Book,  ///< Those of every text of the book, closed ones too, in reading order
};

/// @brief Which kinds the panel shows
enum class FigureKinds {
    All,     ///< Images and tables
    Images,  ///< Only the images
    Tables,  ///< Only the tables
};

/// @brief What the writer asks the panel to do with a figure, in its text
enum class FigureAction {
    Properties,  ///< Image Properties... or Table Properties...
    Caption,     ///< Insert Caption... (the window of its caption)
};

/// @brief The panel Images and Tables
///
/// Lists the figures it is given as cards: a picture of the image (or a drawing of the table's
/// rows and columns), the label and the number of its caption in the color of its kind (from
/// the theme: Settings > Editor > Colors), and the text of its caption; a figure without a
/// caption says so, with the name of the image or the size of the table. In the scope Book the
/// cards are under the title of each text. A search in the captions and the names, the kinds
/// (with their counts) and the scope narrow the list. The panel changes no figure itself: it
/// says what the writer asks for, and shows the figures it is given next.
///
/// Keys in the list: Up and Down select a figure (and the text goes to it), Enter goes to it in
/// its text with the keys there, the menu key or Shift+F10 opens its menu (Go To, Properties,
/// Insert Caption, Copy Caption). Esc goes back to the text from anywhere in the panel; Down in
/// the search goes to the list.
class FiguresPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief The setting: the pictures of the images on the cards
    static constexpr const char* PICTURES_SETTING = "editor.figures.panelPictures";
    /// @brief The setting: the lines of a caption's text on its card
    static constexpr const char* LINES_SETTING = "editor.figures.panelLines";
    /// @brief The fewest and the most lines of a caption on its card
    static constexpr int MIN_LINES = 1;
    static constexpr int MAX_LINES = 6;
    /// @brief The lines of a caption by default
    static constexpr int DEFAULT_LINES = 2;

    /// @brief The theme colors of the kinds (Settings > Editor > Colors)
    static constexpr const char* IMAGE_COLOR_KEY = "figuresPanelImage";
    static constexpr const char* TABLE_COLOR_KEY = "figuresPanelTable";

    /// @brief Gives the picture of an image as it is shown, in these pixels (null: none)
    using PictureSource = std::function<QImage(const FigureEntry& entry, const QSize& pixels)>;

    /// @param parent Parent widget
    explicit FiguresPanel(QWidget* parent = nullptr);
    ~FiguresPanel() override;
    FiguresPanel(const FiguresPanel&) = delete;
    FiguresPanel& operator=(const FiguresPanel&) = delete;
    FiguresPanel(FiguresPanel&&) = delete;
    FiguresPanel& operator=(FiguresPanel&&) = delete;

    /// @brief Where the pictures of the images come from
    void setPictureSource(PictureSource source);

    /// @brief The figures to list (the panel narrows them as the search and the kinds say)
    void setEntries(const std::vector<FigureEntry>& entries);

    /// @brief The figures it was given
    [[nodiscard]] const std::vector<FigureEntry>& entries() const { return m_entries; }

    /// @brief The figures it shows, in their order
    [[nodiscard]] std::vector<FigureEntry> shownEntries() const;

    /// @brief Whose figures it lists
    [[nodiscard]] FigureScope scope() const { return m_scope; }
    void setScope(FigureScope scope);

    /// @brief Whether the whole book can be listed (a book is open)
    void setBookScopeAvailable(bool available);

    /// @brief What the panel says without figures: whether a text is in front
    void setDocumentState(bool inFront);

    /// @brief The name of the text in front on the button of the scope: its kind ("Chapter",
    ///        "Story"); empty: "Text"
    void setTextName(const QString& name);

    /// @brief Whether its figures are only read (a book open read-only): Properties and
    ///        Insert Caption are greyed out then
    void setReadOnly(bool readOnly);
    [[nodiscard]] bool isReadOnly() const { return m_readOnly; }

    /// @brief The kinds shown
    [[nodiscard]] FigureKinds kinds() const { return m_kinds; }
    void setKinds(FigureKinds kinds);

    /// @brief The search text
    [[nodiscard]] QString searchText() const;
    void setSearchText(const QString& text);

    /// @brief Select a figure's card, scrolled into view (empty: none)
    void selectEntry(const QString& key);

    /// @brief The key of the selected figure (empty: none)
    [[nodiscard]] QString selectedKey() const;

    /// @brief Give the keys to the list, the figure @p key selected (else the selected one,
    ///        else the first one)
    void focusList(const QString& key = QString());

    /// @brief Whether the keys are in the panel
    [[nodiscard]] bool hasFocusInside() const;

    /// @brief The colors of the kinds from the current theme, and the settings of the cards
    void applySettings();

    /// @brief The color of a kind, as the cards show it
    [[nodiscard]] QColor kindColor(editor::FigureKind kind) const;

    /// @brief The picture of an image's card, from the source and kept (null: none, or the
    ///        cards show no pictures)
    [[nodiscard]] QImage pictureOf(const FigureEntry& entry, const QSize& pixels);

    /// @brief The menu of the figure @p key, as the menu key opens it (for the tests);
    ///        nullptr: no such figure shown. The caller owns it.
    [[nodiscard]] QMenu* createMenu(const QString& key);

    /// @brief The list of the cards
    [[nodiscard]] QListView* list() const { return m_list; }
    [[nodiscard]] QLineEdit* searchEdit() const { return m_search; }
    [[nodiscard]] QLabel* emptyLabel() const { return m_empty; }
    [[nodiscard]] QAbstractButton* kindButton(FigureKinds kinds) const;
    [[nodiscard]] QAbstractButton* scopeButton(FigureScope scope) const;

signals:
    /// @brief A figure was selected (a click, the arrows): go to it in the text in front
    void entryActivated(const kalahari::gui::FigureEntry& entry);
    /// @brief Going to a figure in its text was asked for, with the keys there (Enter, a
    ///        double click, Go To)
    void openRequested(const kalahari::gui::FigureEntry& entry);
    /// @brief Something to do with a figure in its text was asked for (its menu)
    void actionRequested(const kalahari::gui::FigureEntry& entry,
                         kalahari::gui::FigureAction action);
    /// @brief The writer asked to go back to the text (Esc)
    void editorFocusRequested();
    /// @brief The writer chose another scope
    void scopeChanged(kalahari::gui::FigureScope scope);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// @brief The rows of the figures shown, the selected one kept
    void rebuild();
    /// @brief The rows of the figures shown, the figure @p kept selected when it is shown
    void rebuild(const QString& kept);
    /// @brief The counts on the buttons of the kinds
    void updateCounts();
    /// @brief What the panel says without a card
    void updateEmptyText(bool anyShown);
    /// @brief The figure of a row (nullptr: a heading, or none)
    [[nodiscard]] const FigureEntry* entryAt(int row) const;
    [[nodiscard]] const FigureEntry* selectedEntry() const;
    /// @brief The keys of the list (see the class)
    bool listKeyPressed(const QKeyEvent* event);
    /// @brief The menu of the selected figure, at the card or under the mouse
    void showMenu(const QPoint& globalPos);
    /// @brief The texts of the buttons of the scope
    void updateScopeText();

    std::vector<FigureEntry> m_entries;
    std::vector<int> m_rowEntries;  ///< For each row the index of its entry; -1: a heading
    FigureScope m_scope = FigureScope::Text;
    FigureKinds m_kinds = FigureKinds::All;
    bool m_readOnly = false;
    bool m_inFront = false;
    bool m_bookAvailable = false;
    bool m_updating = false;  ///< The list is made again: no figure activated
    bool m_showPictures = true;  ///< The cards show the pictures of the images
    QString m_textName;
    std::array<QColor, 2> m_kindColors;  ///< Image, table
    PictureSource m_pictureSource;
    QHash<QString, QImage> m_pictures;  ///< The pictures of the cards, by image and size

    QLineEdit* m_search = nullptr;
    QButtonGroup* m_kindGroup = nullptr;
    std::array<QToolButton*, 3> m_kindButtons{};  ///< All, images, tables
    QButtonGroup* m_scopeGroup = nullptr;
    QToolButton* m_textScope = nullptr;
    QToolButton* m_bookScope = nullptr;
    QListView* m_list = nullptr;
    QStandardItemModel* m_model = nullptr;
    QLabel* m_empty = nullptr;
    int m_settingsListener = 0;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_FIGURES_PANEL_H
