/// @file settings_dialog.h
/// @brief Settings dialog for Kalahari application
///
/// The dialog shows a tree of pages (gui/settings/settings_pages.h). A page is built
/// the first time it is opened; each page reads and writes its own settings.
/// Apply/OK writes only the changed settings and emits settingsApplied with their
/// keys, and SettingsCoordinator refreshes the parts of the window they affect. The
/// editors and most panels follow the settings by themselves.

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QStringList>

#include <functional>
#include <map>
#include <vector>

class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari {
namespace editor {
class SpellCheckService;
}
namespace gui {

class GrammarCoordinator;
class SettingsPage;
class ThemePage;
class IconsPage;

/// @brief Settings dialog with hierarchical tree navigation
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
    static constexpr const char* EDITOR_COLORS_PAGE = "editor.colors";  ///< Editor > Colors
    static constexpr const char* SPELLING_PAGE = "editor.spelling";  ///< Editor > Spelling
    static constexpr const char* VERSIONS_PAGE = "editor.versions";  ///< Editor > Versions
    static constexpr const char* ANNOTATIONS_PAGE = "annotations";   ///< Annotations
    static constexpr const char* SHORTCUTS_PAGE = "shortcuts";       ///< Keyboard Shortcuts

    /// @brief Constructor
    /// @param parent Parent widget (usually MainWindow)
    /// @param diagnosticMode Whether the diagnostic menu is shown now (not a stored setting)
    /// @param spelling The dictionary of the editors, whose own words the Spelling page
    ///        edits (none: the page does not list them)
    /// @param grammar The grammar of the editors, which the Grammar page prepares and whose
    ///        server it checks (none: the page does not)
    explicit SettingsDialog(QWidget* parent, bool diagnosticMode = false,
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

    /// @brief Open a page: EDITOR_COLORS_PAGE (the gear of the find bar), SPELLING_PAGE (the
    /// gear of the Spelling and Grammar panel), ANNOTATIONS_PAGE (the gear of the Annotations
    /// panel) or SHORTCUTS_PAGE (as showShortcutsPage())
    /// @return Whether the dialog has the page
    bool openPage(const QString& page);

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

    QTreeWidget* m_navTree;
    QStackedWidget* m_pageStack;
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
};

} // namespace gui
} // namespace kalahari
