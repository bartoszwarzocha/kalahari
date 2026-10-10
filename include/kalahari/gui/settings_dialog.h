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
namespace gui {

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
    /// @brief Constructor
    /// @param parent Parent widget (usually MainWindow)
    /// @param diagnosticMode Whether the diagnostic menu is shown now (not a stored setting)
    explicit SettingsDialog(QWidget* parent, bool diagnosticMode = false);

    ~SettingsDialog() override = default;

    /// @brief Whether any opened page has changes not applied yet
    [[nodiscard]] bool hasChanges() const;

    /// @brief Write the changed settings of all opened pages
    ///
    /// A changed value that cannot be written (SettingsPage::problem()) writes nothing: the
    /// dialog shows its page and says why.
    /// @return Keys of the settings written
    QStringList applyChanges();

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

    /// @brief Add a greyed-out page for planned options
    void addPlannedPage(QTreeWidgetItem* parent, const QString& title, const QString& description);

    /// @brief Show a page, building it on first use
    void showPage(QTreeWidgetItem* item);

    /// @brief Connect pages that depend on each other (theme colors -> icon preview)
    /// @note Called once when the Theme or the Icons page is built
    void connectPages();

    /// @brief Show the length fields of the built pages in another unit
    void setLengthUnit(LengthUnit unit);

    /// @brief Whether every changed value can be written; else show the page of the first
    /// one that cannot, with the focus on its control, and say why
    bool changesCanBeWritten();

    void onApply();

    QTreeWidget* m_navTree;
    QStackedWidget* m_pageStack;
    bool m_diagnosticMode;
    LengthUnit m_lengthUnit;  ///< Unit chosen on the General page, applied or not

    std::map<QTreeWidgetItem*, PageFactory> m_factories;
    std::map<QTreeWidgetItem*, QWidget*> m_builtPages;  ///< Page container in the stack
    std::vector<SettingsPage*> m_pages;                  ///< Built pages with settings
    ThemePage* m_themePage = nullptr;
    IconsPage* m_iconsPage = nullptr;
};

} // namespace gui
} // namespace kalahari
