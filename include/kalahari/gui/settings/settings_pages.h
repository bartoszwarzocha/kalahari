// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file settings_pages.h
/// @brief The pages of the Settings dialog
///
/// Each page builds its controls and binds them to setting keys (SettingsPage);
/// only the theme, icons and diagnostic options add their own steps.

#ifndef KALAHARI_GUI_SETTINGS_SETTINGS_PAGES_H
#define KALAHARI_GUI_SETTINGS_SETTINGS_PAGES_H

#include "kalahari/gui/settings/settings_page.h"

#include <QColor>
#include <QList>

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {
class CommandRegistry;
}  // namespace kalahari::gui

namespace kalahari {
namespace core {
struct Theme;
} // namespace core
namespace editor {
class SpellCheckService;
struct QuickInsertItem;
} // namespace editor
namespace gui {

class ColorConfigWidget;
class FolderField;
class PaletteEdit;
struct ReplacementEntry;

/// @brief General: startup
class GeneralPage : public SettingsPage {
    Q_OBJECT
public:
    explicit GeneralPage(QWidget* parent = nullptr);

signals:
    /// @brief Another length unit was chosen (not applied yet): the length fields of
    ///        the open pages show it at once
    /// @param unit Name of the unit (ui.lengthUnit)
    void lengthUnitChanged(const QString& unit);
};

/// @brief Notifications: where the program's notifications show (the bar at the bottom of
///        the window, the program's window, the system's notifications or the
///        Notifications panel), a sample of them, and their colors in the theme in use
class NotificationsPage : public SettingsPage {
    Q_OBJECT
public:
    explicit NotificationsPage(QWidget* parent = nullptr);

    /// @brief The list of the places
    [[nodiscard]] QComboBox* placeList() const { return m_placeList; }

private:
    QComboBox* m_placeList = nullptr;
};

/// @brief Appearance > General: language and interface font
class AppearanceGeneralPage : public SettingsPage {
    Q_OBJECT
public:
    explicit AppearanceGeneralPage(QWidget* parent = nullptr);
};

/// @brief Appearance > Theme: the theme and its colors
///
/// Colors are stored per theme. A color shows the stored value or else the theme
/// file's default, and is stored only when changed; applying reloads the theme.
class ThemePage : public SettingsPage {
    Q_OBJECT
public:
    explicit ThemePage(QWidget* parent = nullptr);

    std::vector<std::string> apply() override;

    /// @brief Icon colors as currently shown (for the icon preview)
    [[nodiscard]] std::pair<QColor, QColor> iconColors() const;

signals:
    /// @brief An icon color was edited, or the theme changed
    void iconColorsChanged();

private:
    /// @brief Show the selected theme's colors
    void onThemeChanged();

    /// @brief Show the theme file's colors (stored on Apply like any edit)
    void resetToThemeDefaults();

    /// @brief Add a color bound to a key under the selected theme
    /// @param section Group the color is shown in
    /// @param label Visible name
    /// @param toolTip Tooltip
    /// @param keyPrefix Key before the theme name ("themes." or "icons.themes.")
    /// @param keySuffix Key after the theme name (".palette.window")
    /// @param themeColor The color in a theme loaded from its file
    /// @return The color widget
    ColorConfigWidget* addColor(QLayout* section, const QString& label, const QString& toolTip,
                                const std::string& keyPrefix, const std::string& keySuffix,
                                std::function<QColor(const core::Theme&)> themeColor);

    /// @brief The selected theme's name
    [[nodiscard]] std::string themeName() const;

    /// @brief Which palette colors follow the theme's color: those the theme file does not
    ///        list (core::Theme::themeColorPalette), without a color of the user's own
    /// @param reset Also those with a color of the user's own (Reset to Theme Defaults)
    void startFollowing(bool reset);

    /// @brief Show the palette colors that follow the theme's color as it is made from the
    ///        colors shown (the Primary icon color, the selection, the base, the text)
    void followThemeColor();

    QComboBox* m_themeCombo;
    ColorConfigWidget* m_primaryColor = nullptr;
    ColorConfigWidget* m_secondaryColor = nullptr;

    struct ColorField {
        ColorConfigWidget* widget;
        std::function<QColor(const core::Theme&)> themeColor;
        Binding* binding;
    };
    std::vector<ColorField> m_colors;

    /// @brief A palette color of the page
    struct PaletteColor {
        ColorConfigWidget* widget = nullptr;
        std::function<QColor&(core::Theme&)> field;  ///< Its place in a theme
        Binding* binding = nullptr;
        bool follows = false;  ///< Made from the theme's color: none of the user's own is kept
    };
    std::map<std::string, PaletteColor> m_paletteColors;  ///< By the palette's key ("link")
};

/// @brief Appearance > Icons: icon style and sizes
class IconsPage : public SettingsPage {
    Q_OBJECT
public:
    explicit IconsPage(QWidget* parent = nullptr);

    /// @brief Colors the preview draws the icons in
    void setPreviewColors(const QColor& primary, const QColor& secondary);

private:
    void updatePreview();

    QComboBox* m_iconStyle;
    QHBoxLayout* m_previewLayout = nullptr;
    QColor m_primary;
    QColor m_secondary;
};

/// @brief Appearance > Dashboard
class DashboardPage : public SettingsPage {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget* parent = nullptr);
};

/// @brief Editor > General: the font and the typography; Editor > Writing: pasting, opening a
/// chapter, typewriter scrolling and the word count
class EditorGeneralPage : public SettingsPage {
    Q_OBJECT
public:
    /// @brief The pages of the general settings of the editor
    enum class Part {
        Text,    ///< The font, the spacing, the tabs and the scene breaks
        Writing  ///< Pasting, the fields, opening a chapter, typewriter scrolling, the word count
    };

    /// @param commands The commands of its window, whose keys the notes name
    /// @param part The page
    explicit EditorGeneralPage(CommandRegistry& commands, Part part = Part::Text,
                               QWidget* parent = nullptr);

private:
    void createText();
    void createWriting();

    CommandRegistry& m_commands;  ///< The commands of its window
};

/// @brief Editor > Colors: the light and the dark paper, kept for each theme
class EditorColorsPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorColorsPage(QWidget* parent = nullptr);
};

/// @brief Editor > Cursor
class EditorCursorPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorCursorPage(QWidget* parent = nullptr);
};

/// @brief Editor > Spelling: checking as you type, its language and the writer's own words
class EditorSpellingPage : public SettingsPage {
    Q_OBJECT
public:
    /// @param spelling The dictionary of the editors: the page lists its own words and
    ///        changes them when applied (none: the page does not list them)
    /// @param bookOpen Whether a book is open: Open Book Dictionary is enabled
    explicit EditorSpellingPage(editor::SpellCheckService* spelling = nullptr,
                                bool bookOpen = false, QWidget* parent = nullptr);

signals:
    /// @brief Open Book Dictionary was clicked: the settings are applied and the book's
    ///        dictionary shows on its tab
    void bookDictionaryAsked();

protected:
    /// @brief Enter in the field of a new word adds it, and does not close the dialog
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The group of the writer's own words, which the page changes in @p spelling
    void addOwnWords(editor::SpellCheckService* spelling);

    /// @brief The group Book Dictionary: the departures from its rules marked, where the
    ///        panel adds a word, and Open Book Dictionary
    void addBookDictionaryGroup(bool bookOpen);

    /// @brief Add the word of the field to the list
    void addWord();

    /// @brief Take the selected words off the list
    void removeWords();

    /// @brief Enable the buttons that can do something now
    void updateButtons();

    QListWidget* m_words = nullptr;   ///< The writer's own words
    QLineEdit* m_newWord = nullptr;   ///< A word to add
    QPushButton* m_add = nullptr;
    QPushButton* m_remove = nullptr;
};

/// @brief Editor > Pages and Margins, and Editor > Margin Numbering
class EditorPagesPage : public SettingsPage {
    Q_OBJECT
public:
    /// @brief The pages of the pages of the editor
    enum class Part {
        Pages,         ///< The page format, the margins, the text frame border
        MarginNumbers  ///< The working numbers on the left margin
    };

    explicit EditorPagesPage(Part part = Part::Pages, QWidget* parent = nullptr);

private:
    /// @brief The groups of the margin numbering: the working numbers on the left margin
    void addMarginNumbersGroup();
};

/// @brief Editor > Images: how new images go into the text, the captions of new books, the
///        copies of the files and the manuscript for a publisher
///
/// The labels of the captions are a list of the page: Add..., Rename... and Remove change it,
/// and the label of the caption of a new image is one of them. A book has its own labels and
/// numbering from its first caption on (Insert Caption > Numbering...): the page sets those
/// of new books.
class EditorImagesPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorImagesPage(QWidget* parent = nullptr);

signals:
    /// @brief Show: the page Editor > Colors, with the colors of the fields and the frames
    void colorsPageAsked();

private:
    /// @brief The labels as the list has them
    [[nodiscard]] QStringList listedLabels() const;

    /// @brief Fill the list and the label of new images' captions with @p labels
    void showLabels(const QStringList& labels);

    /// @brief Add..., Rename...: a label typed in a window; Remove: the label selected
    void addLabel();
    void renameLabel();
    void removeLabel();

    /// @brief The label of the caption of new images offers the labels of the list
    void fillCaptionLabels();

    /// @brief Rename and Remove for the label selected (one label always stays)
    void updateLabelButtons();

    QListWidget* m_labels = nullptr;        ///< The labels of the captions of new books
    QComboBox* m_captionLabel = nullptr;    ///< The label of the caption of a new image
    QPushButton* m_addLabel = nullptr;
    QPushButton* m_renameLabel = nullptr;
    QPushButton* m_removeLabel = nullptr;
};

/// @brief Editor > Auto-correct: what is changed in the text as it is typed, on three tabs
///
/// While Typing has the replacements of the characters and the text elements typed,
/// Capitals the changes of the capitals and their exceptions (the abbreviations of each
/// language, the words with two capitals), Replacement Table all the pairs of a word and its
/// replacement, the program's own ones and the writer's, each one on or off.
/// Ctrl+Page Down and Ctrl+Page Up go to the next and the previous tab. In the table Space
/// turns the pairs selected on and off, F2 changes a cell, Delete removes the pairs and
/// Insert goes to the field of a new pair.
class EditorAutoCorrectPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorAutoCorrectPage(CommandRegistry& commands, QWidget* parent = nullptr);

protected:
    /// @brief The keys of the table and of the fields of a new pair
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    CommandRegistry& m_commands;  ///< The commands of its window
    /// @brief The tab of the characters replaced as they are typed
    QWidget* createWhileTypingTab();

    /// @brief The tab of the capitals and their exceptions
    QWidget* createCapitalsTab();

    /// @brief The tab of the replacement table
    QWidget* createTableTab();

    /// @brief Fill the table with pairs
    void showPairs(const std::vector<ReplacementEntry>& pairs);

    /// @brief The pairs of the table, in its order
    [[nodiscard]] std::vector<ReplacementEntry> shownPairs() const;

    /// @brief Add a row of a pair at the end of the table
    void appendPair(const ReplacementEntry& pair);

    /// @brief The row of the pair of a word; -1: none
    [[nodiscard]] int rowOfWord(const QString& word, int except = -1) const;

    /// @brief Add the pair of the fields, or give the word of a pair the new replacement
    void addPair();

    /// @brief Remove the pairs selected
    void removePairs();

    /// @brief Turn the pairs selected on, or off when the current one is on
    void togglePairs();

    /// @brief Bring back the program's own pairs as they were; the writer's pairs stay
    void restoreDefaultPairs();

    /// @brief A cell changed: a word or a replacement that cannot be is put back, saying why
    void pairEdited(QTableWidgetItem* item);

    /// @brief Say "built-in" or "your own" in the last column of a row
    void updateSource(int row);

    /// @brief Enable the buttons of the table and, for a word typed, say what Add does
    void updatePairButtons();

    /// @brief Grey out the replacements while auto-correct is off, and the choice of a
    ///        replacement (its dash) while the replacement is off
    void syncReplacements();

    QCheckBox* m_enabled = nullptr;              ///< Auto-correct as a whole
    QCheckBox* m_useTable = nullptr;             ///< The replacements of the table
    std::vector<QCheckBox*> m_replacements;      ///< Greyed out while auto-correct is off
    std::vector<std::pair<QCheckBox*, QWidget*>> m_choices;  ///< A replacement, its choice
    QTableWidget* m_pairs = nullptr;        ///< The replacement table
    QLineEdit* m_newWord = nullptr;         ///< The word of a new pair
    QLineEdit* m_newReplacement = nullptr;  ///< Its replacement
    QPushButton* m_addPair = nullptr;
    QPushButton* m_removePairs = nullptr;
    QLabel* m_pairNote = nullptr;           ///< What Add does, or why an edit was put back
    bool m_fillingPairs = false;            ///< The table is filled, not edited
};

/// @brief Editor > Quick Insert: what @ and # typed at the start of a word open (the names of
/// the cards, the text fragments, the typographic characters), and the writer's fragments and
/// characters
class EditorQuickInsertPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorQuickInsertPage(CommandRegistry& commands, QWidget* parent = nullptr);

private:
    CommandRegistry& m_commands;  ///< The commands of its window
    void createSources();
    void createItems();

    /// @brief The fragments and characters as the list has them
    [[nodiscard]] QList<editor::QuickInsertItem> listedItems() const;
    void showItems(const QList<editor::QuickInsertItem>& items);
    void setRow(QTreeWidgetItem* row, const editor::QuickInsertItem& item);

    // The buttons of the list
    void addItem();
    void editItem();
    void removeItems();
    void restoreCharacters();
    void updateItemButtons();

    QTreeWidget* m_items = nullptr;
    QPushButton* m_edit = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_restore = nullptr;
};

/// @brief Editor > Repetitions: what counts as a word that comes back too close, the words the
/// writer skips, and how the words are marked in the text (with the color of each theme)
class EditorRepetitionsPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorRepetitionsPage(CommandRegistry& commands, QWidget* parent = nullptr);

protected:
    /// @brief Enter in the field of a new word adds it, and does not close the dialog
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    CommandRegistry& m_commands;  ///< The commands of its window
    void addWord();
    void removeWords();
    void updateButtons();

    QListWidget* m_words = nullptr;   ///< The words skipped
    QLineEdit* m_newWord = nullptr;   ///< A word to skip
    QPushButton* m_add = nullptr;
    QPushButton* m_remove = nullptr;
};

/// @brief Editor > Versions: whether Save Version asks for a description, the versions the
/// program keeps by itself and how many, and the colors of the comparison of a version
class EditorVersionsPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorVersionsPage(CommandRegistry& commands, QWidget* parent = nullptr);

private:
    CommandRegistry& m_commands;  ///< The commands of its window
};

/// @brief Editor > Clippings: what Cut to Clippings and Insert > Clipping from the Workshop do,
/// and whether scissors are beside the paragraphs clippings were cut from
class EditorClippingsPage : public SettingsPage {
    Q_OBJECT
public:
    /// @param commands The commands of its window: the page names the keys of the two
    /// @param parent Parent widget
    explicit EditorClippingsPage(CommandRegistry& commands, QWidget* parent = nullptr);

private:
    CommandRegistry& m_commands;  ///< The commands of its window
};

/// @brief Annotations: who new comments, to-dos and notes are by, their marks in the text, their
/// cards under the paragraphs and their colors
class AnnotationsPage : public SettingsPage {
    Q_OBJECT
public:
    /// @brief The pages of Settings > Annotations
    enum class Part {
        General,  ///< The author of new ones, their marks in the text and their colors
        Cards     ///< Their cards under the paragraphs
    };

    explicit AnnotationsPage(Part part = Part::General, QWidget* parent = nullptr);

private:
    /// @brief The groups of the cards under the paragraphs
    void addCardSettings();
};

/// @brief Plot Board: the view a new tab opens on, the layout and the size of the cards, the
/// scenes shown and which scenes there are (those of the texts, those in their plans, or
/// both: PlotBoard::SCENES_VARIANT_SETTING); what the cards show; what colors them, as a strip
/// or their background, the background of the board and the colors of the theme; the columns
/// of the outline; the front and back sections shown as a preview; what happens to the numbers
/// in the titles after a move (PlotBoard::RENUMBER_SETTING)
class PlotBoardPage : public SettingsPage {
    Q_OBJECT
public:
    /// @brief The page with its fields bound to their settings
    explicit PlotBoardPage(QWidget* parent = nullptr);

private:
    /// @brief A list @p label of @p choices (addChoice()) bound to setting @p key, which is its
    /// object name; a value the program does not know shows as the default of the setting
    QComboBox* addChoices(QFormLayout* form, const QString& label, const char* key,
                          const std::vector<Choice>& choices);

    /// @brief A row @p label of check boxes, each with its text and the key of its setting
    void addChecks(QFormLayout* form, const QString& label,
                   const std::vector<std::pair<QString, std::string>>& checks);

    /// @brief View: the view a tab opens on, the layout and the size of the cards, the front
    /// and back sections
    void addViewGroup();
    /// @brief Scenes: shown under the texts or not, and which scenes there are
    void addScenesGroup();
    /// @brief Cards: what they show besides their titles
    void addCardsGroup();
    /// @brief Colors: what colors the cards and where, the background, the theme's colors
    void addColorsGroup();
    /// @brief Outline: its columns besides the title
    void addOutlineGroup();
    /// @brief Numbers in the Titles: what happens to them after a move
    void addNumbersGroup();
};

/// @brief Navigator: the commands that the menu of a node of the Navigator has for adding,
/// what the top of the Add Element window chooses from, what the Materials show and where a
/// list of materials opens, what a material dropped gives, and whether its links name
/// paragraphs; the kinds of materials the Navigator shows (MaterialList::HIDDEN_SETTING), how
/// the texts of the cover are counted, with the targets of their kinds (core::CoverMaterial),
/// and the texts for publishers (core::PublishingTexts)
class NavigatorPage : public SettingsPage {
    Q_OBJECT
public:
    /// @brief The pages of Settings > Navigator, each with its own groups
    enum class Part {
        Adding,     ///< The menus of the nodes, the Add Element window, dropped and pasted
        Lists,      ///< What the Materials show, where the list of a kind opens, the links
        Kinds,      ///< The kinds of materials the Navigator shows
        Cover,      ///< The lengths of the texts of the cover
        Publishing  ///< The texts for publishers: their lengths and the set
    };

    /// @param book The book of its window, whose numbers of materials the kinds show
    /// @param part The page
    NavigatorPage(const core::ProjectManager& book, Part part, QWidget* parent = nullptr);

private:
    void createAdding();
    void createLists();
    void createKinds();
    void createCover();
    /// @brief The texts for publishers: the target lengths of the texts without a template,
    /// the standard pages of a synopsis and the parts of the set of a new book
    /// (core::PublishingTexts)
    void createPublishing();
    /// @brief The target lengths of the texts without a template in both units, in @p group
    void addPublishingTargets(QFormLayout* group);
    /// @brief The parts of the set of a new book, in @p group
    void addSetChoice(QFormLayout* group);

    QList<QCheckBox*> m_kindBoxes;  ///< The kinds of materials shown, with their "kind"
    QComboBox* m_coverUnit = nullptr;  ///< The texts of the cover counted in characters or words
    QList<QSpinBox*> m_characterTargets;  ///< The targets of the texts of the cover in characters
    QList<QSpinBox*> m_wordTargets;       ///< In words
    QList<QCheckBox*> m_setBoxes;  ///< The parts of the set of a new book, with their "part"
    const core::ProjectManager& m_book;  ///< The book of its window
};

/// @brief Cards: the full card in two columns or on tabs (CardPage::LAYOUT_SETTING); the shape
/// of the portraits and what a portrait without a picture shows (CardPage::SHAPE_SETTING,
/// CardPage::NO_PICTURE_SETTING); the colors of the cards of characters, locations and items –
/// the dot of the color of a card beside its name in the Navigator
/// (core::CardColors::NAVIGATOR_DOT_SETTING) and the palette that new cards take their colors
/// from (core::CardColors::PALETTE_SETTING); what the Properties panel shows of the cards at
/// the cursor of a text (CursorCardBox::SETTING) and the signs beside the linked paragraphs;
/// the locations in the Navigator, in a tree or in a list (NavigatorPanel::CARDS_TREE_SETTING);
/// the fields of the cards that new books start with (core::card_fields::DEFAULTS_SETTING), in
/// the window Card Fields; the portraits of parts – the style of new ones
/// (Avatar::STYLE_SETTING), the frame the window Portrait opens with (Avatar::FRAME_SETTING)
/// and the palettes it chooses the colors from (AvatarPalettes::SETTING), in the window
/// Portrait Palettes
class CardsPage : public SettingsPage {
    Q_OBJECT
public:
    /// @param parent Parent widget
    explicit CardsPage(QWidget* parent = nullptr);

    /// @brief The palette of the new cards
    [[nodiscard]] PaletteEdit* paletteEdit() const { return m_palette; }

    /// @brief Restore Defaults of the palette
    [[nodiscard]] QPushButton* restorePaletteButton() const { return m_restorePalette; }

    /// @brief The locations in the Navigator: in a tree, each under the one it is part of
    ///        ("tree"), or in alphabetical order ("list")
    [[nodiscard]] QComboBox* locationsList() const { return m_locations; }

    /// @brief Card at the Cursor: what the Properties panel shows of the cards at the cursor
    ///        (CursorCardBox::SETTING: "names", "paragraphs", "none")
    [[nodiscard]] QComboBox* atCursorList() const { return m_atCursor; }

    /// @brief The full card: two columns or tabs (CardPage::LAYOUT_SETTING: "columns", "tabs")
    [[nodiscard]] QComboBox* layoutList() const { return m_cardLayout; }

    /// @brief The shape of the portraits (CardPage::SHAPE_SETTING: "square", "circle")
    [[nodiscard]] QComboBox* shapeList() const { return m_shape; }

    /// @brief What a portrait without a picture shows (CardPage::NO_PICTURE_SETTING:
    ///        "initials", "icon")
    [[nodiscard]] QComboBox* noPictureList() const { return m_noPicture; }

    /// @brief Default Fields of New Books...: the window Card Fields of the fields new books
    /// start with, which Apply saves
    [[nodiscard]] QPushButton* defaultFieldsButton() const { return m_defaultFieldsButton; }

    /// @brief The style of new portraits of parts (Avatar::STYLE_SETTING: AvatarLook::FLAT,
    ///        AvatarLook::LINE)
    [[nodiscard]] QComboBox* portraitStyleList() const { return m_composedStyle; }

    /// @brief What the window Portrait shows when it opens (Avatar::FRAME_SETTING:
    ///        Avatar::FRAME_BUST, Avatar::FRAME_FULL)
    [[nodiscard]] QComboBox* portraitFrameList() const { return m_composedFrame; }

    /// @brief Portrait Palettes...: the window of the palettes of the portraits of parts, which
    /// Apply saves
    [[nodiscard]] QPushButton* portraitPalettesButton() const { return m_palettesButton; }

    /// @brief What the fields of new books are: those of the book types, or the kinds with
    /// fields of their own
    [[nodiscard]] QLabel* defaultFieldsLabel() const { return m_defaultFieldsLabel; }

private:
    /// @brief The group Full Card: two columns or tabs
    void addFullCardGroup();

    /// @brief The group Portrait: its shape, and what it shows without a picture
    void addPortraitGroup();

    /// @brief The group Colors: the dots in the Navigator, the palette of new cards
    void addColorsGroup();

    /// @brief The group Navigator: the locations in a tree or in a list
    void addNavigatorGroup();

    /// @brief The group In the Text: what the Properties panel shows of the cards at the
    /// cursor
    void addTextGroup();

    /// @brief The group Portraits of Parts: the style of new ones, the frame of the window
    /// Portrait, the palettes
    void addComposedGroup();

    /// @brief Open the window Portrait Palettes
    void editPortraitPalettes();

    /// @brief The group Card Fields: the fields new books start with
    void addFieldsGroup();

    /// @brief Open the window Card Fields for the fields new books start with
    void editDefaultFields();

    /// @brief The sentence of what the fields of new books are
    void showDefaultFields();

    PaletteEdit* m_palette = nullptr;
    QPushButton* m_restorePalette = nullptr;
    QComboBox* m_locations = nullptr;
    QComboBox* m_atCursor = nullptr;
    QComboBox* m_cardLayout = nullptr;
    QComboBox* m_shape = nullptr;
    QComboBox* m_noPicture = nullptr;
    QPushButton* m_defaultFieldsButton = nullptr;
    QLabel* m_defaultFieldsLabel = nullptr;
    nlohmann::json m_defaultFields;  ///< The fields of new books as the page shows them
    QComboBox* m_composedStyle = nullptr;
    QComboBox* m_composedFrame = nullptr;
    QPushButton* m_palettesButton = nullptr;
    nlohmann::json m_portraitPalettes;  ///< The palettes of the portraits as the page shows them
};

/// @brief Materials: how a picture, a page or a picture on the web and a PDF file open
/// (NavigatorPanel::OPEN_MATERIALS_SETTING, NavigatorPanel::OPEN_PAGES_SETTING,
/// PdfFiles::OPEN_SETTING); the preview of PDF files: the layout of the pages, their look in a
/// dark theme, the zoom and the side panel it opens with and whether it opens at the page shown
/// last (PdfFiles); the sources: the style of the bibliography of a book without its own
/// (core::Bibliography::STYLE_SETTING) and the form of a reference in the text
/// (core::Bibliography::REFERENCE_SETTING)
class MaterialsPage : public SettingsPage {
    Q_OBJECT
public:
    /// @brief The page with the settings as they are
    explicit MaterialsPage(QWidget* parent = nullptr);

signals:
    /// @brief Show: the page Editor > Colors, with the colors of the preview of PDF files
    void colorsPageAsked();

private:
    /// @brief A list @p label of @p form with the choices of setting @p key, bound to it, the
    /// note of the chosen one under it (addChoice()); the first one is chosen when the setting
    /// has none of their values
    QComboBox* addChoices(QFormLayout* form, const QString& label, const char* key,
                          const std::vector<Choice>& choices);

    /// @brief The group Opening
    void addOpening();

    /// @brief The group PDF Preview
    void addPdfPreview();

    /// @brief The group Sources
    void addSources();

    /// @brief The lookup of the descriptions on the web in group @p sources: whether the card
    /// of a source has it, and the catalog of an ISBN
    void addLookup(QFormLayout* sources);

    QComboBox* m_openPdf = nullptr;            ///< A PDF file opens on a preview tab or not
    QComboBox* m_pdfZoom = nullptr;            ///< The zoom the preview opens with
    QComboBox* m_pdfSide = nullptr;            ///< The side panel it opens with
    QComboBox* m_style = nullptr;      ///< PN-ISO 690, APA...
    QComboBox* m_reference = nullptr;  ///< (Wiśniewska 2011, s. 45)...
};

/// @brief Network > Materials: whether the program downloads the title of the page or the copy
/// of the picture of a material with a web address (MaterialDownloader::SETTING): when the box
/// of the Add Element window asks for it (the usual one), always, or never
class NetworkMaterialsPage : public SettingsPage {
    Q_OBJECT
public:
    explicit NetworkMaterialsPage(QWidget* parent = nullptr);
};

/// @brief Files > Folders: the folders of the books and of the archives, and where and how
/// many backups of the database of each book are kept
///
/// After a change of the folder of the books or of the archives the windows that start in
/// it start there again, also when they remember the folder used last.
class FoldersPage : public SettingsPage {
    Q_OBJECT
public:
    explicit FoldersPage(QWidget* parent = nullptr);

    std::vector<std::string> apply() override;

    /// @brief A changed folder the program cannot use
    [[nodiscard]] Problem problem() const override;

private:
    /// @brief Add the field of a folder kept in setting @p key, which is empty for the
    /// default folder
    void addFolder(QFormLayout* form, const QString& label, const char* key,
                   const QString& defaultFolder, const QString& chooseTitle);

    /// @brief A field of a folder checked before it is written
    struct CheckedFolder {
        FolderField* field;
        Binding* binding;
    };
    std::vector<CheckedFolder> m_folders;

    QComboBox* m_backupPlace = nullptr;  ///< The backups in the folder of each book, or in one
    FolderField* m_backupFolder = nullptr;
};

/// @brief Files > Import: the choices File > Import > DOCX, Markdown and Plain Text start with
///
/// The window of the import keeps the choices made in it in the same settings, each format
/// its own, so the page shows the choices of the last import of each format.
class ImportPage : public SettingsPage {
    Q_OBJECT
public:
    explicit ImportPage(QWidget* parent = nullptr);

private:
    /// @brief The headings, the dialogue and the cleanups of each format, one format at a time
    void addFormats();
};

/// @brief Advanced > General: the diagnostic menu (this session only, not stored)
class AdvancedGeneralPage : public SettingsPage {
    Q_OBJECT
public:
    /// @param diagnosticMode Whether the diagnostic menu is shown now
    explicit AdvancedGeneralPage(bool diagnosticMode, QWidget* parent = nullptr);

signals:
    /// @brief The diagnostic menu was turned on or off and applied
    void diagnosticModeChanged(bool enabled);
};

/// @brief Advanced > Log
class AdvancedLogPage : public SettingsPage {
    Q_OBJECT
public:
    explicit AdvancedLogPage(QWidget* parent = nullptr);
};

} // namespace gui
} // namespace kalahari

#endif  // KALAHARI_GUI_SETTINGS_SETTINGS_PAGES_H
