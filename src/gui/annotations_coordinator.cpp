/// @file annotations_coordinator.cpp
/// @brief The writer's annotations: the commands for them and the Annotations panel

#include "kalahari/gui/annotations_coordinator.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/book.h"
#include "kalahari/core/book_element.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/document.h"
#include "kalahari/core/part.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/kml_document_model.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/annotation_card.h"
#include "kalahari/gui/panels/annotations_panel.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QDockWidget>
#include <QFileInfo>
#include <QMenu>
#include <QScopedValueRollback>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextDocument>
#include <QTimer>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <string>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief How long after the last key the typed text goes to the document (ms)
constexpr int TEXT_DELAY_MS = 250;

/// @brief How long after the last edit of the text the panel lists the annotations (ms)
constexpr int REFRESH_DELAY_MS = 300;

/// @brief How long the message that there is no further TODO stays (ms)
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

/// @brief A TODO not done yet
bool isOpenTodo(const editor::Annotation& annotation) {
    return annotation.kind == editor::AnnotationKind::Todo && !annotation.done;
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
    m_textTimer = new QTimer(this);
    m_textTimer->setSingleShot(true);
    m_textTimer->setInterval(TEXT_DELAY_MS);
    connect(m_textTimer, &QTimer::timeout, this, &AnnotationsCoordinator::applyPendingText);

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
    connect(m_panel, &AnnotationsPanel::editingStarted, this,
            &AnnotationsCoordinator::onEditingStarted);
    connect(m_panel, &AnnotationsPanel::textEdited, this, &AnnotationsCoordinator::onTextEdited);
    connect(m_panel, &AnnotationsPanel::editingFinished, this,
            &AnnotationsCoordinator::onEditingFinished);
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

    const auto canAdd = [this]() { return m_addingAvailable && currentEditor() != nullptr; };
    const auto hasDocument = [this]() { return currentEditor() != nullptr; };
    connectCommand("insert.annotation", [this]() { showAddMenu(); }, canAdd);
    connectCommand(
        "insert.comment", [this]() { addAnnotation(editor::AnnotationKind::Comment); }, canAdd);
    connectCommand("insert.todo", [this]() { addAnnotation(editor::AnnotationKind::Todo); }, canAdd);
    connectCommand("insert.note", [this]() { addAnnotation(editor::AnnotationKind::Note); }, canAdd);
    connectCommand("edit.nextTodo", [this]() { goToNextTodo(); }, hasDocument);
    connectCommand("edit.previousTodo", [this]() { goToPreviousTodo(); }, hasDocument);

    m_panel->setTodoActions(registry.getAction(QStringLiteral("edit.previousTodo")),
                            registry.getAction(QStringLiteral("edit.nextTodo")));
    onCurrentTabChanged();  // the context menu of the document in front
}

void AnnotationsCoordinator::setAddingAvailable(bool available) {
    m_addingAvailable = available;
    updateCommandStates();
}

QString AnnotationsCoordinator::author() {
    QString name = QString::fromStdString(core::SettingsManager::getInstance().get<std::string>(
                                              "annotations.author", std::string()))
                       .trimmed();
    if (!name.isEmpty()) {
        return name;
    }

    auto& projects = core::ProjectManager::getInstance();
    if (const core::Document* document = projects.isProjectOpen() ? projects.getDocument() : nullptr) {
        name = QString::fromStdString(document->getAuthor()).trimmed();
        if (!name.isEmpty()) {
            return name;
        }
    }

    name = qEnvironmentVariable("USERNAME");  // Windows
    if (name.isEmpty()) {
        name = qEnvironmentVariable("USER");
    }
    return name.trimmed();
}

// =============================================================================
// Commands
// =============================================================================

bool AnnotationsCoordinator::addAnnotation(editor::AnnotationKind kind) {
    EditorPanel* panel = currentEditorPanel();
    editor::BookEditor* editor = panel != nullptr ? panel->getBookEditor() : nullptr;
    if (editor == nullptr || !m_addingAvailable) {
        return false;
    }
    finishSession();  // the text of another one was being edited

    editor::Annotation added;
    {
        const QScopedValueRollback<bool> applying(m_applying, true);
        added = editor->addAnnotation(kind, QString(), author());
    }

    // Its text is typed in its card
    if (m_dock != nullptr) {
        m_dock->show();
        m_dock->raise();
    }
    refresh();
    const QString key = annotationKey(elementIdOf(panel), added.id);
    startSession(key, added.id, editor, true);
    m_panel->editAnnotation(key);
    return true;
}

void AnnotationsCoordinator::showAddMenu() {
    editor::BookEditor* editor = currentEditor();
    if (editor == nullptr || !m_addingAvailable) {
        return;
    }

    auto& art = core::ArtProvider::getInstance();
    QMenu menu(editor);
    QAction* comment = menu.addAction(art.getIcon("insert.comment"), tr("&Comment"));
    QAction* todo = menu.addAction(art.getIcon("insert.todo"), tr("&TODO"));
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
        m_statusBar->showMessage(next ? tr("No TODO after the cursor")
                                      : tr("No TODO before the cursor"),
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

    // The first TODO of a chapter after it, or the last one of a chapter before it
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
        if (const core::BookElement* element =
                core::ProjectManager::getInstance().findElement(elementId)) {
            return QString::fromStdString(element->getTitle());
        }
    }

    // A document outside the book: its tab's text, without the mark of changes
    QString title = m_centralTabs->tabText(m_centralTabs->indexOf(panel));
    if (title.startsWith(QLatin1Char('*'))) {
        title.remove(0, 1);
    }
    return title;
}

editor::BookEditor* AnnotationsCoordinator::openEditor(const QString& elementId) const {
    if (elementId.isEmpty()) {
        return currentEditor();
    }
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (panel != nullptr && elementIdOf(panel) == elementId) {
            return panel->getBookEditor();
        }
    }
    return nullptr;
}

editor::BookEditor* AnnotationsCoordinator::editorFor(const QString& elementId) {
    if (editor::BookEditor* editor = openEditor(elementId)) {
        return editor;
    }
    if (elementId.isEmpty() || !m_openChapter) {
        return nullptr;
    }
    m_openChapter(elementId);
    return openEditor(elementId);
}

std::vector<AnnotationsCoordinator::ChapterAnnotations> AnnotationsCoordinator::bookAnnotations() {
    std::vector<ChapterAnnotations> chapters;
    const core::Document* document = core::ProjectManager::getInstance().getDocument();
    if (document == nullptr) {
        return chapters;
    }

    const auto add = [this, &chapters](const std::shared_ptr<core::BookElement>& element) {
        if (element) {
            const QString id = QString::fromStdString(element->getId());
            chapters.push_back(
                {id, QString::fromStdString(element->getTitle()), chapterAnnotations(id)});
        }
    };
    const core::Book& book = document->getBook();
    for (const auto& element : book.getFrontMatter()) {
        add(element);
    }
    for (const auto& part : book.getBody()) {
        if (part) {
            for (const auto& chapter : part->getChapters()) {
                add(chapter);
            }
        }
    }
    for (const auto& element : book.getBackMatter()) {
        add(element);
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
    const core::BookElement* element = projects.findElement(elementId);
    if (element == nullptr || element->getFile().empty()) {
        return {};
    }
    std::filesystem::path path =
        std::filesystem::path(projects.getProjectPath().toStdWString()) / element->getFile();
    path.replace_extension(".kchapter");
    const QString filePath = QString::fromStdWString(path.wstring());
    const QFileInfo info(filePath);
    if (!info.exists()) {
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
    }
    updateCommandStates();
    if (m_panel->isVisible()) {
        refresh();
    } else {
        m_stale = true;
    }
}

void AnnotationsCoordinator::onEditorContentChanged() {
    // The writer changed the document: the next change of the edited annotation's text is
    // a step of its own
    if (!m_applying && m_session && m_session->editor == sender()) {
        m_session->joinable = false;
    }
    scheduleRefresh();
}

void AnnotationsCoordinator::onEditorDestroyed(QObject* editor) {
    if (m_session && m_session->editor == editor) {
        m_textTimer->stop();
        m_session.reset();
    }
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

void AnnotationsCoordinator::onEditingStarted(const AnnotationEntry& entry) {
    const QString key = entry.key();
    if (m_session && m_session->key == key) {
        return;  // a new one: its editing started when it was added
    }
    finishSession();

    // An annotation of a chapter not open is edited after its chapter is opened, so the
    // chapter's undo history has the change
    const bool open = openEditor(entry.elementId) != nullptr;
    editor::BookEditor* editor = editorFor(entry.elementId);
    if (editor == nullptr) {
        return;
    }
    editor->goToAnnotation(entry.annotation.id);
    startSession(key, entry.annotation.id, editor, false);

    // Opening the chapter may have taken the keys from the card: they go back to it
    if (!open) {
        QTimer::singleShot(0, this, [this, key]() {
            if (const AnnotationCard* card = m_panel->card(key);
                card == nullptr || !card->isEditing()) {
                m_panel->editAnnotation(key);
            }
        });
    }
}

void AnnotationsCoordinator::onTextEdited(const AnnotationEntry& entry, const QString& text) {
    if (m_session && m_session->key == entry.key()) {
        m_session->pendingText = text;
        m_textTimer->start();
    }
}

void AnnotationsCoordinator::onEditingFinished(const AnnotationEntry& entry) {
    if (m_session && m_session->key == entry.key()) {
        finishSession();
    }
}

void AnnotationsCoordinator::onDoneToggled(const AnnotationEntry& entry, bool done) {
    if (m_session && m_session->key == entry.key()) {
        applyPendingText();  // the text typed so far first
    }

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
    // Its text being typed no longer matters
    if (m_session && m_session->key == entry.key()) {
        const EditSession session = *m_session;
        m_textTimer->stop();
        m_session.reset();
        if (session.isNew && session.joinable) {
            // Just added and nothing else done since: as if it was never added
            const QScopedValueRollback<bool> applying(m_applying, true);
            session.editor->undoWithoutRedo();
            refresh();
            return;
        }
    }

    if (editor::BookEditor* editor = editorFor(entry.elementId)) {
        editor->removeAnnotation(entry.annotation.id);
    }
    refresh();
}

void AnnotationsCoordinator::onEditorFocusRequested() {
    if (editor::BookEditor* editor = currentEditor()) {
        editor->setFocus(Qt::OtherFocusReason);
    } else {
        m_panel->focusList();  // the editing ends all the same
    }
}

// =============================================================================
// Editing an annotation's text
// =============================================================================

void AnnotationsCoordinator::startSession(const QString& key, const QString& annotationId,
                                          editor::BookEditor* editor, bool isNew) {
    // A new one's text joins the step that added it
    m_session = EditSession{key, annotationId, editor, isNew, isNew, std::nullopt};
    connect(editor, &QObject::destroyed, this, &AnnotationsCoordinator::onEditorDestroyed,
            Qt::UniqueConnection);
}

void AnnotationsCoordinator::applyPendingText() {
    m_textTimer->stop();
    if (!m_session || !m_session->pendingText) {
        return;
    }
    const QString text = *m_session->pendingText;
    m_session->pendingText.reset();

    editor::BookEditor* editor = m_session->editor;
    const std::optional<editor::AnnotationPlace> place = placeOf(*editor, m_session->annotationId);
    if (!place) {
        m_session.reset();  // undone or removed meanwhile
        return;
    }
    if (place->annotation.text == text) {
        return;
    }

    editor::Annotation changed = place->annotation;
    changed.text = text;
    {
        const QScopedValueRollback<bool> applying(m_applying, true);
        editor->updateAnnotation(changed, m_session->joinable);
    }
    m_session->joinable = true;  // the rest of the typing joins this step
}

void AnnotationsCoordinator::finishSession() {
    applyPendingText();
    if (!m_session) {
        return;
    }
    const EditSession session = *m_session;
    m_session.reset();

    // A new one left without text goes away
    if (session.isNew) {
        const std::optional<editor::AnnotationPlace> place =
            placeOf(*session.editor, session.annotationId);
        if (place && place->annotation.text.trimmed().isEmpty()) {
            const QScopedValueRollback<bool> applying(m_applying, true);
            if (session.joinable) {
                session.editor->undoWithoutRedo();  // nothing else done since it was added
            } else {
                session.editor->removeAnnotation(session.annotationId);
            }
        }
    }
    scheduleRefresh();
}

}  // namespace kalahari::gui
