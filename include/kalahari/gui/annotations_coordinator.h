/// @file annotations_coordinator.h
/// @brief The writer's annotations: the commands for them, the frame their text is written
/// in and the Annotations panel

#pragma once

#include <kalahari/editor/annotation.h>
#include <kalahari/gui/panels/annotation_entry.h>

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTextCursor>

#include <functional>
#include <optional>
#include <vector>

class QAction;
class QDockWidget;
class QStatusBar;
class QTabWidget;
class QTimer;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

class AnnotationFrame;
class AnnotationsPanel;
class EditorPanel;

/// @brief Coordinates the annotations of the documents with the Annotations panel
///
/// Gives the annotation commands their callbacks: Add Comment, Add To Do and Add Note
/// (also in the text's context menu), Add Annotation (a small menu of the kinds at the
/// cursor), Next and Previous To Do; F9 goes to the panel and back.
///
/// An annotation's text is written in a frame at its place in the text: a new one's (on
/// the selection, or on the cursor's place), and one's edited again (from its card, or with
/// a click on its mark). Saving the frame is one step of the document's undo history - the
/// new annotation, or its new text; dropping it (Esc) changes nothing. One frame is open at
/// a time: opening another keeps the text of the open one, and so do saving and closing
/// documents (finishWriting()).
///
/// Lists in the panel the annotations of the document in front, or of the whole book.
/// What the writer does in the panel goes to the document: done or resolved, removing,
/// each a step of the document's undo history. An annotation of a chapter not in front is
/// gone to or changed after its chapter's tab is brought to the front (the chapter opened
/// when it is not): the writer sees the change, and Ctrl+Z undoes it in that chapter.
class AnnotationsCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Opens a chapter of the book in a tab of its own, or brings its tab to the front
    using ChapterOpener = std::function<void(const QString& elementId)>;

    /// @brief Constructor
    /// @param panel The Annotations panel
    /// @param dock The panel's dock (F9 shows and hides it)
    /// @param centralTabs The tabs of the documents
    /// @param openChapter Opens a chapter of the book (none: annotations of chapters not
    ///        open cannot be gone to or changed)
    /// @param statusBar Where a short message tells there is no further to-do (may be null)
    /// @param parent Parent object
    AnnotationsCoordinator(AnnotationsPanel* panel, QDockWidget* dock, QTabWidget* centralTabs,
                           ChapterOpener openChapter, QStatusBar* statusBar,
                           QObject* parent = nullptr);

    /// @brief Destructor
    ~AnnotationsCoordinator() override;

    /// @brief Give the annotation commands their callbacks
    /// @note Call after the commands are registered and the panel's dock is created
    void connectCommands();

    /// @brief Who new annotations are by
    ///
    /// The setting annotations.author; without it defaultAuthor().
    static QString author();

    /// @brief Who new annotations are by without the setting annotations.author
    ///
    /// The open book's author, and without one the name of the computer's user.
    static QString defaultAuthor();

    /// @brief Write a new annotation in the document in front, in a frame at its place
    ///
    /// It goes on the selection, or on the cursor's place without one, when its text is
    /// saved.
    /// @return Whether the frame opened (not without a document in front)
    bool addAnnotation(editor::AnnotationKind kind);

    /// @brief The small menu of the kinds of annotations at the cursor (Add Annotation)
    void showAddMenu();

    /// @brief Edit an annotation's text in a frame at its place
    /// @param elementId Its chapter (empty: the document in front); its tab is brought to
    ///        the front, the chapter opened when it is not
    /// @param annotationId The annotation
    /// @param fromPanel true: the text goes to the annotation first, and the keys go back to
    ///        the panel when the frame closes; false: the cursor stays and the keys go back
    ///        to the text
    /// @return Whether the frame opened
    bool editAnnotation(const QString& elementId, const QString& annotationId, bool fromPanel);

    /// @brief Go to the next to-do not done yet
    ///
    /// After the cursor in the document in front; when the panel lists the book, also in
    /// the chapters after it.
    /// @return Whether there was one
    bool goToNextTodo();

    /// @brief Go to the previous to-do not done yet (see goToNextTodo())
    bool goToPreviousTodo();

    /// @brief F9: to the panel and back
    ///
    /// Shows the panel and gives it the keys, the card of the annotation at the cursor
    /// selected; from the panel, hides it and gives the keys back to the text.
    void togglePanelFocus();

    /// @brief List the annotations in the panel again now
    void refresh();

    /// @brief Whether an annotation's text is being written in its frame
    bool isWriting() const { return m_writing.has_value(); }

    /// @brief The frame an annotation's text is written in (nullptr: none is open)
    AnnotationFrame* frame() const { return m_frame; }

    /// @brief Keep the frame's text (Ctrl+Enter, Save) and close it
    ///
    /// The new annotation is added with it, or the edited one gets it.
    void saveWriting();

    /// @brief Drop the frame's text (Esc) and close it: nothing changes
    void cancelWriting();

    /// @brief Close the frame, keeping its text when it has any
    ///
    /// Before a document is saved or closed and before another frame opens: what was
    /// written is not lost.
    /// @param editor Only the frame over this editor (nullptr: the frame over any)
    void finishWriting(const editor::BookEditor* editor = nullptr);

protected:
    /// @brief F9 (the shortcut of Annotations): togglePanelFocus()
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief An annotation whose text is written in the frame
    struct Writing {
        editor::BookEditor* editor{nullptr};  ///< The editor of its document
        QString elementId;     ///< Its chapter; empty: a document outside the book
        QString annotationId;  ///< The one edited; empty: a new one
        editor::AnnotationKind kind = editor::AnnotationKind::Comment;
        QString author;          ///< Who it is by: a new one by author(), as the frame shows
        QTextCursor range;       ///< Its fragment or place; follows the edits of the text
        bool fromPanel = false;  ///< The keys go back to the panel, else to the text
    };

    /// @brief The annotations of a chapter of the book
    struct ChapterAnnotations {
        QString elementId;
        QString title;
        editor::AnnotationList annotations;  ///< In text order
    };

    /// @brief The annotations a chapter file had when it was last read
    struct CachedFile {
        QDateTime modified;
        editor::AnnotationList annotations;
    };

    // Documents
    EditorPanel* currentEditorPanel() const;
    editor::BookEditor* currentEditor() const;
    QString elementIdOf(const EditorPanel* panel) const;
    QString titleOf(const EditorPanel* panel) const;

    /// @brief The panel of a chapter's open tab (empty id: the document in front)
    EditorPanel* openPanel(const QString& elementId) const;

    /// @brief The editor of a chapter's open tab (empty id: the document in front)
    editor::BookEditor* openEditor(const QString& elementId) const;

    /// @brief The editor of a chapter, its tab brought to the front (opened when it is not)
    editor::BookEditor* editorFor(const QString& elementId);

    /// @brief The book's chapters in order, with their annotations
    std::vector<ChapterAnnotations> bookAnnotations();

    /// @brief The annotations of a chapter: from its editor when it is open, else its file
    editor::AnnotationList chapterAnnotations(const QString& elementId);

    /// @brief The annotations in a chapter's file
    editor::AnnotationList fileAnnotations(const QString& elementId);

    /// @brief The key of the annotation at the cursor in front: the one the cursor is in or
    /// on, else the last one before it, else the first one after it (empty: none)
    QString keyAtCursor() const;

    // The panel
    void onCurrentTabChanged();
    void onEditorContentChanged();
    void scheduleRefresh();
    void updateCommandStates();
    void onAnnotationActivated(const AnnotationEntry& entry);
    void onEditRequested(const AnnotationEntry& entry);
    void onDoneToggled(const AnnotationEntry& entry, bool done);
    void onDeleteRequested(const AnnotationEntry& entry);
    void onEditorFocusRequested();

    // The frame
    void onMarkClicked(const QString& annotationId);
    void openFrame(const Writing& writing, const QString& text);
    void closeFrame();
    void colorFrame();

    /// @brief Go to the next or previous to-do (see goToNextTodo())
    bool goToTodo(bool next);

    /// @brief Go to a to-do in another chapter of the book (see goToNextTodo())
    bool goToTodoInOtherChapter(bool next);

    /// @brief Go to an annotation of a chapter, opened when it is not, and select its card
    bool goToAnnotation(const QString& elementId, const QString& annotationId);

    AnnotationsPanel* m_panel;
    QDockWidget* m_dock;
    QTabWidget* m_centralTabs;
    ChapterOpener m_openChapter;
    QStatusBar* m_statusBar;

    std::optional<Writing> m_writing;
    AnnotationFrame* m_frame{nullptr};
    QAction* m_panelAction{nullptr};  ///< Annotations (F9)
    bool m_stale = false;             ///< The panel's list is old: refresh when it is shown
    QTimer* m_refreshTimer{nullptr};  ///< Lists the annotations again after edits
    QHash<QString, CachedFile> m_fileCache;  ///< By the files' paths
};

}  // namespace kalahari::gui
