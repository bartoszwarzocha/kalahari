/// @file main_window.cpp
/// @brief Main application window implementation

#include "kalahari/gui/main_window.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/dialogs/about_dialog.h"
#include "kalahari/gui/dialogs/icon_downloader_dialog.h"
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
#include "kalahari/core/icon_registry.h"
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
    , m_toolbarManager(nullptr)
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
    , m_diagnosticMode(false)
    , m_diagnosticMenu(nullptr)
    , m_devMode(false)
    , m_devToolsMenu(nullptr)
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
    // ICON REGISTRY - Register default SVG icons (Task #00021)
    // =========================================================================
    auto& iconRegistry = core::IconRegistry::getInstance();

    // File menu icons
    iconRegistry.registerIcon("file.new", "resources/icons/twotone/file-new.svg", "New File");
    iconRegistry.registerIcon("file.open", "resources/icons/twotone/folder_open.svg", "Open File");
    iconRegistry.registerIcon("file.save", "resources/icons/twotone/save.svg", "Save");
    iconRegistry.registerIcon("file.saveAs", "resources/icons/twotone/save-as.svg", "Save As");
    iconRegistry.registerIcon("file.exit", "resources/icons/twotone/exit.svg", "Exit");

    // Edit menu icons
    iconRegistry.registerIcon("edit.undo", "resources/icons/twotone/undo.svg", "Undo");
    iconRegistry.registerIcon("edit.redo", "resources/icons/twotone/redo.svg", "Redo");
    iconRegistry.registerIcon("edit.cut", "resources/icons/twotone/cut.svg", "Cut");
    iconRegistry.registerIcon("edit.copy", "resources/icons/twotone/copy.svg", "Copy");
    iconRegistry.registerIcon("edit.paste", "resources/icons/twotone/paste.svg", "Paste");
    iconRegistry.registerIcon("edit.delete", "resources/icons/twotone/delete.svg", "Delete");
    iconRegistry.registerIcon("edit.selectAll", "resources/icons/twotone/select_all.svg", "Select All");
    iconRegistry.registerIcon("edit.find", "resources/icons/twotone/find.svg", "Find");
    iconRegistry.registerIcon("edit.settings", "resources/icons/twotone/settings.svg", "Settings");

    // Help menu icons
    iconRegistry.registerIcon("help.about", "resources/icons/twotone/information.svg", "About");
    iconRegistry.registerIcon("help.help", "resources/icons/twotone/help.svg", "Help");

    logger.debug("Registered {} default icons with IconRegistry", 16);

    // =========================================================================
    // FILE MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("file.new", "New Book...", "FILE/New Book...", 10, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::New),
                      IconSet::fromStandardIcon(QStyle::SP_FileIcon),
                      [this]() { onNewDocument(); });

    REG_CMD_TOOL_ICON("file.open", "Open Book...", "FILE/Open Book...", 20, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Open),
                      IconSet::fromStandardIcon(QStyle::SP_DirOpenIcon),
                      [this]() { onOpenDocument(); });

    // Recent Books - dynamic submenu (registered separately)

    REG_CMD_TOOL_ICON("file.close", "Close Book", "FILE/Close Book", 40, true, 1,
                      KeyboardShortcut(),
                      IconSet::fromStandardIcon(QStyle::SP_DialogCancelButton),
                      []() {});

    REG_CMD_TOOL_ICON("file.save", "Save", "FILE/Save", 50, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Save),
                      IconSet::fromStandardIcon(QStyle::SP_DialogSaveButton),
                      [this]() { onSaveDocument(); });

    REG_CMD_TOOL_ICON("file.saveAs", "Save As...", "FILE/Save As...", 60, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::SaveAs),
                      IconSet::createPlaceholder("A", QColor(70, 130, 180)),  // Steel blue
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

    REG_CMD_TOOL_ICON("file.exit", "Exit", "FILE/Exit", 200, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Quit),
                      IconSet::fromStandardIcon(QStyle::SP_DialogCloseButton),
                      [this]() { onExit(); });

    // =========================================================================
    // EDIT MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("edit.undo", "Undo", "EDIT/Undo", 10, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Undo),
                      IconSet::fromStandardIcon(QStyle::SP_ArrowBack),
                      [this]() { onUndo(); });

    REG_CMD_TOOL_ICON("edit.redo", "Redo", "EDIT/Redo", 20, true, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Redo),
                      IconSet::fromStandardIcon(QStyle::SP_ArrowForward),
                      [this]() { onRedo(); });

    REG_CMD_TOOL_ICON("edit.cut", "Cut", "EDIT/Cut", 30, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Cut),
                      IconSet::createPlaceholder("X", QColor(220, 53, 69)),  // Red
                      [this]() { onCut(); });

    REG_CMD_TOOL_ICON("edit.copy", "Copy", "EDIT/Copy", 40, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Copy),
                      IconSet::createPlaceholder("C", QColor(0, 123, 255)),  // Blue
                      [this]() { onCopy(); });

    REG_CMD_TOOL_ICON("edit.paste", "Paste", "EDIT/Paste", 50, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Paste),
                      IconSet::createPlaceholder("V", QColor(40, 167, 69)),  // Green
                      [this]() { onPaste(); });

    REG_CMD("edit.pasteSpecial", "Paste Special...", "EDIT/Paste Special...", 60, false, 1);
    REG_CMD("edit.delete", "Delete", "EDIT/Delete", 70, true, 1);

    REG_CMD_TOOL_ICON("edit.selectAll", "Select All", "EDIT/Select All", 80, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::SelectAll),
                      IconSet::createPlaceholder("A", QColor(111, 66, 193)),  // Purple
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

    REG_CMD_TOOL_ICON("book.newChapter", "New Chapter...", "BOOK/New Chapter...", 10, false, 1,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("CH", QColor(40, 167, 69)),  // Green
                      []() {});

    REG_CMD("book.newScene", "New Scene...", "BOOK/New Scene...", 20, true, 1);

    REG_CMD_TOOL_ICON("book.newCharacter", "New Character...", "BOOK/New Character...", 30, false, 1,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("P", QColor(255, 145, 0)),  // Orange (Person)
                      []() {});

    REG_CMD_TOOL_ICON("book.newLocation", "New Location...", "BOOK/New Location...", 40, false, 1,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("L", QColor(0, 123, 255)),  // Blue
                      []() {});

    REG_CMD("book.newItem", "New Item...", "BOOK/New Item...", 50, true, 1);

    REG_CMD("book.newMindMap", "New Mind Map...", "BOOK/New Mind Map...", 60, false, 1);
    REG_CMD("book.newTimeline", "New Timeline...", "BOOK/New Timeline...", 70, true, 1);

    REG_CMD("book.chapterBreak", "Chapter Break", "BOOK/Chapter Break", 80, false, 1);
    REG_CMD("book.sceneBreak", "Scene Break", "BOOK/Scene Break", 90, true, 1);

    REG_CMD_TOOL_ICON("book.properties", "Book Properties...", "BOOK/Book Properties...", 100, false, 1,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("i", QColor(108, 117, 125)),  // Gray (info)
                      []() {});

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

    REG_CMD_TOOL_ICON("tools.stats.wordCount", "Word Count", "TOOLS/Statistics/Word Count", 20, true, 0,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("#", QColor(0, 123, 255)),  // Blue
                      []() {});

    REG_CMD_TOOL_ICON("tools.spellcheck", "Spellchecker", "TOOLS/Spellchecker", 40, false, 2,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("ABC", QColor(220, 53, 69)),  // Red
                      []() {});

    REG_CMD("tools.grammar", "Grammar Check", "TOOLS/Grammar Check", 50, false, 2);
    REG_CMD("tools.readability", "Readability Score", "TOOLS/Readability Score", 60, true, 2);

    // Focus Mode submenu
    REG_CMD_TOOL_ICON("tools.focus.normal", "Normal", "TOOLS/Focus Mode/Normal", 70, false, 1,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("F", QColor(40, 167, 69)),  // Green
                      []() {});

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

    // Panel toggle actions (Task #00019 - for View Toolbar)
    REG_CMD_TOOL_ICON("view.navigator", "Navigator", "VIEW/Panels/Navigator", 10, false, 0,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("N", QColor(0, 123, 255)),  // Blue
                      []() {});

    REG_CMD_TOOL_ICON("view.properties", "Properties", "VIEW/Panels/Properties", 20, false, 0,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("P", QColor(40, 167, 69)),  // Green
                      []() {});

    REG_CMD_TOOL_ICON("view.search", "Search", "VIEW/Panels/Search", 30, false, 0,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("S", QColor(255, 145, 0)),  // Orange
                      []() {});

    REG_CMD_TOOL_ICON("view.assistant", "Assistant", "VIEW/Panels/Assistant", 40, false, 0,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("A", QColor(111, 66, 193)),  // Purple
                      []() {});

    REG_CMD_TOOL_ICON("view.log", "Log", "VIEW/Panels/Log", 50, true, 0,
                      KeyboardShortcut(),
                      IconSet::createPlaceholder("L", QColor(108, 117, 125)),  // Gray
                      []() {});

    // NOTE: Panel submenu also populated by createDocks() using toggleViewAction()
    // Above commands are for toolbar only

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
    logger.debug("Creating toolbars from CommandRegistry using ToolbarManager");

    // Task #00019: Create ToolbarManager and build 5 toolbars
    m_toolbarManager = new ToolbarManager(this);
    CommandRegistry& registry = CommandRegistry::getInstance();
    m_toolbarManager->createToolbars(registry);

    logger.debug("5 toolbars created successfully (File, Edit, Book, View, Tools)");
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

    // Pass current diagnostic mode state to dialog (Task #00018)
    SettingsDialog dialog(this, m_diagnosticMode);

    // Connect diagnostic mode signal (Task #00018)
    connect(&dialog, &SettingsDialog::diagnosticModeChanged,
            this, &MainWindow::onDiagModeChanged);

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

    // Task #00019: Add toolbar toggle actions to View menu
    if (m_toolbarManager) {
        m_toolbarManager->createViewMenuActions(m_viewMenu);
        logger.debug("Toolbar toggle actions added to VIEW menu");
    }

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

    // Task #00019: Save toolbar state (visibility)
    if (m_toolbarManager) {
        m_toolbarManager->saveState();
    }

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

// =============================================================================
// Diagnostic Mode (Task #00018)
// =============================================================================

void MainWindow::enableDiagnosticMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Enabling diagnostic mode");

    m_diagnosticMode = true;
    createDiagnosticMenu();

    statusBar()->showMessage(tr("Diagnostic mode enabled"), 3000);
}

void MainWindow::disableDiagnosticMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Disabling diagnostic mode");

    removeDiagnosticMenu();
    m_diagnosticMode = false;

    statusBar()->showMessage(tr("Diagnostic mode disabled"), 3000);
}

void MainWindow::onDiagModeChanged(bool enabled) {
    if (enabled) {
        enableDiagnosticMode();
    } else {
        disableDiagnosticMode();
    }
}

void MainWindow::createDiagnosticMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating Diagnostics menu");

    // Don't create if already exists
    if (m_diagnosticMenu) {
        logger.warn("Diagnostics menu already exists");
        return;
    }

    // Create menu (inserted between Help and existing menus)
    m_diagnosticMenu = menuBar()->addMenu(tr("&Diagnostics"));

    // === CATEGORY 1: System Information ===
    QAction* sysInfoAction = m_diagnosticMenu->addAction(tr("System Information"));
    connect(sysInfoAction, &QAction::triggered, this, &MainWindow::onDiagSystemInfo);

    QAction* qtEnvAction = m_diagnosticMenu->addAction(tr("Qt Environment"));
    connect(qtEnvAction, &QAction::triggered, this, &MainWindow::onDiagQtEnvironment);

    QAction* fsCheckAction = m_diagnosticMenu->addAction(tr("File System Check"));
    connect(fsCheckAction, &QAction::triggered, this, &MainWindow::onDiagFileSystemCheck);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 2: Application State ===
    QAction* settingsDumpAction = m_diagnosticMenu->addAction(tr("Settings Dump"));
    connect(settingsDumpAction, &QAction::triggered, this, &MainWindow::onDiagSettingsDump);

    QAction* memStatsAction = m_diagnosticMenu->addAction(tr("Memory Statistics"));
    connect(memStatsAction, &QAction::triggered, this, &MainWindow::onDiagMemoryStats);

    QAction* openDocsAction = m_diagnosticMenu->addAction(tr("Open Documents Statistics"));
    connect(openDocsAction, &QAction::triggered, this, &MainWindow::onDiagOpenDocsStats);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 3: Core Systems ===
    QAction* loggerTestAction = m_diagnosticMenu->addAction(tr("Logger Test"));
    connect(loggerTestAction, &QAction::triggered, this, &MainWindow::onDiagLoggerTest);

    QAction* eventBusTestAction = m_diagnosticMenu->addAction(tr("Event Bus Test"));
    connect(eventBusTestAction, &QAction::triggered, this, &MainWindow::onDiagEventBusTest);

    QAction* pluginCheckAction = m_diagnosticMenu->addAction(tr("Plugin Manager Check"));
    connect(pluginCheckAction, &QAction::triggered, this, &MainWindow::onDiagPluginCheck);

    QAction* cmdRegistryDumpAction = m_diagnosticMenu->addAction(tr("Command Registry Dump"));
    connect(cmdRegistryDumpAction, &QAction::triggered, this, &MainWindow::onDiagCommandRegistryDump);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 4: Python Environment ===
    QAction* pyEnvAction = m_diagnosticMenu->addAction(tr("Python Environment"));
    connect(pyEnvAction, &QAction::triggered, this, &MainWindow::onDiagPythonEnvironment);

    QAction* pyImportAction = m_diagnosticMenu->addAction(tr("Python Import Test"));
    connect(pyImportAction, &QAction::triggered, this, &MainWindow::onDiagPythonImportTest);

    QAction* pyMemoryAction = m_diagnosticMenu->addAction(tr("Python Memory Test"));
    connect(pyMemoryAction, &QAction::triggered, this, &MainWindow::onDiagPythonMemoryTest);

    QAction* pyInterpAction = m_diagnosticMenu->addAction(tr("Embedded Interpreter Status"));
    connect(pyInterpAction, &QAction::triggered, this, &MainWindow::onDiagEmbeddedInterpreterStatus);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 5: Performance ===
    QAction* perfBenchAction = m_diagnosticMenu->addAction(tr("Performance Benchmark"));
    connect(perfBenchAction, &QAction::triggered, this, &MainWindow::onDiagPerformanceBenchmark);

    QAction* renderStatsAction = m_diagnosticMenu->addAction(tr("Render Statistics"));
    connect(renderStatsAction, &QAction::triggered, this, &MainWindow::onDiagRenderStats);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 6: Quick Actions ===
    QAction* clearLogAction = m_diagnosticMenu->addAction(tr("Clear Log"));
    connect(clearLogAction, &QAction::triggered, this, &MainWindow::onDiagClearLog);

#ifdef _DEBUG
    m_diagnosticMenu->addSeparator();
    QAction* forceCrashAction = m_diagnosticMenu->addAction(tr("Force Crash (Debug Only)"));
    connect(forceCrashAction, &QAction::triggered, this, &MainWindow::onDiagForceCrash);

    QAction* memLeakAction = m_diagnosticMenu->addAction(tr("Memory Leak Test (Debug Only)"));
    connect(memLeakAction, &QAction::triggered, this, &MainWindow::onDiagMemoryLeakTest);
#endif

    logger.debug("Diagnostics menu created successfully with 18 tools");
}

void MainWindow::removeDiagnosticMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Removing Diagnostics menu");

    if (!m_diagnosticMenu) {
        logger.debug("Diagnostics menu doesn't exist, nothing to remove");
        return;
    }

    // Remove from menu bar
    menuBar()->removeAction(m_diagnosticMenu->menuAction());

    // Delete menu
    delete m_diagnosticMenu;
    m_diagnosticMenu = nullptr;

    logger.debug("Diagnostics menu removed successfully");
}

// =============================================================================
// Diagnostic Tool Implementations (Task #00018)
// =============================================================================

void MainWindow::onDiagSystemInfo() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: System Information ===");
    logger.info("OS: {}", QSysInfo::prettyProductName().toStdString());
    logger.info("Kernel: {}", QSysInfo::kernelType().toStdString() + " " + QSysInfo::kernelVersion().toStdString());
    logger.info("CPU Architecture: {}", QSysInfo::currentCpuArchitecture().toStdString());
    logger.info("Build ABI: {}", QSysInfo::buildAbi().toStdString());
    statusBar()->showMessage(tr("System information logged"), 2000);
}

void MainWindow::onDiagQtEnvironment() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Qt Environment ===");
    logger.info("Qt Version: {}", qVersion());
    logger.info("Qt Build Mode: {}",
#ifdef QT_DEBUG
        "Debug"
#else
        "Release"
#endif
    );
    logger.info("Application: {} {}",
        QCoreApplication::applicationName().toStdString(),
        QCoreApplication::applicationVersion().toStdString());
    logger.info("Organization: {}", QCoreApplication::organizationName().toStdString());
    statusBar()->showMessage(tr("Qt environment logged"), 2000);
}

void MainWindow::onDiagFileSystemCheck() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: File System Check ===");
    logger.info("Working Directory: {}", QDir::currentPath().toStdString());
    logger.info("Application Path: {}", QCoreApplication::applicationDirPath().toStdString());
    logger.info("Temp Path: {}", QDir::tempPath().toStdString());
    logger.info("Home Path: {}", QDir::homePath().toStdString());
    statusBar()->showMessage(tr("File system check logged"), 2000);
}

void MainWindow::onDiagSettingsDump() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Settings Dump ===");
    auto& settings = core::SettingsManager::getInstance();
    logger.info("Theme: {}", settings.getTheme());
    logger.info("Language: {}", settings.getLanguage());
    logger.info("Editor Font: {}", settings.get<std::string>("editor.fontFamily", "N/A"));
    logger.info("Editor Font Size: {}", settings.get<int>("editor.fontSize", 0));
    logger.info("Tab Size: {}", settings.get<int>("editor.tabSize", 0));
    logger.info("Line Numbers: {}", settings.get<bool>("editor.lineNumbers", false));
    logger.info("Word Wrap: {}", settings.get<bool>("editor.wordWrap", false));
    statusBar()->showMessage(tr("Settings dumped to log"), 2000);
}

void MainWindow::onDiagMemoryStats() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Memory Statistics ===");
    logger.info("NOTE: Detailed memory stats require platform-specific code");
    logger.info("Document loaded: {}", m_currentDocument.has_value());
    if (m_currentDocument.has_value()) {
        logger.info("Document dirty: {}", m_isDirty);
        logger.info("Document path: {}", m_currentFilePath.string());
    }
    statusBar()->showMessage(tr("Memory statistics logged"), 2000);
}

void MainWindow::onDiagOpenDocsStats() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Open Documents Statistics ===");
    logger.info("Document loaded: {}", m_currentDocument.has_value());
    if (m_currentDocument.has_value()) {
        logger.info("Dirty flag: {}", m_isDirty);
        logger.info("File path: {}", m_currentFilePath.string());
    }
    logger.info("Open editor tabs: {}", m_centralTabs ? m_centralTabs->count() : 0);
    statusBar()->showMessage(tr("Document statistics logged"), 2000);
}

void MainWindow::onDiagLoggerTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Logger Test ===");
    logger.debug("DEBUG level message");
    logger.info("INFO level message");
    logger.warn("WARN level message");
    logger.error("ERROR level message");
    logger.critical("CRITICAL level message");
    logger.info("Logger test complete - check Log Panel for all levels");
    statusBar()->showMessage(tr("Logger test complete"), 2000);
}

void MainWindow::onDiagEventBusTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Event Bus Test ===");
    logger.info("NOTE: Event Bus implementation pending (Phase 1)");
    logger.info("Event Bus test will be implemented when Event Bus is ready");
    statusBar()->showMessage(tr("Event Bus test logged"), 2000);
}

void MainWindow::onDiagPluginCheck() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Plugin Manager Check ===");
    logger.info("NOTE: Plugin Manager implementation pending (Phase 2)");
    logger.info("Plugin check will be implemented when Plugin Manager is ready");
    statusBar()->showMessage(tr("Plugin check logged"), 2000);
}

void MainWindow::onDiagCommandRegistryDump() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Command Registry Dump ===");
    logger.info("NOTE: CommandRegistry::dump() not yet implemented");
    logger.info("Command Registry diagnostic will be enhanced in future");
    statusBar()->showMessage(tr("Command Registry dump logged"), 2000);
}

void MainWindow::onDiagPythonEnvironment() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Python Environment ===");
    logger.info("NOTE: Python environment check requires pybind11 integration");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    statusBar()->showMessage(tr("Python environment check logged"), 2000);
}

void MainWindow::onDiagPythonImportTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Python Import Test ===");
    logger.info("NOTE: Python import test requires embedded interpreter");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    statusBar()->showMessage(tr("Python import test logged"), 2000);
}

void MainWindow::onDiagPythonMemoryTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Python Memory Test ===");
    logger.info("NOTE: Python memory test requires embedded interpreter");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    statusBar()->showMessage(tr("Python memory test logged"), 2000);
}

void MainWindow::onDiagEmbeddedInterpreterStatus() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Embedded Interpreter Status ===");
    logger.info("NOTE: Embedded interpreter check requires pybind11 integration");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    statusBar()->showMessage(tr("Interpreter status logged"), 2000);
}

void MainWindow::onDiagPerformanceBenchmark() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Performance Benchmark ===");
    logger.info("NOTE: Performance benchmark requires implementation");
    logger.info("This will test editor performance, rendering, file I/O, etc.");
    statusBar()->showMessage(tr("Performance benchmark logged"), 2000);
}

void MainWindow::onDiagRenderStats() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Render Statistics ===");
    logger.info("NOTE: Render statistics require Qt rendering metrics");
    logger.info("This diagnostic will show FPS, paint events, update regions, etc.");
    statusBar()->showMessage(tr("Render statistics logged"), 2000);
}

void MainWindow::onDiagClearLog() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Clear Log ===");

    // Clear log panel
    if (m_logPanel) {
        m_logPanel->clear();
        logger.info("Log Panel cleared");
    }

    statusBar()->showMessage(tr("Log cleared"), 2000);
}

#ifdef _DEBUG
void MainWindow::onDiagForceCrash() {
    auto& logger = core::Logger::getInstance();
    logger.critical("=== DIAGNOSTIC: Force Crash (Debug Only) ===");
    logger.critical("User requested application crash - SIMULATING CRITICAL ERROR");

    // Show confirmation
    QMessageBox::StandardButton reply = QMessageBox::critical(this,
        tr("Force Crash"),
        tr("This will IMMEDIATELY crash the application!\n\n"
           "All unsaved work will be LOST.\n\n"
           "Are you sure you want to continue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        logger.critical("Crashing application NOW!");
        std::abort();  // Immediate crash
    } else {
        logger.info("Crash cancelled by user");
        statusBar()->showMessage(tr("Crash cancelled"), 2000);
    }
}

void MainWindow::onDiagMemoryLeakTest() {
    auto& logger = core::Logger::getInstance();
    logger.warn("=== DIAGNOSTIC: Memory Leak Test (Debug Only) ===");

    // Intentionally leak memory for testing
    const size_t leakSize = 1024 * 1024;  // 1 MB
    char* leak = new char[leakSize];
    (void)leak;  // Suppress unused variable warning

    logger.warn("Leaked {} bytes of memory (intentional)", leakSize);
    logger.info("Use Valgrind/AddressSanitizer to detect this leak");
    statusBar()->showMessage(tr("Memory leak created (1 MB)"), 3000);
}
#endif

// =============================================================================
// Dev Tools Mode (Task #00020)
// =============================================================================

void MainWindow::enableDevMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Enabling dev mode");

    m_devMode = true;
    createDevToolsMenu();

    statusBar()->showMessage(tr("Dev mode enabled"), 3000);
}

void MainWindow::disableDevMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Disabling dev mode");

    m_devMode = false;
    removeDevToolsMenu();

    statusBar()->showMessage(tr("Dev mode disabled"), 3000);
}

void MainWindow::createDevToolsMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating Dev Tools menu");

    // Don't create if already exists
    if (m_devToolsMenu) {
        logger.warn("Dev Tools menu already exists");
        return;
    }

    // Create menu (inserted before Help menu)
    m_devToolsMenu = menuBar()->addMenu(tr("&Dev Tools"));

    // === Icon Downloader ===
    QAction* iconDownloaderAction = m_devToolsMenu->addAction(tr("Icon Downloader"));
    iconDownloaderAction->setToolTip(tr("Download Material Design icons for the project"));
    connect(iconDownloaderAction, &QAction::triggered, this, &MainWindow::onDevToolsIconDownloader);

    logger.info("Dev Tools menu created");
}

void MainWindow::removeDevToolsMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Removing Dev Tools menu");

    if (!m_devToolsMenu) {
        logger.debug("Dev Tools menu doesn't exist, nothing to remove");
        return;
    }

    // Remove from menu bar
    menuBar()->removeAction(m_devToolsMenu->menuAction());

    // Delete menu
    delete m_devToolsMenu;
    m_devToolsMenu = nullptr;

    logger.info("Dev Tools menu removed");
}

// =============================================================================
// Dev Tools Implementations (Task #00020)
// =============================================================================

void MainWindow::onDevToolsIconDownloader() {
    auto& logger = core::Logger::getInstance();
    logger.info("Opening Icon Downloader dialog");

    IconDownloaderDialog dialog(this);
    dialog.exec();

    logger.info("Icon Downloader dialog closed");
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);

    if (m_firstShow) {
        auto& logger = core::Logger::getInstance();
        logger.debug("Restoring window perspective");

        QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
        restoreGeometry(settings.value("geometry").toByteArray());
        restoreState(settings.value("windowState").toByteArray());

        // Task #00019: Restore toolbar state (visibility)
        if (m_toolbarManager) {
            m_toolbarManager->restoreState();
        }

        logger.debug("Window perspective restored");

        m_firstShow = false;
    }
}

} // namespace gui
} // namespace kalahari
