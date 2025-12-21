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
#include "kalahari/core/project_manager.h"
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
#include <QMessageBox>
#include <QProgressDialog>
#include <QApplication>
#include <map>

namespace kalahari {
namespace gui {

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
    , m_currentDocument(std::nullopt)
    , m_currentFilePath("")
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DocumentCoordinator created");
}

bool DocumentCoordinator::maybeSave() {
    if (!m_isDirty()) {
        return true;
    }

    QString filename = m_currentFilePath.empty()
        ? tr("Untitled")
        : QString::fromStdString(m_currentFilePath.filename().string());

    auto reply = QMessageBox::question(
        m_mainWindow,
        tr("Unsaved Changes"),
        tr("Do you want to save changes to %1?").arg(filename),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save
    );

    if (reply == QMessageBox::Save) {
        onSaveDocument();
        return !m_isDirty();  // Return true if save succeeded (dirty cleared)
    } else if (reply == QMessageBox::Cancel) {
        return false;
    }
    // Discard
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

    // Check for unsaved changes in current editor tab
    EditorPanel* currentEditor = getCurrentEditor();
    if (currentEditor && m_isDirty()) {
        auto reply = QMessageBox::question(
            m_mainWindow,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to the current document?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            onSaveDocument();
            if (m_isDirty()) return;  // Save was cancelled or failed
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
        // Discard -> continue
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

    // Create new document
    m_currentDocument = core::Document("Untitled", "User", "en");
    m_currentFilePath = "";
    newEditor->setText("");
    m_setDirty(false);

    // Update navigator panel
    m_navigatorPanel->loadDocument(m_currentDocument.value());

    logger.info("New document created in new tab");
    m_statusBar->showMessage(tr("New document created"), 2000);
    emit documentOpened();
}

void DocumentCoordinator::onNewProject() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: New Project (OpenSpec #00033)");

    // Show NewItemDialog in Project mode
    dialogs::NewItemDialog dialog(dialogs::NewItemMode::Project, m_mainWindow);
    if (dialog.exec() == QDialog::Accepted) {
        auto result = dialog.result();

        // Create project folder path
        QString projectPath = result.location;
        if (result.createSubfolder) {
            projectPath = QDir(result.location).filePath(result.title);
        }

        // Use ProjectManager to create project
        auto& pm = core::ProjectManager::getInstance();
        if (pm.createProject(projectPath, result.title, result.author, result.language, false)) {
            // Project created successfully - update UI
            m_updateWindowTitle();

            // Update navigator if document available
            if (pm.getDocument()) {
                m_navigatorPanel->loadDocument(*pm.getDocument());
            }

            logger.info("Project created: {} at {}", result.title.toStdString(), projectPath.toStdString());
            m_statusBar->showMessage(tr("Project created: %1").arg(result.title), 3000);
            emit documentOpened();
        } else {
            logger.error("Failed to create project: {}", result.title.toStdString());
            QMessageBox::critical(
                m_mainWindow,
                tr("Project Creation Failed"),
                tr("Could not create project '%1'.\n\nCheck that the location is writable and try again.")
                    .arg(result.title)
            );
        }
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
    if (!pm.openProject(filename)) {
        QMessageBox::critical(
            m_mainWindow,
            tr("Open Error"),
            tr("Failed to open book: %1").arg(filename)
        );
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
        QMessageBox::warning(
            m_mainWindow,
            tr("File Not Found"),
            tr("The file '%1' no longer exists.").arg(filePath)
        );

        // Remove from recent files
        core::RecentBooksManager::getInstance().removeRecentFile(filePath);
        return;
    }

    // Check for unsaved changes in current editor tab
    EditorPanel* currentEditor = getCurrentEditor();
    if (currentEditor && m_isDirty()) {
        auto reply = QMessageBox::question(
            m_mainWindow,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to the current document?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            onSaveDocument();
            if (m_isDirty()) return;
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    // Check if file is a .klh manifest - try ProjectManager first
    if (filePath.endsWith(".klh", Qt::CaseInsensitive)) {
        auto& pm = core::ProjectManager::getInstance();
        if (pm.openProject(filePath)) {
            // Successfully opened as project
            logger.info("Opened .klh file as project: {}", filePath.toStdString());
            return;
        }
        // If failed, fall through to try old archive format
        logger.debug("Failed to open .klh as project, trying old archive format");
    }

    // Load document (old archive format)
    std::filesystem::path filepath = filePath.toStdString();
    auto loaded = core::DocumentArchive::load(filepath);

    if (!loaded.has_value()) {
        QMessageBox::critical(
            m_mainWindow,
            tr("Open Error"),
            tr("Failed to open document: %1").arg(filePath)
        );
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

    // Update navigator panel
    m_navigatorPanel->loadDocument(m_currentDocument.value());

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

    // Check if we're in project mode - delegate to Save All for project saves
    auto& pm = core::ProjectManager::getInstance();
    if (pm.isProjectOpen()) {
        // In project mode, Ctrl+S saves the manifest (including metadata like notes)
        // and any dirty chapter content
        onSaveAll();
        return;
    }

    // Phase 0: Single file document mode
    EditorPanel* editor = getCurrentEditor();
    if (!editor) {
        logger.debug("No editor tab active - cannot save");
        m_statusBar->showMessage(tr("No document to save"), 2000);
        return;
    }

    // If no current file, delegate to Save As
    if (m_currentFilePath.empty()) {
        onSaveAsDocument();
        return;
    }

    // Ensure we have a document
    if (!m_currentDocument.has_value()) {
        m_currentDocument = core::Document("Untitled", "User", "en");
    }

    // Get text from current editor and update document
    QString text = editor->getText();
    setPhase0Content(m_currentDocument.value(), text);

    // Save to file
    bool saved = core::DocumentArchive::save(m_currentDocument.value(), m_currentFilePath);

    if (!saved) {
        QMessageBox::critical(
            m_mainWindow,
            tr("Save Error"),
            tr("Failed to save document: %1").arg(QString::fromStdString(m_currentFilePath.string()))
        );
        logger.error("Failed to save document: {}", m_currentFilePath.string());
        return;
    }

    m_setDirty(false);
    logger.info("Document saved: {}", m_currentFilePath.string());
    m_statusBar->showMessage(tr("Document saved"), 2000);
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

    // Show save file dialog
    QString filename = QFileDialog::getSaveFileName(
        m_mainWindow,
        tr("Save Document As"),
        QString(),
        tr("Kalahari Files (*.klh)")
    );

    if (filename.isEmpty()) {
        logger.info("Save As cancelled by user");
        return;
    }

    // Ensure .klh extension
    if (!filename.endsWith(".klh", Qt::CaseInsensitive)) {
        filename += ".klh";
    }

    // Ensure we have a document
    if (!m_currentDocument.has_value()) {
        m_currentDocument = core::Document("Untitled", "User", "en");
    }

    // Get text from current editor and update document
    QString text = editor->getText();
    setPhase0Content(m_currentDocument.value(), text);

    // Update document title from filename
    std::filesystem::path filepath = filename.toStdString();
    std::string title = filepath.stem().string();
    m_currentDocument->setTitle(title);

    // Save to file
    bool saved = core::DocumentArchive::save(m_currentDocument.value(), filepath);

    if (!saved) {
        QMessageBox::critical(
            m_mainWindow,
            tr("Save Error"),
            tr("Failed to save document: %1").arg(filename)
        );
        logger.error("Failed to save document: {}", filepath.string());
        return;
    }

    // Success - update state
    m_currentFilePath = filepath;
    m_setDirty(false);
    logger.info("Document saved as: {}", filepath.string());
    m_statusBar->showMessage(tr("Document saved as: %1").arg(filename), 2000);
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

    // First, update content cache for all dirty chapters from open tabs
    const auto& dirtyChapters = m_navigatorCoordinator->dirtyChapters();
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        EditorPanel* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (!editor) continue;

        QString elemId = editor->property("elementId").toString();
        if (elemId.isEmpty()) continue;

        if (dirtyChapters.value(elemId, false)) {
            core::BookElement* element = pm.findElement(elemId);
            if (element) {
                element->setContent(editor->getContent());
                logger.debug("Updated content cache for: {}", elemId.toStdString());
            }
        }
    }

    // Save all dirty elements via ProjectManager
    bool success = pm.saveAllDirty();

    // Also save the manifest to persist metadata changes (notes, status, etc.)
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
        QMessageBox::warning(m_mainWindow, tr("Save Warning"),
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
        pm.closeProject();
    }

    // Clear document state
    m_currentDocument = std::nullopt;
    m_currentFilePath = "";
    m_setDirty(false);

    // Clear UI
    m_navigatorPanel->clearDocument();
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
        tr("Kalahari Files (*.rtf *.kmap *.ktl);;Rich Text Format (*.rtf);;Mind Maps (*.kmap);;Timelines (*.ktl);;All Files (*.*)")
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

    // Check if file exists
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        QMessageBox::warning(
            m_mainWindow,
            tr("File Not Found"),
            tr("The file '%1' does not exist.").arg(path)
        );
        logger.error("Standalone file not found: {}", path.toStdString());
        return;
    }

    // Read file content
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(
            m_mainWindow,
            tr("Open Error"),
            tr("Failed to open file: %1\n\n%2").arg(path, file.errorString())
        );
        logger.error("Failed to open standalone file: {} ({})",
                     path.toStdString(), file.errorString().toStdString());
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Create new editor tab with icon based on file extension
    EditorPanel* newEditor = new EditorPanel(m_mainWindow);
    QString tabTitle = fileInfo.fileName();
    QString suffix = fileInfo.suffix().toLower();
    QString iconId;
    if (suffix == "rtf") {
        iconId = "template.chapter";
    } else if (suffix == "kmap") {
        iconId = "book.newMindMap";
    } else if (suffix == "ktl") {
        iconId = "book.newTimeline";
    } else {
        iconId = "common.file";
    }
    QIcon tabIcon = core::ArtProvider::getInstance().getIcon(iconId);
    int tabIndex = m_centralTabs->addTab(newEditor, tabIcon, tabTitle);
    m_centralTabs->setCurrentIndex(tabIndex);

    // Store file path for this tab
    newEditor->setProperty("standaloneFilePath", path);
    newEditor->setProperty("isStandaloneFile", true);

    // Set content
    newEditor->setContent(content);

    // Add to standalone files list
    if (!m_standaloneFilePaths.contains(path)) {
        m_standaloneFilePaths.append(path);
    }

    // Add to Navigator "Other Files" section
    m_navigatorPanel->addStandaloneFile(path);

    // Show info bar for standalone files with context-aware message
    m_standaloneInfoBar->setFilePath(path);
    if (core::ProjectManager::getInstance().isProjectOpen()) {
        m_standaloneInfoBar->setMessage(tr("This file is not part of the current project."));
    } else {
        m_standaloneInfoBar->setMessage(tr("This file is not part of a project. Limited features available."));
    }
    m_standaloneInfoBar->show();

    // Connect contentChanged signal for dirty tracking
    connect(newEditor, &EditorPanel::contentChanged,
            this, [this, path, newEditor]() {
                // Mark tab as dirty
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
}

void DocumentCoordinator::onAddToProject() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Add to Project");

    auto& pm = core::ProjectManager::getInstance();

    // Check if a project is open
    if (!pm.isProjectOpen()) {
        QMessageBox::information(
            m_mainWindow,
            tr("No Project Open"),
            tr("Please open or create a book project first.\n\n"
               "Use File > New Book... or File > Open Book... to start.")
        );
        logger.info("Add to Project: No project open");
        return;
    }

    // Get current standalone file path from info bar
    QString filePath = m_standaloneInfoBar->filePath();
    if (filePath.isEmpty()) {
        logger.warn("Add to Project: No file path in info bar");
        return;
    }

    // Show AddToProjectDialog for copy/move options
    dialogs::AddToProjectDialog dialog(filePath, m_mainWindow);
    if (dialog.exec() == QDialog::Accepted) {
        auto result = dialog.result();

        // Add file to project using ProjectManager
        QString elementId = pm.addChapterToSection(
            result.targetSection,
            result.targetPart,
            result.newTitle,
            filePath,
            result.copyFile
        );

        if (!elementId.isEmpty()) {
            // Success - hide info bar and remove from standalone files
            m_standaloneInfoBar->hide();
            m_navigatorPanel->removeStandaloneFile(filePath);

            // Remove standalone file tab if exists
            for (int i = 0; i < m_centralTabs->count(); ++i) {
                EditorPanel* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
                if (editor && editor->property("standaloneFilePath").toString() == filePath) {
                    m_centralTabs->removeTab(i);
                    editor->deleteLater();
                    break;
                }
            }

            // Remove from standalone files list
            m_standaloneFilePaths.removeAll(filePath);

            m_statusBar->showMessage(
                tr("File added to project: %1").arg(result.newTitle), 3000);
            logger.info("Add to Project: Successfully added {} as {}",
                       filePath.toStdString(), elementId.toStdString());
        } else {
            QMessageBox::warning(m_mainWindow, tr("Error"),
                tr("Failed to add file to project. Check logs for details."));
            logger.error("Add to Project: Failed to add file");
        }
    } else {
        logger.info("Add to Project: User cancelled");
    }
}

// =============================================================================
// Archive Operations
// =============================================================================

void DocumentCoordinator::onExportArchive() {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        QMessageBox::information(m_mainWindow, tr("No Project Open"),
            tr("Please open a project first before exporting."));
        return;
    }

    // Get project title for default filename
    QString defaultName = pm.getDocument() ?
        QString::fromStdString(pm.getDocument()->getTitle()) : "project";

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
            auto* element = pm.findElement(id);
            QString title = element ? QString::fromStdString(element->getTitle()) : id;
            byStatus[status].append(title);
        }

        for (const auto& [status, titles] : byStatus) {
            warningText += QStringLiteral("[") + status.toUpper() + QStringLiteral("]: ") + titles.join(QStringLiteral(", ")) + QStringLiteral("\n");
        }

        warningText += QStringLiteral("\n") + tr("Do you want to export anyway?");

        auto reply = QMessageBox::warning(m_mainWindow, tr("Incomplete Files"), warningText,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (reply != QMessageBox::Yes) {
            logger.info("Export cancelled due to incomplete files");
            return;
        }
    }

    // Create progress dialog
    QProgressDialog progress(tr("Exporting project archive..."), tr("Cancel"), 0, 100, m_mainWindow);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);

    bool success = pm.exportArchive(outputPath, [&progress](int percent) {
        progress.setValue(percent);
        QApplication::processEvents();
    });

    if (success) {
        QMessageBox::information(m_mainWindow, tr("Export Complete"),
            tr("Project exported successfully to:\n%1").arg(outputPath));
        logger.info("Project exported to: {}", outputPath.toStdString());
    } else {
        QMessageBox::warning(m_mainWindow, tr("Export Failed"),
            tr("Failed to export project archive."));
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

    // Check if project folder would already exist
    QFileInfo archiveInfo(archivePath);
    QString projectName = archiveInfo.completeBaseName();
    if (projectName.endsWith(".klh", Qt::CaseInsensitive)) {
        projectName.chop(4);
    }
    QString extractDir = targetDir + "/" + projectName;

    if (QDir(extractDir).exists()) {
        auto reply = QMessageBox::question(m_mainWindow, tr("Folder Exists"),
            tr("A folder named '%1' already exists in the destination.\n"
               "Do you want to choose a different location?").arg(projectName),
            QMessageBox::Yes | QMessageBox::Cancel);
        if (reply == QMessageBox::Yes) {
            onImportArchive();  // Retry
        }
        return;
    }

    // Create progress dialog
    QProgressDialog progress(tr("Importing project archive..."), tr("Cancel"), 0, 100, m_mainWindow);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);

    bool success = pm.importArchive(archivePath, targetDir, [&progress](int percent) {
        progress.setValue(percent);
        QApplication::processEvents();
    });

    if (success) {
        QMessageBox::information(m_mainWindow, tr("Import Complete"),
            tr("Project imported and opened successfully."));
        logger.info("Project imported from: {}", archivePath.toStdString());
    } else {
        QMessageBox::warning(m_mainWindow, tr("Import Failed"),
            tr("Failed to import project archive."));
        logger.error("Failed to import project archive");
    }
}

// =============================================================================
// Project Lifecycle
// =============================================================================

void DocumentCoordinator::onProjectOpened(const QString& projectPath) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();
    const core::Document* doc = pm.getDocument();

    if (!doc) {
        logger.error("onProjectOpened: Document is null");
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

    // Update Navigator panel with document structure
    m_navigatorPanel->loadDocument(*doc);

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
    emit windowTitleChanged(QString::fromStdString(doc->getTitle()) + " - Kalahari");

    // Log and status bar
    logger.info("Project opened: {}", projectPath.toStdString());
    m_statusBar->showMessage(tr("Book opened: %1").arg(projectPath), 3000);
    emit documentOpened();
}

void DocumentCoordinator::onProjectClosed() {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    // =========================================================================
    // OpenSpec #00042 Task 7.7: End statistics session and disconnect
    // =========================================================================
    if (m_statisticsCollector) {
        // Disconnect NavigatorCoordinator from statistics collector first
        if (m_navigatorCoordinator) {
            m_navigatorCoordinator->setStatisticsCollector(nullptr);
            logger.debug("NavigatorCoordinator disconnected from StatisticsCollector");
        }

        // End session (flushes stats to database)
        m_statisticsCollector->endSession();

        // Disconnect from database
        m_statisticsCollector->setDatabase(nullptr);
        logger.debug("StatisticsCollector session ended, disconnected from database");
    }

    // =========================================================================
    // OpenSpec #00042 Task 7.6: Disconnect StyleResolver from database
    // =========================================================================
    if (m_styleResolver) {
        // Disconnect PropertiesPanel from StyleResolver first
        if (m_propertiesPanel) {
            m_propertiesPanel->setStyleResolver(nullptr);
        }

        m_styleResolver->setDatabase(nullptr);
        m_styleResolver->invalidateCache();
        logger.debug("StyleResolver disconnected from project database");
    }

    // Save Navigator expansion state before clearing
    QString projectPath = pm.getProjectPath();
    if (!projectPath.isEmpty()) {
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

    // Clear Navigator panel
    m_navigatorPanel->clearAllModifiedIndicators();  // Clear modified indicators first (OpenSpec #00042 Phase 7.5)
    m_navigatorPanel->clearDocument();

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
    emit documentClosed();
}

} // namespace gui
} // namespace kalahari
