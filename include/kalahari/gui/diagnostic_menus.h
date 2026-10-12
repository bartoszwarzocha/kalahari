/// @file diagnostic_menus.h
/// @brief The items of the Diagnostics and Dev Tools menus
///
/// The menus of the diagnostic and the dev mode (DiagnosticController) are filled here, so
/// that the tests check their access keys without the tools they run.

#pragma once

#include <QString>

#include <functional>

class QMenu;
class QObject;

namespace kalahari {
namespace gui {

/// @brief What an item of a developer menu runs
enum class DiagnosticTool {
    SystemInfo,
    QtEnvironment,
    FileSystemCheck,
    SettingsDump,
    MemoryStats,
    OpenDocsStats,
    LoggerTest,
    EventBusTest,
    PluginCheck,
    CommandRegistryDump,
    PythonEnvironment,
    PythonImportTest,
    PythonMemoryTest,
    EmbeddedInterpreterStatus,
    PerformanceBenchmark,
    RenderStats,
    ClearLog,
    ForceCrash,      ///< In a debug build only
    MemoryLeakTest,  ///< In a debug build only
    IconDownloader,
};

/// @brief Runs the tool of a chosen item
using DiagnosticToolRunner = std::function<void(DiagnosticTool)>;

/// @brief The title of the Diagnostics menu, with its access key
[[nodiscard]] QString diagnosticsMenuTitle();

/// @brief The title of the Dev Tools menu, with its access key
[[nodiscard]] QString devToolsMenuTitle();

/// @brief Fill the Diagnostics menu
/// @param menu The menu
/// @param debugTools Also the tools of a debug build (a crash, a memory leak)
/// @param receiver The object the items' signals are connected through (gone with it)
/// @param run Runs the tool of a chosen item
void fillDiagnosticsMenu(QMenu* menu, bool debugTools, QObject* receiver,
                         const DiagnosticToolRunner& run);

/// @brief Fill the Dev Tools menu
/// @param menu The menu
/// @param receiver The object the items' signals are connected through (gone with it)
/// @param run Runs the tool of a chosen item
void fillDevToolsMenu(QMenu* menu, QObject* receiver, const DiagnosticToolRunner& run);

} // namespace gui
} // namespace kalahari
