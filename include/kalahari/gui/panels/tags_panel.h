/// @file tags_panel.h
/// @brief Tags panel for displaying TODO/FIX/CHECK markers (OpenSpec #00042 Task 7.10)
///
/// TagsPanel displays all detected tags in the current document, allowing users to:
/// - View all tags grouped by type (TODO, FIX, CHECK, NOTE, WARNING)
/// - Navigate to tag locations by clicking
/// - Filter by tag type
/// - See tag counts per type
///
/// The panel connects to TagDetector to stay synchronized with the document.

#pragma once

#include <QWidget>
#include <QString>
#include <QMap>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QComboBox;

namespace kalahari::editor {
class TagDetector;
class BookEditor;
enum class TagType;
struct DetectedTag;
}

namespace kalahari {
namespace gui {

/// @brief Panel for displaying and navigating document tags
///
/// Shows a tree of all tags in the document, grouped by type.
/// Provides filtering and navigation to tag locations.
///
/// Usage:
/// @code
/// auto tagsPanel = new TagsPanel(this);
/// tagsPanel->setEditor(bookEditor);
/// @endcode
class TagsPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit TagsPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~TagsPanel() override = default;

    /// @brief Set the BookEditor to track
    /// @param editor BookEditor to track (nullptr to disconnect)
    ///
    /// When set, the panel will display tags from the editor's document
    /// and update automatically when tags change.
    void setEditor(editor::BookEditor* editor);

    /// @brief Get the current BookEditor
    /// @return Current BookEditor, or nullptr if none set
    editor::BookEditor* editor() const { return m_editor; }

    /// @brief Set the TagDetector to use
    /// @param detector TagDetector instance (nullptr to disconnect)
    ///
    /// Usually called internally when editor is set, but can be used
    /// to share a detector between multiple panels.
    void setTagDetector(editor::TagDetector* detector);

    /// @brief Get the current TagDetector
    /// @return Current TagDetector, or nullptr if none set
    editor::TagDetector* tagDetector() const { return m_detector; }

public slots:
    /// @brief Refresh the tags list from the detector
    ///
    /// Re-reads all tags and updates the tree.
    /// Called automatically when tags change.
    void refresh();

    /// @brief Clear the tags list
    ///
    /// Removes all items from the tree. Called when no document is open.
    void clear();

signals:
    /// @brief Emitted when a tag is clicked in the tree
    /// @param paragraphIndex Paragraph index containing the tag
    /// @param position Character position of the tag in the paragraph
    ///
    /// Connect to this signal to navigate the editor to the tag location.
    void tagClicked(int paragraphIndex, int position);

    /// @brief Emitted when a tag is double-clicked
    /// @param paragraphIndex Paragraph index containing the tag
    /// @param position Character position of the tag in the paragraph
    void tagDoubleClicked(int paragraphIndex, int position);

private slots:
    /// @brief Handle tree item click
    /// @param item The clicked item
    /// @param column The clicked column
    void onItemClicked(QTreeWidgetItem* item, int column);

    /// @brief Handle tree item double-click
    /// @param item The double-clicked item
    /// @param column The clicked column
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

    /// @brief Handle filter combo box change
    /// @param index The new filter index
    void onFilterChanged(int index);

    /// @brief Handle tags changed signal from detector
    void onTagsChanged();

private:
    /// @brief Setup UI components
    void setupUI();

    /// @brief Connect signals to detector
    void connectToDetector();

    /// @brief Disconnect signals from current detector
    void disconnectFromDetector();

    /// @brief Add a tag to the tree
    /// @param tag The tag to add
    /// @param parentItem Parent tree item (type group)
    void addTagToTree(const editor::DetectedTag& tag, QTreeWidgetItem* parentItem);

    /// @brief Get or create a type group item
    /// @param type The tag type
    /// @return The tree item for that type group
    QTreeWidgetItem* getTypeGroupItem(editor::TagType type);

    /// @brief Update type group item labels with counts
    void updateGroupCounts();

    /// @brief Get display text for a tag
    /// @param tag The tag
    /// @return Formatted display string
    QString formatTagDisplay(const editor::DetectedTag& tag) const;

    /// @brief Check if a tag type passes the current filter
    /// @param type The tag type to check
    /// @return true if the tag should be shown
    bool passesFilter(editor::TagType type) const;

    QTreeWidget* m_tagsTree;           ///< Tree widget showing tags
    QComboBox* m_filterCombo;          ///< Filter combo box
    QLabel* m_emptyLabel;              ///< Label shown when no tags
    QLabel* m_countLabel;              ///< Label showing total count

    editor::BookEditor* m_editor;      ///< Current BookEditor (not owned)
    editor::TagDetector* m_detector;   ///< Current TagDetector (not owned)
    bool m_ownsDetector;               ///< Whether we created the detector

    /// @brief Map from TagType to its tree item
    QMap<int, QTreeWidgetItem*> m_typeGroupItems;

    /// @brief Current filter (-1 = all, 0-4 = specific type)
    int m_currentFilter;
};

} // namespace gui
} // namespace kalahari
