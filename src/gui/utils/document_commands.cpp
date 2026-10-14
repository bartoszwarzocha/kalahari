/// @file document_commands.cpp
/// @brief Implementation of the commands that work on the document in front, and those of
/// the open book

#include "kalahari/gui/utils/document_commands.h"

#include "kalahari/gui/command_registry.h"

namespace kalahari {
namespace gui {
namespace utils {

const std::vector<std::string>& documentCommands() {
    static const std::vector<std::string> ids = {
        "edit.find",          "edit.findNext",      "edit.findPrevious",    "edit.findReplace",
        "format.bold",        "format.italic",      "format.underline",     "format.strikethrough",
        "format.alignLeft",   "format.alignCenter", "format.alignRight",    "format.justify",
        "view.mode.continuous", "view.mode.page",   "view.zoomIn",          "view.zoomOut",
        "view.resetZoom",     "view.zoomPageWidth", "view.zoomWholePage"};
    return ids;
}

void bindToDocument(CommandRegistry& registry, const std::function<bool()>& hasDocument) {
    for (const std::string& id : documentCommands()) {
        if (Command* command = registry.getCommand(id)) {
            command->isEnabled = hasDocument;
        }
    }
    updateDocumentCommands(registry);
}

void updateDocumentCommands(CommandRegistry& registry) {
    for (const std::string& id : documentCommands()) {
        registry.updateActionState(id);
    }
}

const std::vector<std::string>& openBookCommands() {
    static const std::vector<std::string> ids = {"file.close", "file.export.archive"};
    return ids;
}

void bindToOpenBook(CommandRegistry& registry, const std::function<bool()>& hasBook) {
    for (const std::string& id : openBookCommands()) {
        if (Command* command = registry.getCommand(id)) {
            command->isEnabled = hasBook;
        }
    }
    updateOpenBookCommands(registry);
}

void updateOpenBookCommands(CommandRegistry& registry) {
    for (const std::string& id : openBookCommands()) {
        registry.updateActionState(id);
    }
}

} // namespace utils
} // namespace gui
} // namespace kalahari
