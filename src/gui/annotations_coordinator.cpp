/// @file annotations_coordinator.cpp
/// @brief The writer's annotations: the commands for them, the frame their text is written
/// in and the Annotations panel

#include "kalahari/gui/annotations_coordinator.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/editor/kml_document_model.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/panels/annotation_frame.h"
#include "kalahari/gui/panels/annotations_panel.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QFileInfo>
#include <QMenu>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief How long after the last edit of the text the panel lists the annotations (ms)
constexpr int REFRESH_DELAY_MS = 300;

/// @brief How long the message that there is no further to-do stays (ms)
constexpr int MESSAGE_TIMEOUT_MS = 3000;

/// @brief The commands of the annotations, whose state follows the document in front
const char* const COMMANDS[] = {"insert.annotation", "insert.comment", "insert.todo",
                                "insert.note",       "edit.nextTodo",  "edit.previousTodo"};

/// @brief The annotations of an editor's document, in text order
editor::AnnotationList annotationsOf(const editor::BookEditor& editor) {
    editor::AnnotationList annotations;
    for (const editor::AnnotationPlace& place : editor.annotations()) {
        annotations.append(place.annotation);
    }
    return annotations;
}

/// @brief Where an annotation is in an editor's document
std::optional<editor::AnnotationPlace> placeOf(const editor::BookEditor& editor,
                                               const QString& annotationId) {
    const QTextDocument* document = editor.textDocument();
    if (document == nullptr) {
        return std::nullopt;
    }
    return editor::findAnnotation(*document, annotationId);
}

/// @brief A to-do not done yet
bool isOpenTodo(const editor::Annotation& annotation) {
    return annotation.kind == editor::AnnotationKind::Todo && !annotation.done;
}

/// @brief A cursor over a range of a document: it follows the edits of the text
QTextCursor rangeCursor(QTextDocument* document, int start, int end) {
    QTextCursor cursor(document);
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    return cursor;
}

/// @brief Put the editor's cursor on a place of its document, with nothing selected
void placeCursor(editor::BookEditor& editor, int position) {
    const QTextDocument* document = editor.textDocument();
    if (document == nullptr) {
        return;
    }
    const QTextBlock block = document->findBlock(position);
    editor.clearSelection();
    editor.setCursorPosition({block.blockNumber(), position - block.position()});
}

}  // namespace

AnnotationsCoordinator::AnnotationsCoordinator(AnnotationsPanel* panel, QDockWidget* dock,
                                               QTabWidget* centralTabs, ChapterOpener openChapter,
                                               QStatusBar* statusBar, QObject* parent)
    : QObject(parent)
    , m_panel(panel)
    , m_dock(dock)
    , m_centralTabs(centralTabs)
    , m_openChapter(std::move(openChapter))
    , m_statusBar(statusBar)
{
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(REFRESH_DELAY_MS);
    connect(m_refreshTimer, &QTimer::timeout, this, &AnnotationsCoordinator::refresh);

    connect(m_centralTabs, &QTabWidget::currentChanged, this,
            &AnnotationsCoordinator::onCurrentTabChanged);

    // Another book: other chapters, other files
    auto& projects = core::ProjectManager::getInstance();
    const auto bookChanged = [this]() {
        m_fileCache.clear();
        updateCommandStates();
        scheduleRefresh();
    };
    connect(&projects, &core::ProjectManager::projectOpened, this, bookChanged);
    connect(&projects, &core::ProjectManager::projectClosed, this, bookChanged);

    connect(m_panel, &AnnotationsPanel::annotationActivated, this,
            &AnnotationsCoordinator::onAnnotationActivated);
    connect(m_panel, &AnnotationsPanel::editRequested, this,
            &AnnotationsCoordinator::onEditRequested);
    connect(m_panel, &AnnotationsPanel::doneToggled, this, &AnnotationsCoordinator::onDoneToggled);
    connect(m_panel, &AnnotationsPanel::deleteRequested, this,
            &AnnotationsCoordinator::onDeleteRequested);
    connect(m_panel, &AnnotationsPanel::editorFocusRequested, this,
            &AnnotationsCoordinator::onEditorFocusRequested);
    connect(m_panel, &AnnotationsPanel::scopeChanged, this, &AnnotationsCoordinator::refresh);

    // The list is made while the panel can be seen; one made old meanwhile is made again
    // when it comes into view
    if (m_dock != nullptr) {
        connect(m_dock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            if (visible && m_stale) {
                refresh();
            }
        });
    }

    onCurrentTabChanged();
}

AnnotationsCoordinator::~AnnotationsCoordinator() {
    disconnect(&core::ProjectManager::getInstance(), nullptr, this, nullptr);
    if (m_frame != nullptr) {
        m_frame->disconnect(this);  // the frame goes with its editor
    }
}

void AnnotationsCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    const auto connectCommand = [&registry](const std::string& id, std::function<void()> execute,
                                            std::function<bool()> enabled) {
        if (Command* command = registry.getCommand(id)) {
            command->execute = std::move(execute);
            command->isEnabled = std::move(enabled);
            registry.updateActionState(id);
        }
    };

    const auto hasDocument = [this]() { return currentEditor() != nullptr; };
    connectCommand("insert.annotation", [this]() { showAddMenu(); }, hasDocument);
    connectCommand(
        "insert.comment", [this]() { addAnnotation(editor::AnnotationKind::Comment); }, hasDocument);
    connectCommand(
        "insert.todo", [this]() { addAnnotation(editor::AnnotationKind::Todo); }, hasDocument);
    connectCommand(
        "insert.note", [this]() { addAnnotation(editor::AnnotationKind::Note); }, hasDocument);
    connectCommand("edit.nextTodo", [this]() { goToNextTodo(); }, hasDocument);
    connectCommand("edit.previousTodo", [this]() { goToPreviousTodo(); }, hasDocument);

    // F9 goes to the panel and back; Annotations in the menu shows or hides the panel
    m_panelAction = registry.getAction(QStringLiteral("view.annotations"));
    if (m_panelAction != nullptr) {
        m_panelAction->installEventFilter(this);
    }

    m_panel->setTodoActions(registry.getAction(QStringLiteral("edit.previousTodo")),
                            registry.getAction(QStringLiteral("edit.nextTodo")));
    onCurrentTabChanged();  // the context menu of the document in front
}

QString AnnotationsCoordinator::author() {
    const std::string setting = core::SettingsManager::getInstance().get<std::string>(
        "annotations.author", std::string());
    return QString::fromStdString(setting).trimmed();
}

bool AnnotationsCoordinator::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_panelAction && event->type() == QEvent::Shortcut) {
        togglePanelFocus();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

// =============================================================================
// Commands
// =============================================================================

bool AnnotationsCoordinator::addAnnotation(editor::AnnotationKind kind) {
    EditorPanel* panel = currentEditorPanel();
    editor::BookEditor* editor = panel != nullptr ? panel->getBookEditor() : nullptr;
    if (editor == nullptr) {
        return false;
    }
    finishWriting();  // the text of the open frame is kept
    const QTextCursor selection = editor->selectionCursor();
    if (selection.isNull()) {
        return false;
    }

    Writing writing;
    writing.editor = editor;
    writing.elementId = elementIdOf(panel);
    writing.kind = kind;
    writing.author = author();
    writing.range =
        rangeCursor(selection.document(), selection.selectionStart(), selection.selectionEnd());
    openFrame(writing, QString());
    return true;
}

void AnnotationsCoordinator::showAddMenu() {
    editor::BookEditor* editor = currentEditor();
    if (editor == nullptr) {
        return;
    }

    auto& art = core::ArtProvider::getInstance();
    QMenu menu(editor);
    QAction* comment = menu.addAction(art.getIcon("insert.comment"), tr("&Comment"));
    QAction* todo = menu.addAction(art.getIcon("insert.todo"), tr("&To do"));
    QAction* note = menu.addAction(art.getIcon("insert.note"), tr("&Note"));

    // Under the cursor
    const QRect cursor = editor->inputMethodQuery(Qt::ImCursorRectangle).toRect();
    const QAction* chosen = menu.exec(editor->mapToGlobal(cursor.bottomLeft()));
    if (chosen == comment) {
        addAnnotation(editor::AnnotationKind::Comment);
    } else if (chosen == todo) {
        addAnnotation(editor::AnnotationKind::Todo);
    } else if (chosen == note) {
        addAnnotation(editor::AnnotationKind::Note);
    }
}

bool AnnotationsCoordinator::editAnnotation(const QString& elementId, const QString& annotationId,
                                            bool fromPanel) {
    finishWriting();  // the text of the open frame is kept

    // An annotation of a chapter not in front is edited after its chapter is brought to the
    // front (opened when it is not), so the chapter's undo history has the change
    const editor::BookEditor* inFront = currentEditor();
    editor::BookEditor* editor = editorFor(elementId);
    const std::optional<editor::AnnotationPlace> place =
        editor != nullptr ? placeOf(*editor, annotationId) : std::nullopt;
    if (!place) {
        return false;
    }
    if (fromPanel) {
        editor->goToAnnotation(annotationId);  // the text shows its place
    }

    Writing writing;
    writing.editor = editor;
    writing.elementId = elementId;
    writing.annotationId = annotationId;
    writing.kind = place->annotation.kind;
    writing.author = place->annotation.author;
    writing.range = rangeCursor(editor->textDocument(), place->start, place->end);
    writing.fromPanel = fromPanel;
    openFrame(writing, place->annotation.text);

    // Bringing the chapter to the front may take the keys from the frame: they go back
    if (editor != inFront) {
        AnnotationFrame* frame = m_frame;
        QTimer::singleShot(0, frame, [frame]() {
            const QWidget* focus = QApplication::focusWidget();
            if (focus == nullptr || !frame->isAncestorOf(focus)) {
                frame->startTyping();
            }
        });
    }
    return true;
}

bool AnnotationsCoordinator::goToNextTodo() {
    return goToTodo(true);
}

bool AnnotationsCoordinator::goToPreviousTodo() {
    return goToTodo(false);
}

bool AnnotationsCoordinator::goToTodo(bool next) {
    EditorPanel* panel = currentEditorPanel();
    editor::BookEditor* editor = panel != nullptr ? panel->getBookEditor() : nullptr;
    if (editor == nullptr) {
        return false;
    }

    const QString id = next ? editor->goToNextTodo() : editor->goToPreviousTodo();
    if (!id.isEmpty()) {
        m_panel->selectAnnotation(annotationKey(elementIdOf(panel), id));
        return true;
    }
    if (m_panel->scope() == AnnotationScope::Book && goToTodoInOtherChapter(next)) {
        return true;
    }

    if (m_statusBar != nullptr) {
        m_statusBar->showMessage(next ? tr("No to-do after the cursor")
                                      : tr("No to-do before the cursor"),
                                 MESSAGE_TIMEOUT_MS);
    }
    return false;
}

bool AnnotationsCoordinator::goToTodoInOtherChapter(bool next) {
    const QString currentId = elementIdOf(currentEditorPanel());
    if (currentId.isEmpty() || !core::ProjectManager::getInstance().isProjectOpen()) {
        return false;  // a document outside the book
    }
    const std::vector<ChapterAnnotations> chapters = bookAnnotations();
    const auto current =
        std::find_if(chapters.cbegin(), chapters.cend(),
                     [&currentId](const ChapterAnnotations& c) { return c.elementId == currentId; });
    if (current == chapters.cend()) {
        return false;
    }

    // The first to-do of a chapter after it, or the last one of a chapter before it
    if (next) {
        for (auto chapter = std::next(current); chapter != chapters.cend(); ++chapter) {
            const auto todo = std::find_if(chapter->annotations.cbegin(),
                                           chapter->annotations.cend(), isOpenTodo);
            if (todo != chapter->annotations.cend()) {
                return goToAnnotation(chapter->elementId, todo->id);
            }
        }
        return false;
    }
    for (auto chapter = std::make_reverse_iterator(current); chapter != chapters.crend(); ++chapter) {
        const auto todo = std::find_if(chapter->annotations.crbegin(),
                                       chapter->annotations.crend(), isOpenTodo);
        if (todo != chapter->annotations.crend()) {
            return goToAnnotation(chapter->elementId, todo->id);
        }
    }
    return false;
}

bool AnnotationsCoordinator::goToAnnotation(const QString& elementId,
                                            const QString& annotationId) {
    editor::BookEditor* editor = editorFor(elementId);
    if (editor == nullptr || !editor->goToAnnotation(annotationId)) {
        return false;
    }
    m_panel->selectAnnotation(annotationKey(elementId, annotationId));
    return true;
}

void AnnotationsCoordinator::togglePanelFocus() {
    if (m_dock == nullptr) {
        return;
    }

    // From the panel: it goes, and the keys go back to the text
    if (m_panel->hasFocusInside()) {
        m_dock->hide();
        if (editor::BookEditor* editor = currentEditor()) {
            editor->setFocus(Qt::ShortcutFocusReason);
        }
        return;
    }

    // To the panel, with the card of the annotation at the cursor
    m_dock->show();
    m_dock->raise();
    if (m_stale) {
        refresh();
    }
    m_panel->focusList(keyAtCursor());
}

void AnnotationsCoordinator::refresh() {
    m_refreshTimer->stop();

    auto& projects = core::ProjectManager::getInstance();
    const bool bookOpen = projects.isProjectOpen();
    EditorPanel* current = currentEditorPanel();
    const editor::BookEditor* editor = current != nullptr ? current->getBookEditor() : nullptr;
    m_panel->setBookScopeAvailable(bookOpen);
    m_panel->setDocumentAvailable(editor != nullptr);
    m_stale = false;

    std::vector<AnnotationEntry> entries;
    const auto addEntries = [&entries](const QString& elementId, const QString& title,
                                       int chapterOrder, const editor::AnnotationList& list) {
        int textOrder = 0;
        for (const editor::Annotation& annotation : list) {
            entries.push_back({annotation, elementId, title, chapterOrder, textOrder++});
        }
    };
    if (m_panel->scope() == AnnotationScope::Book && bookOpen) {
        int chapterOrder = 0;
        for (const ChapterAnnotations& chapter : bookAnnotations()) {
            addEntries(chapter.elementId, chapter.title, chapterOrder++, chapter.annotations);
        }
    } else if (editor != nullptr) {
        addEntries(elementIdOf(current), titleOf(current), 0, annotationsOf(*editor));
    }
    m_panel->setEntries(entries);
}

// =============================================================================
// Documents
// =============================================================================

EditorPanel* AnnotationsCoordinator::currentEditorPanel() const {
    return qobject_cast<EditorPanel*>(m_centralTabs->currentWidget());
}

editor::BookEditor* AnnotationsCoordinator::currentEditor() const {
    EditorPanel* panel = currentEditorPanel();
    return panel != nullptr ? panel->getBookEditor() : nullptr;
}

QString AnnotationsCoordinator::elementIdOf(const EditorPanel* panel) const {
    return panel != nullptr ? panel->property("elementId").toString() : QString();
}

QString AnnotationsCoordinator::titleOf(const EditorPanel* panel) const {
    const QString elementId = elementIdOf(panel);
    if (!elementId.isEmpty()) {
        if (const core::ProjectElement* element =
                core::ProjectManager::getInstance().findElement(elementId)) {
            return element->title;
        }
    }

    // A document outside the book: its tab's text, without the mark of changes
    QString title = m_centralTabs->tabText(m_centralTabs->indexOf(panel));
    if (title.startsWith(QLatin1Char('*'))) {
        title.remove(0, 1);
    }
    return title;
}

EditorPanel* AnnotationsCoordinator::openPanel(const QString& elementId) const {
    if (elementId.isEmpty()) {
        return currentEditorPanel();
    }
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (panel != nullptr && elementIdOf(panel) == elementId) {
            return panel;
        }
    }
    return nullptr;
}

editor::BookEditor* AnnotationsCoordinator::openEditor(const QString& elementId) const {
    EditorPanel* panel = openPanel(elementId);
    return panel != nullptr ? panel->getBookEditor() : nullptr;
}

editor::BookEditor* AnnotationsCoordinator::editorFor(const QString& elementId) {
    if (EditorPanel* panel = openPanel(elementId)) {
        // Its tab in front: what is done there is seen, and Ctrl+Z undoes it there
        m_centralTabs->setCurrentWidget(panel);
        return panel->getBookEditor();
    }
    if (elementId.isEmpty() || !m_openChapter) {
        return nullptr;
    }
    m_openChapter(elementId);
    return openEditor(elementId);
}

std::vector<AnnotationsCoordinator::ChapterAnnotations> AnnotationsCoordinator::bookAnnotations() {
    std::vector<ChapterAnnotations> chapters;
    auto& projects = core::ProjectManager::getInstance();
    const core::BookProject* project = projects.project();
    if (project == nullptr) {
        return chapters;
    }

    // The text elements of the book, in reading order
    for (const core::ProjectElement* element : project->readingOrder()) {
        if (projects.formOf(*element) == core::ElementForm::Text) {
            chapters.push_back({element->id, element->title, chapterAnnotations(element->id)});
        }
    }
    return chapters;
}

editor::AnnotationList AnnotationsCoordinator::chapterAnnotations(const QString& elementId) {
    if (const editor::BookEditor* editor = openEditor(elementId)) {
        return annotationsOf(*editor);  // with the changes not saved yet
    }
    return fileAnnotations(elementId);
}

editor::AnnotationList AnnotationsCoordinator::fileAnnotations(const QString& elementId) {
    auto& projects = core::ProjectManager::getInstance();
    const core::ProjectElement* element = projects.findElement(elementId);
    if (element == nullptr || projects.formOf(*element) != core::ElementForm::Text) {
        return {};
    }
    const QString filePath = projects.filePathOf(*element);
    const QFileInfo info(filePath);
    if (filePath.isEmpty() || !info.exists()) {
        return {};
    }

    // Read again only when the file changed
    const QDateTime modified = info.lastModified();
    const auto cached = m_fileCache.constFind(filePath);
    if (cached != m_fileCache.cend() && cached->modified == modified) {
        return cached->annotations;
    }
    editor::AnnotationList annotations;
    if (const std::optional<core::ChapterDocument> chapter = core::ChapterDocument::load(filePath)) {
        annotations = editor::KmlDocumentModel::readAnnotations(chapter->kml());
    }
    m_fileCache.insert(filePath, CachedFile{modified, annotations});
    return annotations;
}

QString AnnotationsCoordinator::keyAtCursor() const {
    EditorPanel* panel = currentEditorPanel();
    const editor::BookEditor* editor = panel != nullptr ? panel->getBookEditor() : nullptr;
    if (editor == nullptr) {
        return {};
    }
    const int position = editor->selectionCursor().selectionStart();
    const std::vector<editor::AnnotationPlace> places = editor->annotations();  // by their ends

    // The one the cursor is in or on, else the last one before it, else the first one after
    const editor::AnnotationPlace* found = nullptr;
    for (const editor::AnnotationPlace& place : places) {
        if (place.start <= position && position <= place.end) {
            found = &place;
            break;
        }
        if (place.end <= position) {
            found = &place;
        }
    }
    if (found == nullptr && !places.empty()) {
        found = &places.front();
    }
    return found != nullptr ? annotationKey(elementIdOf(panel), found->annotation.id) : QString();
}

// =============================================================================
// The panel
// =============================================================================

void AnnotationsCoordinator::onCurrentTabChanged() {
    if (editor::BookEditor* editor = currentEditor()) {
        auto& registry = CommandRegistry::getInstance();
        editor->setContextMenuActions({registry.getAction(QStringLiteral("insert.comment")),
                                       registry.getAction(QStringLiteral("insert.todo")),
                                       registry.getAction(QStringLiteral("insert.note"))});
        connect(editor, &editor::BookEditor::contentChanged, this,
                &AnnotationsCoordinator::onEditorContentChanged, Qt::UniqueConnection);
        connect(editor, &editor::BookEditor::annotationMarkClicked, this,
                &AnnotationsCoordinator::onMarkClicked, Qt::UniqueConnection);
    }
    updateCommandStates();
    if (m_panel->isVisible()) {
        refresh();
    } else {
        m_stale = true;
    }
}

void AnnotationsCoordinator::onEditorContentChanged() {
    // An annotation edited in the frame and taken off the text meanwhile (Ctrl+Z): nothing
    // to write its text to
    if (m_writing && m_writing->editor == sender() && !m_writing->annotationId.isEmpty() &&
        !placeOf(*m_writing->editor, m_writing->annotationId)) {
        cancelWriting();
    }
    scheduleRefresh();
}

void AnnotationsCoordinator::scheduleRefresh() {
    if (m_panel->isVisible()) {
        m_refreshTimer->start();
    } else {
        m_stale = true;
    }
}

void AnnotationsCoordinator::updateCommandStates() {
    auto& registry = CommandRegistry::getInstance();
    for (const char* id : COMMANDS) {
        registry.updateActionState(id);
    }
}

void AnnotationsCoordinator::onAnnotationActivated(const AnnotationEntry& entry) {
    goToAnnotation(entry.elementId, entry.annotation.id);
}

void AnnotationsCoordinator::onEditRequested(const AnnotationEntry& entry) {
    editAnnotation(entry.elementId, entry.annotation.id, true);
}

void AnnotationsCoordinator::onDoneToggled(const AnnotationEntry& entry, bool done) {
    editor::BookEditor* editor = editorFor(entry.elementId);
    const std::optional<editor::AnnotationPlace> place =
        editor != nullptr ? placeOf(*editor, entry.annotation.id) : std::nullopt;
    if (place && place->annotation.done != done) {
        editor::Annotation changed = place->annotation;
        changed.done = done;
        editor->updateAnnotation(changed);  // a step of its own
    }
    refresh();  // the card shows the annotation as it is now
}

void AnnotationsCoordinator::onDeleteRequested(const AnnotationEntry& entry) {
    // Its frame has nothing to write to any more
    if (m_writing && m_writing->annotationId == entry.annotation.id &&
        m_writing->elementId == entry.elementId) {
        cancelWriting();
    }
    if (editor::BookEditor* editor = editorFor(entry.elementId)) {
        editor->removeAnnotation(entry.annotation.id);
    }
    refresh();
}

void AnnotationsCoordinator::onEditorFocusRequested() {
    if (editor::BookEditor* editor = currentEditor()) {
        editor->setFocus(Qt::OtherFocusReason);
    }
}

// =============================================================================
// The frame
// =============================================================================

void AnnotationsCoordinator::onMarkClicked(const QString& annotationId) {
    if (sender() == currentEditor()) {
        editAnnotation(elementIdOf(currentEditorPanel()), annotationId, false);
    }
}

void AnnotationsCoordinator::openFrame(const Writing& writing, const QString& text) {
    m_writing = writing;
    auto* frame = new AnnotationFrame(writing.editor);
    m_frame = frame;
    frame->setKind(writing.kind);
    frame->setAuthor(writing.author);
    frame->setText(text);
    connect(frame, &AnnotationFrame::saveRequested, this, &AnnotationsCoordinator::saveWriting);
    connect(frame, &AnnotationFrame::cancelRequested, this, &AnnotationsCoordinator::cancelWriting);

    // A closed tab takes the frame with its editor: nothing is left to write
    connect(frame, &QObject::destroyed, this, [this]() {
        m_frame = nullptr;
        m_writing.reset();
    });

    // In the colors of the paper, also when the paper or the theme changes
    connect(writing.editor, &editor::BookEditor::appearanceChanged, frame,
            [this]() { colorFrame(); });
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, frame,
            [this]() { colorFrame(); });
    colorFrame();

    // At the end of its fragment, or on its place, as the text moves
    frame->setPlacement([this]() {
        AnnotationFrame::Placement placement;
        if (m_writing && !m_writing->range.isNull()) {
            const QTextCursor& range = m_writing->range;
            placement.place = m_writing->editor->placeRect(range.selectionEnd(), range.hasSelection());
            placement.column = m_writing->editor->textColumnRect();
        }
        return placement;
    });
    frame->show();
    frame->raise();
    frame->startTyping();

    if (!writing.annotationId.isEmpty()) {
        m_panel->selectAnnotation(annotationKey(writing.elementId, writing.annotationId));
    }
}

void AnnotationsCoordinator::saveWriting() {
    if (!m_writing || m_frame == nullptr || !m_frame->canSave()) {
        return;
    }
    const QString text = m_frame->text();
    const Writing writing = *m_writing;
    editor::BookEditor* editor = writing.editor;
    QString annotationId = writing.annotationId;

    if (annotationId.isEmpty()) {
        // A new one, on the text it was written for (or its place, when it went meanwhile)
        const QTextCursor& range = writing.range;
        if (!range.isNull() && range.document() == editor->textDocument()) {
            const QTextCursor selection = editor->selectionCursor();
            const bool selectionStays = range.hasSelection() &&
                                        selection.selectionStart() == range.selectionStart() &&
                                        selection.selectionEnd() == range.selectionEnd();
            annotationId = editor
                               ->addAnnotation(range.selectionStart(), range.selectionEnd(),
                                               writing.kind, text, writing.author)
                               .id;

            // The writer goes on after the fragment: typing does not replace it
            if (selectionStays) {
                placeCursor(*editor, range.selectionEnd());
            }
        }
    } else if (const std::optional<editor::AnnotationPlace> place = placeOf(*editor, annotationId)) {
        if (place->annotation.text != text) {
            editor::Annotation changed = place->annotation;
            changed.text = text;
            editor->updateAnnotation(changed);  // one step
        }
    } else {
        annotationId.clear();  // taken off the text meanwhile
    }

    closeFrame();
    refresh();
    if (!annotationId.isEmpty()) {
        m_panel->selectAnnotation(annotationKey(writing.elementId, annotationId));
    }
}

void AnnotationsCoordinator::cancelWriting() {
    closeFrame();
}

void AnnotationsCoordinator::finishWriting(const editor::BookEditor* editor) {
    if (!m_writing || (editor != nullptr && m_writing->editor != editor)) {
        return;
    }
    if (m_frame != nullptr && m_frame->canSave()) {
        saveWriting();
    } else {
        cancelWriting();
    }
}

void AnnotationsCoordinator::closeFrame() {
    AnnotationFrame* frame = m_frame;
    const std::optional<Writing> writing = m_writing;
    m_frame = nullptr;
    m_writing.reset();
    if (frame == nullptr) {
        return;
    }
    frame->disconnect(this);  // its going tells nothing any more

    // The keys go back where the writing came from, before the frame goes
    const QWidget* focus = QApplication::focusWidget();
    if (writing && focus != nullptr && (focus == frame || frame->isAncestorOf(focus))) {
        const QString key = annotationKey(writing->elementId, writing->annotationId);
        if (writing->fromPanel && m_panel->isVisible()) {
            m_panel->focusList(key);
        } else {
            writing->editor->setFocus(Qt::OtherFocusReason);
        }
    }
    frame->hide();
    frame->deleteLater();
}

void AnnotationsCoordinator::colorFrame() {
    if (m_frame == nullptr || !m_writing) {
        return;
    }

    // The kind's color on this paper: the one its marks have
    const editor::EditorAppearance& appearance = m_writing->editor->appearance();
    const editor::EditorColorMode mode = appearance.colorMode;
    const QColor paper = appearance.colors.background(mode);
    const QColor text = appearance.colors.textColor(mode);
    const editor::EditorColors::AnnotationColors& kinds = appearance.colors.annotations(mode);
    QColor kind;
    switch (m_writing->kind) {
    case editor::AnnotationKind::Comment:
        kind = kinds.comment;
        break;
    case editor::AnnotationKind::Todo:
        kind = kinds.todo;
        break;
    case editor::AnnotationKind::Note:
        kind = kinds.note;
        break;
    }
    if (!kind.isValid()) {
        kind = core::ThemeManager::getInstance().editorColor(
            editor::annotationColorKey(m_writing->kind, mode == editor::EditorColorMode::Dark),
            text);
    }
    m_frame->setColors(annotationCardColors(kind, paper, text));
}

}  // namespace kalahari::gui
