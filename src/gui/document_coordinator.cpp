/// @file document_coordinator.cpp
/// @brief Document lifecycle and file operations coordination implementation
///
/// OpenSpec #00038 - Phase 7: Extract Document Operations from MainWindow

#include "kalahari/gui/document_coordinator.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/widgets/standalone_info_bar.h"
#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/gui/dialogs/add_to_project_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/project_database.h"
#include "kalahari/core/document.h"
#include "kalahari/core/document_archive.h"
#include "kalahari/core/book.h"
#include "kalahari/core/book_element.h"
#include "kalahari/core/part.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/recent_books_manager.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/standalone_file.h"
#include "kalahari/editor/style_resolver.h"
#include "kalahari/editor/statistics_collector.h"
#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>
#include <QTextEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QProgressDialog>
#include <QApplication>
#include <QHash>
#include <functional>
#include <map>

Q_DECLARE_METATYPE(kalahari::core::StandaloneFile)

namespace kalahari {
namespace gui {

namespace {

/// What an editor tab shows, which decides how it is saved
enum class EditorKind {
    ProjectChapter,  ///< Chapter of the open project (saved with the project)
    StandaloneFile,  ///< Chapter or text file opened outside the project (saved on its own)
    SingleDocument   ///< Phase 0 single-file document
};

EditorKind editorKind(const EditorPanel* editor) {
    if (!editor->property("elementId").toString().isEmpty()
        && core::ProjectManager::getInstance().isProjectOpen()) {
        return EditorKind::ProjectChapter;
    }
    if (editor->property("isStandaloneFile").toBool()) {
        return EditorKind::StandaloneFile;
    }
    return EditorKind::SingleDocument;
}

/// File of a standalone file tab
core::StandaloneFile standaloneFileOf(const EditorPanel* editor) {
    return editor->property("standaloneFile").value<core::StandaloneFile>();
}

/// Package of the book type of a template of the New Book window; empty for the empty
/// project, which is a user project
QString bookTypeOf(const QString& templateId) {
    static const QHash<QString, QString> types{
        {QStringLiteral("template.novel"), QStringLiteral("kalahari.novel")},
        {QStringLiteral("template.shortStories"), QStringLiteral("kalahari.short_stories")},
        {QStringLiteral("template.nonfiction"), QStringLiteral("kalahari.nonfiction")},
        {QStringLiteral("template.screenplay"), QStringLiteral("kalahari.screenplay")},
        {QStringLiteral("template.poetry"), QStringLiteral("kalahari.poetry")},
    };
    return types.value(templateId);
}

/// What is wrong, one problem per line, for the details of a message
QString detailsOf(const QStringList& problems) {
    return problems.join(QLatin1Char('\n'));
}

/// Icon of a standalone file's tab
QString standaloneIconId(const QString& path) {
    return core::StandaloneFile::typeOf(path) == core::StandaloneFile::Type::Chapter
        ? QStringLiteral("template.chapter")
        : QStringLiteral("common.file");
}

}  // anonymous namespace

DocumentCoordinator::DocumentCoordinator(QMainWindow* mainWindow,
                                           QTabWidget* centralTabs,
                                           NavigatorPanel* navigatorPanel,
                                           PropertiesPanel* propertiesPanel,
                                           DashboardPanel* dashboardPanel,
                                           NavigatorCoordinator* navigatorCoordinator,
                                           StandaloneInfoBar* standaloneInfoBar,
                                           QStatusBar* statusBar,
                                           DirtyStateGetter isDirty,
                                           DirtySetter setDirty,
                                           WindowTitleUpdater updateTitle,
                                           HasUnsavedChangesGetter hasUnsavedChanges,
                                           QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_centralTabs(centralTabs)
    , m_navigatorPanel(navigatorPanel)
    , m_propertiesPanel(propertiesPanel)
    , m_dashboardPanel(dashboardPanel)
    , m_navigatorCoordinator(navigatorCoordinator)
    , m_standaloneInfoBar(standaloneInfoBar)
    , m_statusBar(statusBar)
    , m_isDirty(std::move(isDirty))
    , m_setDirty(std::move(setDirty))
    , m_updateWindowTitle(std::move(updateTitle))
    , m_hasUnsavedChanges(std::move(hasUnsavedChanges))
    , m_currentDocument(std::nullopt)
    , m_currentFilePath("")
{
    // Files opened outside the project: the info bar shows the file of the current tab,
    // and the navigator's "Other Files" open, add and drop them
    connect(m_centralTabs, &QTabWidget::currentChanged,
            this, &DocumentCoordinator::updateStandaloneInfoBar);
    connect(m_standaloneInfoBar, &StandaloneInfoBar::dismissed, this, [this]() {
        if (EditorPanel* editor = getCurrentEditor()) {
            editor->setProperty("infoBarDismissed", true);  // closed for this tab
        }
    });
    connect(m_navigatorPanel, &NavigatorPanel::standaloneFileSelected,
            this, &DocumentCoordinator::openStandaloneFile);
    connect(m_navigatorPanel, &NavigatorPanel::requestAddToProject,
            this, &DocumentCoordinator::addToProject);
    connect(m_navigatorPanel, &NavigatorPanel::requestRemoveStandaloneFile,
            this, &DocumentCoordinator::removeStandaloneFile);

    auto& logger = core::Logger::getInstance();
    logger.debug("DocumentCoordinator created");
}

bool DocumentCoordinator::maybeSave() {
    // Consult the single source of truth (content + structure + standalone tabs).
    if (!m_hasUnsavedChanges()) {
        return true;
    }

    const QStringList names = unsavedDocumentNames();
    const QString filename = names.isEmpty() ? tr("Untitled") : names.join(QStringLiteral(", "));

    const auto reply = dialogs::MessageDialog::ask(
        m_mainWindow, tr("Unsaved Changes"),
        tr("Do you want to save changes to %1?").arg(filename), tr("&Save"),
        tr("Do&n't Save"));

    if (reply == dialogs::MessageDialog::Answer::Accept) {
        return saveAllChanges();  // True only if everything is now saved
    } else if (reply == dialogs::MessageDialog::Answer::Cancel) {
        return false;
    }
    // Don't Save
    return true;
}

EditorPanel* DocumentCoordinator::getCurrentEditor() const {
    if (!m_centralTabs) {
        return nullptr;
    }
    QWidget* currentWidget = m_centralTabs->currentWidget();
    return qobject_cast<EditorPanel*>(currentWidget);
}

QString DocumentCoordinator::getPhase0Content(const core::Document& doc) const {
    const auto& book = doc.getBook();
    const auto& body = book.getBody();

    if (body.empty()) return QString();

    const auto& firstPart = body[0];
    const auto& chapters = firstPart->getChapters();

    if (chapters.empty()) return QString();

    const auto& firstChapter = chapters[0];
    auto content = firstChapter->getMetadata("_phase0_content");

    return content.has_value() ? QString::fromStdString(content.value()) : QString();
}

void DocumentCoordinator::setPhase0Content(core::Document& doc, const QString& text) {
    auto& book = doc.getBook();
    auto& body = book.getBody();

    // Create Part if doesn't exist
    if (body.empty()) {
        auto part = std::make_shared<core::Part>("part-001", "Content");
        body.push_back(part);
    }

    auto& firstPart = body[0];
    const auto& chapters = firstPart->getChapters();

    // Create Chapter if doesn't exist
    if (chapters.empty()) {
        auto chapter = std::make_shared<core::BookElement>(
            "chapter", "ch-001", "Chapter 1", ""
        );
        firstPart->addChapter(chapter);
    }

    // Store text in metadata
    const auto& chaptersList = firstPart->getChapters();
    chaptersList[0]->setMetadata("_phase0_content", text.toStdString());
    chaptersList[0]->touch();
    doc.touch();
}

// =============================================================================
// Document Operations
// =============================================================================

void DocumentCoordinator::onNewDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: New Document");

    // Check for unsaved changes via the single source of truth.
    if (m_hasUnsavedChanges()) {
        const auto reply = dialogs::MessageDialog::ask(
            m_mainWindow, tr("Unsaved Changes"),
            tr("Do you want to save changes to the current document?"), tr("&Save"),
            tr("Do&n't Save"));

        if (reply == dialogs::MessageDialog::Answer::Accept) {
            if (!saveAllChanges()) return;  // Save was cancelled or failed
        } else if (reply == dialogs::MessageDialog::Answer::Cancel) {
            return;
        }
        // Don't Save -> continue
    }

    // Create new EditorPanel tab (on-demand)
    EditorPanel* newEditor = new EditorPanel(m_mainWindow);
    int tabIndex = m_centralTabs->addTab(newEditor, tr("Untitled"));
    m_centralTabs->setCurrentIndex(tabIndex);

    // Connect contentChanged signal for dirty tracking
    connect(newEditor, &EditorPanel::contentChanged,
            this, [this]() {
                if (!m_currentDocument.has_value()) return;
                m_setDirty(true);
            });

    // Create new document with empty content structure
    m_currentDocument = core::Document("Untitled", "User", "en");
    m_currentFilePath = "";

    // Initialize with empty content so editor has something to work with
    // This ensures the editor panel has a valid document attached
    newEditor->setText("");
    newEditor->setContent("");  // Ensure content is properly initialized
    m_setDirty(false);

    logger.info("New document created in new tab");
    m_statusBar->showMessage(tr("New document created"), 2000);
    emit documentOpened();
}

void DocumentCoordinator::onNewProject() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: New Project (OpenSpec #00033)");

    // Show NewItemDialog in Project mode
    dialogs::NewItemDialog dialog(dialogs::NewItemMode::Project, m_mainWindow);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto result = dialog.result();

    // The new book takes the place of the open one
    if (!agreeToCloseBook(
            tr("Do you want to save changes to '%1' before creating the new book?"),
            tr("Do you want to close '%1' and create the new book?"))) {
        logger.debug("User cancelled creating a new book");
        return;
    }

    // ProjectManager creates the book of the template's type and opens it, and
    // onProjectOpened() shows it
    auto& pm = core::ProjectManager::getInstance();
    QStringList problems;
    if (!pm.createProject(result.location, result.title, result.author, result.language,
                          result.createSubfolder, bookTypeOf(result.templateId), &problems)) {
        logger.error("Failed to create project: {}", result.title.toStdString());
        dialogs::MessageDialog::error(m_mainWindow, tr("New Book"),
                                      tr("Could not create the book '%1'.").arg(result.title),
                                      detailsOf(problems));
        return;
    }

    m_updateWindowTitle();
    logger.info("Project created: {} in {}", result.title.toStdString(),
                result.location.toStdString());
    m_statusBar->showMessage(tr("Book created: %1").arg(result.title), 3000);
    emit documentOpened();

    // The writer can start writing at once
    openFirstText();
}

bool DocumentCoordinator::agreeToCloseBook(const QString& saveQuestion,
                                           const QString& closeQuestion) {
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        return true;
    }

    QString bookName = pm.book() ? pm.book()->title : QString();
    if (bookName.isEmpty()) {
        bookName = QFileInfo(pm.getProjectPath()).fileName();
    }

    if (m_hasUnsavedChanges()) {
        // Dirty: offer to save before switching (single source of truth)
        const auto reply = dialogs::MessageDialog::ask(m_mainWindow, tr("Unsaved Changes"),
                                                       saveQuestion.arg(bookName), tr("&Save"),
                                                       tr("Do&n't Save"));
        if (reply == dialogs::MessageDialog::Answer::Cancel) {
            return false;
        }
        if (reply == dialogs::MessageDialog::Answer::Accept) {
            onSaveAll();
            return !m_hasUnsavedChanges();  // a failed save stops the command
        }
        return true;  // Don't Save: the changes go with the book
    }

    // Clean: plain confirmation to avoid an accidental project switch. ProjectManager closes
    // the book, and its projectAboutToClose() prepares the services for that.
    return dialogs::MessageDialog::confirm(m_mainWindow, tr("Close Book?"),
                                           closeQuestion.arg(bookName), tr("&Close Book"));
}

void DocumentCoordinator::openFirstText() {
    auto& pm = core::ProjectManager::getInstance();
    const core::ProjectBook* book = pm.book();
    if (!book || !m_navigatorCoordinator) {
        return;
    }

    // The first text of the body, also inside a part
    const std::function<const core::ProjectElement*(const QList<core::ProjectElement>&)>
        firstText = [&pm, &firstText](const QList<core::ProjectElement>& elements)
        -> const core::ProjectElement* {
        for (const core::ProjectElement& element : elements) {
            if (pm.formOf(element) == core::ElementForm::Text) {
                return &element;
            }
            if (const core::ProjectElement* inner = firstText(element.elements)) {
                return inner;
            }
        }
        return nullptr;
    };
    if (const core::ProjectElement* text = firstText(book->mainElements)) {
        m_navigatorCoordinator->onElementSelected(text->id, text->title);
    }
}

void DocumentCoordinator::onOpenDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Open Document");

    // Use ProjectManager for opening projects
    QString filename = QFileDialog::getOpenFileName(
        m_mainWindow,
        tr("Open Book"),
        QString(),
        tr("Kalahari Books (*.klh)")
    );

    if (filename.isEmpty()) {
        logger.info("Open cancelled by user");
        return;
    }

    // Use ProjectManager to open the project
    auto& pm = core::ProjectManager::getInstance();

    // If a project is already open, ask user for confirmation before closing it
    if (pm.isProjectOpen()) {
        // If the chosen file IS the already-open project, just ignore — no close/reopen
        // prompt. Compare against the MANIFEST (.klh) path, not getProjectPath() (the
        // project directory), which would never match the chosen .klh file.
        if (QFileInfo(pm.getManifestPath()) == QFileInfo(filename)) {
            logger.debug("Project is already open, ignoring: {}", filename.toStdString());
            return;
        }
    }
    if (!agreeToCloseBook(
            tr("Do you want to save changes to '%1' before opening the selected book?"),
            tr("Do you want to close '%1' and open the selected book?"))) {
        logger.debug("User cancelled opening new project");
        return;
    }

    QStringList problems;
    if (!pm.openProject(filename, &problems)) {
        dialogs::MessageDialog::error(m_mainWindow, tr("Open Error"),
                                      tr("Failed to open book: %1")
                                          .arg(QDir::toNativeSeparators(filename)),
                                      detailsOf(problems));
        return;
    }
    // ProjectManager emits projectOpened, which triggers onProjectOpened()

    // Add to recent files list
    core::RecentBooksManager::getInstance().addRecentFile(filename);
}

void DocumentCoordinator::onOpenRecentFile(const QString& filePath) {
    auto& logger = core::Logger::getInstance();
    logger.info("Opening recent file: {}", filePath.toStdString());

    // Check if file still exists
    if (!QFileInfo::exists(filePath)) {
        dialogs::MessageDialog::warning(
            m_mainWindow, tr("File Not Found"),
            tr("The file '%1' no longer exists.").arg(QDir::toNativeSeparators(filePath)));

        // Remove from recent files
        core::RecentBooksManager::getInstance().removeRecentFile(filePath);
        return;
    }

    const bool isProjectFile = filePath.endsWith(".klh", Qt::CaseInsensitive);

    // Check for unsaved changes via the single source of truth. Switching from an open
    // project asks below, once, after checking that it is a different project.
    if (m_hasUnsavedChanges()
        && !(isProjectFile && core::ProjectManager::getInstance().isProjectOpen())) {
        const auto reply = dialogs::MessageDialog::ask(
            m_mainWindow, tr("Unsaved Changes"),
            tr("Do you want to save changes to the current document?"), tr("&Save"),
            tr("Do&n't Save"));

        if (reply == dialogs::MessageDialog::Answer::Accept) {
            onSaveDocument();
            if (m_hasUnsavedChanges()) return;
        } else if (reply == dialogs::MessageDialog::Answer::Cancel) {
            return;
        }
    }

    // Check if file is a .klh manifest - try ProjectManager first
    if (isProjectFile) {
        auto& pm = core::ProjectManager::getInstance();

        // If a project is already open, check if it's the SAME project
        if (pm.isProjectOpen()) {
            // If trying to open the same project that's already open, just ignore.
            // Compare against the MANIFEST (.klh) path: getProjectPath() is the project
            // DIRECTORY, so comparing it to the clicked .klh file never matched and the
            // "close & reopen?" prompt fired even for the already-open project.
            if (QFileInfo(pm.getManifestPath()) == QFileInfo(filePath)) {
                logger.debug("Project is already open, ignoring: {}", filePath.toStdString());
                return;
            }

            // Different project - handle unsaved changes before closing
            if (!agreeToCloseBook(
                    tr("Do you want to save changes to '%1' before opening the selected book?"),
                    tr("Do you want to close '%1' and open the selected book?"))) {
                logger.debug("User cancelled opening new project");
                return;
            }
        }

        QStringList problems;
        if (pm.openProject(filePath, &problems)) {
            // Successfully opened as project
            logger.info("Opened .klh file as project: {}", filePath.toStdString());
            // Add to recent files so it's remembered as last opened
            core::RecentBooksManager::getInstance().addRecentFile(filePath);
            return;
        }

        // For .klh files, if openProject fails, show error and return
        // Do NOT fall through to try old archive format (which would always fail
        // for JSON manifests and incorrectly remove the file from recent files)
        logger.error("Failed to open .klh file as project: {}", filePath.toStdString());
        dialogs::MessageDialog::error(
            m_mainWindow, tr("Open Error"),
            tr("Failed to open book: %1").arg(QDir::toNativeSeparators(filePath)),
            detailsOf(problems));
        // Do NOT remove from recent files - the project might be recoverable
        return;
    }

    // Load document (old archive format)
    std::filesystem::path filepath = filePath.toStdString();
    auto loaded = core::DocumentArchive::load(filepath);

    if (!loaded.has_value()) {
        dialogs::MessageDialog::error(
            m_mainWindow, tr("Open Error"),
            tr("Failed to open document: %1").arg(QDir::toNativeSeparators(filePath)));
        logger.error("Failed to load recent file: {}", filepath.string());

        // Remove from recent files
        core::RecentBooksManager::getInstance().removeRecentFile(filePath);
        return;
    }

    // Create new EditorPanel tab
    EditorPanel* newEditor = new EditorPanel(m_mainWindow);
    QString docTitle = QString::fromStdString(loaded.value().getTitle());
    int tabIndex = m_centralTabs->addTab(newEditor, docTitle);
    m_centralTabs->setCurrentIndex(tabIndex);

    // Connect contentChanged signal for dirty tracking
    connect(newEditor, &EditorPanel::contentChanged,
            this, [this]() {
                if (!m_currentDocument.has_value()) return;
                m_setDirty(true);
            });

    // Update state
    m_currentDocument = std::move(loaded.value());
    m_currentFilePath = filepath;

    // Extract text and load into editor
    QString content = getPhase0Content(m_currentDocument.value());
    newEditor->setText(content);
    m_setDirty(false);

    // Move to top of recent files
    core::RecentBooksManager::getInstance().addRecentFile(filePath);

    logger.info("Recent file loaded: {}", filepath.string());
    m_statusBar->showMessage(tr("Document opened: %1").arg(filePath), 2000);
    emit documentOpened();
    emit recentFilesUpdated();
}

void DocumentCoordinator::onSaveDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save Document");

    // A file opened outside the project is saved on its own, also with a project open
    EditorPanel* editor = getCurrentEditor();
    if (editor && editorKind(editor) == EditorKind::StandaloneFile) {
        saveEditor(editor);
        return;
    }

    // Check if we're in project mode - delegate to Save All for project saves
    auto& pm = core::ProjectManager::getInstance();
    if (pm.isProjectOpen()) {
        // In project mode, Ctrl+S saves the manifest (including metadata like notes)
        // and any dirty chapter content
        onSaveAll();
        return;
    }

    if (!editor) {
        logger.debug("No editor tab active - cannot save");
        m_statusBar->showMessage(tr("No document to save"), 2000);
        return;
    }

    // Phase 0: single file document
    saveEditor(editor);
}

void DocumentCoordinator::onSaveAsDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save As Document");

    // Get current editor (returns nullptr if Dashboard is active)
    EditorPanel* editor = getCurrentEditor();
    if (!editor) {
        logger.debug("No editor tab active - cannot save");
        m_statusBar->showMessage(tr("No document to save"), 2000);
        return;
    }

    if (editorKind(editor) == EditorKind::StandaloneFile) {
        saveStandaloneFileAs(editor);
        return;
    }
    saveSingleDocument(editor, true);
}

bool DocumentCoordinator::saveSingleDocument(EditorPanel* editor, bool askForPath) {
    auto& logger = core::Logger::getInstance();

    // An unsaved document gets its file name from the user, as with Save As
    const bool newFile = askForPath || m_currentFilePath.empty();
    std::filesystem::path filepath = m_currentFilePath;
    if (newFile) {
        // Show save file dialog
        QString filename = QFileDialog::getSaveFileName(
            m_mainWindow,
            tr("Save Document As"),
            QString(),
            tr("Kalahari Files (*.klh)")
        );

        if (filename.isEmpty()) {
            logger.info("Save As cancelled by user");
            return false;
        }

        // Ensure .klh extension
        if (!filename.endsWith(".klh", Qt::CaseInsensitive)) {
            filename += ".klh";
        }
        filepath = filename.toStdString();
    }

    // Ensure we have a document
    if (!m_currentDocument.has_value()) {
        m_currentDocument = core::Document("Untitled", "User", "en");
    }

    // Get text from the editor and update document
    setPhase0Content(m_currentDocument.value(), editor->getText());

    // A new file gives the document its title
    if (newFile) {
        m_currentDocument->setTitle(filepath.stem().string());
    }

    // Save to file
    const QString filename = QString::fromStdString(filepath.string());
    if (!core::DocumentArchive::save(m_currentDocument.value(), filepath)) {
        dialogs::MessageDialog::error(
            m_mainWindow, tr("Save Error"),
            tr("Failed to save document: %1").arg(QDir::toNativeSeparators(filename)));
        logger.error("Failed to save document: {}", filepath.string());
        return false;
    }

    // Success - update state
    m_currentFilePath = filepath;
    m_setDirty(false);
    logger.info("Document saved: {}", filepath.string());
    m_statusBar->showMessage(newFile ? tr("Document saved as: %1").arg(filename)
                                     : tr("Document saved"), 2000);
    return true;
}

// =============================================================================
// Per-editor save state
// =============================================================================

bool DocumentCoordinator::isEditorDirty(const EditorPanel* editor) const {
    if (!editor) {
        return false;
    }
    switch (editorKind(editor)) {
    case EditorKind::ProjectChapter:
        // Per-open-tab flag (reliable; set only on real edits)
        return m_navigatorCoordinator
            && m_navigatorCoordinator->isChapterDirty(editor->property("elementId").toString());
    case EditorKind::StandaloneFile:
        return editor->property("dirty").toBool();
    case EditorKind::SingleDocument:
        return m_isDirty();
    }
    return false;
}

bool DocumentCoordinator::saveEditor(EditorPanel* editor) {
    if (!editor) {
        return true;
    }
    switch (editorKind(editor)) {
    case EditorKind::ProjectChapter:
        // Chapters are saved with the project; onSaveAll() clears a chapter's flag only
        // once its content is written
        onSaveAll();
        return !isEditorDirty(editor);
    case EditorKind::StandaloneFile:
        return writeStandaloneFile(editor, standaloneFileOf(editor).path());
    case EditorKind::SingleDocument:
        return saveSingleDocument(editor, false);
    }
    return false;
}

void DocumentCoordinator::discardEditorChanges(EditorPanel* editor) {
    if (!editor) {
        return;
    }
    switch (editorKind(editor)) {
    case EditorKind::ProjectChapter:
        if (m_navigatorCoordinator) {
            m_navigatorCoordinator->discardChapterChanges(editor->property("elementId").toString());
        }
        break;
    case EditorKind::StandaloneFile:
        editor->setProperty("dirty", false);
        break;
    case EditorKind::SingleDocument:
        m_setDirty(false);
        break;
    }
}

bool DocumentCoordinator::saveAllChanges() {
    if (core::ProjectManager::getInstance().isProjectOpen()) {
        // Every dirty chapter and the project structure, in one save
        onSaveAll();
    }

    // Then every other tab that still has unsaved changes
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (!editor || !isEditorDirty(editor)) {
            continue;
        }
        if (editorKind(editor) == EditorKind::ProjectChapter) {
            return false;  // onSaveAll() above could not write it, and said so
        }
        if (!saveEditor(editor)) {
            return false;
        }
    }
    return !m_hasUnsavedChanges();
}

QStringList DocumentCoordinator::unsavedDocumentNames() const {
    QStringList names;

    // Chapters and structure are saved together, as the book
    auto& pm = core::ProjectManager::getInstance();
    if (pm.isProjectOpen()) {
        bool bookChanged = pm.isDirty();
        if (m_navigatorCoordinator) {
            for (const bool dirty : m_navigatorCoordinator->dirtyChapters()) {
                bookChanged = bookChanged || dirty;
            }
        }
        const core::ProjectBook* book = pm.book();
        if (bookChanged && book) {
            names << book->title;
        }
    }

    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (editor && editorKind(editor) != EditorKind::ProjectChapter && isEditorDirty(editor)) {
            QString name = m_centralTabs->tabText(i);
            if (name.startsWith(QLatin1Char('*'))) {
                name.remove(0, 1);
            }
            names << name;
        }
    }

    names.removeDuplicates();
    return names;
}

void DocumentCoordinator::onSaveAll() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save All");

    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        logger.debug("No project open - Save All does nothing");
        m_statusBar->showMessage(tr("No project open"), 2000);
        return;
    }

    // First, give ProjectManager the text of all dirty chapters from open tabs
    const auto& dirtyChapters = m_navigatorCoordinator->dirtyChapters();
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        EditorPanel* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (!editor) continue;

        QString elemId = editor->property("elementId").toString();
        if (elemId.isEmpty()) continue;

        if (dirtyChapters.value(elemId, false)) {
            pm.setChapterContent(elemId, editor->getContent());
            logger.debug("Updated content cache for: {}", elemId.toStdString());
        }
    }

    // Save all dirty elements via ProjectManager
    bool success = pm.saveAllDirty();

    // Also save the .klh file to persist the changes of the book's data (title, author...)
    bool manifestSaved = pm.saveManifest();

    if (success && manifestSaved) {
        // Clear dirty flags and update tab titles
        for (int i = 0; i < m_centralTabs->count(); ++i) {
            EditorPanel* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
            if (!editor) continue;

            QString elemId = editor->property("elementId").toString();
            if (!elemId.isEmpty() && dirtyChapters.value(elemId, false)) {
                // Clear dirty flag via NavigatorCoordinator
                m_navigatorCoordinator->setChapterDirty(elemId, false);

                // Remove asterisk from tab title
                QString tabText = m_centralTabs->tabText(i);
                if (tabText.startsWith("*")) {
                    m_centralTabs->setTabText(i, tabText.mid(1));
                }
            }
        }

        // Clear all modified indicators in NavigatorPanel (OpenSpec #00042 Phase 7.5)
        m_navigatorPanel->clearAllModifiedIndicators();

        pm.setDirty(false);
        logger.info("All chapters and manifest saved successfully");
        m_statusBar->showMessage(tr("All changes saved"), 2000);
    } else {
        logger.error("Failed to save some chapters");
        m_statusBar->showMessage(tr("Error saving some chapters"), 3000);
        dialogs::MessageDialog::warning(
            m_mainWindow, tr("Save Warning"),
            tr("Some chapters could not be saved. Check the log for details."));
    }
}

void DocumentCoordinator::onCloseDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Close Document");

    if (!maybeSave()) {
        return;  // User cancelled
    }

    // Close current project if open
    auto& pm = core::ProjectManager::getInstance();
    if (pm.isProjectOpen()) {
        // Prepare services for close BEFORE database is destroyed
        prepareForProjectClose();
        pm.closeProject();
    }

    // Clear document state
    m_currentDocument = std::nullopt;
    m_currentFilePath = "";
    m_setDirty(false);

    // Clear UI
    m_navigatorPanel->clearDocument();
    relistStandaloneFiles();
    m_updateWindowTitle();

    logger.info("Document closed");
    m_statusBar->showMessage(tr("Document closed"), 2000);
    emit documentClosed();
}

// =============================================================================
// Standalone File Operations
// =============================================================================

void DocumentCoordinator::onOpenStandaloneFile() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Open Standalone File");

    // Show file dialog with supported file types
    QString filename = QFileDialog::getOpenFileName(
        m_mainWindow,
        tr("Open File"),
        QString(),
        tr("Chapters and Text Files (*.kchapter *.txt);;Chapters (*.kchapter);;"
           "Text Files (*.txt);;All Files (*)")
    );

    if (filename.isEmpty()) {
        logger.info("Open standalone file cancelled by user");
        return;
    }

    openStandaloneFile(filename);
}

void DocumentCoordinator::openStandaloneFile(const QString& path) {
    auto& logger = core::Logger::getInstance();
    logger.info("Opening standalone file: {}", path.toStdString());

    // A file open in a tab is shown there, not opened twice
    if (EditorPanel* openEditor = findStandaloneEditor(path)) {
        m_centralTabs->setCurrentWidget(openEditor);
        return;
    }

    // Check if file exists
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        dialogs::MessageDialog::warning(
            m_mainWindow, tr("File Not Found"),
            tr("The file '%1' does not exist.").arg(QDir::toNativeSeparators(path)));
        logger.error("Standalone file not found: {}", path.toStdString());
        return;
    }

    // Read the file in its own format
    const core::StandaloneFile::Type type = core::StandaloneFile::typeOf(path);
    if (type == core::StandaloneFile::Type::Unsupported) {
        dialogs::MessageDialog::information(
            m_mainWindow, tr("Unsupported File"),
            tr("Kalahari cannot open '%1'.\n\nIt opens chapters (*.kchapter) and text files (*.txt).")
                .arg(fileInfo.fileName()));
        logger.warn("Unsupported standalone file: {}", path.toStdString());
        return;
    }
    QString content;
    QString error;
    const auto file = core::StandaloneFile::open(path, content, &error);
    if (!file) {
        dialogs::MessageDialog::error(
            m_mainWindow, tr("Open Error"),
            tr("Failed to open file: %1\n\n%2").arg(
                QDir::toNativeSeparators(path),
                type == core::StandaloneFile::Type::Chapter
                    ? tr("It is not a Kalahari chapter, or it cannot be read.")
                    : error));
        logger.error("Failed to open standalone file: {}", path.toStdString());
        return;
    }

    // Create new editor tab with icon based on file type
    EditorPanel* newEditor = new EditorPanel(m_mainWindow);
    QString tabTitle = fileInfo.fileName();
    const QString iconId = standaloneIconId(path);
    QIcon tabIcon = core::ArtProvider::getInstance().getIcon(iconId);
    int tabIndex = m_centralTabs->addTab(newEditor, tabIcon, tabTitle);
    newEditor->setProperty("tabIconId", iconId);
    m_centralTabs->setCurrentIndex(tabIndex);

    // Store file for this tab
    newEditor->setProperty("standaloneFile", QVariant::fromValue(*file));
    newEditor->setProperty("standaloneFilePath", path);
    newEditor->setProperty("isStandaloneFile", true);
    // Per-tab content-dirty flag, seeded clean. Set true on genuine edits below and
    // consulted by MainWindow::hasUnsavedChanges() and the tab-close prompt.
    newEditor->setProperty("dirty", false);

    // Set content
    const bool complete = newEditor->setContent(content);

    // Add to standalone files list
    if (!m_standaloneFilePaths.contains(path)) {
        m_standaloneFilePaths.append(path);
    }

    // Add to Navigator "Other Files" section
    m_navigatorPanel->addStandaloneFile(path);

    // Show info bar for standalone files with context-aware message
    updateStandaloneInfoBar();

    // Connect contentChanged signal for dirty tracking.
    // Connected AFTER setContent() above so merely opening a file does not mark it
    // dirty (same rationale as the project-chapter path in NavigatorCoordinator).
    connect(newEditor, &EditorPanel::contentChanged,
            this, [this, newEditor]() {
                // Real per-tab dirty flag (source of truth for standalone tabs).
                newEditor->setProperty("dirty", true);

                // Mark tab title with asterisk
                int currentIdx = m_centralTabs->indexOf(newEditor);
                if (currentIdx >= 0) {
                    QString tabText = m_centralTabs->tabText(currentIdx);
                    if (!tabText.startsWith("*")) {
                        m_centralTabs->setTabText(currentIdx, "*" + tabText);
                    }
                }
            });

    logger.info("Standalone file opened: {}", path.toStdString());
    m_statusBar->showMessage(tr("Opened: %1").arg(tabTitle), 2000);

    if (!complete) {
        EditorPanel::warnDamagedChapter(m_mainWindow, tabTitle);
    }
}

EditorPanel* DocumentCoordinator::findStandaloneEditor(const QString& path) const {
    const QFileInfo fileInfo(path);
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (editor && editor->property("isStandaloneFile").toBool()
            && QFileInfo(editor->property("standaloneFilePath").toString()) == fileInfo) {
            return editor;
        }
    }
    return nullptr;
}

void DocumentCoordinator::setInfoBarAllowed(bool allowed) {
    m_infoBarAllowed = allowed;
    updateStandaloneInfoBar();
}

void DocumentCoordinator::updateStandaloneInfoBar() {
    EditorPanel* editor = getCurrentEditor();
    if (!m_infoBarAllowed || !editor || editorKind(editor) != EditorKind::StandaloneFile
        || editor->property("infoBarDismissed").toBool()) {
        m_standaloneInfoBar->hide();
        return;
    }

    m_standaloneInfoBar->setFilePath(editor->property("standaloneFilePath").toString());
    if (core::ProjectManager::getInstance().isProjectOpen()) {
        m_standaloneInfoBar->setMessage(tr("This file is not part of the current project."));
    } else {
        m_standaloneInfoBar->setMessage(tr("This file is not part of a project. Limited features available."));
    }
    m_standaloneInfoBar->show();
}

void DocumentCoordinator::relistStandaloneFiles() {
    for (const QString& path : std::as_const(m_standaloneFilePaths)) {
        m_navigatorPanel->addStandaloneFile(path);
    }
}

bool DocumentCoordinator::writeStandaloneFile(EditorPanel* editor, const QString& path) {
    auto& logger = core::Logger::getInstance();

    core::StandaloneFile file = standaloneFileOf(editor);
    const QString oldPath = file.path();
    QString error;
    if (!file.saveAs(path, editor->getContent(), editor->getText(), &error)) {
        dialogs::MessageDialog::error(
            m_mainWindow, tr("Save Error"),
            tr("Failed to save file: %1").arg(QDir::toNativeSeparators(path)), error);
        logger.error("Failed to save standalone file: {}", path.toStdString());
        return false;
    }
    editor->setProperty("standaloneFile", QVariant::fromValue(file));
    editor->setProperty("dirty", false);

    const int index = m_centralTabs->indexOf(editor);
    if (path != oldPath) {
        // The tab, the "Other Files" list and the info bar follow the file to its new name
        editor->setProperty("standaloneFilePath", path);
        const QString iconId = standaloneIconId(path);
        editor->setProperty("tabIconId", iconId);
        if (index >= 0) {
            m_centralTabs->setTabIcon(index, core::ArtProvider::getInstance().getIcon(iconId));
        }
        m_standaloneFilePaths.removeAll(oldPath);
        m_standaloneFilePaths.append(path);
        m_navigatorPanel->removeStandaloneFile(oldPath);
        m_navigatorPanel->addStandaloneFile(path);
        updateStandaloneInfoBar();
    }
    if (index >= 0) {
        m_centralTabs->setTabText(index, QFileInfo(path).fileName());  // without the "*"
    }

    logger.info("Standalone file saved: {}", path.toStdString());
    m_statusBar->showMessage(tr("Saved: %1").arg(QFileInfo(path).fileName()), 2000);
    return true;
}

bool DocumentCoordinator::saveStandaloneFileAs(EditorPanel* editor) {
    const QString oldPath = standaloneFileOf(editor).path();
    const QString chapters = tr("Chapters (*.kchapter)");
    const QString textFiles = tr("Text Files (*.txt)");
    QString selectedFilter =
        core::StandaloneFile::typeOf(oldPath) == core::StandaloneFile::Type::PlainText
            ? textFiles : chapters;
    QString path = QFileDialog::getSaveFileName(
        m_mainWindow,
        tr("Save File As"),
        oldPath,
        chapters + QStringLiteral(";;") + textFiles,
        &selectedFilter
    );
    if (path.isEmpty()) {
        return false;
    }

    // A name without one of the two extensions gets the one of the chosen type
    if (core::StandaloneFile::typeOf(path) == core::StandaloneFile::Type::Unsupported) {
        path += selectedFilter == textFiles ? QStringLiteral(".txt") : QStringLiteral(".kchapter");
    }

    // One file is edited in one tab
    EditorPanel* other = findStandaloneEditor(path);
    if (other && other != editor) {
        dialogs::MessageDialog::warning(
            m_mainWindow, tr("Save Error"),
            tr("'%1' is open in another tab.").arg(QFileInfo(path).fileName()));
        return false;
    }
    return writeStandaloneFile(editor, path);
}

void DocumentCoordinator::onAddToProject() {
    // The info bar shows the file of the current tab
    addToProject(m_standaloneInfoBar->filePath());
}

void DocumentCoordinator::addToProject(const QString& filePath) {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Add to Project");

    auto& pm = core::ProjectManager::getInstance();

    // Check if a project is open
    if (!pm.isProjectOpen()) {
        dialogs::MessageDialog::information(
            m_mainWindow, tr("No Project Open"),
            tr("Please open or create a book project first.\n\n"
               "Use File > New Book... or File > Open Book... to start."));
        logger.info("Add to Project: No project open");
        return;
    }

    if (filePath.isEmpty()) {
        logger.warn("Add to Project: No file path");
        return;
    }

    // The project gets the file with the changes made in its tab
    EditorPanel* fileEditor = findStandaloneEditor(filePath);
    if (fileEditor && isEditorDirty(fileEditor) && !saveEditor(fileEditor)) {
        return;
    }

    // Show AddToProjectDialog for copy/move options
    dialogs::AddToProjectDialog dialog(filePath, m_mainWindow);
    if (dialog.exec() == QDialog::Accepted) {
        auto result = dialog.result();

        // Add file to project using ProjectManager (it saves the project at once)
        QString elementId = pm.addFile(filePath, result.copyFile, result.kind, result.newTitle,
                                       result.place, result.groupId);

        if (!elementId.isEmpty()) {
            // Success - the navigator shows the new element, the file leaves the "Other
            // Files" list, and the chapter takes the place of its tab
            if (m_navigatorCoordinator) {
                m_navigatorCoordinator->refreshNavigator();
            }
            m_navigatorPanel->removeStandaloneFile(filePath);
            m_standaloneFilePaths.removeAll(filePath);
            if (fileEditor) {
                m_centralTabs->removeTab(m_centralTabs->indexOf(fileEditor));
                fileEditor->deleteLater();
                if (m_navigatorCoordinator) {
                    m_navigatorCoordinator->onElementSelected(elementId, result.newTitle);
                }
            }
            updateStandaloneInfoBar();

            m_statusBar->showMessage(
                tr("File added to project: %1").arg(result.newTitle), 3000);
            logger.info("Add to Project: Successfully added {} as {}",
                       filePath.toStdString(), elementId.toStdString());
        } else {
            dialogs::MessageDialog::warning(
                m_mainWindow, tr("Error"),
                tr("Failed to add file to project. Check logs for details."));
            logger.error("Add to Project: Failed to add file");
        }
    } else {
        logger.info("Add to Project: User cancelled");
    }
}

void DocumentCoordinator::removeStandaloneFile(const QString& path) {
    // Only the list entry goes: a tab with the file stays open
    m_navigatorPanel->removeStandaloneFile(path);
    m_standaloneFilePaths.removeAll(path);
}

// =============================================================================
// Archive Operations
// =============================================================================

void DocumentCoordinator::onExportArchive() {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        dialogs::MessageDialog::information(m_mainWindow, tr("No Project Open"),
                                            tr("Please open a project first before exporting."));
        return;
    }

    // Get project title for default filename
    QString defaultName = pm.book() ? pm.book()->title : QStringLiteral("project");

    QString outputPath = QFileDialog::getSaveFileName(
        m_mainWindow,
        tr("Export Project Archive"),
        QDir::homePath() + "/" + defaultName + ".klh.zip",
        tr("Kalahari Archive (*.klh.zip)")
    );

    if (outputPath.isEmpty()) return;

    // Check for incomplete elements (not final status)
    auto incompleteElements = pm.getIncompleteElements();
    if (!incompleteElements.empty()) {
        QString warningText = tr("The project contains %1 file(s) that are not marked as final:").arg(incompleteElements.size());
        warningText += QStringLiteral("\n\n");

        // Group by status
        std::map<QString, QStringList> byStatus;
        for (const auto& [id, status] : incompleteElements) {
            const core::ProjectElement* element = pm.findElement(id);
            QString title = element ? element->title : id;
            byStatus[status].append(title);
        }

        for (const auto& [status, titles] : byStatus) {
            warningText += QStringLiteral("[") + status.toUpper() + QStringLiteral("]: ") + titles.join(QStringLiteral(", ")) + QStringLiteral("\n");
        }

        warningText += QStringLiteral("\n") + tr("Do you want to export anyway?");

        if (!dialogs::MessageDialog::confirm(m_mainWindow, tr("Incomplete Files"), warningText,
                                             tr("&Export"),
                                             dialogs::MessageDialog::Kind::Warning,
                                             dialogs::MessageDialog::DefaultButton::Cancel)) {
            logger.info("Export cancelled due to incomplete files");
            return;
        }
    }

    // Create progress dialog
    QProgressDialog progress(tr("Exporting project archive..."), tr("Cancel"), 0, 100, m_mainWindow);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);

    QStringList problems;
    const bool success = pm.exportArchive(
        outputPath,
        [&progress](int percent) {
            progress.setValue(percent);
            QApplication::processEvents();
        },
        &problems);

    if (success) {
        dialogs::MessageDialog::information(
            m_mainWindow, tr("Export Complete"),
            tr("Project exported successfully to:\n%1").arg(QDir::toNativeSeparators(outputPath)));
        logger.info("Project exported to: {}", outputPath.toStdString());
    } else {
        dialogs::MessageDialog::error(m_mainWindow, tr("Export Failed"),
                                      tr("Failed to export project archive."),
                                      detailsOf(problems));
        logger.error("Failed to export project archive");
    }
}

void DocumentCoordinator::onImportArchive() {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    // Select archive to import
    QString archivePath = QFileDialog::getOpenFileName(
        m_mainWindow,
        tr("Import Project Archive"),
        QDir::homePath(),
        tr("Kalahari Archive (*.klh.zip)")
    );

    if (archivePath.isEmpty()) return;

    // Select target directory
    QString targetDir = QFileDialog::getExistingDirectory(
        m_mainWindow,
        tr("Select Destination Folder"),
        QDir::homePath()
    );

    if (targetDir.isEmpty()) return;

    // The book gets a folder of its own there
    const QString projectName = core::ProjectManager::archiveProjectName(archivePath);
    if (QFileInfo::exists(QDir(targetDir).filePath(projectName))) {
        if (dialogs::MessageDialog::confirm(
                m_mainWindow, tr("Folder Exists"),
                tr("A folder named '%1' already exists in the destination.\n"
                   "Do you want to choose a different location?").arg(projectName),
                tr("C&hoose Another Folder"))) {
            onImportArchive();  // Retry
        }
        return;
    }

    // The imported book takes the place of the open one
    if (!agreeToCloseBook(
            tr("Do you want to save changes to '%1' before importing the archive?"),
            tr("Do you want to close '%1' and open the imported book?"))) {
        logger.debug("User cancelled importing an archive");
        return;
    }

    // Create progress dialog
    QProgressDialog progress(tr("Importing project archive..."), tr("Cancel"), 0, 100, m_mainWindow);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);

    QStringList problems;
    const bool success = pm.importArchive(
        archivePath, targetDir,
        [&progress](int percent) {
            progress.setValue(percent);
            QApplication::processEvents();
        },
        &problems);

    if (success) {
        dialogs::MessageDialog::information(
            m_mainWindow, tr("Import Complete"),
            tr("The book was imported to:\n%1").arg(QDir::toNativeSeparators(pm.getProjectPath())));
        logger.info("Project imported from: {}", archivePath.toStdString());
    } else {
        dialogs::MessageDialog::error(m_mainWindow, tr("Import Failed"),
                                      tr("Failed to import project archive."),
                                      detailsOf(problems));
        logger.error("Failed to import project archive");
    }
}

// =============================================================================
// Project Lifecycle
// =============================================================================

void DocumentCoordinator::onProjectOpened(const QString& projectPath) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();

    if (!project) {
        logger.error("onProjectOpened: No project");
        return;
    }

    // =========================================================================
    // OpenSpec #00042 Task 7.6: Connect StyleResolver to database
    // =========================================================================
    core::ProjectDatabase* database = pm.getDatabase();
    if (database && database->isOpen()) {
        // Create or reset StyleResolver
        if (!m_styleResolver) {
            m_styleResolver = std::make_unique<editor::StyleResolver>(this);
            logger.debug("StyleResolver created for project");
        }

        // Connect to database and load styles
        m_styleResolver->setDatabase(database);
        m_styleResolver->reloadFromDatabase();

        // Connect PropertiesPanel to StyleResolver for style operations
        if (m_propertiesPanel) {
            m_propertiesPanel->setStyleResolver(m_styleResolver.get());
        }

        logger.info("StyleResolver connected to project database, loaded styles");

        // =========================================================================
        // OpenSpec #00042 Task 7.7: Connect StatisticsCollector to database
        // =========================================================================
        if (!m_statisticsCollector) {
            m_statisticsCollector = std::make_unique<editor::StatisticsCollector>(this);
            logger.debug("StatisticsCollector created for project");
        }

        // Connect to database
        m_statisticsCollector->setDatabase(database);

        // Start writing session
        m_statisticsCollector->startSession();
        logger.info("StatisticsCollector connected to project database, session started");

        // Connect NavigatorCoordinator to statistics collector for new editor panels
        if (m_navigatorCoordinator) {
            m_navigatorCoordinator->setStatisticsCollector(m_statisticsCollector.get());
            logger.debug("NavigatorCoordinator connected to StatisticsCollector");
        }
    } else {
        logger.warn("Project database not available for StyleResolver/StatisticsCollector");
    }

    // Update Navigator panel with the book's structure
    m_navigatorPanel->loadProject(*project, pm.bookTypes());

    // Restore Navigator expansion state
    QFileInfo pathInfo(projectPath);
    QString projectId = pathInfo.absoluteFilePath()
        .replace("/", "_")
        .replace("\\", "_")
        .replace(":", "_")
        .replace(" ", "_");
    m_navigatorPanel->restoreExpansionState(projectId);
    logger.debug("Restored expansion state for project: {}", projectId.toStdString());

    // Update window title with book title
    emit windowTitleChanged((pm.book() ? pm.book()->title : QString()) + " - Kalahari");

    // Log and status bar
    logger.info("Project opened: {}", projectPath.toStdString());
    m_statusBar->showMessage(tr("Book opened: %1").arg(projectPath), 3000);
    updateStandaloneInfoBar();  // its message names the open project
    emit documentOpened();
}

void DocumentCoordinator::onProjectClosed() {
    auto& logger = core::Logger::getInstance();

    // =========================================================================
    // OpenSpec #00042 Task 7.7: End statistics session and disconnect
    // NOTE: This may already be done by prepareForProjectClose() if opening
    // a new project. These calls are idempotent and safe to call twice.
    // =========================================================================
    if (m_statisticsCollector) {
        // Disconnect NavigatorCoordinator from statistics collector first
        if (m_navigatorCoordinator) {
            m_navigatorCoordinator->setStatisticsCollector(nullptr);
        }

        // End session (flushes stats to database) - no-op if already ended
        m_statisticsCollector->endSession();

        // Disconnect from database
        m_statisticsCollector->setDatabase(nullptr);
    }

    // =========================================================================
    // OpenSpec #00042 Task 7.6: Disconnect StyleResolver from database
    // NOTE: May already be done by prepareForProjectClose() - idempotent
    // =========================================================================
    if (m_styleResolver) {
        // Disconnect PropertiesPanel from StyleResolver first
        if (m_propertiesPanel) {
            m_propertiesPanel->setStyleResolver(nullptr);
        }

        m_styleResolver->setDatabase(nullptr);
        m_styleResolver->invalidateCache();
    }

    // The book's chapters close with it; their changes were saved or discarded before
    for (int i = m_centralTabs->count() - 1; i >= 0; --i) {
        QWidget* widget = m_centralTabs->widget(i);
        if (qobject_cast<EditorPanel*>(widget) && !widget->property("elementId").toString().isEmpty()) {
            m_centralTabs->removeTab(i);
            widget->deleteLater();
        }
    }

    // Clear Navigator panel
    m_navigatorPanel->clearAllModifiedIndicators();  // Clear modified indicators first (OpenSpec #00042 Phase 7.5)
    m_navigatorPanel->clearDocument();
    relistStandaloneFiles();  // they stay open without the book

    // Reset window title
    emit windowTitleChanged("Kalahari");

    // Clear chapter editing state via NavigatorCoordinator
    if (m_navigatorCoordinator) {
        m_navigatorCoordinator->clearDirtyChapters();
        m_navigatorCoordinator->clearCurrentElement();
    }

    // Log and status bar
    logger.info("Project closed");
    m_statusBar->showMessage(tr("Book closed"), 2000);
    updateStandaloneInfoBar();
    emit documentClosed();
}

void DocumentCoordinator::prepareForProjectClose() {
    auto& logger = core::Logger::getInstance();

    // =========================================================================
    // Save Navigator expansion state while the project path is still known.
    // closeProject() clears the path before it emits projectClosed, so this
    // cannot wait for onProjectClosed().
    // =========================================================================
    const QString projectPath = core::ProjectManager::getInstance().getProjectPath();
    if (!projectPath.isEmpty() && m_navigatorPanel) {
        QFileInfo pathInfo(projectPath);
        QString projectId = pathInfo.absoluteFilePath()
            .replace("/", "_")
            .replace("\\", "_")
            .replace(":", "_")
            .replace(" ", "_");
        m_navigatorPanel->saveExpansionState(projectId);
        // Persist to disk immediately
        core::SettingsManager::getInstance().save();
        logger.debug("Saved expansion state for project: {}", projectId.toStdString());
    }

    // =========================================================================
    // End statistics session BEFORE database is closed
    // This prevents crash from dangling pointer when ProjectManager destroys
    // the database and StatisticsCollector tries to flush
    // =========================================================================
    if (m_statisticsCollector) {
        // Disconnect NavigatorCoordinator from statistics collector first
        if (m_navigatorCoordinator) {
            m_navigatorCoordinator->setStatisticsCollector(nullptr);
            logger.debug("prepareForProjectClose: NavigatorCoordinator disconnected from StatisticsCollector");
        }

        // End session (flushes stats to database while it's still open)
        m_statisticsCollector->endSession();

        // Disconnect from database
        m_statisticsCollector->setDatabase(nullptr);
        logger.debug("prepareForProjectClose: StatisticsCollector session ended, disconnected from database");
    }

    // =========================================================================
    // Disconnect StyleResolver from database
    // =========================================================================
    if (m_styleResolver) {
        // Disconnect PropertiesPanel from StyleResolver first
        if (m_propertiesPanel) {
            m_propertiesPanel->setStyleResolver(nullptr);
        }

        m_styleResolver->setDatabase(nullptr);
        m_styleResolver->invalidateCache();
        logger.debug("prepareForProjectClose: StyleResolver disconnected from project database");
    }
}

} // namespace gui
} // namespace kalahari
