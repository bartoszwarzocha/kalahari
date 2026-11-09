/// @file settings_pages.h
/// @brief The pages of the Settings dialog
///
/// Each page builds its controls and binds them to setting keys (SettingsPage);
/// only the theme, icons and diagnostic options add their own steps.

#pragma once

#include "kalahari/gui/settings/settings_page.h"

#include <QColor>
#include <QList>

#include <functional>
#include <utility>
#include <vector>

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;
class QSpinBox;

namespace kalahari {
namespace core {
struct Theme;
}
namespace editor {
class SpellCheckService;
struct QuickInsertItem;
}
namespace gui {

class ColorConfigWidget;
class FolderField;
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

    QComboBox* m_themeCombo;
    ColorConfigWidget* m_primaryColor = nullptr;
    ColorConfigWidget* m_secondaryColor = nullptr;

    struct ColorField {
        ColorConfigWidget* widget;
        std::function<QColor(const core::Theme&)> themeColor;
        Binding* binding;
    };
    std::vector<ColorField> m_colors;
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
    QHBoxLayout* m_previewLayout;
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
        Writing  ///< Pasting, opening a chapter, typewriter scrolling, the word count
    };

    explicit EditorGeneralPage(Part part = Part::Text, QWidget* parent = nullptr);

private:
    void createText();
    void createWriting();
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
    explicit EditorSpellingPage(editor::SpellCheckService* spelling = nullptr,
                                QWidget* parent = nullptr);

protected:
    /// @brief Enter in the field of a new word adds it, and does not close the dialog
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The group of the writer's own words, which the page changes in @p spelling
    void addOwnWords(editor::SpellCheckService* spelling);

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
    explicit EditorAutoCorrectPage(QWidget* parent = nullptr);

protected:
    /// @brief The keys of the table and of the fields of a new pair
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
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
    explicit EditorQuickInsertPage(QWidget* parent = nullptr);

private:
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
    explicit EditorRepetitionsPage(QWidget* parent = nullptr);

protected:
    /// @brief Enter in the field of a new word adds it, and does not close the dialog
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
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
    explicit EditorVersionsPage(QWidget* parent = nullptr);
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

/// @brief Navigator: the commands that the menu of a node of the Navigator has for adding,
/// what the top of the Add Element window chooses from, what the Materials show and where a
/// list of materials opens, how a material opens, and whether its links name paragraphs; the
/// kinds of materials the Navigator shows (MaterialList::HIDDEN_SETTING) and how the texts of
/// the cover are counted, with the targets of their kinds (core::CoverMaterial)
class NavigatorPage : public SettingsPage {
    Q_OBJECT
public:
    /// @brief The pages of Settings > Navigator, each with its own groups
    enum class Part {
        Adding,     ///< The menus of the nodes, the Add Element window, dropped and pasted
        Materials,  ///< The lists of the materials, how they open, their links
        Kinds,      ///< The kinds of materials the Navigator shows
        Cover       ///< The lengths of the texts of the cover
    };

    explicit NavigatorPage(Part part, QWidget* parent = nullptr);

private:
    void createAdding();
    void createMaterials();
    void createKinds();
    void createCover();

    QList<QCheckBox*> m_kindBoxes;  ///< The kinds of materials shown, with their "kind"
    QComboBox* m_coverUnit = nullptr;  ///< The texts of the cover counted in characters or words
    QList<QSpinBox*> m_characterTargets;  ///< The targets of the texts of the cover in characters
    QList<QSpinBox*> m_wordTargets;       ///< In words
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
