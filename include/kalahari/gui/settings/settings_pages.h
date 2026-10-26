/// @file settings_pages.h
/// @brief The pages of the Settings dialog
///
/// Each page builds its controls and binds them to setting keys (SettingsPage);
/// only the theme, icons and diagnostic options add their own steps.

#pragma once

#include "kalahari/gui/settings/settings_page.h"

#include <QColor>

#include <functional>
#include <utility>
#include <vector>

class QComboBox;
class QHBoxLayout;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QTableWidget;
class QTableWidgetItem;

namespace kalahari {
namespace core {
struct Theme;
}
namespace editor {
class SpellCheckService;
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

/// @brief Editor > General: font, typography, typewriter scrolling
class EditorGeneralPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorGeneralPage(QWidget* parent = nullptr);
};

/// @brief Editor > Colors: light and dark paper
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

/// @brief Editor > Pages and Margins
class EditorPagesPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorPagesPage(QWidget* parent = nullptr);
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

/// @brief Annotations: who new comments, to-dos and notes are by, and their marks in the text
class AnnotationsPage : public SettingsPage {
    Q_OBJECT
public:
    explicit AnnotationsPage(QWidget* parent = nullptr);
};

/// @brief Navigator: the commands that the menu of a node of the Navigator has for adding
class NavigatorPage : public SettingsPage {
    Q_OBJECT
public:
    explicit NavigatorPage(QWidget* parent = nullptr);

private:
    QRadioButton* m_named = nullptr;   ///< "Add..." and the commands named after kinds
    QRadioButton* m_single = nullptr;  ///< "Add..." only
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

    QRadioButton* m_inEachBook = nullptr;   ///< The backups in the folder of each book
    QRadioButton* m_inOneFolder = nullptr;  ///< The backups in one folder for all books
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
