/// @file annotations_coordinator.h
/// @brief The writer's annotations: the commands for them and the Annotations panel

#pragma once

#include <kalahari/editor/annotation.h>
#include <kalahari/gui/panels/annotation_entry.h>

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

class QDockWidget;
class QStatusBar;
class QTabWidget;
class QTimer;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

class AnnotationsPanel;
class EditorPanel;

/// @brief Coordinates the annotations of the documents with the Annotations panel
///
/// Gives the annotation commands their callbacks: Add Comment, Add TODO and Add Note
/// (also in the text's context menu), Add Annotation (a small menu of the kinds at the
/// cursor) and Next and Previous TODO. A new annotation is anchored to the selection or
/// to the cursor's place and gets its text in its card in the panel; no dialog asks for
/// it.
///
/// Lists in the panel the annotations of the document in front, or of the whole book.
/// What the writer does in the panel goes to the document: the text typed in a card,
/// done or resolved, removing. Each of those is a step of the document's undo history, and
/// the text typed in a card is one step. An annotation of a chapter that is not open is
/// changed after its chapter is opened, so its undo history has the change.
class AnnotationsCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Opens a chapter of the book in a tab of its own, or brings its tab to the front
    using ChapterOpener = std::function<void(const QString& elementId)>;

    /// @brief Constructor
    /// @param panel The Annotations panel
    /// @param dock The panel's dock, shown when an annotation is added
    /// @param centralTabs The tabs of the documents
    /// @param openChapter Opens a chapter of the book (none: annotations of chapters not
    ///        open cannot be gone to or changed)
    /// @param statusBar Where a short message tells there is no further TODO (may be null)
    /// @param parent Parent object
    AnnotationsCoordinator(AnnotationsPanel* panel, QDockWidget* dock, QTabWidget* centralTabs,
                           ChapterOpener openChapter, QStatusBar* statusBar,
                           QObject* parent = nullptr);

    /// @brief Destructor
    ~AnnotationsCoordinator() override;

    /// @brief Give the annotation commands their callbacks
    /// @note Call after the commands are registered and the panel's dock is created
    void connectCommands();

    /// @brief Whether annotations can be added now (not while writing without distraction:
    ///        the panel the text is typed in is hidden then)
    void setAddingAvailable(bool available);

    /// @brief Who new annotations are by
    ///
    /// The setting annotations.author; without it the book's author, and without one the
    /// name of the computer's user.
    static QString author();

    /// @brief Add an annotation to the document in front and edit its text in its card
    ///
    /// It goes on the selection, or on the cursor's place without one. Left without text,
    /// it goes away when its editing ends.
    /// @return Whether it was added (not without a document in front)
    bool addAnnotation(editor::AnnotationKind kind);

    /// @brief The small menu of the kinds of annotations at the cursor (Add Annotation)
    void showAddMenu();

    /// @brief Go to the next TODO not done yet
    ///
    /// After the cursor in the document in front; when the panel lists the book, also in
    /// the chapters after it.
    /// @return Whether there was one
    bool goToNextTodo();

    /// @brief Go to the previous TODO not done yet (see goToNextTodo())
    bool goToPreviousTodo();

    /// @brief List the annotations in the panel again now
    void refresh();

    /// @brief Whether the text of an annotation is being edited in its card
    bool isEditing() const { return m_session.has_value(); }

private:
    /// @brief An annotation whose text is being edited in its card
    struct EditSession {
        QString key;                        ///< Its entry's key
        QString annotationId;               ///< Its id
        editor::BookEditor* editor{nullptr};  ///< The editor of its document
        bool isNew = false;    ///< Just added: it goes away when left without text
        bool joinable = false;  ///< The next change of its text joins the last undo step
        std::optional<QString> pendingText;  ///< Typed, not given to the document yet
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

    /// @brief The editor of a chapter's open tab (empty id: the document in front)
    editor::BookEditor* openEditor(const QString& elementId) const;

    /// @brief The editor of a chapter, opened when it is not
    editor::BookEditor* editorFor(const QString& elementId);

    /// @brief The book's chapters in order, with their annotations
    std::vector<ChapterAnnotations> bookAnnotations();

    /// @brief The annotations of a chapter: from its editor when it is open, else its file
    editor::AnnotationList chapterAnnotations(const QString& elementId);

    /// @brief The annotations in a chapter's file
    editor::AnnotationList fileAnnotations(const QString& elementId);

    // The panel
    void onCurrentTabChanged();
    void onEditorContentChanged();
    void onEditorDestroyed(QObject* editor);
    void scheduleRefresh();
    void updateCommandStates();
    void onAnnotationActivated(const AnnotationEntry& entry);
    void onEditingStarted(const AnnotationEntry& entry);
    void onTextEdited(const AnnotationEntry& entry, const QString& text);
    void onEditingFinished(const AnnotationEntry& entry);
    void onDoneToggled(const AnnotationEntry& entry, bool done);
    void onDeleteRequested(const AnnotationEntry& entry);
    void onEditorFocusRequested();

    // Editing an annotation's text
    void startSession(const QString& key, const QString& annotationId, editor::BookEditor* editor,
                      bool isNew);
    void applyPendingText();
    void finishSession();

    /// @brief Go to the next or previous TODO (see goToNextTodo())
    bool goToTodo(bool next);

    /// @brief Go to a TODO in another chapter of the book (see goToNextTodo())
    bool goToTodoInOtherChapter(bool next);

    /// @brief Go to an annotation of a chapter, opened when it is not, and select its card
    bool goToAnnotation(const QString& elementId, const QString& annotationId);

    AnnotationsPanel* m_panel;
    QDockWidget* m_dock;
    QTabWidget* m_centralTabs;
    ChapterOpener m_openChapter;
    QStatusBar* m_statusBar;

    std::optional<EditSession> m_session;
    bool m_applying = false;          ///< The coordinator is changing a document
    bool m_addingAvailable = true;
    bool m_stale = false;             ///< The panel's list is old: refresh when it is shown
    QTimer* m_textTimer{nullptr};     ///< Gives the typed text to the document
    QTimer* m_refreshTimer{nullptr};  ///< Lists the annotations again after edits
    QHash<QString, CachedFile> m_fileCache;  ///< By the files' paths
};

}  // namespace kalahari::gui
