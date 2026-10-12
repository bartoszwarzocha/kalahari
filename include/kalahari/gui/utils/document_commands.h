/// @file document_commands.h
/// @brief The commands that work on the document in front

#ifndef KALAHARI_GUI_UTILS_DOCUMENT_COMMANDS_H
#define KALAHARI_GUI_UTILS_DOCUMENT_COMMANDS_H

#include <functional>
#include <string>
#include <vector>

namespace kalahari {
namespace gui {

class CommandRegistry;

namespace utils {

/// @brief The commands that work on the document of the tab in front, a chapter or a text
/// file: Find with Find Next, Find Previous and Find and Replace, the character formats,
/// the alignments, the view mode and the zoom
const std::vector<std::string>& documentCommands();

/// @brief Grey out the commands of the document while no document is in front
///
/// Over the Dashboard or without a tab they would do nothing: they are greyed out, also
/// their keys, as in Word without a document. The caller shows their states again when
/// the tab in front changes (updateDocumentCommands()).
/// @param registry The commands
/// @param hasDocument Whether a document is in front
void bindToDocument(CommandRegistry& registry, const std::function<bool()>& hasDocument);

/// @brief Show the enabled and checked states of the commands of the document again
/// @param registry The commands
void updateDocumentCommands(CommandRegistry& registry);

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_DOCUMENT_COMMANDS_H
