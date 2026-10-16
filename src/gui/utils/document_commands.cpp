/// @file document_commands.cpp
/// @brief Implementation of the commands that work on the document in front, those of the
/// open book, and Save with Save As

#include "kalahari/gui/utils/document_commands.h"

#include "kalahari/gui/command_registry.h"

#include <QAction>
#include <QCoreApplication>
#include <QPointer>
#include <QShortcut>
#include <QStatusBar>
#include <QWidget>

namespace kalahari {
namespace gui {
namespace utils {

namespace {

/// How long the status bar keeps the reason why a command is greyed out, after its keys
constexpr int REASON_MS = 5000;

/// While @p action is greyed out with a reason, a shortcut of @p window takes its keys and
/// shows the reason in @p statusBar; it follows the keys the writer gives the command
void explainKeysOf(QAction* action, QWidget* window, QStatusBar* statusBar) {
    auto* standIn = new QShortcut(window);
    standIn->setContext(action->shortcutContext());
    standIn->setAutoRepeat(false);
    const QPointer<QAction> command(action);
    const auto follow = [standIn, command]() {
        if (!command) {
            return;
        }
        standIn->setKeys(command->shortcuts());
        // Enabled, the command takes its keys itself: the two never share them
        standIn->setEnabled(!command->isEnabled() && !command->statusTip().isEmpty());
    };
    QObject::connect(action, &QAction::changed, standIn, follow);
    QObject::connect(action, &QObject::destroyed, standIn, &QObject::deleteLater);
    QObject::connect(standIn, &QShortcut::activated, statusBar, [command, statusBar]() {
        if (command) {
            statusBar->showMessage(command->statusTip(), REASON_MS);
        }
    });
    follow();
}

} // namespace

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
            command->disabledReason = []() {
                //: The tooltip of Close Book and of the export of the book's archive while
                //: they are greyed out. In Polish: Żadna książka nie jest otwarta
                return QCoreApplication::translate("DocumentCommands", "No book is open");
            };
        }
    }
    updateOpenBookCommands(registry);
}

void updateOpenBookCommands(CommandRegistry& registry) {
    for (const std::string& id : openBookCommands()) {
        registry.updateActionState(id);
    }
}

void bindSaveCommands(CommandRegistry& registry, const std::function<QString()>& whyNotSave,
                      const std::function<QString()>& whyNotSaveAs) {
    const auto bind = [&registry](const char* id, const std::function<QString()>& whyNot) {
        if (Command* command = registry.getCommand(id)) {
            command->isEnabled = [whyNot]() { return whyNot().isEmpty(); };
            command->disabledReason = whyNot;
        }
    };
    bind("file.save", whyNotSave);
    bind("file.saveAs", whyNotSaveAs);
    updateSaveCommands(registry);
}

void updateSaveCommands(CommandRegistry& registry) {
    registry.updateActionState("file.save");
    registry.updateActionState("file.saveAs");
}

void explainGreyedKeys(CommandRegistry& registry, QWidget* window, QStatusBar* statusBar) {
    std::vector<std::string> ids = openBookCommands();
    ids.insert(ids.end(), {"file.save", "file.saveAs"});
    for (const std::string& id : ids) {
        if (QAction* action = registry.getAction(id)) {
            explainKeysOf(action, window, statusBar);
        }
    }
}

} // namespace utils
} // namespace gui
} // namespace kalahari
