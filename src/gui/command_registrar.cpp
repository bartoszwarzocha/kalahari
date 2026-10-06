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
// We use QCoreApplication::translate() as a workaround.
// lupdate cannot see a label passed through a macro parameter, so every call site
// marks its label with QT_TRANSLATE_NOOP("CommandRegistrar", ...).
static QString tr(const char* sourceText) {
    return QCoreApplication::translate("CommandRegistrar", sourceText);
}

int registerAllCommands(const CommandCallbacks& callbacks) {
    auto& logger = core::Logger::getInstance();
    logger.debug("Registering commands with CommandRegistry");

    CommandRegistry& registry = CommandRegistry::getInstance();
    int count = 0;

    // =========================================================================
    // MACROS
    // =========================================================================

    // Standard menu command (no toolbar, no shortcut).
    // No execute callback: the feature is not implemented yet, so its action is disabled.
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

    // Menu command with shortcut (no toolbar). No execute callback, see REG_CMD.
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
            registry.registerCommand(cmd); \
            count++; \
        } while(0)

    // Menu command with shortcut and callback (no toolbar)
    #define REG_CMD_KEY_CB(id_, label_tr_, path_, order_, sep_, phase_, shortcut_, callback_) \
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
            cmd.execute = callback_; \
            registry.registerCommand(cmd); \
            count++; \
        } while(0)

    // =========================================================================
    // FILE MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("file.new", QT_TRANSLATE_NOOP("CommandRegistrar", "New File"), "FILE/New File", 10, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::New),
                      IconSet(),
                      callbacks.onNewDocument);

    // OpenSpec #00033: New Book command (Ctrl+Shift+N)
    REG_CMD_TOOL_ICON("file.new.project", QT_TRANSLATE_NOOP("CommandRegistrar", "New Book..."), "FILE/New Book...", 15, false, 0,
                      KeyboardShortcut(Qt::Key_N, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onNewProject);

    REG_CMD_TOOL_ICON("file.open", QT_TRANSLATE_NOOP("CommandRegistrar", "Open Book..."), "FILE/Open Book...", 20, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Open),
                      IconSet(),
                      callbacks.onOpenDocument);

    // OpenSpec #00033 Phase F: Open standalone file (Ctrl+Shift+O)
    REG_CMD_TOOL_ICON("file.open.file", QT_TRANSLATE_NOOP("CommandRegistrar", "Open File..."), "FILE/Open/Open File...", 35, false, 0,
                      KeyboardShortcut(Qt::Key_O, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onOpenStandaloneFile);

    // Recent Books - dynamic submenu (registered separately)

    // OpenSpec #00030: Added Ctrl+W shortcut for Close Book
    REG_CMD_TOOL_ICON("file.close", QT_TRANSLATE_NOOP("CommandRegistrar", "Close Book"), "FILE/Close Book", 40, true, 1,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Close),
                      IconSet(),
                      callbacks.onCloseDocument);

    REG_CMD_TOOL_ICON("file.save", QT_TRANSLATE_NOOP("CommandRegistrar", "Save"), "FILE/Save", 50, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Save),
                      IconSet(),
                      callbacks.onSaveDocument);

    REG_CMD_TOOL_ICON("file.saveAs", QT_TRANSLATE_NOOP("CommandRegistrar", "Save As..."), "FILE/Save As...", 60, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::SaveAs),
                      IconSet(),
                      callbacks.onSaveAsDocument);

    REG_CMD("file.saveAll", QT_TRANSLATE_NOOP("CommandRegistrar", "Save All"), "FILE/Save All", 70, true, 1);

    // Import submenu
    REG_CMD("file.import.docx", QT_TRANSLATE_NOOP("CommandRegistrar", "DOCX Document..."), "FILE/Import/DOCX Document...", 80, false, 1);
    REG_CMD("file.import.pdf", QT_TRANSLATE_NOOP("CommandRegistrar", "PDF Reference..."), "FILE/Import/PDF Reference...", 90, false, 2);
    REG_CMD("file.import.text", QT_TRANSLATE_NOOP("CommandRegistrar", "Plain Text..."), "FILE/Import/Plain Text...", 100, false, 1);
    REG_CMD("file.import.scrivener", QT_TRANSLATE_NOOP("CommandRegistrar", "Scrivener Project..."), "FILE/Import/Scrivener Project...", 110, false, 2);

    // Import Archive (priority 75 in Import submenu)
    REG_CMD_TOOL_ICON("file.import.archive", QT_TRANSLATE_NOOP("CommandRegistrar", "Project Archive..."), "FILE/Import/Project Archive...", 75, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onImportArchive);

    // Export submenu
    REG_CMD("file.export.docx", QT_TRANSLATE_NOOP("CommandRegistrar", "DOCX"), "FILE/Export/DOCX", 120, false, 1);
    REG_CMD("file.export.pdf", QT_TRANSLATE_NOOP("CommandRegistrar", "PDF"), "FILE/Export/PDF", 130, false, 1);
    REG_CMD("file.export.markdown", QT_TRANSLATE_NOOP("CommandRegistrar", "Markdown"), "FILE/Export/Markdown", 140, true, 1);
    REG_CMD("file.export.epub", QT_TRANSLATE_NOOP("CommandRegistrar", "EPUB"), "FILE/Export/EPUB", 150, false, 2);
    REG_CMD("file.export.mobi", QT_TRANSLATE_NOOP("CommandRegistrar", "MOBI"), "FILE/Export/MOBI", 160, false, 2);
    REG_CMD("file.export.icml", QT_TRANSLATE_NOOP("CommandRegistrar", "InDesign ICML"), "FILE/Export/InDesign ICML", 170, false, 3);
    REG_CMD("file.export.latex", QT_TRANSLATE_NOOP("CommandRegistrar", "LaTeX"), "FILE/Export/LaTeX", 180, false, 3);
    REG_CMD("file.export.settings", QT_TRANSLATE_NOOP("CommandRegistrar", "Export Settings..."), "FILE/Export/Export Settings...", 190, true, 2);

    // Export Archive (priority 195 = end of Export submenu)
    REG_CMD_TOOL_ICON("file.export.archive", QT_TRANSLATE_NOOP("CommandRegistrar", "Project Archive..."), "FILE/Export/Project Archive...", 195, true, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onExportArchive);

    REG_CMD_TOOL_ICON("file.exit", QT_TRANSLATE_NOOP("CommandRegistrar", "Exit"), "FILE/Exit", 200, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Quit),
                      IconSet(),
                      callbacks.onExit);

    // =========================================================================
    // EDIT MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("edit.undo", QT_TRANSLATE_NOOP("CommandRegistrar", "Undo"), "EDIT/Undo", 10, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Undo),
                      IconSet(),
                      callbacks.onUndo);

    REG_CMD_TOOL_ICON("edit.redo", QT_TRANSLATE_NOOP("CommandRegistrar", "Redo"), "EDIT/Redo", 20, true, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Redo),
                      IconSet(),
                      callbacks.onRedo);

    REG_CMD_TOOL_ICON("edit.cut", QT_TRANSLATE_NOOP("CommandRegistrar", "Cut"), "EDIT/Cut", 30, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Cut),
                      IconSet(),
                      callbacks.onCut);

    REG_CMD_TOOL_ICON("edit.copy", QT_TRANSLATE_NOOP("CommandRegistrar", "Copy"), "EDIT/Copy", 40, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Copy),
                      IconSet(),
                      callbacks.onCopy);

    REG_CMD_TOOL_ICON("edit.paste", QT_TRANSLATE_NOOP("CommandRegistrar", "Paste"), "EDIT/Paste", 50, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Paste),
                      IconSet(),
                      callbacks.onPaste);

    REG_CMD("edit.pasteSpecial", QT_TRANSLATE_NOOP("CommandRegistrar", "Paste Special..."), "EDIT/Paste Special...", 60, false, 1);
    REG_CMD("edit.delete", QT_TRANSLATE_NOOP("CommandRegistrar", "Delete"), "EDIT/Delete", 70, true, 1);

    REG_CMD_TOOL_ICON("edit.selectAll", QT_TRANSLATE_NOOP("CommandRegistrar", "Select All"), "EDIT/Select All", 80, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::SelectAll),
                      IconSet(),
                      callbacks.onSelectAll);

    REG_CMD("edit.selectWord", QT_TRANSLATE_NOOP("CommandRegistrar", "Select Word"), "EDIT/Select Word", 90, false, 1);
    REG_CMD("edit.selectParagraph", QT_TRANSLATE_NOOP("CommandRegistrar", "Select Paragraph"), "EDIT/Select Paragraph", 100, true, 1);

    // OpenSpec #00030: Added keyboard shortcuts for Find operations
    REG_CMD_KEY_CB("edit.find", QT_TRANSLATE_NOOP("CommandRegistrar", "Find..."), "EDIT/Find...", 110, false, 1,
                   KeyboardShortcut::fromQKeySequence(QKeySequence::Find), callbacks.onFind);
    REG_CMD_KEY_CB("edit.findNext", QT_TRANSLATE_NOOP("CommandRegistrar", "Find Next"), "EDIT/Find Next", 120, false, 1,
                   KeyboardShortcut::fromQKeySequence(QKeySequence::FindNext),
                   callbacks.onFindNext);
    REG_CMD_KEY_CB("edit.findPrevious", QT_TRANSLATE_NOOP("CommandRegistrar", "Find Previous"), "EDIT/Find Previous", 130, false, 1,
                   KeyboardShortcut::fromQKeySequence(QKeySequence::FindPrevious),
                   callbacks.onFindPrevious);
    REG_CMD_KEY_CB("edit.findReplace", QT_TRANSLATE_NOOP("CommandRegistrar", "Find & Replace..."), "EDIT/Find & Replace...", 140, false, 1,
                   KeyboardShortcut::fromQKeySequence(QKeySequence::Replace),
                   callbacks.onFindReplace);
    REG_CMD("edit.findInBook", QT_TRANSLATE_NOOP("CommandRegistrar", "Find in Book..."), "EDIT/Find in Book...", 150, true, 1);

    REG_CMD_CB("edit.preferences", QT_TRANSLATE_NOOP("CommandRegistrar", "Preferences..."), "EDIT/Preferences...", 160, false, 0,
               callbacks.onSettings);

    // OpenSpec #00037: edit.settings command (alias for Settings dialog, used in Quick Actions toolbar)
    REG_CMD_TOOL_ICON("edit.settings", QT_TRANSLATE_NOOP("CommandRegistrar", "Settings..."), "EDIT/Settings...", 165, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onSettings);

    // =========================================================================
    // BOOK MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("book.newChapter", QT_TRANSLATE_NOOP("CommandRegistrar", "New Chapter..."), "BOOK/New Chapter...", 10, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      nullptr);

    REG_CMD("book.newScene", QT_TRANSLATE_NOOP("CommandRegistrar", "New Scene..."), "BOOK/New Scene...", 20, true, 1);

    REG_CMD_TOOL_ICON("book.newCharacter", QT_TRANSLATE_NOOP("CommandRegistrar", "New Character..."), "BOOK/New Character...", 30, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      nullptr);

    REG_CMD_TOOL_ICON("book.newLocation", QT_TRANSLATE_NOOP("CommandRegistrar", "New Location..."), "BOOK/New Location...", 40, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      nullptr);

    REG_CMD("book.newItem", QT_TRANSLATE_NOOP("CommandRegistrar", "New Item..."), "BOOK/New Item...", 50, true, 1);

    REG_CMD("book.newMindMap", QT_TRANSLATE_NOOP("CommandRegistrar", "New Mind Map..."), "BOOK/New Mind Map...", 60, false, 1);
    REG_CMD("book.newTimeline", QT_TRANSLATE_NOOP("CommandRegistrar", "New Timeline..."), "BOOK/New Timeline...", 70, true, 1);

    REG_CMD("book.chapterBreak", QT_TRANSLATE_NOOP("CommandRegistrar", "Chapter Break"), "BOOK/Chapter Break", 80, false, 1);
    REG_CMD("book.sceneBreak", QT_TRANSLATE_NOOP("CommandRegistrar", "Scene Break"), "BOOK/Scene Break", 90, true, 1);

    REG_CMD_TOOL_ICON("book.properties", QT_TRANSLATE_NOOP("CommandRegistrar", "Book Properties..."), "BOOK/Book Properties...", 100, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      nullptr);

    // =========================================================================
    // INSERT MENU
    // =========================================================================

    REG_CMD("insert.image", QT_TRANSLATE_NOOP("CommandRegistrar", "Image..."), "INSERT/Image...", 10, false, 1);
    REG_CMD("insert.table", QT_TRANSLATE_NOOP("CommandRegistrar", "Table..."), "INSERT/Table...", 20, false, 1);
    REG_CMD("insert.link", QT_TRANSLATE_NOOP("CommandRegistrar", "Link..."), "INSERT/Link...", 30, true, 1);

    REG_CMD("insert.footnote", QT_TRANSLATE_NOOP("CommandRegistrar", "Footnote"), "INSERT/Footnote", 40, false, 1);
    REG_CMD("insert.endnote", QT_TRANSLATE_NOOP("CommandRegistrar", "Endnote"), "INSERT/Endnote", 50, false, 1);

    // OpenSpec #00042 Phase 7.9: Insert Comment - DISABLED (backend stubs only)
    // REG_CMD_TOOL_ICON("insert.comment", QT_TRANSLATE_NOOP("CommandRegistrar", "Comment"), "INSERT/Comment", 60, false, 0,
    //                   KeyboardShortcut(Qt::Key_C, Qt::ControlModifier | Qt::AltModifier),
    //                   IconSet(),
    //                   callbacks.onInsertComment);

    REG_CMD("insert.annotation", QT_TRANSLATE_NOOP("CommandRegistrar", "Annotation"), "INSERT/Annotation", 70, true, 1);

    REG_CMD("insert.specialChar", QT_TRANSLATE_NOOP("CommandRegistrar", "Special Character..."), "INSERT/Special Character...", 80, false, 1);
    REG_CMD("insert.dateTime", QT_TRANSLATE_NOOP("CommandRegistrar", "Date & Time"), "INSERT/Date & Time", 90, false, 1);
    REG_CMD("insert.field", QT_TRANSLATE_NOOP("CommandRegistrar", "Field..."), "INSERT/Field...", 100, false, 1);

    // =========================================================================
    // FORMAT MENU
    // =========================================================================

    REG_CMD("format.font", QT_TRANSLATE_NOOP("CommandRegistrar", "Font..."), "FORMAT/Font...", 10, false, 1);
    REG_CMD("format.paragraph", QT_TRANSLATE_NOOP("CommandRegistrar", "Paragraph..."), "FORMAT/Paragraph...", 20, true, 1);

    // Text Style submenu
    REG_CMD("format.style.heading1", QT_TRANSLATE_NOOP("CommandRegistrar", "Heading 1"), "FORMAT/Text Style/Heading 1", 30, false, 1);
    REG_CMD("format.style.heading2", QT_TRANSLATE_NOOP("CommandRegistrar", "Heading 2"), "FORMAT/Text Style/Heading 2", 40, false, 1);
    REG_CMD("format.style.heading3", QT_TRANSLATE_NOOP("CommandRegistrar", "Heading 3"), "FORMAT/Text Style/Heading 3", 50, false, 1);
    REG_CMD("format.style.body", QT_TRANSLATE_NOOP("CommandRegistrar", "Body Text"), "FORMAT/Text Style/Body Text", 60, false, 1);
    REG_CMD("format.style.quote", QT_TRANSLATE_NOOP("CommandRegistrar", "Quote"), "FORMAT/Text Style/Quote", 70, false, 1);
    REG_CMD("format.style.code", QT_TRANSLATE_NOOP("CommandRegistrar", "Code"), "FORMAT/Text Style/Code", 80, true, 1);
    REG_CMD("format.style.manage", QT_TRANSLATE_NOOP("CommandRegistrar", "Manage Styles..."), "FORMAT/Text Style/Manage Styles...", 90, false, 1);

    // OpenSpec #00030: Added standard formatting shortcuts (Ctrl+B/I/U)
    // OpenSpec #00042 Phase 7.2: Connected to toolbar and editor
    REG_CMD_TOOL_ICON("format.bold", QT_TRANSLATE_NOOP("CommandRegistrar", "Bold"), "FORMAT/Bold", 100, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Bold),
                      IconSet(),
                      callbacks.onFormatBold);
    REG_CMD_TOOL_ICON("format.italic", QT_TRANSLATE_NOOP("CommandRegistrar", "Italic"), "FORMAT/Italic", 110, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Italic),
                      IconSet(),
                      callbacks.onFormatItalic);
    REG_CMD_TOOL_ICON("format.underline", QT_TRANSLATE_NOOP("CommandRegistrar", "Underline"), "FORMAT/Underline", 120, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Underline),
                      IconSet(),
                      callbacks.onFormatUnderline);
    REG_CMD_TOOL_ICON("format.strikethrough", QT_TRANSLATE_NOOP("CommandRegistrar", "Strikethrough"), "FORMAT/Strikethrough", 130, true, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onFormatStrikethrough);

    REG_CMD_TOOL_ICON("format.alignLeft", QT_TRANSLATE_NOOP("CommandRegistrar", "Align Left"), "FORMAT/Align Left", 140, false, 0,
                      KeyboardShortcut::fromString("Ctrl+L"),
                      IconSet(),
                      callbacks.onAlignLeft);
    REG_CMD_TOOL_ICON("format.alignCenter", QT_TRANSLATE_NOOP("CommandRegistrar", "Align Center"), "FORMAT/Align Center", 150, false, 0,
                      KeyboardShortcut::fromString("Ctrl+E"),
                      IconSet(),
                      callbacks.onAlignCenter);
    REG_CMD_TOOL_ICON("format.alignRight", QT_TRANSLATE_NOOP("CommandRegistrar", "Align Right"), "FORMAT/Align Right", 160, false, 0,
                      KeyboardShortcut::fromString("Ctrl+R"),
                      IconSet(),
                      callbacks.onAlignRight);
    REG_CMD_TOOL_ICON("format.justify", QT_TRANSLATE_NOOP("CommandRegistrar", "Justify"), "FORMAT/Justify", 170, true, 0,
                      KeyboardShortcut::fromString("Ctrl+J"),
                      IconSet(),
                      callbacks.onAlignJustify);

    REG_CMD("format.increaseIndent", QT_TRANSLATE_NOOP("CommandRegistrar", "Increase Indent"), "FORMAT/Increase Indent", 180, false, 1);
    REG_CMD("format.decreaseIndent", QT_TRANSLATE_NOOP("CommandRegistrar", "Decrease Indent"), "FORMAT/Decrease Indent", 190, true, 1);

    REG_CMD("format.bullets", QT_TRANSLATE_NOOP("CommandRegistrar", "Bullets"), "FORMAT/Bullets", 200, false, 1);
    REG_CMD("format.numbering", QT_TRANSLATE_NOOP("CommandRegistrar", "Numbering"), "FORMAT/Numbering", 210, true, 1);

    REG_CMD("format.color", QT_TRANSLATE_NOOP("CommandRegistrar", "Color"), "FORMAT/Color", 220, true, 1);

    REG_CMD("format.clearFormatting", QT_TRANSLATE_NOOP("CommandRegistrar", "Clear Formatting"), "FORMAT/Clear Formatting", 230, false, 1);

    // =========================================================================
    // TOOLS MENU
    // =========================================================================

    // Statistics submenu (analysis tools only, panels in VIEW)
    REG_CMD("tools.stats.full", QT_TRANSLATE_NOOP("CommandRegistrar", "Full Statistics..."), "TOOLS/Statistics/Full Statistics...", 10, false, 2);

    REG_CMD_TOOL_ICON("tools.stats.wordCount", QT_TRANSLATE_NOOP("CommandRegistrar", "Word Count"), "TOOLS/Statistics/Word Count", 20, true, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      nullptr);

    REG_CMD_TOOL_ICON("tools.spellcheck", QT_TRANSLATE_NOOP("CommandRegistrar", "Spellchecker"), "TOOLS/Spellchecker", 40, false, 2,
                      KeyboardShortcut(),
                      IconSet(),
                      nullptr);

    REG_CMD("tools.grammar", QT_TRANSLATE_NOOP("CommandRegistrar", "Grammar Check"), "TOOLS/Grammar Check", 50, false, 2);
    REG_CMD("tools.readability", QT_TRANSLATE_NOOP("CommandRegistrar", "Readability Score"), "TOOLS/Readability Score", 60, true, 2);

    // Focus Mode submenu
    REG_CMD_TOOL_ICON("tools.focus.normal", QT_TRANSLATE_NOOP("CommandRegistrar", "Normal"), "TOOLS/Focus Mode/Normal", 70, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      nullptr);

    REG_CMD("tools.focus.focused", QT_TRANSLATE_NOOP("CommandRegistrar", "Focused"), "TOOLS/Focus Mode/Focused", 80, false, 1);
    REG_CMD("tools.focus.distractionFree", QT_TRANSLATE_NOOP("CommandRegistrar", "Distraction-Free"), "TOOLS/Focus Mode/Distraction-Free", 90, false, 1);

    REG_CMD("tools.backupNow", QT_TRANSLATE_NOOP("CommandRegistrar", "Backup Now"), "TOOLS/Backup Now", 100, false, 2);
    REG_CMD("tools.autoSaveSettings", QT_TRANSLATE_NOOP("CommandRegistrar", "Auto-Save Settings..."), "TOOLS/Auto-Save Settings...", 110, false, 1);
    REG_CMD("tools.versionHistory", QT_TRANSLATE_NOOP("CommandRegistrar", "Version History..."), "TOOLS/Version History...", 120, true, 2);

    // Plugins submenu
    REG_CMD("tools.plugins.manager", QT_TRANSLATE_NOOP("CommandRegistrar", "Plugin Manager..."), "TOOLS/Plugins/Plugin Manager...", 130, false, 2);
    REG_CMD("tools.plugins.updates", QT_TRANSLATE_NOOP("CommandRegistrar", "Check for Updates..."), "TOOLS/Plugins/Check for Updates...", 150, true, 2);
    REG_CMD("tools.plugins.reload", QT_TRANSLATE_NOOP("CommandRegistrar", "Reload Plugins"), "TOOLS/Plugins/Reload Plugins", 160, false, 2);

    REG_CMD("tools.challenges", QT_TRANSLATE_NOOP("CommandRegistrar", "Challenges & Badges..."), "TOOLS/Challenges & Badges...", 170, false, 2);
    REG_CMD("tools.writingGoals", QT_TRANSLATE_NOOP("CommandRegistrar", "Writing Goals & Deadlines..."), "TOOLS/Writing Goals & Deadlines...", 180, true, 2);

    // OpenSpec #00037: Toolbar Manager command (used in Quick Actions toolbar)
    REG_CMD_TOOL_ICON("tools.toolbarManager", QT_TRANSLATE_NOOP("CommandRegistrar", "Customize Toolbars..."), "TOOLS/Customize Toolbars...", 210, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onToolbarManager);

    // =========================================================================
    // ASSISTANT MENU
    // =========================================================================

    REG_CMD("assistant.ask", QT_TRANSLATE_NOOP("CommandRegistrar", "Ask Assistant..."), "ASSISTANT/Ask Assistant...", 10, true, 2);

    REG_CMD("assistant.switch", QT_TRANSLATE_NOOP("CommandRegistrar", "Switch Assistant..."), "ASSISTANT/Switch Assistant...", 20, true, 2);

    // Assistant Actions submenu
    REG_CMD("assistant.action.grammar", QT_TRANSLATE_NOOP("CommandRegistrar", "Check Grammar"), "ASSISTANT/Assistant Actions/Check Grammar", 30, false, 2);
    REG_CMD("assistant.action.style", QT_TRANSLATE_NOOP("CommandRegistrar", "Improve Style"), "ASSISTANT/Assistant Actions/Improve Style", 40, false, 2);
    REG_CMD("assistant.action.plot", QT_TRANSLATE_NOOP("CommandRegistrar", "Analyze Plot"), "ASSISTANT/Assistant Actions/Analyze Plot", 50, false, 2);
    REG_CMD("assistant.action.research", QT_TRANSLATE_NOOP("CommandRegistrar", "Research Topic..."), "ASSISTANT/Assistant Actions/Research Topic...", 60, false, 2);
    REG_CMD("assistant.action.speedDraft", QT_TRANSLATE_NOOP("CommandRegistrar", "Speed Draft Mode"), "ASSISTANT/Assistant Actions/Speed Draft Mode", 70, false, 2);

    REG_CMD("assistant.settings", QT_TRANSLATE_NOOP("CommandRegistrar", "Assistant Settings..."), "ASSISTANT/Assistant Settings...", 80, false, 2);

    // =========================================================================
    // VIEW MENU
    // =========================================================================

    // Dashboard command - shows/activates Dashboard tab (OpenSpec #00036 Phase D)
    REG_CMD_CB("view.dashboard", QT_TRANSLATE_NOOP("CommandRegistrar", "Dashboard"), "VIEW/Dashboard", 5, true, 0,
               callbacks.onDashboard);

    // Panel toggle commands - registered here for CommandRegistry/Toolbar system
    // Execute callbacks are set later in createDocks() after dock widgets exist
    REG_CMD_KEY("view.navigator", QT_TRANSLATE_NOOP("CommandRegistrar", "Navigator"), "VIEW/Panels/Navigator", 10, false, 0,
                KeyboardShortcut(Qt::Key_F2, Qt::NoModifier));

    // F8, not F3: F3 is Find Next (edit.findNext)
    REG_CMD_KEY("view.properties", QT_TRANSLATE_NOOP("CommandRegistrar", "Properties"), "VIEW/Panels/Properties", 20, false, 0,
                KeyboardShortcut(Qt::Key_F8, Qt::NoModifier));

    REG_CMD_KEY("view.log", QT_TRANSLATE_NOOP("CommandRegistrar", "Log"), "VIEW/Panels/Log", 30, false, 0,
                KeyboardShortcut(Qt::Key_F4, Qt::NoModifier));

    REG_CMD_KEY("view.search", QT_TRANSLATE_NOOP("CommandRegistrar", "Search"), "VIEW/Panels/Search", 40, false, 0,
                KeyboardShortcut(Qt::Key_F5, Qt::NoModifier));

    REG_CMD_KEY("view.assistant", QT_TRANSLATE_NOOP("CommandRegistrar", "Assistant"), "VIEW/Panels/Assistant", 50, false, 0,
                KeyboardShortcut(Qt::Key_F6, Qt::NoModifier));

    // View Mode submenu (OpenSpec #00042 Phase 7.3)
    // View modes for BookEditor - radio group (only one can be active)
    REG_CMD_TOOL_ICON("view.mode.continuous", QT_TRANSLATE_NOOP("CommandRegistrar", "Continuous"), "VIEW/View Mode/Continuous", 55, false, 0,
                      KeyboardShortcut(Qt::Key_1, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onViewModeContinuous);
    REG_CMD_TOOL_ICON("view.mode.page", QT_TRANSLATE_NOOP("CommandRegistrar", "Page Layout"), "VIEW/View Mode/Page Layout", 56, false, 0,
                      KeyboardShortcut(Qt::Key_2, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onViewModePage);
    REG_CMD_TOOL_ICON("view.mode.focus", QT_TRANSLATE_NOOP("CommandRegistrar", "Focus"), "VIEW/View Mode/Focus", 58, false, 0,
                      KeyboardShortcut(Qt::Key_4, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onViewModeFocus);
    REG_CMD_TOOL_ICON("view.mode.distraction-free", QT_TRANSLATE_NOOP("CommandRegistrar", "Distraction-Free"), "VIEW/View Mode/Distraction-Free", 59, true, 0,
                      KeyboardShortcut(Qt::Key_F11, Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onViewModeDistFree);

    // Typewriter scrolling: a toggle on top of the view mode (the cursor line stays at a
    // fixed height of the view)
    REG_CMD_KEY_CB("view.typewriter", QT_TRANSLATE_NOOP("CommandRegistrar", "Typewriter Scrolling"), "VIEW/Typewriter Scrolling", 60, true, 0,
                   KeyboardShortcut(Qt::Key_3, Qt::ControlModifier),
                   callbacks.onTypewriterToggle);

    // Perspectives submenu
    REG_CMD("view.perspectives.writer", QT_TRANSLATE_NOOP("CommandRegistrar", "Writer"), "VIEW/Perspectives/Writer", 70, false, 1);
    REG_CMD("view.perspectives.editor", QT_TRANSLATE_NOOP("CommandRegistrar", "Editor"), "VIEW/Perspectives/Editor", 80, false, 1);
    REG_CMD("view.perspectives.researcher", QT_TRANSLATE_NOOP("CommandRegistrar", "Researcher"), "VIEW/Perspectives/Researcher", 90, false, 1);
    REG_CMD("view.perspectives.planner", QT_TRANSLATE_NOOP("CommandRegistrar", "Planner"), "VIEW/Perspectives/Planner", 100, true, 1);
    REG_CMD("view.perspectives.save", QT_TRANSLATE_NOOP("CommandRegistrar", "Save Current Perspective..."), "VIEW/Perspectives/Save Current Perspective...", 110, false, 1);
    REG_CMD("view.perspectives.manage", QT_TRANSLATE_NOOP("CommandRegistrar", "Manage Perspectives..."), "VIEW/Perspectives/Manage Perspectives...", 120, false, 1);

    // OpenSpec #00030: VIEW/Toolbars submenu is created DYNAMICALLY
    // by ToolbarManager::createViewMenuActions() - no static commands here.
    // Toolbar toggle actions + "Toolbar Manager..." are all dynamic.

    REG_CMD("view.showStatusBar", QT_TRANSLATE_NOOP("CommandRegistrar", "Show Status Bar"), "VIEW/Show Status Bar", 180, false, 0);
    REG_CMD("view.showStatsBar", QT_TRANSLATE_NOOP("CommandRegistrar", "Show Statistics Bar"), "VIEW/Show Statistics Bar", 190, false, 1);
    REG_CMD("view.showFormattingMarks", QT_TRANSLATE_NOOP("CommandRegistrar", "Show Formatting Marks"), "VIEW/Show Formatting Marks", 210, true, 1);

    REG_CMD_KEY_CB("view.zoomIn", QT_TRANSLATE_NOOP("CommandRegistrar", "Zoom In"), "VIEW/Zoom/Zoom In", 220, false, 0,
                   KeyboardShortcut::fromQKeySequence(QKeySequence::ZoomIn), callbacks.onZoomIn);
    REG_CMD_KEY_CB("view.zoomOut", QT_TRANSLATE_NOOP("CommandRegistrar", "Zoom Out"), "VIEW/Zoom/Zoom Out", 221, false, 0,
                   KeyboardShortcut::fromQKeySequence(QKeySequence::ZoomOut), callbacks.onZoomOut);
    REG_CMD_KEY_CB("view.resetZoom", QT_TRANSLATE_NOOP("CommandRegistrar", "Zoom 100%"), "VIEW/Zoom/Zoom 100%", 222, true, 0,
                   KeyboardShortcut(Qt::Key_0, Qt::ControlModifier), callbacks.onZoomReset);
    REG_CMD_CB("view.zoomPageWidth", QT_TRANSLATE_NOOP("CommandRegistrar", "Page Width"), "VIEW/Zoom/Page Width", 223, false, 0,
               callbacks.onZoomPageWidth);
    REG_CMD_CB("view.zoomWholePage", QT_TRANSLATE_NOOP("CommandRegistrar", "Whole Page"), "VIEW/Zoom/Whole Page", 224, true, 0,
               callbacks.onZoomWholePage);

    // OpenSpec #00030: F11 for Full Screen (standard)
    REG_CMD_KEY("view.fullScreen", QT_TRANSLATE_NOOP("CommandRegistrar", "Full Screen"), "VIEW/Full Screen", 250, true, 0,
                KeyboardShortcut::fromQKeySequence(QKeySequence::FullScreen));

    REG_CMD_CB("view.resetLayout", QT_TRANSLATE_NOOP("CommandRegistrar", "Reset Layout"), "VIEW/Reset Layout", 260, false, 0,
               callbacks.onResetLayout);

    // =========================================================================
    // HELP MENU
    // =========================================================================

    // OpenSpec #00030: F1 for Help (standard)
    REG_CMD_KEY("help.manual", QT_TRANSLATE_NOOP("CommandRegistrar", "Kalahari Help"), "HELP/Kalahari Help", 10, false, 2,
                KeyboardShortcut::fromQKeySequence(QKeySequence::HelpContents));
    REG_CMD("help.tutorial", QT_TRANSLATE_NOOP("CommandRegistrar", "Getting Started Tutorial"), "HELP/Getting Started Tutorial", 20, true, 2);

    REG_CMD("help.shortcuts", QT_TRANSLATE_NOOP("CommandRegistrar", "Keyboard Shortcuts"), "HELP/Keyboard Shortcuts", 30, false, 1);
    REG_CMD("help.tipsTricks", QT_TRANSLATE_NOOP("CommandRegistrar", "Tips & Tricks"), "HELP/Tips & Tricks", 40, false, 2);
    REG_CMD("help.whatsNew", QT_TRANSLATE_NOOP("CommandRegistrar", "What's New"), "HELP/What's New", 50, true, 1);

    REG_CMD("help.reportBug", QT_TRANSLATE_NOOP("CommandRegistrar", "Report a Bug..."), "HELP/Report a Bug...", 60, false, 1);
    REG_CMD("help.suggestFeature", QT_TRANSLATE_NOOP("CommandRegistrar", "Suggest a Feature..."), "HELP/Suggest a Feature...", 70, false, 1);
    REG_CMD("help.communityForum", QT_TRANSLATE_NOOP("CommandRegistrar", "Community Forum"), "HELP/Community Forum", 80, true, 2);

    REG_CMD("help.checkUpdates", QT_TRANSLATE_NOOP("CommandRegistrar", "Check for Updates..."), "HELP/Check for Updates...", 90, true, 2);

    REG_CMD_CB("help.about", QT_TRANSLATE_NOOP("CommandRegistrar", "About Kalahari"), "HELP/About Kalahari", 100, false, 0,
               callbacks.onAbout);

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
