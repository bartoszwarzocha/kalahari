/// @file command_registrar.h
/// @brief Application command registration with CommandRegistry
///
/// This module is extracted from MainWindow to reduce its size (OpenSpec #00038).
/// Contains all command registrations for the application.
///
/// Uses a callback struct pattern to allow commands to execute methods on MainWindow
/// without creating circular dependencies.

#ifndef KALAHARI_GUI_COMMAND_REGISTRAR_H
#define KALAHARI_GUI_COMMAND_REGISTRAR_H

#include <QString>

#include <functional>

namespace kalahari {
namespace editor {
enum class CaseChange;  // editor/character_format.h
}  // namespace editor

namespace gui {

/// @brief Callbacks for MainWindow actions triggered by commands.
///
/// This struct provides a way for CommandRegistrar to call MainWindow methods
/// without creating a direct dependency. MainWindow creates this struct with
/// lambdas that capture `this` and call the appropriate methods.
///
/// Commands that don't need MainWindow interaction use empty callbacks.
struct CommandCallbacks {
    // =========================================================================
    // FILE COMMANDS
    // =========================================================================
    std::function<void()> onNewDocument;        ///< File > New
    std::function<void()> onNewProject;         ///< File > New Book...
    std::function<void()> onOpenDocument;       ///< File > Open Book...
    std::function<void()> onOpenStandaloneFile; ///< File > Open File...
    std::function<void()> onSaveDocument;       ///< File > Save
    std::function<void()> onSaveAsDocument;     ///< File > Save As...
    std::function<void()> onSaveAll;            ///< File > Save All
    std::function<void()> onCloseDocument;      ///< File > Close Book
    std::function<void()> onImportArchive;      ///< File > Import > Project Archive...
    std::function<void()> onExportArchive;      ///< File > Export > Project Archive...
    std::function<void()> onExit;               ///< File > Exit

    // =========================================================================
    // EDIT COMMANDS
    // =========================================================================
    std::function<void()> onUndo;               ///< Edit > Undo
    std::function<void()> onRedo;               ///< Edit > Redo
    std::function<void()> onCut;                ///< Edit > Cut
    std::function<void()> onCopy;               ///< Edit > Copy
    std::function<void()> onPaste;              ///< Edit > Paste
    std::function<void()> onPasteSourceFormatting;  ///< Edit > Paste with Source Formatting
    std::function<void()> onPasteMergeFormatting;   ///< Edit > Paste and Merge Formatting
    std::function<void()> onPasteTextOnly;      ///< Edit > Paste Text Only
    std::function<void()> onDelete;             ///< Edit > Delete
    std::function<void()> onSelectAll;          ///< Edit > Select All
    std::function<void()> onSelectWord;         ///< Edit > Select Word
    std::function<void()> onSelectParagraph;    ///< Edit > Select Paragraph
    std::function<void()> onFind;               ///< Edit > Find...
    std::function<void()> onFindNext;           ///< Edit > Find Next
    std::function<void()> onFindPrevious;       ///< Edit > Find Previous
    std::function<void()> onFindReplace;        ///< Edit > Find & Replace...
    std::function<void()> onSettings;           ///< Edit > Preferences/Settings

    // =========================================================================
    // BOOK COMMANDS
    // =========================================================================
    std::function<void()> onAddElement;         ///< Book > Add...: Add Element for what is
                                                ///< in front
    std::function<void()> onNewChapter;         ///< Book > New Chapter... (named after the
                                                ///< main texts of the book: New Story...)
    /// Book > New Character..., New Location..., New Item..., New Mind Map..., New
    /// Timeline...: Add Element in the Workshop with the kind of the base package of that id
    /// ("character")
    std::function<void(const QString& kindId)> onNewWorkshopElement;
    std::function<void()> onBookProperties;     ///< Book > Book Properties...

    // =========================================================================
    // INSERT COMMANDS
    // =========================================================================
    std::function<void()> onInsertHorizontalLine;  ///< Insert > Horizontal Line
    std::function<void()> onInsertSceneBreak;   ///< Insert > Scene Break, Book > Scene Break
    std::function<void()> onInsertPageBreak;    ///< Insert > Page Break
    std::function<void()> onSpecialCharacter;   ///< Insert > Special Character...
    std::function<void()> onQuickCharacter;     ///< Insert > Quick Character (the palette)
    /// Insert > Formatting Character: the characters into the text with the keys
    std::function<void(const QString& characters)> onInsertCharacters;

    // =========================================================================
    // FORMAT COMMANDS (OpenSpec #00042 Phase 7.2)
    // =========================================================================
    std::function<void()> onFormatBold;         ///< Format > Bold
    std::function<void()> onFormatItalic;       ///< Format > Italic
    std::function<void()> onFormatUnderline;    ///< Format > Underline
    std::function<void()> onFormatStrikethrough; ///< Format > Text Effects > Strikethrough
    std::function<void()> onClearFormatting;    ///< Format > Clear Formatting

    // Format > Font... and Paragraph..., the windows of the selected text and paragraphs
    std::function<void()> onFont;               ///< Format > Font...
    std::function<void()> onParagraph;          ///< Format > Paragraph...
    std::function<void()> onInsertLink;         ///< Insert > Link... (also edits a link)
    std::function<void()> onInsertTable;        ///< Insert > Table...

    // Format > Text Effects
    std::function<void()> onSuperscript;        ///< Format > Text Effects > Superscript
    std::function<void()> onSubscript;          ///< Format > Text Effects > Subscript
    std::function<void()> onSmallCaps;          ///< Format > Text Effects > Small Caps
    std::function<void()> onAllCaps;            ///< Format > Text Effects > All Caps

    /// Format > Change Case: the letters of the selection, or of the word with the cursor,
    /// in this case
    std::function<void(editor::CaseChange)> onChangeCase;

    // The colors: the command gives the color chosen last in its submenu (a toolbar button)
    std::function<void()> onFontColor;          ///< Format > Font Color
    std::function<void()> onHighlightColor;     ///< Format > Highlight Color

    std::function<void()> onCopyFormatting;     ///< Format > Copy Formatting
    std::function<void()> onPasteFormatting;    ///< Format > Paste Formatting

    // Paragraph Alignment
    std::function<void()> onAlignLeft;          ///< Format > Align Text > Align Left
    std::function<void()> onAlignCenter;        ///< Format > Align Text > Align Center
    std::function<void()> onAlignRight;         ///< Format > Align Text > Align Right
    std::function<void()> onAlignJustify;       ///< Format > Align Text > Justify

    // Indents and lists
    std::function<void()> onIncreaseIndent;     ///< Format > Increase Indent
    std::function<void()> onDecreaseIndent;     ///< Format > Decrease Indent
    std::function<void()> onBullets;            ///< Format > Bullets
    std::function<void()> onNumbering;          ///< Format > Numbering
    std::function<void()> onBulletsNumbering;   ///< Format > Bullets and Numbering...

    /// Format > Text Style: puts the paragraphs of the cursor or the selection in the style
    /// of this id (editor/paragraph_styles.h)
    std::function<void(const QString&)> onParagraphStyle;

    // =========================================================================
    // VIEW COMMANDS
    // =========================================================================
    std::function<void()> onDashboard;          ///< View > Dashboard
    std::function<void()> onResetLayout;        ///< View > Reset Layout

    // View Mode Commands (OpenSpec #00042 Phase 7.3)
    std::function<void()> onViewModeContinuous; ///< View > View Mode > Continuous
    std::function<void()> onViewModePage;       ///< View > View Mode > Page Layout
    std::function<void()> onTypewriterToggle;   ///< View > Typewriter Scrolling
    std::function<void()> onFocusToggle;        ///< View > Focus
    std::function<void()> onDarkPaperToggle;    ///< View > Dark Paper
    std::function<void()> onTextFrameBorderToggle;  ///< View > Text Frame Border
    std::function<void()> onDistractionFreeToggle;  ///< View > Distraction-Free
    std::function<void()> onFormattingMarksToggle;  ///< View > Show Formatting Marks

    // Zoom (the editor in front)
    std::function<void()> onZoomIn;             ///< View > Zoom > Zoom In
    std::function<void()> onZoomOut;            ///< View > Zoom > Zoom Out
    std::function<void()> onZoomReset;          ///< View > Zoom > Zoom 100%
    std::function<void()> onZoomPageWidth;      ///< View > Zoom > Page Width
    std::function<void()> onZoomWholePage;      ///< View > Zoom > Whole Page

    // =========================================================================
    // TOOLS COMMANDS
    // =========================================================================
    std::function<void()> onToolbarManager;     ///< Tools > Customize Toolbars...

    // =========================================================================
    // HELP COMMANDS
    // =========================================================================
    std::function<void()> onKeyboardShortcuts;  ///< Help > Keyboard Shortcuts
    std::function<void()> onAbout;              ///< Help > About Kalahari
};

/// @brief Register all application commands with CommandRegistry.
///
/// This function registers all menu and toolbar commands with the central
/// CommandRegistry. Commands are organized by menu category:
/// - File menu commands
/// - Edit menu commands
/// - Book menu commands
/// - Insert menu commands
/// - Format menu commands
/// - Tools menu commands
/// - Assistant menu commands
/// - View menu commands
/// - Help menu commands
///
/// Call once during application startup, before creating menus and toolbars.
///
/// @param callbacks Struct containing callbacks to MainWindow methods
/// @return Number of commands registered
///
/// @note This was extracted from MainWindow::registerCommands() in OpenSpec #00038.
/// @note Icon registration is handled separately by registerAllIcons().
int registerAllCommands(const CommandCallbacks& callbacks);

} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_COMMAND_REGISTRAR_H
