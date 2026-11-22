// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file settings_dialog.h
/// @brief Settings dialog for Kalahari application
///
/// The dialog shows a tree of pages (gui/settings/settings_pages.h). A page is built
/// the first time it is opened; each page reads and writes its own settings.
/// Apply/OK writes only the changed settings and emits settingsApplied with their
/// keys, and SettingsCoordinator refreshes the parts of the window they affect. The
/// editors and most panels follow the settings by themselves.

#ifndef KALAHARI_GUI_SETTINGS_DIALOG_H
#define KALAHARI_GUI_SETTINGS_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/settings/settings_search.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QPointer>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <map>
#include <vector>

class QLabel;
class QLineEdit;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {
class CommandRegistry;
}  // namespace kalahari::gui

namespace kalahari {
namespace editor {
class SpellCheckService;
} // namespace editor
namespace gui {

class GrammarCoordinator;
class SettingsPage;
class ThemePage;
class IconsPage;

/// @brief Settings dialog with hierarchical tree navigation
///
/// The search above the tree (Ctrl+F) narrows the tree to the pages whose options, or whose
/// names, hold the words typed, and marks the options found on the page shown; the pages not
/// built yet are built for it the first time.
///
/// Example usage:
/// @code
/// SettingsDialog dialog(this, diagnosticMode);
/// connect(&dialog, &SettingsDialog::settingsApplied, this, &MyCoordinator::onApplied);
/// dialog.exec();
/// @endcode
class SettingsDialog : public dialogs::KalahariDialog {
    Q_OBJECT

public:
    /// @brief The pages the dialog can be opened at (openPage())
    static constexpr const char* GENERAL_PAGE = "general";           ///< General
    /// Notifications: where the program's notifications show and their colors (the gears of the
    /// bar of notifications, of their window and of the Notifications panel)
    static constexpr const char* NOTIFICATIONS_PAGE = "notifications";
    static constexpr const char* EDITOR_COLORS_PAGE = "editor.colors";  ///< Editor > Colors
    /// Editor > Writing, scrolled to its group Word Count, the cursor in its counting of the
    /// notes' words (the gear of Format > Footnote Layout...)
    static constexpr const char* WORD_COUNT_PAGE = "editor.general.wordCount";
    /// Editor > Writing, the cursor in the gray background of the fields (the gear of Insert >
    /// Field...)
    static constexpr const char* FIELDS_PAGE = "editor.general.fields";
    /// Editor > Writing, the cursor in the Paste Command (the gear of Edit > Paste Special...)
    static constexpr const char* PASTE_PAGE = "editor.general.paste";
    /// Editor > Writing, the cursor in the group Footnotes and Endnotes (the gear of the
    /// Footnotes panel)
    static constexpr const char* FOOTNOTES_PAGE = "editor.general.footnotes";
    static constexpr const char* PAGES_PAGE = "editor.pages";        ///< Editor > Pages and Margins
    static constexpr const char* LISTS_PAGE = "editor.lists";        ///< Editor > Lists
    /// Editor > Margin Numbering, the cursor in the choice of the numbering
    static constexpr const char* MARGIN_NUMBERING_PAGE = "editor.pages.marginNumbering";
    static constexpr const char* IMAGES_PAGE = "editor.images";      ///< Editor > Images
    /// Editor > Images, the cursor in the group of the panel Images and Tables (the gear of the
    /// panel)
    static constexpr const char* FIGURES_PANEL_PAGE = "editor.images.figuresPanel";
    static constexpr const char* TABLES_PAGE = "editor.tables";      ///< Editor > Tables
    static constexpr const char* SPELLING_PAGE = "editor.spelling";  ///< Editor > Spelling
    /// Editor > Auto-correct (the gear of File > Import: the cleanups of the text follow it)
    static constexpr const char* AUTO_CORRECT_PAGE = "editor.autoCorrect";
    static constexpr const char* IMPORT_PAGE = "files.import";  ///< Files > Import
    /// Files > File Associations
    static constexpr const char* FILE_ASSOCIATIONS_PAGE = "files.associations";
    static constexpr const char* REPETITIONS_PAGE = "editor.repetitions";  ///< Editor > Repetitions
    static constexpr const char* VERSIONS_PAGE = "editor.versions";  ///< Editor > Versions
    static constexpr const char* CLIPPINGS_PAGE = "editor.clippings";  ///< Editor > Clippings
    static constexpr const char* ANNOTATIONS_PAGE = "annotations";   ///< Annotations > General
    static constexpr const char* SHORTCUTS_PAGE = "shortcuts";       ///< Keyboard Shortcuts
    /// Navigator > Material Kinds
    static constexpr const char* NAVIGATOR_PAGE = "navigator";
    /// Navigator > Adding (the menus of the nodes and the Add Element window)
    static constexpr const char* NAVIGATOR_ADDING_PAGE = "navigator.adding";
    /// Navigator > Texts for Publishers (the gear of the texts for publishers)
    static constexpr const char* PUBLISHING_PAGE = "navigator.publishing";
    static constexpr const char* CARDS_PAGE = "cards";               ///< Cards
    static constexpr const char* MATERIALS_PAGE = "materials";       ///< Materials
    static constexpr const char* PLOT_BOARD_PAGE = "plotBoard";      ///< Plot Board
    static constexpr const char* STATS_BAR_PAGE = "stats.bar";       ///< Statistics > Bar and Panel
    static constexpr const char* STATS_COUNTING_PAGE = "stats.counting";  ///< Statistics > Counting
    static constexpr const char* STATS_NOTIFICATIONS_PAGE = "stats.notifications";  ///< > Notifications
    static constexpr const char* STATS_COLORS_PAGE = "stats.colors";  ///< Statistics > Colors

    /// @brief Constructor
    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param parent Parent widget (usually MainWindow)
    /// @param diagnosticMode Whether the diagnostic menu is shown now (not a stored setting)
    /// @param spelling The dictionary of the editors, whose own words the Spelling page
    ///        edits (none: the page does not list them)
    /// @param grammar The grammar of the editors, which the Grammar page prepares and whose
    ///        server it checks (none: the page does not)
    explicit SettingsDialog(const core::ProjectManager& book, CommandRegistry& commands,
                            QWidget* parent, bool diagnosticMode = false,
                            editor::SpellCheckService* spelling = nullptr,
                            GrammarCoordinator* grammar = nullptr);

    ~SettingsDialog() override = default;

    /// @brief Whether any opened page has changes not applied yet
    [[nodiscard]] bool hasChanges() const;

    /// @brief Write the changed settings of all opened pages
    ///
    /// A changed value that cannot be written (SettingsPage::problem()) writes nothing: the
    /// dialog shows its page and says why.
    /// @return Keys of the settings written
    QStringList applyChanges();

    /// @brief Open the Keyboard Shortcuts page (Help > Keyboard Shortcuts), the cursor in its
    /// search
    void showShortcutsPage();

    /// @brief Open a page: EDITOR_COLORS_PAGE (the gears of Insert > Link and of the find bar),
    /// IMAGES_PAGE (the gears of the windows of images and captions), TABLES_PAGE (the gear of
    /// the window of the caption of a table), PAGES_PAGE,
    /// MARGIN_NUMBERING_PAGE (View > Margin Numbering > Numbering Settings...),
    /// FIELDS_PAGE (the gear of Insert > Field...), FOOTNOTES_PAGE (the gear of the Footnotes
    /// panel), SPELLING_PAGE (the gear of the Spelling and
    /// Grammar panel), VERSIONS_PAGE (the gear of the versions), CLIPPINGS_PAGE (the gears of
    /// the windows of the clippings), REPETITIONS_PAGE (the gear of the Repetitions panel),
    /// ANNOTATIONS_PAGE (the gear of the Annotations panel), NAVIGATOR_PAGE (the gear of a list
    /// of materials), NAVIGATOR_ADDING_PAGE, MATERIALS_PAGE (the gear of the entry of a source),
    /// CARDS_PAGE (the gear of a card of a character, a location or an item) or SHORTCUTS_PAGE
    /// (as showShortcutsPage())
    /// @return Whether the dialog has the page
    bool openPage(const QString& page);

    /// @brief Whether the dialog was closed with Open Book Dictionary (Editor > Spelling): the
    ///        window shows the book's dictionary
    [[nodiscard]] bool isBookDictionaryAsked() const { return m_bookDictionaryAsked; }

    /// @brief Find the options with these words: the search shows them, the tree narrows to
    ///        their pages at once (empty: every page again)
    void findSettings(const QString& text);

    /// @brief The search above the tree
    [[nodiscard]] QLineEdit* searchField() const { return m_search; }

    /// @brief The options the search marks on the page shown
    [[nodiscard]] std::vector<QWidget*> markedOptions() const;

public slots:
    /// @brief OK: write the changed settings, then close; a value that cannot be written
    /// keeps the dialog open on its page
    void accept() override;

signals:
    /// @brief Emitted after Apply/OK wrote changed settings
    /// @param changedKeys Keys of the settings written
    void settingsApplied(const QStringList& changedKeys);

    /// @brief The diagnostic menu was turned on or off (this session only)
    void diagnosticModeChanged(bool enabled);

private:
    /// @brief Builds the widget of one page
    using PageFactory = std::function<QWidget*()>;

    void createNavigationTree();

    /// @brief Add a page to the tree; it is built when first opened
    QTreeWidgetItem* addPage(QTreeWidgetItem* parent, const QString& title, PageFactory factory);

    /// @brief Add a muted page for planned options
    void addPlannedPage(QTreeWidgetItem* parent, const QString& title, const QString& description);

    /// @brief Color the planned pages in the tree for the current theme
    void updatePlannedItemColors();

    /// @brief Show a page, building it on first use
    void showPage(QTreeWidgetItem* item);

    /// @brief Connect pages that depend on each other (theme colors -> icon preview)
    /// @note Called once when the Theme or the Icons page is built
    void connectPages();

    /// @brief Keep the shared check boxes of a newly built page (SettingsPage::SHARED_CHECK_BOX)
    ///        alike with those of the pages built before it: the same choice on every page
    void shareCheckBoxes(QWidget* page);

    /// @brief Show the length fields of the built pages in another unit
    void setLengthUnit(LengthUnit unit);

    /// @brief Whether every changed value can be written; else show the page of the first
    /// one that cannot, with the focus on its control, and say why
    bool changesCanBeWritten();

    void onApply();

    /// @brief Build a page if it is not built yet
    /// @return Its container in the stack (null: a category)
    QWidget* buildPage(QTreeWidgetItem* item);

    /// @brief The page of an item, inside its container (null: not built, or a category)
    [[nodiscard]] QWidget* pageOf(QTreeWidgetItem* item) const;

    /// @brief Narrow the tree to the pages of the words of the search, mark the options found
    void search();

    /// @brief Whether a page shows in the search: its name or an option of it has the words;
    ///        keeps the options found for the marks
    bool searchPage(QTreeWidgetItem* item, const QString& category);

    /// @brief Mark the options found on the page shown, the first in view
    void showMarks(QTreeWidgetItem* item);

    /// @brief The keys of the search: Esc clears it, Down goes to the tree, Enter to the first
    ///        option found
    bool searchKeyPressed(int key);

    bool eventFilter(QObject* watched, QEvent* event) override;

    QTreeWidget* m_navTree = nullptr;
    QLineEdit* m_search = nullptr;
    QLabel* m_searchEmpty = nullptr;  ///< Says that nothing has the words
    QTimer m_searchTimer;             ///< The search after a pause in typing
    SettingsSearch m_searchWords;
    /// The options of each page the search found
    std::map<QTreeWidgetItem*, std::vector<QPointer<QWidget>>> m_found;
    QStackedWidget* m_pageStack = nullptr;
    bool m_diagnosticMode;
    editor::SpellCheckService* m_spelling;  ///< The dictionary of the editors (not owned)
    GrammarCoordinator* m_grammar;          ///< The grammar of the editors (not owned)
    LengthUnit m_lengthUnit;  ///< Unit chosen on the General page, applied or not

    std::map<QTreeWidgetItem*, PageFactory> m_factories;
    std::map<QTreeWidgetItem*, QWidget*> m_builtPages;  ///< Page container in the stack
    std::vector<SettingsPage*> m_pages;                  ///< Built pages with settings
    ThemePage* m_themePage = nullptr;
    IconsPage* m_iconsPage = nullptr;
    std::map<QString, QTreeWidgetItem*> m_pageItems;  ///< The pages of openPage()
    std::vector<QTreeWidgetItem*> m_plannedItems;  ///< Pages of options not available yet
    CommandRegistry& m_commands;  ///< The commands of its window
    const core::ProjectManager& m_book;  ///< The book of its window
    bool m_bookDictionaryAsked = false;  ///< Closed with Open Book Dictionary
};

} // namespace gui
} // namespace kalahari

#endif  // KALAHARI_GUI_SETTINGS_DIALOG_H
