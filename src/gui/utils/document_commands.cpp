/// @file document_commands.cpp
/// @brief Implementation of the commands that work on the document in front, those of the
/// open book, and Save with Save As and Save All

#include "kalahari/gui/utils/document_commands.h"

#include "kalahari/gui/command_registry.h"

#include <QAction>
#include <QCoreApplication>
#include <QShortcut>
#include <QStatusBar>
#include <QWidget>

namespace kalahari {
namespace gui {
namespace utils {

namespace {

/// How long the status bar keeps the reason why a command is greyed out, after its keys
constexpr int REASON_MS = 5000;

/// While its command is greyed out with a reason, it takes the command's keys and shows the
/// reason in the status bar; it follows the keys the writer gives the command
///
/// It keeps the command by a plain pointer, cleared when the command goes: clang-tidy's
/// analyzer reports a QPointer, kept or copied into a connection, as a use of freed memory.
class GreyedKeys : public QShortcut {
public:
    GreyedKeys(QAction* command, QWidget* window, QStatusBar* statusBar)
        : QShortcut(window), m_command(command) {
        setContext(command->shortcutContext());
        setAutoRepeat(false);
        connect(command, &QAction::changed, this, &GreyedKeys::follow);
        connect(command, &QObject::destroyed, this, &GreyedKeys::forget);
        // The status bar ends the connection when it goes
        connect(this, &QShortcut::activated, statusBar, [this, statusBar]() {
            if (m_command != nullptr) {
                statusBar->showMessage(m_command->statusTip(), REASON_MS);
            }
        });
        follow();
    }

private:
    /// Take the keys of the command, while it is greyed out with a reason
    void follow() {
        if (m_command == nullptr) {
            return;
        }
        setKeys(m_command->shortcuts());
        // Enabled, the command takes its keys itself: the two never share them
        setEnabled(!m_command->isEnabled() && !m_command->statusTip().isEmpty());
    }

    /// Without its command it has nothing to say: it goes as well
    void forget() {
        m_command = nullptr;
        setEnabled(false);
        deleteLater();
    }

    QAction* m_command;  ///< Null once the command is gone (forget())
};

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
                      const std::function<QString()>& whyNotSaveAs,
                      const std::function<QString()>& whyNotSaveAll) {
    const auto bind = [&registry](const char* id, const std::function<QString()>& whyNot) {
        if (Command* command = registry.getCommand(id)) {
            command->isEnabled = [whyNot]() { return whyNot().isEmpty(); };
            command->disabledReason = whyNot;
        }
    };
    bind("file.save", whyNotSave);
    bind("file.saveAs", whyNotSaveAs);
    bind("file.saveAll", whyNotSaveAll);
    updateSaveCommands(registry);
}

void updateSaveCommands(CommandRegistry& registry) {
    registry.updateActionState("file.save");
    registry.updateActionState("file.saveAs");
    registry.updateActionState("file.saveAll");
}

void explainGreyedKeys(CommandRegistry& registry, QWidget* window, QStatusBar* statusBar) {
    std::vector<std::string> ids = openBookCommands();
    ids.insert(ids.end(), {"file.save", "file.saveAs", "file.saveAll"});
    for (const std::string& id : ids) {
        if (QAction* action = registry.getAction(id)) {
            new GreyedKeys(action, window, statusBar);
        }
    }
}

} // namespace utils
} // namespace gui
} // namespace kalahari
