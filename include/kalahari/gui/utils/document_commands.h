/// @file document_commands.h
/// @brief The commands that work on the document in front, those of the open book, and Save
/// with Save As and Save All

#ifndef KALAHARI_GUI_UTILS_DOCUMENT_COMMANDS_H
#define KALAHARI_GUI_UTILS_DOCUMENT_COMMANDS_H

#include <QString>

#include <functional>
#include <string>
#include <vector>

class QStatusBar;
class QWidget;

namespace kalahari {
namespace gui {

class CommandRegistry;

namespace utils {

/// @brief The commands that work on the document of the tab in front, a chapter or a text
/// file: Find with Find Next, Find Previous and Find and Replace, the windows Font,
/// Paragraph and Link, the character formats with their colors, the cases and Copy and Paste
/// Formatting, the alignments, the indents and lists, the paragraph styles, the view mode and
/// the zoom
const std::vector<std::string>& documentCommands();

/// @brief The commands of the document that change it: all of them but Find with Find Next
/// and Find Previous, Copy Formatting, the view mode and the zoom
const std::vector<std::string>& changingDocumentCommands();

/// @brief Grey out the commands of the document while no document is in front, and those that
/// change it while the document in front is read-only
///
/// Over the Dashboard or without a tab they would do nothing: they are greyed out, also
/// their keys, as in Word without a document. Over a chapter of a book open read-only, those
/// that change it are greyed out, their tooltips and the status bar saying why. The caller
/// shows their states again when the tab in front changes (updateDocumentCommands()).
/// @param registry The commands
/// @param hasDocument Whether a document is in front
/// @param isReadOnly Whether the document in front is read-only; none: never
void bindToDocument(CommandRegistry& registry, const std::function<bool()>& hasDocument,
                    const std::function<bool()>& isReadOnly = {});

/// @brief Show the enabled and checked states of the commands of the document again
/// @param registry The commands
void updateDocumentCommands(CommandRegistry& registry);

/// @brief The commands of the File menu that work on the open book: Close Book and the
/// export of its archive (the Book menu has its own rules, see MainWindow)
const std::vector<std::string>& openBookCommands();

/// @brief Grey out the commands of the open book while no book is open
///
/// Without a book they could only say that one must be opened first: their tooltips and the
/// status bar say that no book is open. The caller shows their states again when a book
/// opens or closes (updateOpenBookCommands()).
/// @param registry The commands
/// @param hasBook Whether a book is open
void bindToOpenBook(CommandRegistry& registry, const std::function<bool()>& hasBook);

/// @brief Show the enabled states of the commands of the open book again
/// @param registry The commands
void updateOpenBookCommands(CommandRegistry& registry);

/// @brief Grey out Save, Save As and Save All while they have nothing to save, their tooltips
/// and the status bar saying why
///
/// Save saves the book, or the file outside it in front; Save As only a file outside the
/// book, as a chapter of the book is saved with the book; Save All the book and every file
/// outside it in the tabs. A greyed out command takes no keys either. The caller shows their
/// states again when the tabs, or what the tab in front shows, or the open book change
/// (updateSaveCommands()).
/// @param registry The commands
/// @param whyNotSave Why Save has nothing to save now; empty while it has
/// @param whyNotSaveAs Why Save As has nothing to save now; empty while it has
/// @param whyNotSaveAll Why Save All has nothing to save now; empty while it has
void bindSaveCommands(CommandRegistry& registry, const std::function<QString()>& whyNotSave,
                      const std::function<QString()>& whyNotSaveAs,
                      const std::function<QString()>& whyNotSaveAll);

/// @brief Show the enabled states of Save, Save As and Save All again, with their reasons
/// @param registry The commands
void updateSaveCommands(CommandRegistry& registry);

/// @brief The keys of Close Book, of the export of the book's archive, of Save, of Save As, of
/// Save All, of the commands that change the document, of those of the Book menu and of those
/// that add annotations say why the command is greyed out
///
/// A greyed out command takes no keys, so they would do nothing: while it is greyed out with a
/// reason, a shortcut of @p window takes them and shows the reason in @p statusBar, as its
/// menu item does under the mouse. It follows the keys the writer gives the command.
/// @param registry The commands, bound (bindToOpenBook(), bindSaveCommands(),
/// bindToDocument())
/// @param window The window whose keys these are
/// @param statusBar The status bar that shows the reason
void explainGreyedKeys(CommandRegistry& registry, QWidget* window, QStatusBar* statusBar);

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_DOCUMENT_COMMANDS_H
