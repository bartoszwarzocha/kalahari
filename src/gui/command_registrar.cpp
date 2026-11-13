/// @file command_registrar.cpp
/// @brief Application command registration with CommandRegistry
///
/// This module is extracted from MainWindow to reduce its size (OpenSpec #00038).
/// Contains all command registrations for the application.

#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/utils/access_keys.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/core/logger.h"
#include "kalahari/editor/character_format.h"

#include <QCoreApplication>

namespace kalahari {
namespace gui {

// Helper function for tr() - must be in a QObject context
// We use QCoreApplication::translate() as a workaround.
// lupdate cannot see a label passed through a macro parameter, so every call site
// marks its label with QT_TRANSLATE_NOOP("CommandRegistrar", ...). The label is the menu
// text, with its access key: unique in its menu, in each language (test_menu_access_keys).
static QString tr(const char* sourceText) {
    return QCoreApplication::translate("CommandRegistrar", sourceText);
}

// A command's texts from its translated menu text, with its access key ("Save &As..."): the
// label and the tooltip are without it
static void setTexts(Command& cmd, const char* menuText) {
    const QString text = tr(menuText);
    cmd.menuText = text.toStdString();
    cmd.label = withoutAccessKey(text).toStdString();
    cmd.tooltip = cmd.label;
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
            setTexts(cmd, label_tr_); \
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
            setTexts(cmd, label_tr_); \
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
            setTexts(cmd, label_tr_); \
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
            setTexts(cmd, label_tr_); \
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
            setTexts(cmd, label_tr_); \
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
    // SHORTCUTS
    // =========================================================================

    // One set of shortcuts, the same on Windows and Linux, each written out: Qt's standard
    // keys (QKeySequence::StandardKey) differ between the systems and the Linux desktops
    // (Close is Ctrl+F4 on Windows and Ctrl+W on Linux; Find & Replace is Ctrl+H, Ctrl+R or
    // none). No shortcut uses Ctrl+Alt on Windows and Linux: Windows takes it for AltGr,
    // which types letters (ą, ć, ę...).
    // macOS shows Ctrl as Cmd (Qt::MetaModifier is its Control key), and some commands
    // have the keys of macOS there: programs quit with Cmd+Q (Alt+F4 does nothing), Cmd+H
    // hides the program, F11 shows the desktop, Option+Up and Option+Down move the cursor by
    // paragraphs in the text (editor/text_keys.h), and Option with a digit types a character,
    // so the paragraph styles have Option+Cmd with it (and the pastes Option+Shift+Cmd with a
    // letter). Option with the hyphen types the dashes itself, so their commands have no keys.
#ifdef Q_OS_MACOS
    const KeyboardShortcut exitShortcut(Qt::Key_Q, Qt::ControlModifier);
    const KeyboardShortcut replaceShortcut(Qt::Key_F, Qt::ControlModifier | Qt::AltModifier);
    const KeyboardShortcut fullScreenShortcut(Qt::Key_F, Qt::ControlModifier | Qt::MetaModifier);
    const KeyboardShortcut nextTodoShortcut(Qt::Key_Down, Qt::ControlModifier | Qt::AltModifier);
    const KeyboardShortcut previousTodoShortcut(Qt::Key_Up, Qt::ControlModifier | Qt::AltModifier);
    const Qt::KeyboardModifiers styleModifiers = Qt::ControlModifier | Qt::AltModifier;
    const Qt::KeyboardModifiers pasteModifiers =
        Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier;
    const KeyboardShortcut clearFormattingShortcut(Qt::Key_Backslash, Qt::ControlModifier);
    const KeyboardShortcut enDashShortcut;
    const KeyboardShortcut emDashShortcut;
    // Option+Return is a new paragraph in the text: Option+Cmd+I, the inspector of macOS
    const KeyboardShortcut imagePropertiesShortcut(Qt::Key_I,
                                                   Qt::ControlModifier | Qt::AltModifier);
#else
    const KeyboardShortcut exitShortcut(Qt::Key_F4, Qt::AltModifier);
    const KeyboardShortcut replaceShortcut(Qt::Key_H, Qt::ControlModifier);
    const KeyboardShortcut fullScreenShortcut(Qt::Key_F11, Qt::NoModifier);
    const KeyboardShortcut nextTodoShortcut(Qt::Key_Down, Qt::AltModifier);
    const KeyboardShortcut previousTodoShortcut(Qt::Key_Up, Qt::AltModifier);
    const Qt::KeyboardModifiers styleModifiers = Qt::AltModifier;
    const Qt::KeyboardModifiers pasteModifiers = Qt::AltModifier | Qt::ShiftModifier;
    const KeyboardShortcut clearFormattingShortcut(Qt::Key_Space, Qt::ControlModifier);
    // Word has Ctrl with the minus of the keypad, here Zoom Out
    const KeyboardShortcut enDashShortcut(Qt::Key_Minus, Qt::AltModifier);
    const KeyboardShortcut emDashShortcut(Qt::Key_Underscore, Qt::AltModifier);
    // Alt+Enter: Properties in the system's file managers
    const KeyboardShortcut imagePropertiesShortcut(Qt::Key_Return, Qt::AltModifier);
#endif

    // =========================================================================
    // FILE MENU
    // =========================================================================

    // The files first, then the books: New File, Open File, New Book, Open Book, in this order
    // also on the File and Quick Actions toolbars and on the Dashboard

    // A new file of the open book, of the kind and at the place the writer chooses; without a
    // book, a new file outside one
    REG_CMD_TOOL_ICON("file.new", QT_TRANSLATE_NOOP("CommandRegistrar", "&New File..."), "FILE/New File...", 10, false, 0,
                      KeyboardShortcut(Qt::Key_N, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onNewDocument);

    // A file outside the book (Ctrl+Shift+O)
    REG_CMD_TOOL_ICON("file.open.file", QT_TRANSLATE_NOOP("CommandRegistrar", "Open &File..."), "FILE/Open File...", 15, false, 0,
                      KeyboardShortcut(Qt::Key_O, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onOpenStandaloneFile);

    // OpenSpec #00033: New Book command (Ctrl+Shift+N)
    REG_CMD_TOOL_ICON("file.new.project", QT_TRANSLATE_NOOP("CommandRegistrar", "New &Book..."), "FILE/New Book...", 20, false, 0,
                      KeyboardShortcut(Qt::Key_N, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onNewProject);

    REG_CMD_TOOL_ICON("file.open", QT_TRANSLATE_NOOP("CommandRegistrar", "&Open Book..."), "FILE/Open Book...", 25, false, 0,
                      KeyboardShortcut(Qt::Key_O, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onOpenDocument);

    // Recent Books - dynamic submenu (registered separately)

    // Ctrl+F4, as Windows programs close a document. Not Ctrl+W: next to Ctrl+S and Ctrl+E,
    // it would close the book by mistake while writing.
    REG_CMD_TOOL_ICON("file.close", QT_TRANSLATE_NOOP("CommandRegistrar", "&Close Book"), "FILE/Close Book", 40, true, 1,
                      KeyboardShortcut(Qt::Key_F4, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onCloseDocument);

    REG_CMD_TOOL_ICON("file.save", QT_TRANSLATE_NOOP("CommandRegistrar", "&Save"), "FILE/Save", 50, false, 0,
                      KeyboardShortcut(Qt::Key_S, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onSaveDocument);

    REG_CMD_TOOL_ICON("file.saveAs", QT_TRANSLATE_NOOP("CommandRegistrar", "Save &As..."), "FILE/Save As...", 60, false, 0,
                      KeyboardShortcut(Qt::Key_S, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onSaveAsDocument);

    // The book and every open file outside it; no shortcut of its own
    REG_CMD_TOOL_ICON("file.saveAll", QT_TRANSLATE_NOOP("CommandRegistrar", "Save A&ll"),
                      "FILE/Save All", 70, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onSaveAll);

    // The versions of the text in front (VersionsCoordinator): Alt+Shift with the letter, as
    // the pastes (Cmd+Option+Shift on macOS, where Option with a letter types a character)
    REG_CMD_TOOL_ICON("file.saveVersion", QT_TRANSLATE_NOOP("CommandRegistrar", "Save &Version..."),
                      "FILE/Save Version...", 72, false, 0,
                      KeyboardShortcut(Qt::Key_S, pasteModifiers),
                      IconSet(),
                      nullptr);
    REG_CMD_TOOL_ICON("file.versionHistory",
                      QT_TRANSLATE_NOOP("CommandRegistrar", "Version &History..."),
                      "FILE/Version History...", 74, true, 0,
                      KeyboardShortcut(Qt::Key_H, pasteModifiers),
                      IconSet(),
                      nullptr);

    // Import submenu
    REG_CMD("file.import.docx", QT_TRANSLATE_NOOP("CommandRegistrar", "&DOCX Document..."), "FILE/Import/DOCX Document...", 80, false, 1);
    REG_CMD("file.import.pdf", QT_TRANSLATE_NOOP("CommandRegistrar", "&PDF Reference..."), "FILE/Import/PDF Reference...", 90, false, 2);
    REG_CMD("file.import.text", QT_TRANSLATE_NOOP("CommandRegistrar", "Plain &Text..."), "FILE/Import/Plain Text...", 100, false, 1);
    REG_CMD("file.import.scrivener", QT_TRANSLATE_NOOP("CommandRegistrar", "&Scrivener Project..."), "FILE/Import/Scrivener Project...", 110, false, 2);

    // Import Archive (priority 75 in Import submenu)
    REG_CMD_TOOL_ICON("file.import.archive", QT_TRANSLATE_NOOP("CommandRegistrar", "Project &Archive..."), "FILE/Import/Project Archive...", 75, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onImportArchive);

    // Export submenu
    REG_CMD("file.export.docx", QT_TRANSLATE_NOOP("CommandRegistrar", "&DOCX"), "FILE/Export/DOCX", 120, false, 1);
    REG_CMD("file.export.pdf", QT_TRANSLATE_NOOP("CommandRegistrar", "&PDF"), "FILE/Export/PDF", 130, false, 1);
    REG_CMD("file.export.markdown", QT_TRANSLATE_NOOP("CommandRegistrar", "&Markdown"), "FILE/Export/Markdown", 140, true, 1);
    REG_CMD("file.export.epub", QT_TRANSLATE_NOOP("CommandRegistrar", "&EPUB"), "FILE/Export/EPUB", 150, false, 2);
    REG_CMD("file.export.mobi", QT_TRANSLATE_NOOP("CommandRegistrar", "M&OBI"), "FILE/Export/MOBI", 160, false, 2);
    REG_CMD("file.export.icml", QT_TRANSLATE_NOOP("CommandRegistrar", "&InDesign ICML"), "FILE/Export/InDesign ICML", 170, false, 3);
    REG_CMD("file.export.latex", QT_TRANSLATE_NOOP("CommandRegistrar", "&LaTeX"), "FILE/Export/LaTeX", 180, false, 3);
    REG_CMD("file.export.settings", QT_TRANSLATE_NOOP("CommandRegistrar", "Export &Settings..."), "FILE/Export/Export Settings...", 190, true, 2);

    // Export Archive (priority 195 = end of Export submenu)
    REG_CMD_TOOL_ICON("file.export.archive", QT_TRANSLATE_NOOP("CommandRegistrar", "Project &Archive..."), "FILE/Export/Project Archive...", 195, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onExportArchive);

    // Alt+F4 (Cmd+Q on macOS, see SHORTCUTS): it closes the window on Windows and on Linux
    // desktops anyway; the menu shows it, and it works also where the window manager leaves
    // it to the program
    REG_CMD_TOOL_ICON("file.exit", QT_TRANSLATE_NOOP("CommandRegistrar", "E&xit"), "FILE/Exit", 200, false, 0,
                      exitShortcut,
                      IconSet(),
                      callbacks.onExit);

    // =========================================================================
    // EDIT MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("edit.undo", QT_TRANSLATE_NOOP("CommandRegistrar", "&Undo"), "EDIT/Undo", 10, false, 0,
                      KeyboardShortcut(Qt::Key_Z, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onUndo);

    // Ctrl+Y; the editor takes Ctrl+Shift+Z for it too
    REG_CMD_TOOL_ICON("edit.redo", QT_TRANSLATE_NOOP("CommandRegistrar", "&Redo"), "EDIT/Redo", 20, true, 0,
                      KeyboardShortcut(Qt::Key_Y, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onRedo);

    REG_CMD_TOOL_ICON("edit.cut", QT_TRANSLATE_NOOP("CommandRegistrar", "Cu&t"), "EDIT/Cut", 30, false, 0,
                      KeyboardShortcut(Qt::Key_X, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onCut);

    REG_CMD_TOOL_ICON("edit.copy", QT_TRANSLATE_NOOP("CommandRegistrar", "&Copy"), "EDIT/Copy", 40, false, 0,
                      KeyboardShortcut(Qt::Key_C, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onCopy);

    REG_CMD_TOOL_ICON("edit.paste", QT_TRANSLATE_NOOP("CommandRegistrar", "&Paste"), "EDIT/Paste", 50, false, 0,
                      KeyboardShortcut(Qt::Key_V, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onPaste);

    // Text of other programs in each of the paste modes, whatever Paste does (Settings >
    // Editor > General): without formatting with Ctrl+Shift+V, as in LibreOffice and the
    // browsers, the others with Alt+Shift (Option+Shift+Cmd on macOS); Ctrl+Alt is AltGr
    REG_CMD_KEY_CB("edit.pasteSourceFormatting", QT_TRANSLATE_NOOP("CommandRegistrar", "Paste with Source Formatt&ing"), "EDIT/Paste with Source Formatting", 52, false, 0,
                   KeyboardShortcut(Qt::Key_V, pasteModifiers), callbacks.onPasteSourceFormatting);
    REG_CMD_KEY_CB("edit.pasteMergeFormatting", QT_TRANSLATE_NOOP("CommandRegistrar", "Paste and &Merge Formatting"), "EDIT/Paste and Merge Formatting", 54, false, 0,
                   KeyboardShortcut(Qt::Key_M, pasteModifiers), callbacks.onPasteMergeFormatting);
    REG_CMD_KEY_CB("edit.pasteTextOnly", QT_TRANSLATE_NOOP("CommandRegistrar", "Paste Text Onl&y"), "EDIT/Paste Text Only", 56, false, 0,
                   KeyboardShortcut(Qt::Key_V, Qt::ControlModifier | Qt::ShiftModifier), callbacks.onPasteTextOnly);

    REG_CMD("edit.pasteSpecial", QT_TRANSLATE_NOOP("CommandRegistrar", "Paste &Special..."), "EDIT/Paste Special...", 60, false, 1);
    // Delete: in the text and the fields the key deletes the selection itself (and the
    // character after the cursor without one); the command takes it elsewhere, greyed out
    REG_CMD_KEY_CB("edit.delete", QT_TRANSLATE_NOOP("CommandRegistrar", "&Delete"), "EDIT/Delete", 70, true, 0,
                   KeyboardShortcut(Qt::Key_Delete, Qt::NoModifier), callbacks.onDelete);

    REG_CMD_TOOL_ICON("edit.selectAll", QT_TRANSLATE_NOOP("CommandRegistrar", "Select &All"), "EDIT/Select All", 80, false, 0,
                      KeyboardShortcut(Qt::Key_A, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onSelectAll);

    REG_CMD_CB("edit.selectWord", QT_TRANSLATE_NOOP("CommandRegistrar", "Select &Word"), "EDIT/Select Word", 90, false, 0,
               callbacks.onSelectWord);
    REG_CMD_CB("edit.selectParagraph", QT_TRANSLATE_NOOP("CommandRegistrar", "Select Paragrap&h"), "EDIT/Select Paragraph", 100, true, 0,
               callbacks.onSelectParagraph);

    // OpenSpec #00030: Added keyboard shortcuts for Find operations
    REG_CMD_KEY_CB("edit.find", QT_TRANSLATE_NOOP("CommandRegistrar", "&Find..."), "EDIT/Find...", 110, false, 1,
                   KeyboardShortcut(Qt::Key_F, Qt::ControlModifier), callbacks.onFind);
    REG_CMD_KEY_CB("edit.findNext", QT_TRANSLATE_NOOP("CommandRegistrar", "Find &Next"), "EDIT/Find Next", 120, false, 1,
                   KeyboardShortcut(Qt::Key_F3, Qt::NoModifier),
                   callbacks.onFindNext);
    REG_CMD_KEY_CB("edit.findPrevious", QT_TRANSLATE_NOOP("CommandRegistrar", "Find Pre&vious"), "EDIT/Find Previous", 130, false, 1,
                   KeyboardShortcut(Qt::Key_F3, Qt::ShiftModifier),
                   callbacks.onFindPrevious);
    REG_CMD_KEY_CB("edit.findReplace", QT_TRANSLATE_NOOP("CommandRegistrar", "Find && Rep&lace..."), "EDIT/Find & Replace...", 140, false, 1,
                   replaceShortcut,
                   callbacks.onFindReplace);
    REG_CMD("edit.findInBook", QT_TRANSLATE_NOOP("CommandRegistrar", "Find in &Book..."), "EDIT/Find in Book...", 150, true, 1);

    // The to-dos not done yet; AnnotationsCoordinator gives them their callbacks
    REG_CMD_KEY("edit.nextTodo", QT_TRANSLATE_NOOP("CommandRegistrar", "Ne&xt To Do"), "EDIT/Next To Do", 152, false, 0,
                nextTodoShortcut);
    REG_CMD_KEY("edit.previousTodo", QT_TRANSLATE_NOOP("CommandRegistrar", "Previ&ous To Do"), "EDIT/Previous To Do", 154, true, 0,
                previousTodoShortcut);

    REG_CMD_CB("edit.preferences", QT_TRANSLATE_NOOP("CommandRegistrar", "Pr&eferences..."), "EDIT/Preferences...", 160, false, 0,
               callbacks.onSettings);

    // OpenSpec #00037: edit.settings command (alias for Settings dialog, used in Quick Actions toolbar)
    REG_CMD_TOOL_ICON("edit.settings", QT_TRANSLATE_NOOP("CommandRegistrar", "Settin&gs..."), "EDIT/Settings...", 165, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onSettings);

    // =========================================================================
    // BOOK MENU
    // =========================================================================

    // Add... for what is in front: right after the element in the editor, else in the whole
    // book (MainWindow)
    REG_CMD_KEY_CB("book.addElement", QT_TRANSLATE_NOOP("CommandRegistrar", "&Add..."), "BOOK/Add...", 5, true, 1,
                   KeyboardShortcut(Qt::Key_Return, Qt::ControlModifier | Qt::ShiftModifier),
                   callbacks.onAddElement);

    // The open book names it after its main texts, "New Story..." (MainWindow)
    REG_CMD_TOOL_ICON("book.newChapter", QT_TRANSLATE_NOOP("CommandRegistrar", "&New Chapter..."), "BOOK/New Chapter...", 10, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onNewChapter);

    REG_CMD("book.newScene", QT_TRANSLATE_NOOP("CommandRegistrar", "New &Scene..."), "BOOK/New Scene...", 20, false, 1);

    // The kinds of the Workshop: Add Element with the kind chosen, when the program has its
    // editor (MainWindow greys them out until then, saying why)
    const auto newInWorkshop = [&callbacks](const char* kindId) -> std::function<void()> {
        if (!callbacks.onNewWorkshopElement) {
            return nullptr;
        }
        return [add = callbacks.onNewWorkshopElement, kindId]() {
            add(QString::fromLatin1(kindId));
        };
    };

    REG_CMD_TOOL_ICON("book.newCharacter", QT_TRANSLATE_NOOP("CommandRegistrar", "New &Character..."), "BOOK/New Character...", 30, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      newInWorkshop("character"));

    REG_CMD_TOOL_ICON("book.newLocation", QT_TRANSLATE_NOOP("CommandRegistrar", "New &Location..."), "BOOK/New Location...", 40, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      newInWorkshop("location"));

    REG_CMD_CB("book.newItem", QT_TRANSLATE_NOOP("CommandRegistrar", "New &Item..."), "BOOK/New Item...", 50, false, 1,
               newInWorkshop("item"));

    REG_CMD_CB("book.newMindMap", QT_TRANSLATE_NOOP("CommandRegistrar", "New &Mind Map..."), "BOOK/New Mind Map...", 60, false, 1,
               newInWorkshop("mindmap"));
    REG_CMD_CB("book.newTimeline", QT_TRANSLATE_NOOP("CommandRegistrar", "New &Timeline..."), "BOOK/New Timeline...", 70, true, 1,
               newInWorkshop("timeline"));

    // The same as Insert > Scene Break
    REG_CMD_CB("book.sceneBreak", QT_TRANSLATE_NOOP("CommandRegistrar", "Sc&ene Break"), "BOOK/Scene Break", 90, true, 0,
               callbacks.onInsertSceneBreak);

    REG_CMD_TOOL_ICON("book.properties", QT_TRANSLATE_NOOP("CommandRegistrar", "Book &Properties..."), "BOOK/Book Properties...", 100, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onBookProperties);

    // =========================================================================
    // INSERT MENU
    // =========================================================================

    // An image: the system window choosing a picture, copied into the book's folder of
    // images (no keys, as in Word)
    REG_CMD_CB("insert.image", QT_TRANSLATE_NOOP("CommandRegistrar", "&Image..."), "INSERT/Image...", 10, false, 1,
               callbacks.onInsertImage);
    // A table: the window of its columns and rows (no keys: Ctrl+F12 of LibreOffice switches
    // the workspaces of Xfce)
    REG_CMD_CB("insert.table", QT_TRANSLATE_NOOP("CommandRegistrar", "Ta&ble..."), "INSERT/Table...", 20, false, 1,
               callbacks.onInsertTable);
    // The numbered captions: the caption of the image at the cursor (Ctrl+Shift+G, free in the
    // program and in the systems; also with the cursor just after the image), a cross-reference
    // to a caption and the list of figures. CaptionCoordinator gives them their callbacks.
    REG_CMD_KEY("insert.caption", QT_TRANSLATE_NOOP("CommandRegistrar", "&Caption..."), "INSERT/Caption...", 22, false, 0,
                KeyboardShortcut(Qt::Key_G, Qt::ControlModifier | Qt::ShiftModifier));
    REG_CMD("insert.crossReference", QT_TRANSLATE_NOOP("CommandRegistrar", "Cross-&Reference..."), "INSERT/Cross-Reference...", 24, false, 0);
    REG_CMD("insert.figureList", QT_TRANSLATE_NOOP("CommandRegistrar", "List of Fi&gures"), "INSERT/List of Figures", 26, false, 0);
    // A link: the window of its text and address, on a link of the text its editing (Ctrl+K,
    // as in Word and LibreOffice)
    REG_CMD_KEY_CB("insert.link", QT_TRANSLATE_NOOP("CommandRegistrar", "&Link..."), "INSERT/Link...", 30, true, 1,
                   KeyboardShortcut(Qt::Key_K, Qt::ControlModifier), callbacks.onInsertLink);

    REG_CMD("insert.footnote", QT_TRANSLATE_NOOP("CommandRegistrar", "F&ootnote"), "INSERT/Footnote", 40, false, 1);
    REG_CMD("insert.endnote", QT_TRANSLATE_NOOP("CommandRegistrar", "&Endnote"), "INSERT/Endnote", 50, false, 1);

    // Annotations on the text; AnnotationsCoordinator gives them their callbacks. Add
    // Annotation opens a small menu of the kinds at the cursor. Not Ctrl+M (it minimizes
    // the window on macOS), nor Ctrl+Alt+letter (AltGr on Polish keyboards). Add Comment,
    // Add To Do and Add Note are also in the text's context menu: their access keys are
    // unique there too, beside Cut, Copy and Paste.
    REG_CMD_KEY("insert.annotation", QT_TRANSLATE_NOOP("CommandRegistrar", "Add &Annotation..."), "INSERT/Add Annotation...", 70, false, 0,
                KeyboardShortcut(Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier));
    REG_CMD("insert.comment", QT_TRANSLATE_NOOP("CommandRegistrar", "Add Co&mment"), "INSERT/Add Comment", 71, false, 0);
    REG_CMD("insert.todo", QT_TRANSLATE_NOOP("CommandRegistrar", "Add To &Do"), "INSERT/Add To Do", 72, false, 0);
    REG_CMD("insert.note", QT_TRANSLATE_NOOP("CommandRegistrar", "Add &Note"), "INSERT/Add Note", 73, true, 0);

    REG_CMD_CB("insert.specialChar", QT_TRANSLATE_NOOP("CommandRegistrar", "&Special Character..."), "INSERT/Special Character...", 80, false, 0,
               callbacks.onSpecialCharacter);
    // The favorites, the characters used last and a search at the cursor. Ctrl+Shift+I: I as
    // insert, beside Ctrl+I (italic); neither the program nor the systems have it (Ctrl+Shift+E
    // and Ctrl+. pick an emoji on some Linux desktops, Ctrl+Shift+U types a code there)
    REG_CMD_KEY_CB("insert.quickCharacter", QT_TRANSLATE_NOOP("CommandRegistrar", "&Quick Character"), "INSERT/Quick Character", 81, false, 0,
                   KeyboardShortcut(Qt::Key_I, Qt::ControlModifier | Qt::ShiftModifier),
                   callbacks.onQuickCharacter);

    // Formatting Character: the characters of the typesetting a keyboard does not have, as in
    // Word and LibreOffice, with Word's keys. Ctrl+_ is Ctrl+Shift+- (the keys are written
    // as the list of shortcuts records them)
    const auto insertCharacters = [&callbacks](const QString& characters) -> std::function<void()> {
        if (!callbacks.onInsertCharacters) {
            return {};
        }
        return [onInsert = callbacks.onInsertCharacters, characters]() { onInsert(characters); };
    };
    REG_CMD_KEY_CB("insert.noBreakSpace", QT_TRANSLATE_NOOP("CommandRegistrar", "&Non-breaking Space"), "INSERT/Formatting Character/Non-breaking Space", 82, false, 0,
                   KeyboardShortcut(Qt::Key_Space, Qt::ControlModifier | Qt::ShiftModifier),
                   insertCharacters(QString(QChar(0x00A0))));
    REG_CMD_KEY_CB("insert.nonBreakingHyphen", QT_TRANSLATE_NOOP("CommandRegistrar", "Non-breaking &Hyphen"), "INSERT/Formatting Character/Non-breaking Hyphen", 83, false, 0,
                   KeyboardShortcut(Qt::Key_Underscore, Qt::ControlModifier),
                   insertCharacters(QString(QChar(0x2011))));
    REG_CMD_CB("insert.optionalHyphen", QT_TRANSLATE_NOOP("CommandRegistrar", "&Optional Hyphen"), "INSERT/Formatting Character/Optional Hyphen", 84, true, 0,
               insertCharacters(QString(QChar(0x00AD))));
    REG_CMD_KEY_CB("insert.enDash", QT_TRANSLATE_NOOP("CommandRegistrar", "&En Dash"), "INSERT/Formatting Character/En Dash", 85, false, 0,
                   enDashShortcut, insertCharacters(QString(QChar(0x2013))));
    REG_CMD_KEY_CB("insert.emDash", QT_TRANSLATE_NOOP("CommandRegistrar", "E&m Dash"), "INSERT/Formatting Character/Em Dash", 86, false, 0,
                   emDashShortcut, insertCharacters(QString(QChar(0x2014))));
    // The list of quick insert at the cursor with all its items, as @ and # typed at the start of
    // a word open it. Ctrl+Shift+W: the program has no other command on it
    REG_CMD_KEY_CB("insert.quickInsert", QT_TRANSLATE_NOOP("CommandRegistrar", "Q&uick Insert..."), "INSERT/Quick Insert...", 87, false, 0,
                   KeyboardShortcut(Qt::Key_W, Qt::ControlModifier | Qt::ShiftModifier), callbacks.onQuickInsert);
    REG_CMD("insert.dateTime", QT_TRANSLATE_NOOP("CommandRegistrar", "Date && &Time"), "INSERT/Date & Time", 90, false, 1);
    REG_CMD("insert.field", QT_TRANSLATE_NOOP("CommandRegistrar", "&Field..."), "INSERT/Field...", 100, true, 1);

    // The objects of the text (editor/paragraph_styles.h): each a block of its own. Ctrl+Enter
    // as in Word and LibreOffice; the annotation frame keeps it for its text
    REG_CMD_CB("insert.horizontalLine", QT_TRANSLATE_NOOP("CommandRegistrar", "Hori&zontal Line"), "INSERT/Horizontal Line", 110, false, 0,
               callbacks.onInsertHorizontalLine);
    REG_CMD_CB("insert.sceneBreak", QT_TRANSLATE_NOOP("CommandRegistrar", "Scene Brea&k"), "INSERT/Scene Break", 120, false, 0,
               callbacks.onInsertSceneBreak);
    REG_CMD_KEY_CB("insert.pageBreak", QT_TRANSLATE_NOOP("CommandRegistrar", "&Page Break"), "INSERT/Page Break", 130, false, 0,
                   KeyboardShortcut(Qt::Key_Return, Qt::ControlModifier), callbacks.onInsertPageBreak);

    // =========================================================================
    // FORMAT MENU
    // =========================================================================

    // The windows of the selected text and paragraphs (gui/dialogs/font_dialog.h,
    // paragraph_dialog.h). Ctrl+D as in Word
    REG_CMD_KEY_CB("format.font", QT_TRANSLATE_NOOP("CommandRegistrar", "&Font..."), "FORMAT/Font...", 10, false, 0,
                   KeyboardShortcut(Qt::Key_D, Qt::ControlModifier), callbacks.onFont);
    REG_CMD_CB("format.paragraph", QT_TRANSLATE_NOOP("CommandRegistrar", "&Paragraph..."), "FORMAT/Paragraph...", 20, false, 0,
               callbacks.onParagraph);
    // The formats of the levels of the list in the selected paragraphs (gui/dialogs/list_dialogs.h)
    REG_CMD_CB("format.bulletsNumbering", QT_TRANSLATE_NOOP("CommandRegistrar", "Bullets and Numberin&g..."), "FORMAT/Bullets and Numbering...", 25, true, 0,
               callbacks.onBulletsNumbering);

    // Text Style submenu: the paragraph styles every book has (the package "Base"), by the ids
    // of editor/paragraph_styles.h; the menu adds the other styles of the book after Code.
    // Alt with a digit (Option+Cmd on macOS): Alt with a letter opens the menus
    const auto toStyle = [&callbacks](const char* styleId) -> std::function<void()> {
        if (!callbacks.onParagraphStyle) {
            return {};
        }
        return [onStyle = callbacks.onParagraphStyle, id = QString::fromLatin1(styleId)]() {
            onStyle(id);
        };
    };
    REG_CMD_KEY_CB("format.style.heading1", QT_TRANSLATE_NOOP("CommandRegistrar", "Heading &1"), "FORMAT/Text Style/Heading 1", 30, false, 0,
                   KeyboardShortcut(Qt::Key_1, styleModifiers), toStyle("heading_1"));
    REG_CMD_KEY_CB("format.style.heading2", QT_TRANSLATE_NOOP("CommandRegistrar", "Heading &2"), "FORMAT/Text Style/Heading 2", 40, false, 0,
                   KeyboardShortcut(Qt::Key_2, styleModifiers), toStyle("heading_2"));
    REG_CMD_KEY_CB("format.style.heading3", QT_TRANSLATE_NOOP("CommandRegistrar", "Heading &3"), "FORMAT/Text Style/Heading 3", 50, false, 0,
                   KeyboardShortcut(Qt::Key_3, styleModifiers), toStyle("heading_3"));
    REG_CMD_KEY_CB("format.style.body", QT_TRANSLATE_NOOP("CommandRegistrar", "&Normal"), "FORMAT/Text Style/Normal", 60, false, 0,
                   KeyboardShortcut(Qt::Key_0, styleModifiers), toStyle("normal"));
    REG_CMD_CB("format.style.quote", QT_TRANSLATE_NOOP("CommandRegistrar", "&Quote"), "FORMAT/Text Style/Quote", 70, false, 0,
               toStyle("quote"));
    REG_CMD_CB("format.style.dialogue", QT_TRANSLATE_NOOP("CommandRegistrar", "&Dialogue"), "FORMAT/Text Style/Dialogue", 75, false, 0,
               toStyle("dialogue"));
    REG_CMD_CB("format.style.code", QT_TRANSLATE_NOOP("CommandRegistrar", "&Code"), "FORMAT/Text Style/Code", 80, true, 0,
               toStyle("code"));
    // The styles of the book: a new one from the paragraph at the cursor, the one at the cursor
    // changed or deleted, all of them in one window (gui/style_coordinator.h)
    REG_CMD("format.style.new", QT_TRANSLATE_NOOP("CommandRegistrar", "Ne&w Style..."), "FORMAT/Text Style/New Style...", 82, false, 1);
    REG_CMD("format.style.modify", QT_TRANSLATE_NOOP("CommandRegistrar", "Modi&fy Style..."), "FORMAT/Text Style/Modify Style...", 84, false, 1);
    REG_CMD("format.style.delete", QT_TRANSLATE_NOOP("CommandRegistrar", "Dele&te Style"), "FORMAT/Text Style/Delete Style", 86, false, 1);
    REG_CMD("format.style.manage", QT_TRANSLATE_NOOP("CommandRegistrar", "&Manage Styles..."), "FORMAT/Text Style/Manage Styles...", 90, true, 1);

    // OpenSpec #00030: Added standard formatting shortcuts (Ctrl+B/I/U)
    // OpenSpec #00042 Phase 7.2: Connected to toolbar and editor
    REG_CMD_TOOL_ICON("format.bold", QT_TRANSLATE_NOOP("CommandRegistrar", "&Bold"), "FORMAT/Bold", 100, false, 0,
                      KeyboardShortcut(Qt::Key_B, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onFormatBold);
    REG_CMD_TOOL_ICON("format.italic", QT_TRANSLATE_NOOP("CommandRegistrar", "&Italic"), "FORMAT/Italic", 110, false, 0,
                      KeyboardShortcut(Qt::Key_I, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onFormatItalic);
    REG_CMD_TOOL_ICON("format.underline", QT_TRANSLATE_NOOP("CommandRegistrar", "&Underline"), "FORMAT/Underline", 120, false, 0,
                      KeyboardShortcut(Qt::Key_U, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onFormatUnderline);

    // Text Effects submenu: in the Format menu each item needs an access key of its own,
    // and the menu has more items than its texts have letters. The keys of the indexes as in
    // LibreOffice, those of the capitals as in Word
    REG_CMD_TOOL_ICON("format.strikethrough", QT_TRANSLATE_NOOP("CommandRegistrar", "Stri&kethrough"), "FORMAT/Text Effects/Strikethrough", 122, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onFormatStrikethrough);
    REG_CMD_TOOL_ICON("format.superscript", QT_TRANSLATE_NOOP("CommandRegistrar", "Su&perscript"), "FORMAT/Text Effects/Superscript", 124, false, 0,
                      KeyboardShortcut(Qt::Key_P, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onSuperscript);
    REG_CMD_TOOL_ICON("format.subscript", QT_TRANSLATE_NOOP("CommandRegistrar", "Su&bscript"), "FORMAT/Text Effects/Subscript", 126, false, 0,
                      KeyboardShortcut(Qt::Key_B, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onSubscript);
    REG_CMD_TOOL_ICON("format.smallCaps", QT_TRANSLATE_NOOP("CommandRegistrar", "S&mall Caps"), "FORMAT/Text Effects/Small Caps", 127, false, 0,
                      KeyboardShortcut(Qt::Key_K, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onSmallCaps);
    REG_CMD_TOOL_ICON("format.allCaps", QT_TRANSLATE_NOOP("CommandRegistrar", "&All Caps"), "FORMAT/Text Effects/All Caps", 128, false, 0,
                      KeyboardShortcut(Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onAllCaps);

    // Change Case submenu, as in Word: each text shows its case
    const auto toCase = [&callbacks](editor::CaseChange change) -> std::function<void()> {
        if (!callbacks.onChangeCase) {
            return {};
        }
        return [onCase = callbacks.onChangeCase, change]() { onCase(change); };
    };
    REG_CMD_CB("format.case.sentence", QT_TRANSLATE_NOOP("CommandRegistrar", "&Sentence case"), "FORMAT/Change Case/Sentence case", 131, false, 0,
               toCase(editor::CaseChange::Sentence));
    REG_CMD_CB("format.case.lower", QT_TRANSLATE_NOOP("CommandRegistrar", "&lowercase"), "FORMAT/Change Case/lowercase", 132, false, 0,
               toCase(editor::CaseChange::Lower));
    REG_CMD_CB("format.case.upper", QT_TRANSLATE_NOOP("CommandRegistrar", "&UPPERCASE"), "FORMAT/Change Case/UPPERCASE", 133, false, 0,
               toCase(editor::CaseChange::Upper));
    REG_CMD_CB("format.case.capitalize", QT_TRANSLATE_NOOP("CommandRegistrar", "&Capitalize Each Word"), "FORMAT/Change Case/Capitalize Each Word", 134, false, 0,
               toCase(editor::CaseChange::Capitalize));
    REG_CMD_CB("format.case.toggle", QT_TRANSLATE_NOOP("CommandRegistrar", "&tOGGLE cASE"), "FORMAT/Change Case/tOGGLE cASE", 135, true, 0,
               toCase(editor::CaseChange::Toggle));

    // Align Text submenu, for the access keys as Text Effects
    REG_CMD_TOOL_ICON("format.alignLeft", QT_TRANSLATE_NOOP("CommandRegistrar", "Align &Left"), "FORMAT/Align Text/Align Left", 140, false, 0,
                      KeyboardShortcut(Qt::Key_L, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onAlignLeft);
    REG_CMD_TOOL_ICON("format.alignCenter", QT_TRANSLATE_NOOP("CommandRegistrar", "Align &Center"), "FORMAT/Align Text/Align Center", 150, false, 0,
                      KeyboardShortcut(Qt::Key_E, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onAlignCenter);
    REG_CMD_TOOL_ICON("format.alignRight", QT_TRANSLATE_NOOP("CommandRegistrar", "Align &Right"), "FORMAT/Align Text/Align Right", 160, false, 0,
                      KeyboardShortcut(Qt::Key_R, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onAlignRight);
    REG_CMD_TOOL_ICON("format.justify", QT_TRANSLATE_NOOP("CommandRegistrar", "&Justify"), "FORMAT/Align Text/Justify", 170, false, 0,
                      KeyboardShortcut(Qt::Key_J, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onAlignJustify);

    // A list item goes a level down or up (Tab and Shift+Tab at its start), another paragraph
    // a step of the indent (editor/text_lists.h, editor/paragraph_format.h)
    REG_CMD_TOOL_ICON("format.increaseIndent", QT_TRANSLATE_NOOP("CommandRegistrar", "Incre&ase Indent"), "FORMAT/Increase Indent", 180, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onIncreaseIndent);
    REG_CMD_TOOL_ICON("format.decreaseIndent", QT_TRANSLATE_NOOP("CommandRegistrar", "&Decrease Indent"), "FORMAT/Decrease Indent", 190, true, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onDecreaseIndent);

    // As in LibreOffice: F12 and Shift+F12
    REG_CMD_TOOL_ICON("format.bullets", QT_TRANSLATE_NOOP("CommandRegistrar", "Bull&ets"), "FORMAT/Bullets", 200, false, 0,
                      KeyboardShortcut(Qt::Key_F12, Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onBullets);
    REG_CMD_TOOL_ICON("format.numbering", QT_TRANSLATE_NOOP("CommandRegistrar", "&Numbering"), "FORMAT/Numbering", 210, true, 0,
                      KeyboardShortcut(Qt::Key_F12),
                      IconSet(),
                      callbacks.onNumbering);

    // The window gives each its submenu of colors (gui/widgets/color_menu.h); the command
    // itself, a toolbar button, gives the color chosen last
    REG_CMD_TOOL_ICON("format.fontColor", QT_TRANSLATE_NOOP("CommandRegistrar", "F&ont Color"), "FORMAT/Font Color", 220, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onFontColor);
    REG_CMD_TOOL_ICON("format.highlightColor", QT_TRANSLATE_NOOP("CommandRegistrar", "&Highlight Color"), "FORMAT/Highlight Color", 225, true, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onHighlightColor);

    // Without keys: Ctrl+Shift+C and Ctrl+Shift+V are the clipboard's here (Paste Text Only)
    REG_CMD_TOOL_ICON("format.copyFormatting", QT_TRANSLATE_NOOP("CommandRegistrar", "Cop&y Formatting"), "FORMAT/Copy Formatting", 230, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onCopyFormatting);
    REG_CMD_TOOL_ICON("format.pasteFormatting", QT_TRANSLATE_NOOP("CommandRegistrar", "Paste Fo&rmatting"), "FORMAT/Paste Formatting", 235, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onPasteFormatting);

    // As in Word: Ctrl+Space, and Cmd+\ on macOS, where Cmd+Space opens Spotlight
    REG_CMD_KEY_CB("format.clearFormatting", QT_TRANSLATE_NOOP("CommandRegistrar", "Clear For&matting"), "FORMAT/Clear Formatting", 240, false, 0,
                   clearFormattingShortcut, callbacks.onClearFormatting);

    // =========================================================================
    // TABLE MENU
    // =========================================================================

    // The table of the cursor: its rows, columns and cells, its header, the order of its
    // rows, text into a table and back, and its properties (gui/dialogs/table_dialogs.h).
    // Without keys of their own: Alt+Shift+Up and Alt+Shift+Down of the text move its rows
    // (editor/text_keys.h). The access keys are those of the cell's context menu too.
    const auto table = [&callbacks](TableCommand command) {
        return [onTable = callbacks.onTable, command]() {
            if (onTable) {
                onTable(command);
            }
        };
    };
    REG_CMD_CB("table.insert", QT_TRANSLATE_NOOP("CommandRegistrar", "Insert &Table..."), "TABLE/Insert Table...", 10, true, 0,
               table(TableCommand::InsertTable));
    REG_CMD_CB("table.insertRowsAbove", QT_TRANSLATE_NOOP("CommandRegistrar", "Rows &Above"), "TABLE/Insert/Rows Above", 20, false, 0,
               table(TableCommand::InsertRowsAbove));
    REG_CMD_CB("table.insertRowsBelow", QT_TRANSLATE_NOOP("CommandRegistrar", "Rows &Below"), "TABLE/Insert/Rows Below", 21, false, 0,
               table(TableCommand::InsertRowsBelow));
    REG_CMD_CB("table.insertColumnsLeft", QT_TRANSLATE_NOOP("CommandRegistrar", "Columns to the &Left"), "TABLE/Insert/Columns to the Left", 22, false, 0,
               table(TableCommand::InsertColumnsLeft));
    REG_CMD_CB("table.insertColumnsRight", QT_TRANSLATE_NOOP("CommandRegistrar", "Columns to the &Right"), "TABLE/Insert/Columns to the Right", 23, false, 0,
               table(TableCommand::InsertColumnsRight));
    REG_CMD_CB("table.deleteRows", QT_TRANSLATE_NOOP("CommandRegistrar", "&Rows"), "TABLE/Delete/Rows", 30, false, 0,
               table(TableCommand::DeleteRows));
    REG_CMD_CB("table.deleteColumns", QT_TRANSLATE_NOOP("CommandRegistrar", "&Columns"), "TABLE/Delete/Columns", 31, false, 0,
               table(TableCommand::DeleteColumns));
    REG_CMD_CB("table.deleteTable", QT_TRANSLATE_NOOP("CommandRegistrar", "&Table"), "TABLE/Delete/Table", 32, false, 0,
               table(TableCommand::DeleteTable));
    REG_CMD_CB("table.selectCell", QT_TRANSLATE_NOOP("CommandRegistrar", "C&ell"), "TABLE/Select/Cell", 40, false, 0,
               table(TableCommand::SelectCell));
    REG_CMD_CB("table.selectRow", QT_TRANSLATE_NOOP("CommandRegistrar", "&Row"), "TABLE/Select/Row", 41, false, 0,
               table(TableCommand::SelectRow));
    REG_CMD_CB("table.selectColumn", QT_TRANSLATE_NOOP("CommandRegistrar", "&Column"), "TABLE/Select/Column", 42, false, 0,
               table(TableCommand::SelectColumn));
    REG_CMD_CB("table.selectTable", QT_TRANSLATE_NOOP("CommandRegistrar", "&Table"), "TABLE/Select/Table", 43, true, 0,
               table(TableCommand::SelectTable));
    REG_CMD_CB("table.mergeCells", QT_TRANSLATE_NOOP("CommandRegistrar", "Mer&ge Cells"), "TABLE/Merge Cells", 50, false, 0,
               table(TableCommand::MergeCells));
    REG_CMD_CB("table.splitCells", QT_TRANSLATE_NOOP("CommandRegistrar", "Spl&it Cells..."), "TABLE/Split Cells...", 60, true, 0,
               table(TableCommand::SplitCells));
    REG_CMD_CB("table.distributeRows", QT_TRANSLATE_NOOP("CommandRegistrar", "Distribute Ro&ws Evenly"), "TABLE/Distribute Rows Evenly", 70, false, 0,
               table(TableCommand::DistributeRows));
    REG_CMD_CB("table.distributeColumns", QT_TRANSLATE_NOOP("CommandRegistrar", "Distribute Col&umns Evenly"), "TABLE/Distribute Columns Evenly", 80, true, 0,
               table(TableCommand::DistributeColumns));
    REG_CMD_CB("table.headerRows", QT_TRANSLATE_NOOP("CommandRegistrar", "Repeat &Header Rows"), "TABLE/Repeat Header Rows", 90, true, 0,
               table(TableCommand::HeaderRows));
    REG_CMD_CB("table.sort", QT_TRANSLATE_NOOP("CommandRegistrar", "S&ort..."), "TABLE/Sort...", 100, false, 0,
               table(TableCommand::Sort));
    REG_CMD_CB("table.textToTable", QT_TRANSLATE_NOOP("CommandRegistrar", "&Text to Table..."), "TABLE/Convert/Text to Table...", 110, false, 0,
               table(TableCommand::TextToTable));
    REG_CMD_CB("table.tableToText", QT_TRANSLATE_NOOP("CommandRegistrar", "Table to Te&xt..."), "TABLE/Convert/Table to Text...", 111, true, 0,
               table(TableCommand::TableToText));
    REG_CMD_CB("table.properties", QT_TRANSLATE_NOOP("CommandRegistrar", "Ta&ble Properties..."), "TABLE/Table Properties...", 120, false, 0,
               table(TableCommand::Properties));
    if (Command* header = registry.getCommand("table.headerRows")) {
        header->isChecked = []() { return false; };  // the text at the cursor checks it
    }

    // =========================================================================
    // IMAGE MENU
    // =========================================================================

    // The selected image of the text: its window, its caption and description, its turn,
    // mirror and filter, its crop and size, its file. Ctrl+] and Ctrl+[ make it larger and
    // smaller and Esc goes back to the text (fixed keys of the selected image,
    // editor/book_editor.h). The access keys are those of its context menu too, unique there
    // with the items of the text (Copy, Paste, Add To Do).
    const auto image = [&callbacks](ImageCommand command) {
        return [onImage = callbacks.onImage, command]() {
            if (onImage) {
                onImage(command);
            }
        };
    };
    REG_CMD_KEY_CB("image.properties", QT_TRANSLATE_NOOP("CommandRegistrar", "Image Prop&erties..."), "IMAGE/Image Properties...", 10, false, 0,
                   imagePropertiesShortcut, image(ImageCommand::Properties));
    REG_CMD_CB("image.addCaption", QT_TRANSLATE_NOOP("CommandRegistrar", "Insert Capt&ion..."), "IMAGE/Insert Caption...", 20, false, 0,
               image(ImageCommand::AddCaption));
    REG_CMD_CB("image.altText", QT_TRANSLATE_NOOP("CommandRegistrar", "A&lternative Text..."), "IMAGE/Alternative Text...", 30, true, 0,
               image(ImageCommand::AltText));
    REG_CMD_CB("image.rotateRight", QT_TRANSLATE_NOOP("CommandRegistrar", "Rotate &Right 90°"), "IMAGE/Rotate/Rotate Right 90°", 40, false, 0,
               image(ImageCommand::RotateRight));
    REG_CMD_CB("image.rotateLeft", QT_TRANSLATE_NOOP("CommandRegistrar", "Rotate &Left 90°"), "IMAGE/Rotate/Rotate Left 90°", 41, false, 0,
               image(ImageCommand::RotateLeft));
    REG_CMD_CB("image.flipHorizontal", QT_TRANSLATE_NOOP("CommandRegistrar", "Flip &Horizontally"), "IMAGE/Rotate/Flip Horizontally", 42, false, 0,
               image(ImageCommand::FlipHorizontal));
    REG_CMD_CB("image.flipVertical", QT_TRANSLATE_NOOP("CommandRegistrar", "Flip &Vertically"), "IMAGE/Rotate/Flip Vertically", 43, true, 0,
               image(ImageCommand::FlipVertical));
    REG_CMD_CB("image.rotateOptions", QT_TRANSLATE_NOOP("CommandRegistrar", "&More Rotation Options..."), "IMAGE/Rotate/More Rotation Options...", 44, false, 0,
               image(ImageCommand::RotateOptions));
    REG_CMD_CB("image.filterNone", QT_TRANSLATE_NOOP("CommandRegistrar", "&Original"), "IMAGE/Filter/Original", 50, false, 0,
               image(ImageCommand::FilterNone));
    REG_CMD_CB("image.filterGrayscale", QT_TRANSLATE_NOOP("CommandRegistrar", "&Grayscale"), "IMAGE/Filter/Grayscale", 51, false, 0,
               image(ImageCommand::FilterGrayscale));
    REG_CMD_CB("image.filterSepia", QT_TRANSLATE_NOOP("CommandRegistrar", "&Sepia"), "IMAGE/Filter/Sepia", 52, false, 0,
               image(ImageCommand::FilterSepia));
    REG_CMD_CB("image.filterBlackWhite", QT_TRANSLATE_NOOP("CommandRegistrar", "&Black and White"), "IMAGE/Filter/Black and White", 53, false, 0,
               image(ImageCommand::FilterBlackWhite));
    REG_CMD_CB("image.filterWash", QT_TRANSLATE_NOOP("CommandRegistrar", "&Washout"), "IMAGE/Filter/Washout", 54, true, 0,
               image(ImageCommand::FilterWash));
    // The filter of the selected image for every image of the book (open texts and closed)
    REG_CMD_CB("image.filterAll", QT_TRANSLATE_NOOP("CommandRegistrar", "Same Filter for &All Images..."), "IMAGE/Filter/Same Filter for All Images...", 55, false, 0,
               image(ImageCommand::FilterAll));
    REG_CMD_CB("image.colorOptions", QT_TRANSLATE_NOOP("CommandRegistrar", "&Colors and Brightness..."), "IMAGE/Filter/Colors and Brightness...", 56, false, 0,
               image(ImageCommand::ColorOptions));
    REG_CMD_CB("image.crop", QT_TRANSLATE_NOOP("CommandRegistrar", "C&rop"), "IMAGE/Crop", 60, false, 0,
               image(ImageCommand::Crop));
    REG_CMD_CB("image.originalSize", QT_TRANSLATE_NOOP("CommandRegistrar", "Original Si&ze"), "IMAGE/Original Size", 70, true, 0,
               image(ImageCommand::OriginalSize));
    REG_CMD_CB("image.change", QT_TRANSLATE_NOOP("CommandRegistrar", "C&hange Image..."), "IMAGE/Change Image...", 80, false, 0,
               image(ImageCommand::Change));
    REG_CMD_CB("image.saveAs", QT_TRANSLATE_NOOP("CommandRegistrar", "Sa&ve as Image..."), "IMAGE/Save as Image...", 90, false, 0,
               image(ImageCommand::SaveAs));
    // The filter of the selected image is checked (MainWindow)
    for (const char* id : {"image.filterNone", "image.filterGrayscale", "image.filterSepia",
                           "image.filterBlackWhite", "image.filterWash"}) {
        if (Command* filter = registry.getCommand(id)) {
            filter->isChecked = []() { return false; };
        }
    }

    // =========================================================================
    // TOOLS MENU
    // =========================================================================

    // Statistics submenu (analysis tools only, panels in VIEW)
    REG_CMD("tools.stats.full", QT_TRANSLATE_NOOP("CommandRegistrar", "&Full Statistics..."), "TOOLS/Statistics/Full Statistics...", 10, false, 2);

    REG_CMD_TOOL_ICON("tools.stats.wordCount", QT_TRANSLATE_NOOP("CommandRegistrar", "&Word Count"), "TOOLS/Statistics/Word Count", 20, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      nullptr);

    // Turns the spelling as you type on and off (SpellingCoordinator gives it its callback)
    REG_CMD_TOOL_ICON("tools.spellcheck", QT_TRANSLATE_NOOP("CommandRegistrar", "Check S&pelling as You Type"), "TOOLS/Check Spelling as You Type", 40, false, 0,
                      KeyboardShortcut(Qt::Key_F7, Qt::ShiftModifier),
                      IconSet(),
                      nullptr);

    // Selects the next misspelled word or grammar issue and offers what to put in its place
    // (SpellingCoordinator)
    REG_CMD_KEY("tools.nextMisspelling", QT_TRANSLATE_NOOP("CommandRegistrar", "&Next Spelling or Grammar Issue"), "TOOLS/Next Spelling or Grammar Issue", 45, false, 0,
                KeyboardShortcut(Qt::Key_F7, Qt::NoModifier));

    // Turns the grammar as you type on and off (GrammarCoordinator gives it its callbacks); on
    // the Tools toolbar right after the spelling
    REG_CMD_TOOL_ICON("tools.grammar", QT_TRANSLATE_NOOP("CommandRegistrar", "Check &Grammar as You Type"), "TOOLS/Check Grammar as You Type", 50, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      nullptr);

    // Turns auto-correct on and off as a whole (AutoCorrectCoordinator gives it its callback);
    // on the Tools toolbar after the checking of the text
    REG_CMD_TOOL_ICON("tools.autocorrect", QT_TRANSLATE_NOOP("CommandRegistrar", "A&uto-correct as You Type"), "TOOLS/Auto-correct as You Type", 55, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      nullptr);

    // The panel Repetitions with the keyboard in it, and the next repetition of a word in the
    // text (RepetitionsCoordinator gives them their callbacks); Ctrl+Shift+F7 beside F7, the next
    // spelling or grammar issue
    REG_CMD("tools.repetitions", QT_TRANSLATE_NOOP("CommandRegistrar", "W&ord Repetitions"), "TOOLS/Word Repetitions", 57, false, 0);
    REG_CMD_KEY("tools.nextRepetition", QT_TRANSLATE_NOOP("CommandRegistrar", "Ne&xt Repetition"), "TOOLS/Next Repetition", 58, false, 0,
                KeyboardShortcut(Qt::Key_F7, Qt::ControlModifier | Qt::ShiftModifier));

    REG_CMD("tools.readability", QT_TRANSLATE_NOOP("CommandRegistrar", "&Readability Score"), "TOOLS/Readability Score", 60, true, 2);

    REG_CMD("tools.backupNow", QT_TRANSLATE_NOOP("CommandRegistrar", "&Backup Now"), "TOOLS/Backup Now", 100, false, 2);
    REG_CMD("tools.autoSaveSettings", QT_TRANSLATE_NOOP("CommandRegistrar", "&Auto-Save Settings..."), "TOOLS/Auto-Save Settings...", 110, true, 1);

    // Plugins submenu
    REG_CMD("tools.plugins.manager", QT_TRANSLATE_NOOP("CommandRegistrar", "Plugin &Manager..."), "TOOLS/Plugins/Plugin Manager...", 130, false, 2);
    REG_CMD("tools.plugins.updates", QT_TRANSLATE_NOOP("CommandRegistrar", "Check for &Updates..."), "TOOLS/Plugins/Check for Updates...", 150, true, 2);
    REG_CMD("tools.plugins.reload", QT_TRANSLATE_NOOP("CommandRegistrar", "&Reload Plugins"), "TOOLS/Plugins/Reload Plugins", 160, false, 2);

    REG_CMD("tools.challenges", QT_TRANSLATE_NOOP("CommandRegistrar", "&Challenges && Badges..."), "TOOLS/Challenges & Badges...", 170, false, 2);
    REG_CMD("tools.addChallenge", QT_TRANSLATE_NOOP("CommandRegistrar", "A&dd Challenge..."), "TOOLS/Add Challenge...", 175, false, 1);
    REG_CMD("tools.writingGoals", QT_TRANSLATE_NOOP("CommandRegistrar", "&Writing Goals && Deadlines..."), "TOOLS/Writing Goals & Deadlines...", 180, false, 2);
    // Settings > Statistics and Goals (WritingStatsCoordinator); on the Statistics toolbar
    REG_CMD("tools.statsSettings", QT_TRANSLATE_NOOP("CommandRegistrar", "Statistics S&ettings..."), "TOOLS/Statistics Settings...", 185, true, 1);

    // OpenSpec #00037: Toolbar Manager command (used in Quick Actions toolbar)
    REG_CMD_TOOL_ICON("tools.toolbarManager", QT_TRANSLATE_NOOP("CommandRegistrar", "Customize &Toolbars..."), "TOOLS/Customize Toolbars...", 210, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      callbacks.onToolbarManager);

    // =========================================================================
    // ASSISTANT MENU
    // =========================================================================

    REG_CMD("assistant.ask", QT_TRANSLATE_NOOP("CommandRegistrar", "&Ask Assistant..."), "ASSISTANT/Ask Assistant...", 10, true, 2);

    REG_CMD("assistant.switch", QT_TRANSLATE_NOOP("CommandRegistrar", "&Switch Assistant..."), "ASSISTANT/Switch Assistant...", 20, true, 2);

    // Assistant Actions submenu
    REG_CMD("assistant.action.grammar", QT_TRANSLATE_NOOP("CommandRegistrar", "Check &Grammar"), "ASSISTANT/Assistant Actions/Check Grammar", 30, false, 2);
    REG_CMD("assistant.action.style", QT_TRANSLATE_NOOP("CommandRegistrar", "&Improve Style"), "ASSISTANT/Assistant Actions/Improve Style", 40, false, 2);
    REG_CMD("assistant.action.plot", QT_TRANSLATE_NOOP("CommandRegistrar", "&Analyze Plot"), "ASSISTANT/Assistant Actions/Analyze Plot", 50, false, 2);
    REG_CMD("assistant.action.research", QT_TRANSLATE_NOOP("CommandRegistrar", "&Research Topic..."), "ASSISTANT/Assistant Actions/Research Topic...", 60, false, 2);
    REG_CMD("assistant.action.speedDraft", QT_TRANSLATE_NOOP("CommandRegistrar", "&Speed Draft Mode"), "ASSISTANT/Assistant Actions/Speed Draft Mode", 70, false, 2);

    REG_CMD("assistant.settings", QT_TRANSLATE_NOOP("CommandRegistrar", "Assistant Se&ttings..."), "ASSISTANT/Assistant Settings...", 80, false, 2);

    // =========================================================================
    // VIEW MENU
    // =========================================================================

    // Dashboard command - shows/activates Dashboard tab (OpenSpec #00036 Phase D)
    REG_CMD_CB("view.dashboard", QT_TRANSLATE_NOOP("CommandRegistrar", "&Dashboard"), "VIEW/Dashboard", 5, true, 0,
               callbacks.onDashboard);

    // Panel toggle commands - registered here for CommandRegistry/Toolbar system
    // Execute callbacks are set later in createDocks() after dock widgets exist
    REG_CMD_KEY("view.navigator", QT_TRANSLATE_NOOP("CommandRegistrar", "&Navigator"), "VIEW/Panels/Navigator", 10, false, 0,
                KeyboardShortcut(Qt::Key_F2, Qt::NoModifier));

    // F8, not F3: F3 is Find Next (edit.findNext)
    REG_CMD_KEY("view.properties", QT_TRANSLATE_NOOP("CommandRegistrar", "&Properties"), "VIEW/Panels/Properties", 20, false, 0,
                KeyboardShortcut(Qt::Key_F8, Qt::NoModifier));

    // A panel of its own, or the lower part of Properties (Settings > Appearance > General)
    REG_CMD("view.paragraph", QT_TRANSLATE_NOOP("CommandRegistrar", "Pa&ragraph and Styles"), "VIEW/Panels/Paragraph and Styles", 25, false, 0);

    REG_CMD_KEY("view.log", QT_TRANSLATE_NOOP("CommandRegistrar", "&Log"), "VIEW/Panels/Log", 30, false, 0,
                KeyboardShortcut(Qt::Key_F4, Qt::NoModifier));

    REG_CMD_KEY("view.search", QT_TRANSLATE_NOOP("CommandRegistrar", "&Search"), "VIEW/Panels/Search", 40, false, 0,
                KeyboardShortcut(Qt::Key_F5, Qt::NoModifier));

    REG_CMD_KEY("view.assistant", QT_TRANSLATE_NOOP("CommandRegistrar", "&Assistant"), "VIEW/Panels/Assistant", 50, false, 0,
                KeyboardShortcut(Qt::Key_F6, Qt::NoModifier));

    // F9: F7 stays for spelling
    REG_CMD_KEY("view.annotations", QT_TRANSLATE_NOOP("CommandRegistrar", "Ann&otations"), "VIEW/Panels/Annotations", 60, false, 0,
                KeyboardShortcut(Qt::Key_F9, Qt::NoModifier));

    // No key of its own: Next Spelling or Grammar Issue (F7) opens it at the issue it finds
    REG_CMD("view.spelling", QT_TRANSLATE_NOOP("CommandRegistrar", "Spelling and &Grammar"), "VIEW/Panels/Spelling and Grammar", 70, false, 0);
    // No key of its own: Tools > Word Repetitions opens it with the keyboard in it
    REG_CMD("view.repetitionsPanel", QT_TRANSLATE_NOOP("CommandRegistrar", "R&epetitions"), "VIEW/Panels/Repetitions", 75, false, 0);

    // The views of the writing: today, the week, 30 days, the book, the times of writing
    REG_CMD("view.statistics", QT_TRANSLATE_NOOP("CommandRegistrar", "Statis&tics"), "VIEW/Panels/Statistics", 80, false, 0);

    // View Mode submenu (OpenSpec #00042 Phase 7.3)
    // View modes for BookEditor - radio group (only one can be active)
    REG_CMD_TOOL_ICON("view.mode.continuous", QT_TRANSLATE_NOOP("CommandRegistrar", "&Continuous"), "VIEW/View Mode/Continuous", 55, false, 0,
                      KeyboardShortcut(Qt::Key_1, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onViewModeContinuous);
    REG_CMD_TOOL_ICON("view.mode.page", QT_TRANSLATE_NOOP("CommandRegistrar", "&Page Layout"), "VIEW/View Mode/Page Layout", 56, false, 0,
                      KeyboardShortcut(Qt::Key_2, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onViewModePage);

    // Distraction-Free, typewriter scrolling and Focus: toggles on top of the view mode
    // (the window fills the screen and hides its bars and panels; the cursor line stays at
    // a fixed height of the view; every paragraph but the cursor's is dimmed). The id of
    // Distraction-Free stays from the time it was a view mode: toolbars and shortcuts the
    // user set keep it.
    REG_CMD_TOOL_ICON("view.mode.distraction-free", QT_TRANSLATE_NOOP("CommandRegistrar", "D&istraction-Free"), "VIEW/Distraction-Free", 59, false, 0,
                      KeyboardShortcut(Qt::Key_F11, Qt::ShiftModifier),
                      IconSet(),
                      callbacks.onDistractionFreeToggle);
    REG_CMD_KEY_CB("view.typewriter", QT_TRANSLATE_NOOP("CommandRegistrar", "Type&writer Scrolling"), "VIEW/Typewriter Scrolling", 60, false, 0,
                   KeyboardShortcut(Qt::Key_3, Qt::ControlModifier),
                   callbacks.onTypewriterToggle);
    REG_CMD_TOOL_ICON("view.focus", QT_TRANSLATE_NOOP("CommandRegistrar", "&Focus"), "VIEW/Focus", 61, false, 0,
                      KeyboardShortcut(Qt::Key_4, Qt::ControlModifier),
                      IconSet(),
                      callbacks.onFocusToggle);

    // The light or dark paper of every editor, as in the editor's context menu, and the
    // border around the text of every editor, as in the Settings dialog
    REG_CMD_CB("view.darkPaper", QT_TRANSLATE_NOOP("CommandRegistrar", "Dar&k Paper"), "VIEW/Dark Paper", 62, false, 0,
               callbacks.onDarkPaperToggle);
    REG_CMD_CB("view.textFrameBorder", QT_TRANSLATE_NOOP("CommandRegistrar", "Te&xt Frame Border"), "VIEW/Text Frame Border", 63, false, 0,
               callbacks.onTextFrameBorderToggle);

    // The working numbers on the margin of every editor, also as in the Settings dialog: one
    // choice of three (the numbering on chosen again turns it off, as its toolbar button)
    const auto toNumbering = [&callbacks](const char* mode) -> std::function<void()> {
        if (!callbacks.onMarginNumbering) {
            return {};
        }
        return [onNumbering = callbacks.onMarginNumbering, mode = QString::fromLatin1(mode)]() {
            onNumbering(mode);
        };
    };
    REG_CMD_CB("view.marginNumbers.none", QT_TRANSLATE_NOOP("CommandRegistrar", "&No Numbering"), "VIEW/Margin Numbering/No Numbering", 64, false, 0,
               toNumbering(editor::MARGIN_NUMBERING_NONE));
    REG_CMD_CB("view.marginNumbers.lines", QT_TRANSLATE_NOOP("CommandRegistrar", "&Line Numbers"), "VIEW/Margin Numbering/Line Numbers", 65, false, 0,
               toNumbering(editor::MARGIN_NUMBERING_LINES));
    REG_CMD_CB("view.marginNumbers.paragraphs", QT_TRANSLATE_NOOP("CommandRegistrar", "&Paragraph Numbers"), "VIEW/Margin Numbering/Paragraph Numbers", 66, true, 0,
               toNumbering(editor::MARGIN_NUMBERING_PARAGRAPHS));
    REG_CMD_CB("view.marginNumbers.settings", QT_TRANSLATE_NOOP("CommandRegistrar", "Numbering &Settings..."), "VIEW/Margin Numbering/Numbering Settings...", 67, true, 0,
               callbacks.onMarginNumberingSettings);

    // The cards of the annotations under their paragraphs, a setting of every editor as in
    // the Settings dialog; collapsing and expanding all of them in the editor in front
    // (AnnotationsCoordinator)
    REG_CMD_CB("view.annotationCards", QT_TRANSLATE_NOOP("CommandRegistrar", "Annotation &Cards Under Paragraphs"), "VIEW/Annotation Cards Under Paragraphs", 68, false, 0,
               callbacks.onAnnotationCardsToggle);
    REG_CMD("view.collapseAllAnnotations", QT_TRANSLATE_NOOP("CommandRegistrar", "Collapse &All Annotations"), "VIEW/Collapse All Annotations", 69, false, 0);
    REG_CMD("view.expandAllAnnotations", QT_TRANSLATE_NOOP("CommandRegistrar", "&Expand All Annotations"), "VIEW/Expand All Annotations", 70, true, 0);

    // Perspectives submenu
    REG_CMD("view.perspectives.writer", QT_TRANSLATE_NOOP("CommandRegistrar", "&Writer"), "VIEW/Perspectives/Writer", 71, false, 1);
    REG_CMD("view.perspectives.editor", QT_TRANSLATE_NOOP("CommandRegistrar", "&Editor"), "VIEW/Perspectives/Editor", 80, false, 1);
    REG_CMD("view.perspectives.researcher", QT_TRANSLATE_NOOP("CommandRegistrar", "&Researcher"), "VIEW/Perspectives/Researcher", 90, false, 1);
    REG_CMD("view.perspectives.planner", QT_TRANSLATE_NOOP("CommandRegistrar", "&Planner"), "VIEW/Perspectives/Planner", 100, true, 1);
    REG_CMD("view.perspectives.save", QT_TRANSLATE_NOOP("CommandRegistrar", "&Save Current Perspective..."), "VIEW/Perspectives/Save Current Perspective...", 110, false, 1);
    REG_CMD("view.perspectives.manage", QT_TRANSLATE_NOOP("CommandRegistrar", "&Manage Perspectives..."), "VIEW/Perspectives/Manage Perspectives...", 120, false, 1);

    // OpenSpec #00030: VIEW/Toolbars submenu is created DYNAMICALLY
    // by ToolbarManager::createViewMenuActions() - no static commands here.
    // Toolbar toggle actions + "Toolbar Manager..." are all dynamic.

    REG_CMD("view.showStatusBar", QT_TRANSLATE_NOOP("CommandRegistrar", "Show &Status Bar"), "VIEW/Show Status Bar", 180, false, 0);
    REG_CMD("view.showStatsBar", QT_TRANSLATE_NOOP("CommandRegistrar", "Show Statistics &Bar"), "VIEW/Show Statistics Bar", 190, false, 1);
    // The repetitions of words marked in the text while the panel Repetitions is open: the
    // setting editor.repetitions.highlight (RepetitionsCoordinator gives it its callback)
    REG_CMD_TOOL_ICON("view.repetitionMarks", QT_TRANSLATE_NOOP("CommandRegistrar", "Repetiti&ons in Text"), "VIEW/Repetitions in Text", 205, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      nullptr);
    // Ctrl+Shift+8 (Shift+Cmd+8 on macOS), as in Word: written with the key, not with the "*"
    // it types on some keyboards, on every system; Qt matches the key on every keyboard
    REG_CMD_KEY_CB("view.showFormattingMarks", QT_TRANSLATE_NOOP("CommandRegistrar", "Show Formattin&g Marks"), "VIEW/Show Formatting Marks", 210, true, 0,
                   KeyboardShortcut(Qt::Key_8, Qt::ControlModifier | Qt::ShiftModifier), callbacks.onFormattingMarksToggle);

    REG_CMD_KEY_CB("view.zoomIn", QT_TRANSLATE_NOOP("CommandRegistrar", "Zoom &In"), "VIEW/Zoom/Zoom In", 220, false, 0,
                   KeyboardShortcut(Qt::Key_Plus, Qt::ControlModifier), callbacks.onZoomIn);
    REG_CMD_KEY_CB("view.zoomOut", QT_TRANSLATE_NOOP("CommandRegistrar", "Zoom &Out"), "VIEW/Zoom/Zoom Out", 221, false, 0,
                   KeyboardShortcut(Qt::Key_Minus, Qt::ControlModifier), callbacks.onZoomOut);
    REG_CMD_KEY_CB("view.resetZoom", QT_TRANSLATE_NOOP("CommandRegistrar", "Zoom &100%"), "VIEW/Zoom/Zoom 100%", 222, true, 0,
                   KeyboardShortcut(Qt::Key_0, Qt::ControlModifier), callbacks.onZoomReset);
    REG_CMD_CB("view.zoomPageWidth", QT_TRANSLATE_NOOP("CommandRegistrar", "&Page Width"), "VIEW/Zoom/Page Width", 223, false, 0,
               callbacks.onZoomPageWidth);
    REG_CMD_CB("view.zoomWholePage", QT_TRANSLATE_NOOP("CommandRegistrar", "&Whole Page"), "VIEW/Zoom/Whole Page", 224, false, 0,
               callbacks.onZoomWholePage);

    // F11 (Control+Cmd+F on macOS, see SHORTCUTS)
    REG_CMD_KEY("view.fullScreen", QT_TRANSLATE_NOOP("CommandRegistrar", "F&ull Screen"), "VIEW/Full Screen", 250, true, 0,
                fullScreenShortcut);

    REG_CMD_CB("view.resetLayout", QT_TRANSLATE_NOOP("CommandRegistrar", "Reset &Layout"), "VIEW/Reset Layout", 260, false, 0,
               callbacks.onResetLayout);

    // =========================================================================
    // HELP MENU
    // =========================================================================

    // OpenSpec #00030: F1 for Help (standard)
    REG_CMD_KEY("help.manual", QT_TRANSLATE_NOOP("CommandRegistrar", "Kalahari &Help"), "HELP/Kalahari Help", 10, false, 2,
                KeyboardShortcut(Qt::Key_F1, Qt::NoModifier));
    // F1 waits for the help: the list of the shortcuts says it is not there yet
    if (Command* help = registry.getCommand("help.manual")) {
        help->unavailableNote =
            QCoreApplication::translate("CommandRegistrar", "not available yet", "Kalahari Help")
                .toStdString();
    }
    REG_CMD("help.tutorial", QT_TRANSLATE_NOOP("CommandRegistrar", "&Getting Started Tutorial"), "HELP/Getting Started Tutorial", 20, true, 2);

    // Opens Settings > Keyboard Shortcuts: the list of the shortcuts, where they are changed
    REG_CMD_CB("help.shortcuts", QT_TRANSLATE_NOOP("CommandRegistrar", "&Keyboard Shortcuts"), "HELP/Keyboard Shortcuts", 30, false, 0,
               callbacks.onKeyboardShortcuts);
    REG_CMD("help.tipsTricks", QT_TRANSLATE_NOOP("CommandRegistrar", "&Tips && Tricks"), "HELP/Tips & Tricks", 40, false, 2);
    REG_CMD("help.whatsNew", QT_TRANSLATE_NOOP("CommandRegistrar", "&What's New"), "HELP/What's New", 50, true, 1);

    REG_CMD("help.reportBug", QT_TRANSLATE_NOOP("CommandRegistrar", "&Report a Bug..."), "HELP/Report a Bug...", 60, false, 1);
    REG_CMD("help.suggestFeature", QT_TRANSLATE_NOOP("CommandRegistrar", "&Suggest a Feature..."), "HELP/Suggest a Feature...", 70, false, 1);
    REG_CMD("help.communityForum", QT_TRANSLATE_NOOP("CommandRegistrar", "&Community Forum"), "HELP/Community Forum", 80, true, 2);

    REG_CMD("help.checkUpdates", QT_TRANSLATE_NOOP("CommandRegistrar", "Check for &Updates..."), "HELP/Check for Updates...", 90, true, 2);

    REG_CMD_CB("help.about", QT_TRANSLATE_NOOP("CommandRegistrar", "&About Kalahari"), "HELP/About Kalahari", 100, false, 0,
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
