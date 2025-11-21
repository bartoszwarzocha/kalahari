/// @file main_window.cpp
/// @brief Main application window implementation

#include "kalahari/gui/main_window.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/toolbar_builder.h"
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

    // Create UI components (Command Registry pattern)
    registerCommands();    // Register all commands with CommandRegistry
    createMenus();         // Build menus from CommandRegistry
    createToolbars();      // Build toolbars from CommandRegistry
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

void MainWindow::registerCommands() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Registering commands with CommandRegistry");

    CommandRegistry& registry = CommandRegistry::getInstance();

    // ===== FILE CATEGORY =====

    // File > New
    registry.registerCommand(Command{
        "file.new",
        "&New",
        "Create a new document",
        "File",
        IconSet(),  // No icon for now
        KeyboardShortcut::fromQKeySequence(QKeySequence::New),
        [this]() { onNewDocument(); },
        nullptr,  // Always enabled
        nullptr,  // Not checkable
        true,     // Show in menu
        true      // Show in toolbar
    });

    // File > Open
    registry.registerCommand(Command{
        "file.open",
        "&Open...",
        "Open an existing document",
        "File",
        IconSet(),
        KeyboardShortcut::fromQKeySequence(QKeySequence::Open),
        [this]() { onOpenDocument(); },
        nullptr,
        nullptr,
        true,
        true
    });

    // File > Save
    registry.registerCommand(Command{
        "file.save",
        "&Save",
        "Save the current document",
        "File",
        IconSet(),
        KeyboardShortcut::fromQKeySequence(QKeySequence::Save),
        [this]() { onSaveDocument(); },
        nullptr,
        nullptr,
        true,
        true
    });

    // File > Save As
    registry.registerCommand(Command{
        "file.saveAs",
        "Save &As...",
        "Save the current document with a new name",
        "File",
        IconSet(),
        KeyboardShortcut::fromQKeySequence(QKeySequence::SaveAs),
        [this]() { onSaveAsDocument(); },
        nullptr,
        nullptr,
        true,
        false  // Not in toolbar
    });

    // File > Settings
    registry.registerCommand(Command{
        "file.settings",
        "&Settings...",
        "Open settings dialog",
        "File",
        IconSet(),
        KeyboardShortcut::fromQKeySequence(QKeySequence(tr("Ctrl+,"))),
        [this]() { onSettings(); },
        nullptr,
        nullptr,
        true,
        false
    });

    // File > Exit
    registry.registerCommand(Command{
        "file.exit",
        "E&xit",
        "Exit the application",
        "File",
        IconSet(),
        KeyboardShortcut::fromQKeySequence(QKeySequence::Quit),
        [this]() { onExit(); },
        nullptr,
        nullptr,
        true,
        false
    });

    // ===== EDIT CATEGORY =====

    // Edit > Undo
    registry.registerCommand(Command{
        "edit.undo",
        "&Undo",
        "Undo the last operation",
        "Edit",
        IconSet(),
        KeyboardShortcut::fromQKeySequence(QKeySequence::Undo),
        [this]() { onUndo(); },
        nullptr,
        nullptr,
        true,
        true
    });

    // Edit > Redo
    registry.registerCommand(Command{
        "edit.redo",
        "&Redo",
        "Redo the last undone operation",
        "Edit",
        IconSet(),
        KeyboardShortcut::fromQKeySequence(QKeySequence::Redo),
        [this]() { onRedo(); },
        nullptr,
        nullptr,
        true,
        true
    });

    // Edit > Cut
    registry.registerCommand(Command{
        "edit.cut",
        "Cu&t",
        "Cut the selection to clipboard",
        "Edit",
        IconSet(),
        KeyboardShortcut::fromQKeySequence(QKeySequence::Cut),
        [this]() { onCut(); },
        nullptr,
        nullptr,
        true,
        true
    });

    // Edit > Copy
    registry.registerCommand(Command{
        "edit.copy",
        "&Copy",
        "Copy the selection to clipboard",
        "Edit",
        IconSet(),
        KeyboardShortcut::fromQKeySequence(QKeySequence::Copy),
        [this]() { onCopy(); },
        nullptr,
        nullptr,
        true,
        true
    });

    // Edit > Paste
    registry.registerCommand(Command{
        "edit.paste",
        "&Paste",
        "Paste from clipboard",
        "Edit",
        IconSet(),
        KeyboardShortcut::fromQKeySequence(QKeySequence::Paste),
        [this]() { onPaste(); },
        nullptr,
        nullptr,
        true,
        true
    });

    // Edit > Select All
    registry.registerCommand(Command{
        "edit.selectAll",
        "Select &All",
        "Select all text",
        "Edit",
        IconSet(),
        KeyboardShortcut::fromQKeySequence(QKeySequence::SelectAll),
        [this]() { onSelectAll(); },
        nullptr,
        nullptr,
        true,
        false  // Not in toolbar
    });

    // ===== HELP CATEGORY =====

    // Help > About Kalahari
    registry.registerCommand(Command{
        "help.about",
        "&About Kalahari",
        "Show information about Kalahari",
        "Help",
        IconSet(),
        KeyboardShortcut(),  // No shortcut
        [this]() { onAbout(); },
        nullptr,
        nullptr,
        true,
        false
    });

    // Help > About Qt
    registry.registerCommand(Command{
        "help.aboutQt",
        "About &Qt",
        "Show information about Qt",
        "Help",
        IconSet(),
        KeyboardShortcut(),
        [this]() { onAboutQt(); },
        nullptr,
        nullptr,
        true,
        false
    });

    logger.debug("Commands registered successfully (15 commands)");
}

void MainWindow::createMenus() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating menus from CommandRegistry");

    // Build menu bar from CommandRegistry using MenuBuilder
    MenuBuilder builder;
    CommandRegistry& registry = CommandRegistry::getInstance();
    builder.buildMenuBar(registry, this);

    // Store menu pointers for later access (optional)
    m_fileMenu = menuBar()->findChild<QMenu*>("", Qt::FindDirectChildrenOnly);

    // Note: View menu will be populated by createDocks() with panel toggles
    // This is intentional - View menu is created empty by MenuBuilder
    // and filled with QDockWidget toggleViewAction() later

    logger.debug("Menus created successfully from CommandRegistry");
}

void MainWindow::createToolbars() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating toolbars from CommandRegistry");

    // Build toolbar from CommandRegistry using ToolbarBuilder
    ToolbarBuilder builder;
    CommandRegistry& registry = CommandRegistry::getInstance();
    m_fileToolbar = builder.buildToolBar(registry, this);

    // Add toolbar to window
    addToolBar(m_fileToolbar);

    // Toolbar is movable and floatable by default in ToolbarBuilder
    logger.debug("Toolbars created successfully from CommandRegistry");
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
