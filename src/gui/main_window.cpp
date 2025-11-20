/// @file main_window.cpp
/// @brief Main application window implementation

#include "kalahari/gui/main_window.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/search_panel.h"
#include "kalahari/gui/panels/assistant_panel.h"
#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/document.h"
#include "kalahari/core/document_archive.h"
#include "kalahari/core/book.h"
#include "kalahari/core/book_element.h"
#include "kalahari/core/part.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QApplication>
#include <QSettings>
#include <QDockWidget>
#include <QCloseEvent>
#include <QShowEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QPlainTextEdit>

namespace kalahari {
namespace gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_newAction(nullptr)
    , m_openAction(nullptr)
    , m_saveAction(nullptr)
    , m_saveAsAction(nullptr)
    , m_exitAction(nullptr)
    , m_undoAction(nullptr)
    , m_redoAction(nullptr)
    , m_cutAction(nullptr)
    , m_copyAction(nullptr)
    , m_pasteAction(nullptr)
    , m_settingsAction(nullptr)
    , m_fileMenu(nullptr)
    , m_editMenu(nullptr)
    , m_viewMenu(nullptr)
    , m_fileToolbar(nullptr)
    , m_viewNavigatorAction(nullptr)
    , m_viewPropertiesAction(nullptr)
    , m_viewLogAction(nullptr)
    , m_viewSearchAction(nullptr)
    , m_viewAssistantAction(nullptr)
    , m_resetLayoutAction(nullptr)
    , m_navigatorDock(nullptr)
    , m_propertiesDock(nullptr)
    , m_logDock(nullptr)
    , m_searchDock(nullptr)
    , m_assistantDock(nullptr)
    , m_editorPanel(nullptr)
    , m_navigatorPanel(nullptr)
    , m_propertiesPanel(nullptr)
    , m_searchPanel(nullptr)
    , m_assistantPanel(nullptr)
    , m_logPanel(nullptr)
    , m_firstShow(true)
    , m_currentDocument(std::nullopt)
    , m_currentFilePath("")
    , m_isDirty(false)
{
    auto& logger = core::Logger::getInstance();
    logger.info("MainWindow constructor called");

    // Set window properties
    setWindowTitle("Kalahari Writer's IDE");
    resize(1280, 720);

    // Create UI components
    createActions();
    createMenus();
    createToolbars();
    createStatusBar();
    createDocks();

    // Connect editor textChanged signal to dirty state tracking (Task #00008)
    connect(m_editorPanel->getTextEdit(), &QPlainTextEdit::textChanged,
            this, [this]() {
                if (!m_currentDocument.has_value()) return;
                setDirty(true);
            });

    logger.info("MainWindow initialized successfully");
}

void MainWindow::createActions() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating actions");

    // File actions
    m_newAction = new QAction(tr("&New"), this);
    m_newAction->setShortcut(QKeySequence::New);
    m_newAction->setStatusTip(tr("Create a new document"));
    connect(m_newAction, &QAction::triggered, this, &MainWindow::onNewDocument);

    m_openAction = new QAction(tr("&Open..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setStatusTip(tr("Open an existing document"));
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpenDocument);

    m_saveAction = new QAction(tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setStatusTip(tr("Save the current document"));
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveDocument);

    m_saveAsAction = new QAction(tr("Save &As..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setStatusTip(tr("Save the current document with a new name"));
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAsDocument);

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setStatusTip(tr("Exit the application"));
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::onExit);

    // Edit actions
    m_undoAction = new QAction(tr("&Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setStatusTip(tr("Undo the last operation"));
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndo);

    m_redoAction = new QAction(tr("&Redo"), this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setStatusTip(tr("Redo the last undone operation"));
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    m_cutAction = new QAction(tr("Cu&t"), this);
    m_cutAction->setShortcut(QKeySequence::Cut);
    m_cutAction->setStatusTip(tr("Cut the selection to clipboard"));
    connect(m_cutAction, &QAction::triggered, this, &MainWindow::onCut);

    m_copyAction = new QAction(tr("&Copy"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_copyAction->setStatusTip(tr("Copy the selection to clipboard"));
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::onCopy);

    m_pasteAction = new QAction(tr("&Paste"), this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_pasteAction->setStatusTip(tr("Paste from clipboard"));
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::onPaste);

    m_selectAllAction = new QAction(tr("Select &All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_selectAllAction->setStatusTip(tr("Select all text"));
    connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::onSelectAll);

    m_settingsAction = new QAction(tr("&Settings..."), this);
    m_settingsAction->setShortcut(QKeySequence(tr("Ctrl+,")));
    m_settingsAction->setStatusTip(tr("Open settings dialog"));
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onSettings);

    // Help actions
    m_aboutAction = new QAction(tr("&About Kalahari"), this);
    m_aboutAction->setStatusTip(tr("Show information about Kalahari"));
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    m_aboutQtAction = new QAction(tr("About &Qt"), this);
    m_aboutQtAction->setStatusTip(tr("Show information about Qt"));
    connect(m_aboutQtAction, &QAction::triggered, this, &MainWindow::onAboutQt);

    logger.debug("Actions created successfully");
}

void MainWindow::createMenus() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating menus");

    // File menu
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileMenu->addAction(m_newAction);
    m_fileMenu->addAction(m_openAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_saveAction);
    m_fileMenu->addAction(m_saveAsAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAction);

    // Edit menu
    m_editMenu = menuBar()->addMenu(tr("&Edit"));
    m_editMenu->addAction(m_undoAction);
    m_editMenu->addAction(m_redoAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_cutAction);
    m_editMenu->addAction(m_copyAction);
    m_editMenu->addAction(m_pasteAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_selectAllAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_settingsAction);

    // Help menu
    m_helpMenu = menuBar()->addMenu(tr("&Help"));
    m_helpMenu->addAction(m_aboutAction);
    m_helpMenu->addAction(m_aboutQtAction);

    logger.debug("Menus created successfully");
}

void MainWindow::createToolbars() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating toolbars");

    // File toolbar
    m_fileToolbar = addToolBar(tr("File"));
    m_fileToolbar->setMovable(true);      // Can be moved between dock areas
    m_fileToolbar->setFloatable(true);    // Can be detached as floating window
    m_fileToolbar->addAction(m_newAction);
    m_fileToolbar->addAction(m_openAction);
    m_fileToolbar->addAction(m_saveAction);

    logger.debug("Toolbars created successfully (movable and floatable)");
}

void MainWindow::createStatusBar() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating status bar");

    statusBar()->showMessage(tr("Ready"), 3000);  // Show for 3 seconds

    logger.debug("Status bar created successfully");
}

// Slots implementation
void MainWindow::onNewDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: New Document");

    // Check for unsaved changes
    if (m_isDirty) {
        auto reply = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to the current document?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            onSaveDocument();
            if (m_isDirty) return;  // Save was cancelled or failed
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
        // Discard → continue
    }

    // Create new document
    m_currentDocument = core::Document("Untitled", "User", "en");
    m_currentFilePath = "";
    m_editorPanel->setText("");
    setDirty(false);

    // Update navigator panel
    m_navigatorPanel->loadDocument(m_currentDocument.value());

    logger.info("New document created");
    statusBar()->showMessage(tr("New document created"), 2000);
}

void MainWindow::onOpenDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Open Document");

    // Check for unsaved changes
    if (m_isDirty) {
        auto reply = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to the current document?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            onSaveDocument();
            if (m_isDirty) return;
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    // Show file dialog
    QString filename = QFileDialog::getOpenFileName(
        this,
        tr("Open Document"),
        QString(),
        tr("Kalahari Files (*.klh)")
    );

    if (filename.isEmpty()) {
        logger.info("Open cancelled by user");
        return;
    }

    // Load document
    std::filesystem::path filepath = filename.toStdString();
    auto loaded = core::DocumentArchive::load(filepath);

    if (!loaded.has_value()) {
        QMessageBox::critical(
            this,
            tr("Open Error"),
            tr("Failed to open document: %1").arg(filename)
        );
        logger.error("Failed to load document: {}", filepath.string());
        return;
    }

    // Success - update state
    m_currentDocument = std::move(loaded.value());
    m_currentFilePath = filepath;

    // Extract text and load into editor
    QString content = getPhase0Content(m_currentDocument.value());
    m_editorPanel->setText(content);

    setDirty(false);

    // Update navigator panel
    m_navigatorPanel->loadDocument(m_currentDocument.value());

    logger.info("Document loaded: {}", filepath.string());
    statusBar()->showMessage(tr("Document opened: %1").arg(filename), 2000);
}

void MainWindow::onSaveDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save Document");

    // If no current file, delegate to Save As
    if (m_currentFilePath.empty()) {
        onSaveAsDocument();
        return;
    }

    // Ensure we have a document
    if (!m_currentDocument.has_value()) {
        m_currentDocument = core::Document("Untitled", "User", "en");
    }

    // Get text from editor and update document
    QString text = m_editorPanel->getText();
    setPhase0Content(m_currentDocument.value(), text);

    // Save to file
    bool saved = core::DocumentArchive::save(m_currentDocument.value(), m_currentFilePath);

    if (!saved) {
        QMessageBox::critical(
            this,
            tr("Save Error"),
            tr("Failed to save document: %1").arg(QString::fromStdString(m_currentFilePath.string()))
        );
        logger.error("Failed to save document: {}", m_currentFilePath.string());
        return;
    }

    setDirty(false);
    logger.info("Document saved: {}", m_currentFilePath.string());
    statusBar()->showMessage(tr("Document saved"), 2000);
}

void MainWindow::onSaveAsDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save As Document");

    // Show save file dialog
    QString filename = QFileDialog::getSaveFileName(
        this,
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

    // Get text from editor and update document
    QString text = m_editorPanel->getText();
    setPhase0Content(m_currentDocument.value(), text);

    // Update document title from filename
    std::filesystem::path filepath = filename.toStdString();
    std::string title = filepath.stem().string();
    m_currentDocument->setTitle(title);

    // Save to file
    bool saved = core::DocumentArchive::save(m_currentDocument.value(), filepath);

    if (!saved) {
        QMessageBox::critical(
            this,
            tr("Save Error"),
            tr("Failed to save document: %1").arg(filename)
        );
        logger.error("Failed to save document: {}", filepath.string());
        return;
    }

    // Success - update state
    m_currentFilePath = filepath;
    setDirty(false);
    logger.info("Document saved as: {}", filepath.string());
    statusBar()->showMessage(tr("Document saved as: %1").arg(filename), 2000);
}

void MainWindow::onExit() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Exit");
    QApplication::quit();
}

void MainWindow::onUndo() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Undo");

    m_editorPanel->getTextEdit()->undo();

    statusBar()->showMessage(tr("Undo performed"), 2000);
}

void MainWindow::onRedo() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Redo");

    m_editorPanel->getTextEdit()->redo();

    statusBar()->showMessage(tr("Redo performed"), 2000);
}

void MainWindow::onCut() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Cut");

    m_editorPanel->getTextEdit()->cut();

    statusBar()->showMessage(tr("Cut to clipboard"), 2000);
}

void MainWindow::onCopy() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Copy");

    m_editorPanel->getTextEdit()->copy();

    statusBar()->showMessage(tr("Copied to clipboard"), 2000);
}

void MainWindow::onPaste() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Paste");

    m_editorPanel->getTextEdit()->paste();

    statusBar()->showMessage(tr("Pasted from clipboard"), 2000);
}

void MainWindow::onSelectAll() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Select All");

    m_editorPanel->getTextEdit()->selectAll();

    statusBar()->showMessage(tr("All text selected"), 2000);
}

void MainWindow::onSettings() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Settings");

    SettingsDialog dialog(this);
    int result = dialog.exec();

    if (result == QDialog::Accepted) {
        logger.info("Settings dialog: OK clicked (settings saved)");
        statusBar()->showMessage(tr("Settings saved"), 2000);
    } else {
        logger.info("Settings dialog: Cancel clicked (changes discarded)");
        statusBar()->showMessage(tr("Settings changes discarded"), 2000);
    }
}

void MainWindow::onAbout() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: About Kalahari");

    QString aboutText = tr(
        "<h2>Kalahari Writer's IDE</h2>"
        "<p><b>Version:</b> 0.3.0-alpha (Qt6 Migration)</p>"
        "<p><b>License:</b> MIT License</p>"
        "<p><b>Copyright:</b> © 2025 Kalahari Project</p>"
        "<br>"
        "<p>Advanced writing environment for book authors.</p>"
        "<p>Built with Qt %1</p>"
        "<br>"
        "<p>For more information, visit: "
        "<a href='https://github.com/yourusername/kalahari'>GitHub</a></p>"
    ).arg(qVersion());

    QMessageBox::about(this, tr("About Kalahari"), aboutText);

    logger.info("About dialog displayed");
}

void MainWindow::onAboutQt() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: About Qt");

    QMessageBox::aboutQt(this, tr("About Qt"));

    logger.info("About Qt dialog displayed");
}

// Dock management
void MainWindow::createDocks() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating dock widgets");

    // Create central editor panel
    m_editorPanel = new EditorPanel(this);
    setCentralWidget(m_editorPanel);

    // Navigator dock (left)
    m_navigatorPanel = new NavigatorPanel(this);
    m_navigatorDock = new QDockWidget(tr("Navigator"), this);
    m_navigatorDock->setWidget(m_navigatorPanel);
    m_navigatorDock->setObjectName("NavigatorDock");  // Required for saveState!
    addDockWidget(Qt::LeftDockWidgetArea, m_navigatorDock);

    // Properties dock (right)
    m_propertiesPanel = new PropertiesPanel(this);
    m_propertiesDock = new QDockWidget(tr("Properties"), this);
    m_propertiesDock->setWidget(m_propertiesPanel);
    m_propertiesDock->setObjectName("PropertiesDock");
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // Log dock (bottom)
    m_logPanel = new LogPanel(this);
    m_logDock = new QDockWidget(tr("Log"), this);
    m_logDock->setWidget(m_logPanel);
    m_logDock->setObjectName("LogDock");
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    // Search dock (right, tabbed with Properties)
    m_searchPanel = new SearchPanel(this);
    m_searchDock = new QDockWidget(tr("Search"), this);
    m_searchDock->setWidget(m_searchPanel);
    m_searchDock->setObjectName("SearchDock");
    addDockWidget(Qt::RightDockWidgetArea, m_searchDock);
    tabifyDockWidget(m_propertiesDock, m_searchDock);

    // Assistant dock (right, tabbed with Properties/Search)
    m_assistantPanel = new AssistantPanel(this);
    m_assistantDock = new QDockWidget(tr("Assistant"), this);
    m_assistantDock->setWidget(m_assistantPanel);
    m_assistantDock->setObjectName("AssistantDock");
    addDockWidget(Qt::RightDockWidgetArea, m_assistantDock);
    tabifyDockWidget(m_searchDock, m_assistantDock);

    // Raise Properties tab (default visible)
    m_propertiesDock->raise();

    // Create View menu
    m_viewMenu = menuBar()->addMenu(tr("&View"));

    // Create toggle actions (use QDockWidget's built-in toggleViewAction!)
    m_viewNavigatorAction = m_navigatorDock->toggleViewAction();
    m_viewNavigatorAction->setShortcut(QKeySequence(tr("Ctrl+1")));
    m_viewMenu->addAction(m_viewNavigatorAction);

    m_viewPropertiesAction = m_propertiesDock->toggleViewAction();
    m_viewPropertiesAction->setShortcut(QKeySequence(tr("Ctrl+2")));
    m_viewMenu->addAction(m_viewPropertiesAction);

    m_viewLogAction = m_logDock->toggleViewAction();
    m_viewLogAction->setShortcut(QKeySequence(tr("Ctrl+3")));
    m_viewMenu->addAction(m_viewLogAction);

    m_viewSearchAction = m_searchDock->toggleViewAction();
    m_viewSearchAction->setShortcut(QKeySequence(tr("Ctrl+4")));
    m_viewMenu->addAction(m_viewSearchAction);

    m_viewAssistantAction = m_assistantDock->toggleViewAction();
    m_viewAssistantAction->setShortcut(QKeySequence(tr("Ctrl+5")));
    m_viewMenu->addAction(m_viewAssistantAction);

    m_viewMenu->addSeparator();

    // Reset layout action
    m_resetLayoutAction = new QAction(tr("Reset Layout"), this);
    m_resetLayoutAction->setShortcut(QKeySequence(tr("Ctrl+0")));
    m_resetLayoutAction->setStatusTip(tr("Reset dock layout to default"));
    connect(m_resetLayoutAction, &QAction::triggered, this, &MainWindow::resetLayout);
    m_viewMenu->addAction(m_resetLayoutAction);

    logger.debug("Dock widgets created successfully");
}

void MainWindow::resetLayout() {
    auto& logger = core::Logger::getInstance();
    logger.info("Resetting dock layout to default");

    // Remove all docks
    removeDockWidget(m_navigatorDock);
    removeDockWidget(m_propertiesDock);
    removeDockWidget(m_logDock);
    removeDockWidget(m_searchDock);
    removeDockWidget(m_assistantDock);

    // Re-add in default layout
    addDockWidget(Qt::LeftDockWidgetArea, m_navigatorDock);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    addDockWidget(Qt::RightDockWidgetArea, m_searchDock);
    addDockWidget(Qt::RightDockWidgetArea, m_assistantDock);

    // Tab right-side docks
    tabifyDockWidget(m_propertiesDock, m_searchDock);
    tabifyDockWidget(m_searchDock, m_assistantDock);

    // Raise Properties tab
    m_propertiesDock->raise();

    // Show all docks
    m_navigatorDock->show();
    m_propertiesDock->show();
    m_logDock->show();
    m_searchDock->show();
    m_assistantDock->show();

    statusBar()->showMessage(tr("Layout reset to default"), 2000);
}

// Perspective save/restore
void MainWindow::closeEvent(QCloseEvent* event) {
    auto& logger = core::Logger::getInstance();
    logger.debug("MainWindow::closeEvent triggered");

    // Check for unsaved changes (Task #00008)
    if (m_isDirty) {
        QString filename = m_currentFilePath.empty()
            ? "Untitled"
            : QString::fromStdString(m_currentFilePath.filename().string());

        auto reply = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to %1?").arg(filename),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            onSaveDocument();
            if (m_isDirty) {
                // Save was cancelled or failed
                event->ignore();
                return;
            }
        } else if (reply == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        // Discard → continue with close
    }

    // Save perspective (existing code)
    logger.debug("Saving window perspective");

    QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    logger.debug("Window perspective saved");

    event->accept();
}

// Helper methods (Task #00008)
void MainWindow::setDirty(bool dirty) {
    m_isDirty = dirty;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    QString title = "Kalahari - ";

    if (!m_currentFilePath.empty()) {
        QString filename = QString::fromStdString(m_currentFilePath.filename().string());
        title += filename;
    } else {
        title += "Untitled";
    }

    if (m_isDirty) {
        title = "Kalahari - *" + title.mid(11);  // Insert "*" after "Kalahari - "
    }

    setWindowTitle(title);
}

QString MainWindow::getPhase0Content(const core::Document& doc) const {
    const auto& book = doc.getBook();
    const auto& body = book.getBody();

    if (body.empty()) return "";

    const auto& firstPart = body[0];
    const auto& chapters = firstPart->getChapters();

    if (chapters.empty()) return "";

    const auto& firstChapter = chapters[0];
    auto content = firstChapter->getMetadata("_phase0_content");

    return content.has_value() ? QString::fromStdString(content.value()) : "";
}

void MainWindow::setPhase0Content(core::Document& doc, const QString& text) {
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
        firstPart->addChapter(chapter);  // Use addChapter() instead of push_back()
    }

    // Store text in metadata (chapters vector is const, but elements are mutable)
    const auto& chaptersList = firstPart->getChapters();
    chaptersList[0]->setMetadata("_phase0_content", text.toStdString());
    chaptersList[0]->touch();  // Update modified timestamp
    doc.touch();
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);

    if (m_firstShow) {
        auto& logger = core::Logger::getInstance();
        logger.debug("Restoring window perspective");

        QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
        restoreGeometry(settings.value("geometry").toByteArray());
        restoreState(settings.value("windowState").toByteArray());

        logger.debug("Window perspective restored");

        m_firstShow = false;
    }
}

} // namespace gui
} // namespace kalahari
