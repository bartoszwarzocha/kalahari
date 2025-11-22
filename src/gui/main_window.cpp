/// @file main_window.cpp
/// @brief Main application window implementation

#include "kalahari/gui/main_window.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/dialogs/about_dialog.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/toolbar_builder.h"
#include "kalahari/gui/panels/dashboard_panel.h"
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
    , m_centralTabs(nullptr)
    , m_dashboardPanel(nullptr)
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

    // NOTE (Task #00015): EditorPanel textChanged signal connected when tab created
    // No m_editorPanel at startup - Dashboard is default first tab

    logger.info("MainWindow initialized successfully");
}

void MainWindow::registerCommands() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Registering commands with CommandRegistry");

    CommandRegistry& registry = CommandRegistry::getInstance();
    int count = 0;

    #include "register_commands.hpp"

    // =========================================================================
    // FILE MENU
    // =========================================================================

    REG_CMD_TOOL("file.new", "New Book...", "FILE/New Book...", 10, false, 0,
                 KeyboardShortcut::fromQKeySequence(QKeySequence::New),
                 [this]() { onNewDocument(); });

    REG_CMD_TOOL("file.open", "Open Book...", "FILE/Open Book...", 20, false, 0,
                 KeyboardShortcut::fromQKeySequence(QKeySequence::Open),
                 [this]() { onOpenDocument(); });

    // Recent Books - dynamic submenu (registered separately)

    REG_CMD_CB("file.close", "Close Book", "FILE/Close Book", 40, true, 1,
               []() {});

    REG_CMD_TOOL("file.save", "Save", "FILE/Save", 50, false, 0,
                 KeyboardShortcut::fromQKeySequence(QKeySequence::Save),
                 [this]() { onSaveDocument(); });

    REG_CMD_TOOL("file.saveAs", "Save As...", "FILE/Save As...", 60, false, 0,
                 KeyboardShortcut::fromQKeySequence(QKeySequence::SaveAs),
                 [this]() { onSaveAsDocument(); });

    REG_CMD("file.saveAll", "Save All", "FILE/Save All", 70, true, 1);

    // Import submenu
    REG_CMD("file.import.docx", "DOCX Document...", "FILE/Import/DOCX Document...", 80, false, 1);
    REG_CMD("file.import.pdf", "PDF Reference...", "FILE/Import/PDF Reference...", 90, false, 2);
    REG_CMD("file.import.text", "Plain Text...", "FILE/Import/Plain Text...", 100, false, 1);
    REG_CMD("file.import.scrivener", "Scrivener Project...", "FILE/Import/Scrivener Project...", 110, false, 2);

    // Export submenu
    REG_CMD("file.export.docx", "DOCX", "FILE/Export/DOCX", 120, false, 1);
    REG_CMD("file.export.pdf", "PDF", "FILE/Export/PDF", 130, false, 1);
    REG_CMD("file.export.markdown", "Markdown", "FILE/Export/Markdown", 140, true, 1);
    REG_CMD("file.export.epub", "EPUB", "FILE/Export/EPUB", 150, false, 2);
    REG_CMD("file.export.mobi", "MOBI", "FILE/Export/MOBI", 160, false, 2);
    REG_CMD("file.export.icml", "InDesign ICML", "FILE/Export/InDesign ICML", 170, false, 3);
    REG_CMD("file.export.latex", "LaTeX", "FILE/Export/LaTeX", 180, false, 3);
    REG_CMD("file.export.settings", "Export Settings...", "FILE/Export/Export Settings...", 190, true, 2);

    REG_CMD_TOOL("file.exit", "Exit", "FILE/Exit", 200, false, 0,
                 KeyboardShortcut::fromQKeySequence(QKeySequence::Quit),
                 [this]() { onExit(); });

    // =========================================================================
    // EDIT MENU
    // =========================================================================

    REG_CMD_TOOL("edit.undo", "Undo", "EDIT/Undo", 10, false, 0,
                 KeyboardShortcut::fromQKeySequence(QKeySequence::Undo),
                 [this]() { onUndo(); });

    REG_CMD_TOOL("edit.redo", "Redo", "EDIT/Redo", 20, true, 0,
                 KeyboardShortcut::fromQKeySequence(QKeySequence::Redo),
                 [this]() { onRedo(); });

    REG_CMD_TOOL("edit.cut", "Cut", "EDIT/Cut", 30, false, 0,
                 KeyboardShortcut::fromQKeySequence(QKeySequence::Cut),
                 [this]() { onCut(); });

    REG_CMD_TOOL("edit.copy", "Copy", "EDIT/Copy", 40, false, 0,
                 KeyboardShortcut::fromQKeySequence(QKeySequence::Copy),
                 [this]() { onCopy(); });

    REG_CMD_TOOL("edit.paste", "Paste", "EDIT/Paste", 50, false, 0,
                 KeyboardShortcut::fromQKeySequence(QKeySequence::Paste),
                 [this]() { onPaste(); });

    REG_CMD("edit.pasteSpecial", "Paste Special...", "EDIT/Paste Special...", 60, false, 1);
    REG_CMD("edit.delete", "Delete", "EDIT/Delete", 70, true, 1);

    REG_CMD_CB("edit.selectAll", "Select All", "EDIT/Select All", 80, false, 0,
               [this]() { onSelectAll(); });

    REG_CMD("edit.selectWord", "Select Word", "EDIT/Select Word", 90, false, 1);
    REG_CMD("edit.selectParagraph", "Select Paragraph", "EDIT/Select Paragraph", 100, true, 1);

    REG_CMD("edit.find", "Find...", "EDIT/Find...", 110, false, 1);
    REG_CMD("edit.findNext", "Find Next", "EDIT/Find Next", 120, false, 1);
    REG_CMD("edit.findPrevious", "Find Previous", "EDIT/Find Previous", 130, false, 1);
    REG_CMD("edit.findReplace", "Find & Replace...", "EDIT/Find & Replace...", 140, false, 1);
    REG_CMD("edit.findInBook", "Find in Book...", "EDIT/Find in Book...", 150, true, 1);

    REG_CMD_CB("edit.preferences", "Preferences...", "EDIT/Preferences...", 160, false, 0,
               [this]() { onSettings(); });

    // =========================================================================
    // BOOK MENU
    // =========================================================================

    REG_CMD("book.newChapter", "New Chapter...", "BOOK/New Chapter...", 10, false, 1);
    REG_CMD("book.newScene", "New Scene...", "BOOK/New Scene...", 20, true, 1);

    REG_CMD("book.newCharacter", "New Character...", "BOOK/New Character...", 30, false, 1);
    REG_CMD("book.newLocation", "New Location...", "BOOK/New Location...", 40, false, 1);
    REG_CMD("book.newItem", "New Item...", "BOOK/New Item...", 50, true, 1);

    REG_CMD("book.newMindMap", "New Mind Map...", "BOOK/New Mind Map...", 60, false, 1);
    REG_CMD("book.newTimeline", "New Timeline...", "BOOK/New Timeline...", 70, true, 1);

    REG_CMD("book.chapterBreak", "Chapter Break", "BOOK/Chapter Break", 80, false, 1);
    REG_CMD("book.sceneBreak", "Scene Break", "BOOK/Scene Break", 90, true, 1);

    REG_CMD("book.properties", "Book Properties...", "BOOK/Book Properties...", 100, false, 1);

    // =========================================================================
    // INSERT MENU
    // =========================================================================

    REG_CMD("insert.image", "Image...", "INSERT/Image...", 10, false, 1);
    REG_CMD("insert.table", "Table...", "INSERT/Table...", 20, false, 1);
    REG_CMD("insert.link", "Link...", "INSERT/Link...", 30, true, 1);

    REG_CMD("insert.footnote", "Footnote", "INSERT/Footnote", 40, false, 1);
    REG_CMD("insert.endnote", "Endnote", "INSERT/Endnote", 50, false, 1);
    REG_CMD("insert.comment", "Comment", "INSERT/Comment", 60, false, 1);
    REG_CMD("insert.annotation", "Annotation", "INSERT/Annotation", 70, true, 1);

    REG_CMD("insert.specialChar", "Special Character...", "INSERT/Special Character...", 80, false, 1);
    REG_CMD("insert.dateTime", "Date & Time", "INSERT/Date & Time", 90, false, 1);
    REG_CMD("insert.field", "Field...", "INSERT/Field...", 100, false, 1);

    // =========================================================================
    // FORMAT MENU
    // =========================================================================

    REG_CMD("format.font", "Font...", "FORMAT/Font...", 10, false, 1);
    REG_CMD("format.paragraph", "Paragraph...", "FORMAT/Paragraph...", 20, true, 1);

    // Text Style submenu
    REG_CMD("format.style.heading1", "Heading 1", "FORMAT/Text Style/Heading 1", 30, false, 1);
    REG_CMD("format.style.heading2", "Heading 2", "FORMAT/Text Style/Heading 2", 40, false, 1);
    REG_CMD("format.style.heading3", "Heading 3", "FORMAT/Text Style/Heading 3", 50, false, 1);
    REG_CMD("format.style.body", "Body Text", "FORMAT/Text Style/Body Text", 60, false, 1);
    REG_CMD("format.style.quote", "Quote", "FORMAT/Text Style/Quote", 70, false, 1);
    REG_CMD("format.style.code", "Code", "FORMAT/Text Style/Code", 80, true, 1);
    REG_CMD("format.style.manage", "Manage Styles...", "FORMAT/Text Style/Manage Styles...", 90, false, 1);

    REG_CMD("format.bold", "Bold", "FORMAT/Bold", 100, false, 1);
    REG_CMD("format.italic", "Italic", "FORMAT/Italic", 110, false, 1);
    REG_CMD("format.underline", "Underline", "FORMAT/Underline", 120, false, 1);
    REG_CMD("format.strikethrough", "Strikethrough", "FORMAT/Strikethrough", 130, true, 1);

    REG_CMD("format.alignLeft", "Align Left", "FORMAT/Align Left", 140, false, 1);
    REG_CMD("format.alignCenter", "Align Center", "FORMAT/Align Center", 150, false, 1);
    REG_CMD("format.alignRight", "Align Right", "FORMAT/Align Right", 160, false, 1);
    REG_CMD("format.justify", "Justify", "FORMAT/Justify", 170, true, 1);

    REG_CMD("format.increaseIndent", "Increase Indent", "FORMAT/Increase Indent", 180, false, 1);
    REG_CMD("format.decreaseIndent", "Decrease Indent", "FORMAT/Decrease Indent", 190, true, 1);

    REG_CMD("format.bullets", "Bullets", "FORMAT/Bullets", 200, false, 1);
    REG_CMD("format.numbering", "Numbering", "FORMAT/Numbering", 210, true, 1);

    REG_CMD("format.color", "Color", "FORMAT/Color", 220, true, 1);

    REG_CMD("format.clearFormatting", "Clear Formatting", "FORMAT/Clear Formatting", 230, false, 1);

    // =========================================================================
    // TOOLS MENU
    // =========================================================================

    // Statistics submenu (analysis tools only, panels in VIEW)
    REG_CMD("tools.stats.full", "Full Statistics...", "TOOLS/Statistics/Full Statistics...", 10, false, 2);

    REG_CMD("tools.spellcheck", "Spellchecker", "TOOLS/Spellchecker", 40, false, 2);
    REG_CMD("tools.grammar", "Grammar Check", "TOOLS/Grammar Check", 50, false, 2);
    REG_CMD("tools.readability", "Readability Score", "TOOLS/Readability Score", 60, true, 2);

    // Focus Mode submenu
    REG_CMD("tools.focus.normal", "Normal", "TOOLS/Focus Mode/Normal", 70, false, 1);
    REG_CMD("tools.focus.focused", "Focused", "TOOLS/Focus Mode/Focused", 80, false, 1);
    REG_CMD("tools.focus.distractionFree", "Distraction-Free", "TOOLS/Focus Mode/Distraction-Free", 90, false, 1);

    REG_CMD("tools.backupNow", "Backup Now", "TOOLS/Backup Now", 100, false, 2);
    REG_CMD("tools.autoSaveSettings", "Auto-Save Settings...", "TOOLS/Auto-Save Settings...", 110, false, 1);
    REG_CMD("tools.versionHistory", "Version History...", "TOOLS/Version History...", 120, true, 2);

    // Plugins submenu
    REG_CMD("tools.plugins.manager", "Plugin Manager...", "TOOLS/Plugins/Plugin Manager...", 130, false, 2);
    REG_CMD("tools.plugins.marketplace", "Browse Marketplace...", "TOOLS/Plugins/Browse Marketplace...", 140, false, 2);
    REG_CMD("tools.plugins.updates", "Check for Updates...", "TOOLS/Plugins/Check for Updates...", 150, true, 2);
    REG_CMD("tools.plugins.reload", "Reload Plugins", "TOOLS/Plugins/Reload Plugins", 160, false, 2);

    REG_CMD("tools.challenges", "Challenges & Badges...", "TOOLS/Challenges & Badges...", 170, false, 2);
    REG_CMD("tools.writingGoals", "Writing Goals & Deadlines...", "TOOLS/Writing Goals & Deadlines...", 180, true, 2);

    REG_CMD("tools.cloudSync", "Cloud Sync...", "TOOLS/Cloud Sync...", 190, false, 3);
    REG_CMD("tools.collaboration", "Collaboration...", "TOOLS/Collaboration...", 200, false, 3);

    // =========================================================================
    // ASSISTANT MENU
    // =========================================================================

    REG_CMD("assistant.ask", "Ask Assistant...", "ASSISTANT/Ask Assistant...", 10, true, 2);

    REG_CMD("assistant.switch", "Switch Assistant...", "ASSISTANT/Switch Assistant...", 20, true, 2);

    // Assistant Actions submenu
    REG_CMD("assistant.action.grammar", "Check Grammar", "ASSISTANT/Assistant Actions/Check Grammar", 30, false, 2);
    REG_CMD("assistant.action.style", "Improve Style", "ASSISTANT/Assistant Actions/Improve Style", 40, false, 2);
    REG_CMD("assistant.action.plot", "Analyze Plot", "ASSISTANT/Assistant Actions/Analyze Plot", 50, false, 2);
    REG_CMD("assistant.action.research", "Research Topic...", "ASSISTANT/Assistant Actions/Research Topic...", 60, false, 2);
    REG_CMD("assistant.action.speedDraft", "Speed Draft Mode", "ASSISTANT/Assistant Actions/Speed Draft Mode", 70, false, 2);

    REG_CMD("assistant.settings", "Assistant Settings...", "ASSISTANT/Assistant Settings...", 80, false, 2);

    // =========================================================================
    // VIEW MENU
    // =========================================================================

    // NOTE: Panels submenu populated by createDocks() using toggleViewAction()
    // No registration here to avoid duplicates!

    // Perspectives submenu
    REG_CMD("view.perspectives.writer", "Writer", "VIEW/Perspectives/Writer", 70, false, 1);
    REG_CMD("view.perspectives.editor", "Editor", "VIEW/Perspectives/Editor", 80, false, 1);
    REG_CMD("view.perspectives.researcher", "Researcher", "VIEW/Perspectives/Researcher", 90, false, 1);
    REG_CMD("view.perspectives.planner", "Planner", "VIEW/Perspectives/Planner", 100, true, 1);
    REG_CMD("view.perspectives.save", "Save Current Perspective...", "VIEW/Perspectives/Save Current Perspective...", 110, false, 1);
    REG_CMD("view.perspectives.manage", "Manage Perspectives...", "VIEW/Perspectives/Manage Perspectives...", 120, false, 1);

    // Toolbars submenu
    REG_CMD("view.toolbars.standard", "Standard Toolbar", "VIEW/Toolbars/Standard Toolbar", 130, false, 1);
    REG_CMD("view.toolbars.book", "Book Toolbar", "VIEW/Toolbars/Book Toolbar", 140, false, 1);
    REG_CMD("view.toolbars.format", "Format Toolbar", "VIEW/Toolbars/Format Toolbar", 150, false, 1);
    REG_CMD("view.toolbars.quickAccess", "Quick Access Toolbar", "VIEW/Toolbars/Quick Access Toolbar", 160, true, 1);
    REG_CMD("view.toolbars.customize", "Customize Toolbars...", "VIEW/Toolbars/Customize Toolbars...", 170, false, 1);

    REG_CMD("view.showStatusBar", "Show Status Bar", "VIEW/Show Status Bar", 180, false, 0);
    REG_CMD("view.showStatsBar", "Show Statistics Bar", "VIEW/Show Statistics Bar", 190, false, 1);
    REG_CMD("view.showFormattingMarks", "Show Formatting Marks", "VIEW/Show Formatting Marks", 210, true, 1);

    REG_CMD("view.zoomIn", "Zoom In", "VIEW/Zoom In", 220, false, 1);
    REG_CMD("view.zoomOut", "Zoom Out", "VIEW/Zoom Out", 230, false, 1);
    REG_CMD("view.resetZoom", "Reset Zoom", "VIEW/Reset Zoom", 240, true, 1);

    REG_CMD("view.fullScreen", "Full Screen", "VIEW/Full Screen", 250, true, 1);

    REG_CMD("view.resetLayout", "Reset Layout", "VIEW/Reset Layout", 260, false, 0);

    // =========================================================================
    // HELP MENU
    // =========================================================================

    REG_CMD("help.manual", "Kalahari Help", "HELP/Kalahari Help", 10, false, 2);
    REG_CMD("help.tutorial", "Getting Started Tutorial", "HELP/Getting Started Tutorial", 20, true, 2);

    REG_CMD("help.shortcuts", "Keyboard Shortcuts", "HELP/Keyboard Shortcuts", 30, false, 1);
    REG_CMD("help.tipsTricks", "Tips & Tricks", "HELP/Tips & Tricks", 40, false, 2);
    REG_CMD("help.whatsNew", "What's New", "HELP/What's New", 50, true, 1);

    REG_CMD("help.reportBug", "Report a Bug...", "HELP/Report a Bug...", 60, false, 1);
    REG_CMD("help.suggestFeature", "Suggest a Feature...", "HELP/Suggest a Feature...", 70, false, 1);
    REG_CMD("help.communityForum", "Community Forum", "HELP/Community Forum", 80, true, 2);

    REG_CMD("help.checkUpdates", "Check for Updates...", "HELP/Check for Updates...", 90, true, 2);

    REG_CMD_CB("help.about", "About Kalahari", "HELP/About Kalahari", 100, false, 0,
               [this]() { onAbout(); });

    logger.debug("Commands registered successfully ({} commands)", count);
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

    // Task #00015: Check for unsaved changes in current editor tab
    EditorPanel* currentEditor = getCurrentEditor();
    if (currentEditor && m_isDirty) {
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

    // Task #00015: Create new EditorPanel tab (on-demand)
    EditorPanel* newEditor = new EditorPanel(this);
    int tabIndex = m_centralTabs->addTab(newEditor, tr("Untitled"));
    m_centralTabs->setCurrentIndex(tabIndex);

    // Connect textChanged signal for dirty tracking
    connect(newEditor->getTextEdit(), &QPlainTextEdit::textChanged,
            this, [this]() {
                if (!m_currentDocument.has_value()) return;
                setDirty(true);
            });

    // Create new document
    m_currentDocument = core::Document("Untitled", "User", "en");
    m_currentFilePath = "";
    newEditor->setText("");
    setDirty(false);

    // Update navigator panel
    m_navigatorPanel->loadDocument(m_currentDocument.value());

    logger.info("New document created in new tab");
    statusBar()->showMessage(tr("New document created"), 2000);
}

void MainWindow::onOpenDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Open Document");

    // Task #00015: Check for unsaved changes in current editor tab
    EditorPanel* currentEditor = getCurrentEditor();
    if (currentEditor && m_isDirty) {
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

    // Task #00015: Create new EditorPanel tab (on-demand)
    EditorPanel* newEditor = new EditorPanel(this);

    // Get document title for tab name
    QString docTitle = QString::fromStdString(loaded.value().getTitle());
    int tabIndex = m_centralTabs->addTab(newEditor, docTitle);
    m_centralTabs->setCurrentIndex(tabIndex);

    // Connect textChanged signal for dirty tracking
    connect(newEditor->getTextEdit(), &QPlainTextEdit::textChanged,
            this, [this]() {
                if (!m_currentDocument.has_value()) return;
                setDirty(true);
            });

    // Success - update state
    m_currentDocument = std::move(loaded.value());
    m_currentFilePath = filepath;

    // Extract text and load into editor
    QString content = getPhase0Content(m_currentDocument.value());
    newEditor->setText(content);

    setDirty(false);

    // Update navigator panel
    m_navigatorPanel->loadDocument(m_currentDocument.value());

    logger.info("Document loaded in new tab: {}", filepath.string());
    statusBar()->showMessage(tr("Document opened: %1").arg(filename), 2000);
}

void MainWindow::onSaveDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save Document");

    // Task #00015: Get current editor (returns nullptr if Dashboard is active)
    EditorPanel* editor = getCurrentEditor();
    if (!editor) {
        logger.debug("No editor tab active - cannot save");
        statusBar()->showMessage(tr("No document to save"), 2000);
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

    // Task #00015: Get current editor (returns nullptr if Dashboard is active)
    EditorPanel* editor = getCurrentEditor();
    if (!editor) {
        logger.debug("No editor tab active - cannot save");
        statusBar()->showMessage(tr("No document to save"), 2000);
        return;
    }

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

    dialogs::AboutDialog dialog(this);
    dialog.exec();

    logger.info("About dialog closed");
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

    // Create central tabbed workspace (Task #00015)
    m_centralTabs = new QTabWidget(this);
    m_centralTabs->setTabsClosable(true);       // All tabs can be closed
    m_centralTabs->setMovable(true);            // Tabs can be reordered
    m_centralTabs->setDocumentMode(true);       // Better look on macOS/Windows
    setCentralWidget(m_centralTabs);

    // Add Dashboard as first tab (default at startup, closable)
    m_dashboardPanel = new DashboardPanel(this);
    int dashboardIndex = m_centralTabs->addTab(m_dashboardPanel, tr("Dashboard"));
    m_centralTabs->setCurrentIndex(dashboardIndex);

    // Connect tab close signal (Task #00015)
    connect(m_centralTabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        auto& logger = core::Logger::getInstance();
        QWidget* widget = m_centralTabs->widget(index);
        EditorPanel* editor = qobject_cast<EditorPanel*>(widget);

        // Check for unsaved changes if EditorPanel
        if (editor && m_isDirty) {
            // TODO (Phase 1): Prompt user to save changes
            // For now: just close
        }

        // Remove tab and delete widget
        m_centralTabs->removeTab(index);
        widget->deleteLater();

        logger.debug("Tab closed at index {}", index);
    });

    // NOTE (Task #00015): EditorPanel tabs created ON-DEMAND via File > New/Open
    // No m_editorPanel at startup! Use getCurrentEditor() to access active editor.

    // Navigator dock (left)
    m_navigatorPanel = new NavigatorPanel(this);
    m_navigatorDock = new QDockWidget(tr("Navigator"), this);
    m_navigatorDock->setWidget(m_navigatorPanel);
    m_navigatorDock->setObjectName("NavigatorDock");  // Required for saveState!
    addDockWidget(Qt::LeftDockWidgetArea, m_navigatorDock);

    // Connect Navigator double-click signal (Task #00015)
    connect(m_navigatorPanel, &NavigatorPanel::chapterDoubleClicked,
            this, &MainWindow::onNavigatorItemDoubleClicked);

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

    // Find existing View menu from MenuBuilder (DON'T create a new one!)
    QList<QAction*> menuActions = menuBar()->actions();
    for (QAction* action : menuActions) {
        if (action->text().contains("View", Qt::CaseInsensitive)) {
            m_viewMenu = action->menu();
            break;
        }
    }

    if (!m_viewMenu) {
        logger.warn("VIEW menu not found! Creating fallback menu.");
        m_viewMenu = menuBar()->addMenu(tr("&View"));
    }

    // Create Panels submenu (not in CommandRegistry - dynamic toggleViewAction only)
    QMenu* panelsSubmenu = m_viewMenu->addMenu(tr("Panels"));
    logger.debug("Created VIEW/Panels submenu for dock toggles");

    // Create toggle actions and add to Panels submenu (use QDockWidget's built-in toggleViewAction!)
    m_viewNavigatorAction = m_navigatorDock->toggleViewAction();
    m_viewNavigatorAction->setShortcut(QKeySequence(tr("Ctrl+1")));
    panelsSubmenu->addAction(m_viewNavigatorAction);

    m_viewPropertiesAction = m_propertiesDock->toggleViewAction();
    m_viewPropertiesAction->setShortcut(QKeySequence(tr("Ctrl+2")));
    panelsSubmenu->addAction(m_viewPropertiesAction);

    m_viewLogAction = m_logDock->toggleViewAction();
    m_viewLogAction->setShortcut(QKeySequence(tr("Ctrl+3")));
    panelsSubmenu->addAction(m_viewLogAction);

    m_viewSearchAction = m_searchDock->toggleViewAction();
    m_viewSearchAction->setShortcut(QKeySequence(tr("Ctrl+4")));
    panelsSubmenu->addAction(m_viewSearchAction);

    m_viewAssistantAction = m_assistantDock->toggleViewAction();
    m_viewAssistantAction->setShortcut(QKeySequence(tr("Ctrl+5")));
    panelsSubmenu->addAction(m_viewAssistantAction);

    // Reset layout action (will appear in main VIEW menu)
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

EditorPanel* MainWindow::getCurrentEditor() {
    // Task #00015: Get active EditorPanel tab (or nullptr)
    if (!m_centralTabs) {
        return nullptr;
    }

    QWidget* currentWidget = m_centralTabs->currentWidget();
    return qobject_cast<EditorPanel*>(currentWidget);  // nullptr if not EditorPanel
}

void MainWindow::onNavigatorItemDoubleClicked(const QString& chapterTitle) {
    // Task #00015: Handle Navigator double-click
    auto& logger = core::Logger::getInstance();
    logger.info("Navigator item double-clicked: {}", chapterTitle.toStdString());

    // Check if document is loaded
    if (!m_currentDocument.has_value()) {
        logger.debug("No document loaded - ignoring Navigator double-click");
        statusBar()->showMessage(tr("No document loaded"), 2000);
        return;
    }

    // Phase 0: Open whole document content in new editor tab
    // Phase 1+: Open specific chapter content based on chapterTitle

    EditorPanel* newEditor = new EditorPanel(this);
    QString tabTitle = chapterTitle;  // Use chapter title as tab name
    int tabIndex = m_centralTabs->addTab(newEditor, tabTitle);
    m_centralTabs->setCurrentIndex(tabIndex);

    // Connect textChanged signal for dirty tracking
    connect(newEditor->getTextEdit(), &QPlainTextEdit::textChanged,
            this, [this]() {
                if (!m_currentDocument.has_value()) return;
                setDirty(true);
            });

    // Load document content (Phase 0: whole document)
    QString content = getPhase0Content(m_currentDocument.value());
    newEditor->setText(content);

    logger.info("Opened chapter '{}' in new editor tab", chapterTitle.toStdString());
    statusBar()->showMessage(tr("Opened: %1").arg(chapterTitle), 2000);
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
