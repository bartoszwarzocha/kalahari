/// @file comments_panel.h
/// @brief Comments panel for displaying and managing document comments (OpenSpec #00042 Task 7.9)
///
/// CommentsPanel displays all comments in the current document, allowing users to:
/// - View all comments in a list
/// - Navigate to commented text by clicking
/// - Delete comments
/// - Edit comment text
///
/// The panel connects to BookEditor to stay synchronized with the document.

#pragma once

#include <QWidget>
#include <QString>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;

namespace kalahari::editor {
class BookEditor;
class KmlDocument;
class KmlComment;
}

namespace kalahari {
namespace gui {

/// @brief Panel for displaying and managing document comments
///
/// Shows a list of all comments in the document with their text excerpts.
/// Provides actions for navigation, editing, and deletion of comments.
///
/// Usage:
/// @code
/// auto commentsPanel = new CommentsPanel(this);
/// commentsPanel->setEditor(bookEditor);
/// @endcode
class CommentsPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit CommentsPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~CommentsPanel() override = default;

    /// @brief Set the BookEditor to track
    /// @param editor BookEditor to track (nullptr to disconnect)
    ///
    /// When set, the panel will display comments from the editor's document
    /// and update automatically when comments change.
    void setEditor(editor::BookEditor* editor);

    /// @brief Get the current BookEditor
    /// @return Current BookEditor, or nullptr if none set
    editor::BookEditor* editor() const { return m_editor; }

public slots:
    /// @brief Refresh the comments list from the document
    ///
    /// Re-reads all comments from the document and updates the list.
    /// Called automatically when document changes.
    void refresh();

    /// @brief Clear the comments list
    ///
    /// Removes all items from the list. Called when no document is open.
    void clear();

signals:
    /// @brief Emitted when a comment is clicked in the list
    /// @param paragraphIndex Paragraph index containing the comment
    /// @param commentId ID of the clicked comment
    ///
    /// Connect to this signal to navigate the editor to the commented text.
    void commentClicked(int paragraphIndex, const QString& commentId);

    /// @brief Emitted when delete is requested for a comment
    /// @param paragraphIndex Paragraph index containing the comment
    /// @param commentId ID of the comment to delete
    void deleteRequested(int paragraphIndex, const QString& commentId);

    /// @brief Emitted when edit is requested for a comment
    /// @param paragraphIndex Paragraph index containing the comment
    /// @param commentId ID of the comment to edit
    void editRequested(int paragraphIndex, const QString& commentId);

private slots:
    /// @brief Handle list item click
    /// @param item The clicked item
    void onItemClicked(QListWidgetItem* item);

    /// @brief Handle list item double-click
    /// @param item The double-clicked item
    void onItemDoubleClicked(QListWidgetItem* item);

    /// @brief Handle delete button click
    void onDeleteClicked();

    /// @brief Handle edit button click
    void onEditClicked();

    /// @brief Handle comment added signal from BookEditor
    /// @param paragraphIndex Paragraph where comment was added
    void onCommentAdded(int paragraphIndex);

    /// @brief Handle comment removed signal from BookEditor
    /// @param paragraphIndex Paragraph from which comment was removed
    /// @param commentId ID of the removed comment
    void onCommentRemoved(int paragraphIndex, const QString& commentId);

private:
    /// @brief Setup UI components
    void setupUI();

    /// @brief Connect signals to BookEditor
    void connectToEditor();

    /// @brief Disconnect signals from current editor
    void disconnectFromEditor();

    /// @brief Get the currently selected comment info
    /// @param[out] paragraphIndex Paragraph index of selected comment
    /// @param[out] commentId ID of selected comment
    /// @return true if a comment is selected
    bool getSelectedComment(int& paragraphIndex, QString& commentId) const;

    /// @brief Create display text for a comment
    /// @param comment The comment
    /// @param paragraphText Text of the paragraph containing the comment
    /// @return Formatted display string
    QString formatCommentDisplay(const editor::KmlComment& comment,
                                 const QString& paragraphText) const;

    QListWidget* m_commentsList;      ///< List widget showing comments
    QPushButton* m_deleteButton;      ///< Delete button
    QPushButton* m_editButton;        ///< Edit button
    QLabel* m_emptyLabel;             ///< Label shown when no comments

    editor::BookEditor* m_editor;     ///< Current BookEditor (not owned)
};

} // namespace gui
} // namespace kalahari
