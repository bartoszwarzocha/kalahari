/// @file settings_schema.cpp
/// @brief Default values of all application settings

#include <kalahari/core/settings_schema.h>

#include <algorithm>

namespace kalahari {
namespace core {
namespace settings_schema {

namespace {

using json = nlohmann::json;

std::map<std::string, json> buildDefaults() {
    return {
        // Main window
        {"window.width", 1280},
        {"window.height", 800},
        {"window.x", 100},
        {"window.y", 100},
        {"window.maximized", false},

        // Interface
        {"ui.language", "en"},
        {"appearance.theme", "Light"},
        {"appearance.uiFontSize", 12},
        {"appearance.iconTheme", "twotone"},

        // Icons
        {"icons.sizes.toolbar", 24},
        {"icons.sizes.menu", 16},
        {"icons.sizes.panel", 20},
        {"icons.sizes.dialog", 32},
        {"icons.sizes.treeView", 16},
        {"icons.sizes.tabBar", 16},
        {"icons.sizes.statusBar", 16},
        {"icons.sizes.button", 20},
        {"icons.sizes.comboBox", 16},

        // Toolbars
        {"toolbars.locked", false},
        {"toolbars.configurations", "{}"},

        // Log panel
        {"log.bufferSize", 500},

        // Dashboard and startup
        {"dashboard.maxItems", 5},
        {"dashboard.iconSize", 48},
        {"dashboard.showKalahariNews", true},
        {"dashboard.showRecentFiles", true},
        {"startup.autoLoadLastProject", false},

        // New projects
        {"project.defaultAuthor", ""},
        {"project.defaultLanguage", "en"},
        {"project.defaultLocation", ""},

        // Plugins
        {"plugins.allowUnsigned", false},
        {"plugins.trustedKeys", "[]"},

        // Editor: text
        {"editor.fontFamily", "Georgia"},
        {"editor.fontSize", 14},
        {"editor.tabSize", 4},
        {"editor.lineNumbers", true},
        {"editor.wordWrap", false},
        {"editor.lineHeight", 1.6},
        {"editor.paragraphSpacing", 12.0},
        {"editor.firstLineIndent", true},
        {"editor.indentSize", 24.0},

        // Editor: grammar. The address of the user's LanguageTool server; empty: off
        {"editor.grammarCheck.serverUrl", ""},

        // Editor: colors
        {"editor.darkMode", true},
        {"editor.colors.backgroundLight", "#ffffff"},
        {"editor.colors.textLight", "#1e1e1e"},
        {"editor.colors.inactiveLight", "#aaaaaa"},
        {"editor.colors.backgroundDark", "#232328"},
        {"editor.colors.textDark", "#e0e0e0"},
        {"editor.colors.inactiveDark", "#78787d"},

        // Editor: cursor
        {"editor.cursor.style", 0},
        {"editor.cursor.useCustomColor", false},
        {"editor.cursor.customColor", "#ffffff"},
        {"editor.cursor.blinking", true},
        {"editor.cursor.blinkInterval", 500},
        {"editor.cursor.lineWidth", 2},

        // Editor: margins (view margins in pixels, page margins in millimetres)
        {"editor.margins.pageTop", 25.4},
        {"editor.margins.pageBottom", 25.4},
        {"editor.margins.pageLeft", 25.4},
        {"editor.margins.pageRight", 25.4},
        {"editor.margins.mirrorEnabled", false},
        {"editor.margins.pageInner", 30.0},
        {"editor.margins.pageOuter", 20.0},

        // Editor: page view
        {"editor.page.size", "A4"},
        {"editor.page.customWidth", 210.0},
        {"editor.page.customHeight", 297.0},
        {"editor.page.gap", 20},
        {"editor.page.showNumbers", true},

        // Editor: typewriter scrolling
        {"editor.typewriter.enabled", false},
        {"editor.typewriter.focusPosition", 0.5},
        {"editor.typewriter.smoothScroll", true},

        // Editor: Focus (View > Focus dims every paragraph but the cursor's)
        {"editor.focus.enabled", false},

        // Editor: text frame border
        {"editor.textFrameBorder.show", false},
        {"editor.textFrameBorder.color", "#b4b4b4"},
        {"editor.textFrameBorder.width", 1},
    };
}

} // namespace

const std::map<std::string, nlohmann::json>& defaults() {
    static const std::map<std::string, json> table = buildDefaults();
    return table;
}

const nlohmann::json* defaultValue(const std::string& key) {
    std::string normalized = key;
    std::replace(normalized.begin(), normalized.end(), '/', '.');

    const auto& table = defaults();
    auto it = table.find(normalized);
    return it != table.end() ? &it->second : nullptr;
}

} // namespace settings_schema
} // namespace core
} // namespace kalahari
