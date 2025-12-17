/// @file command_registrar.cpp
/// @brief Application command registration with CommandRegistry
///
/// This module is extracted from MainWindow to reduce its size (OpenSpec #00038).
/// Contains all command registrations for the application.

#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/command.h"
#include "kalahari/core/logger.h"

#include <QCoreApplication>
#include <QKeySequence>

namespace kalahari {
namespace gui {

// Helper function for tr() - must be in a QObject context
// We use QCoreApplication::translate() as a workaround
static QString tr(const char* sourceText) {
    return QCoreApplication::translate("CommandRegistrar", sourceText);
}

int registerAllCommands(const CommandCallbacks& callbacks) {
    auto& logger = core::Logger::getInstance();
    logger.debug("Registering commands with CommandRegistry");

    CommandRegistry& registry = CommandRegistry::getInstance();
    int count = 0;

    // =========================================================================
    // MACROS - Same as register_commands.hpp but adapted for this context
    // =========================================================================

    // Standard menu command (no toolbar, no shortcut)
    #define REG_CMD(id_, label_tr_, path_, order_, sep_, phase_) \
        do { \
            Command cmd; \
            cmd.id = id_; \
            cmd.label = tr(label_tr_).toStdString(); \
            cmd.tooltip = tr(label_tr_).toStdString(); \
            cmd.category = std::string(path_).substr(0, std::string(path_).find('/')); \
            cmd.menuPath = path_; \
            cmd.menuOrder = order_; \
            cmd.addSeparatorAfter = sep_; \
            cmd.phase = phase_; \
            cmd.showInMenu = true; \
            cmd.showInToolbar = false; \
            cmd.execute = []() {}; \
            registry.registerCommand(cmd); \
            count++; \
        } while(0)

    // Menu command with callback
    #define REG_CMD_CB(id_, label_tr_, path_, order_, sep_, phase_, callback_) \
        do { \
            Command cmd; \
            cmd.id = id_; \
            cmd.label = tr(label_tr_).toStdString(); \
            cmd.tooltip = tr(label_tr_).toStdString(); \
            cmd.category = std::string(path_).substr(0, std::string(path_).find('/')); \
            cmd.menuPath = path_; \
            cmd.menuOrder = order_; \
            cmd.addSeparatorAfter = sep_; \
            cmd.phase = phase_; \
            cmd.showInMenu = true; \
            cmd.showInToolbar = false; \
            cmd.execute = callback_; \
            registry.registerCommand(cmd); \
            count++; \
        } while(0)

    // Menu command with toolbar, shortcut, and icon
    #define REG_CMD_TOOL_ICON(id_, label_tr_, path_, order_, sep_, phase_, shortcut_, icon_, callback_) \
        do { \
            Command cmd; \
            cmd.id = id_; \
            cmd.label = tr(label_tr_).toStdString(); \
            cmd.tooltip = tr(label_tr_).toStdString(); \
            cmd.category = std::string(path_).substr(0, std::string(path_).find('/')); \
            cmd.menuPath = path_; \
            cmd.menuOrder = order_; \
            cmd.addSeparatorAfter = sep_; \
            cmd.phase = phase_; \
            cmd.showInMenu = true; \
            cmd.showInToolbar = true; \
            cmd.shortcut = shortcut_; \
            cmd.icons = icon_; \
            cmd.execute = callback_; \
            registry.registerCommand(cmd); \
            count++; \
        } while(0)

    // Menu command with shortcut (no toolbar)
    #define REG_CMD_KEY(id_, label_tr_, path_, order_, sep_, phase_, shortcut_) \
        do { \
            Command cmd; \
            cmd.id = id_; \
            cmd.label = tr(label_tr_).toStdString(); \
            cmd.tooltip = tr(label_tr_).toStdString(); \
            cmd.category = std::string(path_).substr(0, std::string(path_).find('/')); \
            cmd.menuPath = path_; \
            cmd.menuOrder = order_; \
            cmd.addSeparatorAfter = sep_; \
            cmd.phase = phase_; \
            cmd.showInMenu = true; \
            cmd.showInToolbar = false; \
            cmd.shortcut = shortcut_; \
            cmd.execute = []() {}; \
            registry.registerCommand(cmd); \
            count++; \
        } while(0)

    // =========================================================================
    // FILE MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("file.new", "New File", "FILE/New File", 10, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::New),
                      IconSet(),
                      callbacks.onNewDocument ? callbacks.onNewDocument : []() {});

    // OpenSpec #00033: New Book command (Ctrl+Shift+N)
    REG_CMD_TOOL_ICON("file.new.project", "New Book...", "FILE/New Book...", 15, false, 0,
                      KeyboardShortcut(Qt::Key_N, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onNewProject ? callbacks.onNewProject : []() {});

    REG_CMD_TOOL_ICON("file.open", "Open Book...", "FILE/Open Book...", 20, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Open),
                      IconSet(),
                      callbacks.onOpenDocument ? callbacks.onOpenDocument : []() {});

    // OpenSpec #00033 Phase F: Open standalone file (Ctrl+Shift+O)
    REG_CMD_TOOL_ICON("file.open.file", "Open File...", "FILE/Open/Open File...", 35, false, 0,
                      KeyboardShortcut(Qt::Key_O, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onOpenStandaloneFile ? callbacks.onOpenStandaloneFile : []() {});

    // Recent Books - dynamic submenu (registered separately)

    // OpenSpec #00030: Added Ctrl+W shortcut for Close Book
    REG_CMD_TOOL_ICON("file.close", "Close Book", "FILE/Close Book", 40, true, 1,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Close),
                      IconSet(),
                      []() {});

    REG_CMD_TOOL_ICON("file.save", "Save", "FILE/Save", 50, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Save),
                      IconSet(),
                      callbacks.onSaveDocument ? callbacks.onSaveDocument : []() {});

    REG_CMD_TOOL_ICON("file.saveAs", "Save As...", "FILE/Save As...", 60, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::SaveAs),
                      IconSet(),
                      callbacks.onSaveAsDocument ? callbacks.onSaveAsDocument : []() {});

    REG_CMD("file.saveAll", "Save All", "FILE/Save All", 70, true, 1);

    // Import submenu
    REG_CMD("file.import.docx", "DOCX Document...", "FILE/Import/DOCX Document...", 80, false, 1);
    REG_CMD("file.import.pdf", "PDF Reference...", "FILE/Import/PDF Reference...", 90, false, 2);
    REG_CMD("file.import.text", "Plain Text...", "FILE/Import/Plain Text...", 100, false, 1);
    REG_CMD("file.import.scrivener", "Scrivener Project...", "FILE/Import/Scrivener Project...", 110, false, 2);

    // Import Archive (priority 75 in Import submenu)
    REG_CMD_TOOL_ICON("file.import.archive", "Project Archive...", "FILE/Import/Project Archive...", 75, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onImportArchive ? callbacks.onImportArchive : []() {});

    // Export submenu
    REG_CMD("file.export.docx", "DOCX", "FILE/Export/DOCX", 120, false, 1);
    REG_CMD("file.export.pdf", "PDF", "FILE/Export/PDF", 130, false, 1);
    REG_CMD("file.export.markdown", "Markdown", "FILE/Export/Markdown", 140, true, 1);
    REG_CMD("file.export.epub", "EPUB", "FILE/Export/EPUB", 150, false, 2);
    REG_CMD("file.export.mobi", "MOBI", "FILE/Export/MOBI", 160, false, 2);
    REG_CMD("file.export.icml", "InDesign ICML", "FILE/Export/InDesign ICML", 170, false, 3);
    REG_CMD("file.export.latex", "LaTeX", "FILE/Export/LaTeX", 180, false, 3);
    REG_CMD("file.export.settings", "Export Settings...", "FILE/Export/Export Settings...", 190, true, 2);

    // Export Archive (priority 195 = end of Export submenu)
    REG_CMD_TOOL_ICON("file.export.archive", "Project Archive...", "FILE/Export/Project Archive...", 195, true, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onExportArchive ? callbacks.onExportArchive : []() {});

    REG_CMD_TOOL_ICON("file.exit", "Exit", "FILE/Exit", 200, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Quit),
                      IconSet(),
                      callbacks.onExit ? callbacks.onExit : []() {});

    // =========================================================================
    // EDIT MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("edit.undo", "Undo", "EDIT/Undo", 10, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Undo),
                      IconSet(),
                      callbacks.onUndo ? callbacks.onUndo : []() {});

    REG_CMD_TOOL_ICON("edit.redo", "Redo", "EDIT/Redo", 20, true, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Redo),
                      IconSet(),
                      callbacks.onRedo ? callbacks.onRedo : []() {});

    REG_CMD_TOOL_ICON("edit.cut", "Cut", "EDIT/Cut", 30, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Cut),
                      IconSet(),
                      callbacks.onCut ? callbacks.onCut : []() {});

    REG_CMD_TOOL_ICON("edit.copy", "Copy", "EDIT/Copy", 40, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Copy),
                      IconSet(),
                      callbacks.onCopy ? callbacks.onCopy : []() {});

    REG_CMD_TOOL_ICON("edit.paste", "Paste", "EDIT/Paste", 50, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Paste),
                      IconSet(),
                      callbacks.onPaste ? callbacks.onPaste : []() {});

    REG_CMD("edit.pasteSpecial", "Paste Special...", "EDIT/Paste Special...", 60, false, 1);
    REG_CMD("edit.delete", "Delete", "EDIT/Delete", 70, true, 1);

    REG_CMD_TOOL_ICON("edit.selectAll", "Select All", "EDIT/Select All", 80, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::SelectAll),
                      IconSet(),
                      callbacks.onSelectAll ? callbacks.onSelectAll : []() {});

    REG_CMD("edit.selectWord", "Select Word", "EDIT/Select Word", 90, false, 1);
    REG_CMD("edit.selectParagraph", "Select Paragraph", "EDIT/Select Paragraph", 100, true, 1);

    // OpenSpec #00030: Added keyboard shortcuts for Find operations
    REG_CMD_KEY("edit.find", "Find...", "EDIT/Find...", 110, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::Find));
    REG_CMD_KEY("edit.findNext", "Find Next", "EDIT/Find Next", 120, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::FindNext));
    REG_CMD_KEY("edit.findPrevious", "Find Previous", "EDIT/Find Previous", 130, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::FindPrevious));
    REG_CMD_KEY("edit.findReplace", "Find & Replace...", "EDIT/Find & Replace...", 140, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::Replace));
    REG_CMD("edit.findInBook", "Find in Book...", "EDIT/Find in Book...", 150, true, 1);

    REG_CMD_CB("edit.preferences", "Preferences...", "EDIT/Preferences...", 160, false, 0,
               callbacks.onSettings ? callbacks.onSettings : []() {});

    // OpenSpec #00037: edit.settings command (alias for Settings dialog, used in Quick Actions toolbar)
    REG_CMD_TOOL_ICON("edit.settings", "Settings...", "EDIT/Settings...", 165, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onSettings ? callbacks.onSettings : []() {});

    // =========================================================================
    // BOOK MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("book.newChapter", "New Chapter...", "BOOK/New Chapter...", 10, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      []() {});

    REG_CMD("book.newScene", "New Scene...", "BOOK/New Scene...", 20, true, 1);

    REG_CMD_TOOL_ICON("book.newCharacter", "New Character...", "BOOK/New Character...", 30, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      []() {});

    REG_CMD_TOOL_ICON("book.newLocation", "New Location...", "BOOK/New Location...", 40, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      []() {});

    REG_CMD("book.newItem", "New Item...", "BOOK/New Item...", 50, true, 1);

    REG_CMD("book.newMindMap", "New Mind Map...", "BOOK/New Mind Map...", 60, false, 1);
    REG_CMD("book.newTimeline", "New Timeline...", "BOOK/New Timeline...", 70, true, 1);

    REG_CMD("book.chapterBreak", "Chapter Break", "BOOK/Chapter Break", 80, false, 1);
    REG_CMD("book.sceneBreak", "Scene Break", "BOOK/Scene Break", 90, true, 1);

    REG_CMD_TOOL_ICON("book.properties", "Book Properties...", "BOOK/Book Properties...", 100, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
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

    // OpenSpec #00030: Added standard formatting shortcuts (Ctrl+B/I/U)
    REG_CMD_KEY("format.bold", "Bold", "FORMAT/Bold", 100, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::Bold));
    REG_CMD_KEY("format.italic", "Italic", "FORMAT/Italic", 110, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::Italic));
    REG_CMD_KEY("format.underline", "Underline", "FORMAT/Underline", 120, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::Underline));
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
                      IconSet(),
                      []() {});

    REG_CMD_TOOL_ICON("tools.spellcheck", "Spellchecker", "TOOLS/Spellchecker", 40, false, 2,
                      KeyboardShortcut(),
                      IconSet(),
                      []() {});

    REG_CMD("tools.grammar", "Grammar Check", "TOOLS/Grammar Check", 50, false, 2);
    REG_CMD("tools.readability", "Readability Score", "TOOLS/Readability Score", 60, true, 2);

    // Focus Mode submenu
    REG_CMD_TOOL_ICON("tools.focus.normal", "Normal", "TOOLS/Focus Mode/Normal", 70, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
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
    REG_CMD("tools.collaboration", "Collaboration...", "TOOLS/Collaboration...", 200, true, 3);

    // OpenSpec #00037: Toolbar Manager command (used in Quick Actions toolbar)
    REG_CMD_TOOL_ICON("tools.toolbarManager", "Customize Toolbars...", "TOOLS/Customize Toolbars...", 210, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onToolbarManager ? callbacks.onToolbarManager : []() {});

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

    // Dashboard command - shows/activates Dashboard tab (OpenSpec #00036 Phase D)
    REG_CMD_CB("view.dashboard", "Dashboard", "VIEW/Dashboard", 5, true, 0,
               callbacks.onDashboard ? callbacks.onDashboard : []() {});

    // Panel toggle commands - registered here for CommandRegistry/Toolbar system
    // Execute callbacks are set later in createDocks() after dock widgets exist
    REG_CMD_KEY("view.navigator", "Navigator", "VIEW/Panels/Navigator", 10, false, 0,
                KeyboardShortcut(Qt::Key_F2, Qt::NoModifier));

    REG_CMD_KEY("view.properties", "Properties", "VIEW/Panels/Properties", 20, false, 0,
                KeyboardShortcut(Qt::Key_F3, Qt::NoModifier));

    REG_CMD_KEY("view.log", "Log", "VIEW/Panels/Log", 30, false, 0,
                KeyboardShortcut(Qt::Key_F4, Qt::NoModifier));

    REG_CMD_KEY("view.search", "Search", "VIEW/Panels/Search", 40, false, 0,
                KeyboardShortcut(Qt::Key_F5, Qt::NoModifier));

    REG_CMD_KEY("view.assistant", "Assistant", "VIEW/Panels/Assistant", 50, false, 0,
                KeyboardShortcut(Qt::Key_F6, Qt::NoModifier));

    // Perspectives submenu
    REG_CMD("view.perspectives.writer", "Writer", "VIEW/Perspectives/Writer", 70, false, 1);
    REG_CMD("view.perspectives.editor", "Editor", "VIEW/Perspectives/Editor", 80, false, 1);
    REG_CMD("view.perspectives.researcher", "Researcher", "VIEW/Perspectives/Researcher", 90, false, 1);
    REG_CMD("view.perspectives.planner", "Planner", "VIEW/Perspectives/Planner", 100, true, 1);
    REG_CMD("view.perspectives.save", "Save Current Perspective...", "VIEW/Perspectives/Save Current Perspective...", 110, false, 1);
    REG_CMD("view.perspectives.manage", "Manage Perspectives...", "VIEW/Perspectives/Manage Perspectives...", 120, false, 1);

    // OpenSpec #00030: VIEW/Toolbars submenu is created DYNAMICALLY
    // by ToolbarManager::createViewMenuActions() - no static commands here.
    // Toolbar toggle actions + "Toolbar Manager..." are all dynamic.

    REG_CMD("view.showStatusBar", "Show Status Bar", "VIEW/Show Status Bar", 180, false, 0);
    REG_CMD("view.showStatsBar", "Show Statistics Bar", "VIEW/Show Statistics Bar", 190, false, 1);
    REG_CMD("view.showFormattingMarks", "Show Formatting Marks", "VIEW/Show Formatting Marks", 210, true, 1);

    REG_CMD("view.zoomIn", "Zoom In", "VIEW/Zoom In", 220, false, 1);
    REG_CMD("view.zoomOut", "Zoom Out", "VIEW/Zoom Out", 230, false, 1);
    REG_CMD("view.resetZoom", "Reset Zoom", "VIEW/Reset Zoom", 240, true, 1);

    // OpenSpec #00030: F11 for Full Screen (standard)
    REG_CMD_KEY("view.fullScreen", "Full Screen", "VIEW/Full Screen", 250, true, 0,
                KeyboardShortcut::fromQKeySequence(QKeySequence::FullScreen));

    REG_CMD_CB("view.resetLayout", "Reset Layout", "VIEW/Reset Layout", 260, false, 0,
               callbacks.onResetLayout ? callbacks.onResetLayout : []() {});

    // =========================================================================
    // HELP MENU
    // =========================================================================

    // OpenSpec #00030: F1 for Help (standard)
    REG_CMD_KEY("help.manual", "Kalahari Help", "HELP/Kalahari Help", 10, false, 2,
                KeyboardShortcut::fromQKeySequence(QKeySequence::HelpContents));
    REG_CMD("help.tutorial", "Getting Started Tutorial", "HELP/Getting Started Tutorial", 20, true, 2);

    REG_CMD("help.shortcuts", "Keyboard Shortcuts", "HELP/Keyboard Shortcuts", 30, false, 1);
    REG_CMD("help.tipsTricks", "Tips & Tricks", "HELP/Tips & Tricks", 40, false, 2);
    REG_CMD("help.whatsNew", "What's New", "HELP/What's New", 50, true, 1);

    REG_CMD("help.reportBug", "Report a Bug...", "HELP/Report a Bug...", 60, false, 1);
    REG_CMD("help.suggestFeature", "Suggest a Feature...", "HELP/Suggest a Feature...", 70, false, 1);
    REG_CMD("help.communityForum", "Community Forum", "HELP/Community Forum", 80, true, 2);

    REG_CMD("help.checkUpdates", "Check for Updates...", "HELP/Check for Updates...", 90, true, 2);

    REG_CMD_CB("help.about", "About Kalahari", "HELP/About Kalahari", 100, false, 0,
               callbacks.onAbout ? callbacks.onAbout : []() {});

    // =========================================================================
    // Cleanup macros
    // =========================================================================
    #undef REG_CMD
    #undef REG_CMD_CB
    #undef REG_CMD_TOOL_ICON
    #undef REG_CMD_KEY

    logger.debug("Commands registered successfully ({} commands)", count);
    return count;
}

} // namespace gui
} // namespace kalahari
