/// @file diagnostic_menus.cpp
/// @brief The items of the Diagnostics and Dev Tools menus

#include "kalahari/gui/diagnostic_menus.h"

#include <QAction>
#include <QCoreApplication>
#include <QMenu>

#include <vector>

namespace kalahari {
namespace gui {

namespace {

/// The texts are DiagnosticController's, as before they were moved here
constexpr char CONTEXT[] = "kalahari::gui::DiagnosticController";

struct Item {
    DiagnosticTool tool;
    const char* text;  ///< With its access key, unique in the menu (test_menu_access_keys)
};

/// Items in groups, a separator between two
using Groups = std::vector<std::vector<Item>>;

const Groups& diagnosticsGroups() {
    static const Groups groups = {
        {{DiagnosticTool::SystemInfo,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "&System Information")},
         {DiagnosticTool::QtEnvironment,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "&Qt Environment")},
         {DiagnosticTool::FileSystemCheck,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "&File System Check")}},
        {{DiagnosticTool::SettingsDump,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "Settings &Dump")},
         {DiagnosticTool::MemoryStats,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "&Memory Statistics")},
         {DiagnosticTool::OpenDocsStats,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController",
                            "&Open Documents Statistics")}},
        {{DiagnosticTool::LoggerTest,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "&Logger Test")},
         {DiagnosticTool::EventBusTest,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "&Event Bus Test")},
         {DiagnosticTool::PluginCheck,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "&Plugin Manager Check")},
         {DiagnosticTool::CommandRegistryDump,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "Command &Registry Dump")}},
        {{DiagnosticTool::PythonEnvironment,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "P&ython Environment")},
         {DiagnosticTool::PythonImportTest,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "Python &Import Test")},
         {DiagnosticTool::PythonMemoryTest,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "Python Memory &Test")},
         {DiagnosticTool::EmbeddedInterpreterStatus,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController",
                            "Em&bedded Interpreter Status")}},
        {{DiagnosticTool::PerformanceBenchmark,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "Performance Be&nchmark")},
         {DiagnosticTool::RenderStats,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "Render St&atistics")}},
        {{DiagnosticTool::ClearLog,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "&Clear Log")}},
    };
    return groups;
}

const Groups& debugGroups() {
    static const Groups groups = {
        {{DiagnosticTool::ForceCrash,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController", "Force Cras&h (Debug Only)")},
         {DiagnosticTool::MemoryLeakTest,
          QT_TRANSLATE_NOOP("kalahari::gui::DiagnosticController",
                            "Memory Lea&k Test (Debug Only)")}},
    };
    return groups;
}

void addGroups(QMenu* menu, const Groups& groups, QObject* receiver,
               const DiagnosticToolRunner& run) {
    for (const auto& group : groups) {
        if (!menu->isEmpty()) {
            menu->addSeparator();
        }
        for (const Item& item : group) {
            QAction* action = menu->addAction(QCoreApplication::translate(CONTEXT, item.text));
            QObject::connect(action, &QAction::triggered, receiver,
                             [run, tool = item.tool]() { run(tool); });
        }
    }
}

}  // namespace

QString diagnosticsMenuTitle() {
    return QCoreApplication::translate("kalahari::gui::DiagnosticController", "Dia&gnostics");
}

QString devToolsMenuTitle() {
    return QCoreApplication::translate("kalahari::gui::DiagnosticController", "&Dev Tools");
}

void fillDiagnosticsMenu(QMenu* menu, bool debugTools, QObject* receiver,
                         const DiagnosticToolRunner& run) {
    addGroups(menu, diagnosticsGroups(), receiver, run);
    if (debugTools) {
        addGroups(menu, debugGroups(), receiver, run);
    }
}

void fillDevToolsMenu(QMenu* menu, QObject* receiver, const DiagnosticToolRunner& run) {
    QAction* iconDownloader = menu->addAction(QCoreApplication::translate(
        "kalahari::gui::DiagnosticController", "&Icon Downloader"));
    iconDownloader->setToolTip(QCoreApplication::translate(
        "kalahari::gui::DiagnosticController", "Download Material Design icons for the project"));
    QObject::connect(iconDownloader, &QAction::triggered, receiver,
                     [run]() { run(DiagnosticTool::IconDownloader); });
}

} // namespace gui
} // namespace kalahari
