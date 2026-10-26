/// @file settings_schema.cpp
/// @brief Default values of all application settings

#include <kalahari/core/settings_schema.h>

#include <algorithm>
#include <set>

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
        {"window.geometry", ""},  // QMainWindow::saveGeometry() as base64
        {"window.state", ""},     // QMainWindow::saveState() as base64 (bars and panels)
        // The bar that says the panels on the right were hidden for a small screen was shown
        {"window.smallScreenNoticeShown", false},

        // The Paragraph and Styles panel: a panel of its own ("panel") or the lower part of
        // the Properties panel ("properties"), folded there to its band or not, with the
        // border of the parts where the writer put it (QSplitter::saveState() as base64;
        // empty: halfway). Its palette shows the styles in their look or by their names.
        {"panels.paragraphPlace", "panel"},
        {"panels.paragraphFolded", false},
        {"panels.propertiesSplit", ""},
        {"panels.stylePreview", false},
        // The Log button of the Panels toolbar, for those who read the log (Settings >
        // Advanced > Log); View > Panels > Log (F4) opens the panel anyway
        {"panels.showLogButton", false},
        {"recent_files", json::array()},

        // Interface
        {"ui.language", "en"},
        // Unit the length fields show (px, mm, cm, in, pt); each setting keeps its own
        {"ui.lengthUnit", "mm"},
        // Colors chosen last in the color window, newest first ("#rrggbb")
        {"ui.recentColors", json::array()},
        // Colors the writer keeps in the color window, in the order added ("#rrggbb")
        {"ui.myColors", json::array()},
        // Palette the color window shows: basic, kalahari, okavango, pastel, ink, savannaDusk
        {"ui.colorPalette", "basic"},
        // The color window shows the HEX and RGB values
        {"ui.colorValuesShown", false},
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
        // The commands added to the built-in toolbars that the saved toolbars have: a
        // customized toolbar gets the later ones once (ToolbarManager)
        {"toolbars.addedCommands", 0},
        {"toolbars.configVersion", 0},

        // Keyboard shortcuts the user changed: command id -> keys ("Ctrl+Shift+F", "" for
        // none); the other commands have the program's (Settings > Keyboard Shortcuts)
        {"keyboard.shortcuts", json::object()},

        // Log panel
        {"log.bufferSize", 500},

        // Dashboard and startup
        {"dashboard.maxItems", 5},
        {"dashboard.iconSize", 48},
        {"dashboard.showKalahariNews", true},
        {"dashboard.showRecentFiles", true},
        {"startup.autoLoadLastProject", false},
        // The book opened last (its .klh file), opened at startup with the option above. A book
        // created or imported without opening is the first of the recent books but not this
        // one. Empty: the first of the recent books
        {"startup.lastBook", ""},

        // Annotations: the name new ones are made with. Empty: by no one named
        {"annotations.author", ""},

        // The commands that the menu of a node of the Navigator has for adding: "named" ("Add..."
        // and the commands named after the kinds most added there) or "single" ("Add..." only)
        {"navigator.addCommands", "named"},

        // New projects
        {"project.defaultAuthor", ""},
        // Language of new books ("pl"); empty: the language of the program
        {"project.defaultLanguage", ""},
        // Folder of the books (Settings > Files > Folders); empty: Kalahari in Documents
        {"project.defaultLocation", ""},
        // Folder of the archives of books; empty: Archives in the default folder of the books
        {"project.archiveLocation", ""},
        // One folder of the database backups of all books; empty: the .backups folder of each
        // book
        {"project.backupFolder", ""},
        // How many of the newest database backups of each book are kept
        {"project.backupCount", 5},
        // The folders the windows of these operations used last (empty: none yet)
        {"project.lastFolders.newBook", ""},
        {"project.lastFolders.openBook", ""},
        {"project.lastFolders.openFile", ""},
        {"project.lastFolders.exportArchive", ""},
        {"project.lastFolders.importArchive", ""},
        {"project.lastFolders.importDestination", ""},
        {"project.lastFolders.addMaterial", ""},
        // The writer's last choice of sections for a book type that shows them: a set of
        // names ("sections", "matter", "fragments", "arc"), own names ("custom") or "none"
        {"project.sectionSet", "sections"},
        // The writer's own names of the front, main and back section, for "custom"
        {"project.sectionNames", json::array()},

        // Plugins
        {"plugins.allowUnsigned", false},
        {"plugins.trustedKeys", "[]"},

        // Editor: text
        {"editor.fontFamily", "Georgia"},
        {"editor.fontSize", 14},
        // How far apart the tab stops are: "indent", every first-line indent of the paragraph's
        // style (a style without one: editor.indentSize), or "spaces", every editor.tabSize
        // spaces of its font
        {"editor.tabStops", "indent"},
        {"editor.tabSize", 4},
        // Where a chapter opens: "lastEdit", where its cursor was when it was closed last, or
        // "start"
        {"editor.chapterOpensAt", "lastEdit"},
        // How text from other programs is pasted: "merge" (it takes the look of the text it
        // goes to, keeping headings, quotes, emphasis and lines), "source" (also its fonts,
        // sizes and colors) or "text" (only the text)
        {"editor.paste.mode", "merge"},
        {"editor.lineHeight", 1.6},
        {"editor.paragraphSpacing", 12.0},
        {"editor.firstLineIndent", true},
        {"editor.indentSize", 24.0},

        // Editor: spelling as you type. The language is a dictionary (pl_PL) or a language
        // (pl); empty: the book's language
        {"editor.spellCheck.enabled", true},
        {"editor.spellCheck.language", ""},

        // Editor: grammar as you type. Mode "local": LanguageTool on this computer (Java and
        // the folder of LanguageTool, which the program downloads and fills in; the port of
        // its server; or a server started outside the program); "remote": a server in the
        // internet (its address, and the account of LanguageTool Premium). The rules ignored:
        // rule id -> what it looks for
        {"editor.grammarCheck.enabled", false},
        {"editor.grammarCheck.mode", "local"},
        {"editor.grammarCheck.serverUrl", "https://api.languagetool.org"},
        {"editor.grammarCheck.username", ""},
        {"editor.grammarCheck.apiKey", ""},
        {"editor.grammarCheck.javaPath", ""},
        {"editor.grammarCheck.languageToolFolder", ""},
        {"editor.grammarCheck.port", 8081},
        {"editor.grammarCheck.externalServer", false},
        {"editor.grammarCheck.ignoredRules", json::object()},

        // The Spelling and Grammar panel: the fix buttons on the selected card only
        // ("selected") or on every card ("every"); Next Spelling or Grammar Issue (F7) selects
        // the issue in the panel ("panel") or opens its menu at the word ("menu"); while the
        // panel is shown, the grammar of the whole text is checked ("whole") or only around the
        // part in view ("nearby"). The frames of the cards have the colors of the underlines
        {"spellingPanel.fixButtons", "selected"},
        {"spellingPanel.nextIssue", "panel"},
        {"spellingPanel.grammarScope", "whole"},

        // Editor: colors. The user's values of the theme's editor colors (the underlines of
        // the misspelled words and grammar issues, the annotations) are kept for each theme,
        // themes.<theme>.editor.<name in the theme's "editor" section>, without a default here:
        // the theme file gives it (ThemeManager::editorColorSetting()). "Optimize colors in
        // themes" shows them darker on a light background and lighter on a dark one where they
        // are hard to see there
        {"editor.darkMode", true},
        {"editor.colors.optimize", true},
        {"editor.colors.backgroundLight", "#ffffff"},
        {"editor.colors.textLight", "#1e1e1e"},
        {"editor.colors.inactiveLight", "#aaaaaa"},
        {"editor.colors.formattingMarksLight", "#5b7fc0"},
        {"editor.colors.backgroundDark", "#232328"},
        {"editor.colors.textDark", "#e0e0e0"},
        {"editor.colors.inactiveDark", "#78787d"},
        {"editor.colors.formattingMarksDark", "#7a96d0"},

        // Editor: cursor
        {"editor.cursor.style", 0},
        {"editor.cursor.useCustomColor", false},
        {"editor.cursor.customColor", "#ffffff"},
        {"editor.cursor.blinking", true},
        {"editor.cursor.blinkInterval", 500},
        {"editor.cursor.lineWidth", 2},

        // Editor: page margins in millimetres. Mirror margins: the inner one at the binding,
        // the outer one at the edge; the first page of a chapter is a right page unless the
        // chapter starts on a left one (the chapter's own setting, in the book)
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

        // Editor: the formatting marks (View > Show Formatting Marks: ¶ · ° → ↵ ¬), in the
        // colors editor.colors.formattingMarksLight/Dark
        {"editor.showFormattingMarks", false},

        // Editor: the working numbers on the left margin (View > Margin Numbering), only on the
        // screen: "none", "lines" (every line) or "paragraphs" (every paragraph with text). A
        // number every "interval" lines or paragraphs; with "restartEachPage" the lines are
        // counted from 1 on every page of the page view. The font and its size in points at
        // 100% zoom, and one color for the light and the dark paper
        {"editor.marginNumbers.mode", "none"},
        {"editor.marginNumbers.interval", 1},
        {"editor.marginNumbers.restartEachPage", false},
        {"editor.marginNumbers.font", "Courier New"},
        {"editor.marginNumbers.size", 8},
        {"editor.marginNumbers.color", "#808080"},

        // Editor: how the scene breaks look: "asterisks" (* * *), "asterism" (⁂), "fleuron"
        // (❦), "line" (a short line) or "blank" (an empty line); the files keep the breaks
        {"editor.sceneBreak.style", "asterisks"},

        // Editor: text frame border
        {"editor.textFrameBorder.show", false},
        {"editor.textFrameBorder.color", "#b4b4b4"},
        {"editor.textFrameBorder.width", 1},

        // Editor: the size of the annotations' marks in the text, in percent of the size
        // the text's font gives them
        {"editor.annotationMarkSize", 100},

        // Word count in the whole program: a dash standing alone (a dialogue dash) is a word
        {"editor.wordCount.dashesAsWords", false},

        // Editor: autocorrect, the changes of the text as it is typed (Settings > Editor >
        // Auto-correct): on or off as a whole (also Tools > Auto-correct as You Type), and
        // each replacement. The dialogue dash is "emDash" (—) or "enDash" (–).
        {"editor.autoCorrect.enabled", true},
        {"editor.autoCorrect.quotes", true},
        {"editor.autoCorrect.apostrophe", true},
        {"editor.autoCorrect.dialogueDash", true},
        {"editor.autoCorrect.dialogueDashType", "emDash"},
        {"editor.autoCorrect.ellipsis", true},
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

bool requiresRestart(const std::string& key) {
    // Read once at startup (main.cpp loads the translations before any window exists)
    static const std::set<std::string> restartKeys = {"ui.language"};

    std::string normalized = key;
    std::replace(normalized.begin(), normalized.end(), '/', '.');
    return restartKeys.count(normalized) > 0;
}

} // namespace settings_schema
} // namespace core
} // namespace kalahari
