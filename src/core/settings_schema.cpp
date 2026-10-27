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

        // Insert > Special Character...: the writer's favorite characters (at first those a
        // writer of fiction uses most), the ones inserted last (the last first), whether
        // the window stays open after Insert, and the view it opens in: the one shown last
        // ("grid" or "list")
        {"specialCharacters.favorites",
         json::array({"—", "–", "…", "„", "”", "⁂", "❦", "→", "°", "§"})},
        {"specialCharacters.recent", json::array()},
        {"specialCharacters.keepOpen", true},
        {"specialCharacters.view", "grid"},

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
        {"editor.tabSize", 4},
        {"editor.lineNumbers", true},
        {"editor.wordWrap", false},
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
        {"editor.colors.linkLight", "#0563c1"},
        {"editor.colors.backgroundDark", "#232328"},
        {"editor.colors.textDark", "#e0e0e0"},
        {"editor.colors.inactiveDark", "#78787d"},
        {"editor.colors.formattingMarksDark", "#7a96d0"},
        {"editor.colors.linkDark", "#8ab4f8"},

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

        // Editor: the formatting marks (View > Show Formatting Marks: ¶ · ° → ↵ ¬), in the
        // colors editor.colors.formattingMarksLight/Dark
        {"editor.showFormattingMarks", false},

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

        // Autocorrect of the words, as a space, a punctuation mark or Enter ends them: a
        // hyphen between words to a dash ("enDash" or "emDash"; in a line of dialogue the
        // dash it starts with), two hyphens to an en dash, the replacement table, capitals
        // (the first letter of a sentence, "TWo", a word typed with Caps Lock on), "***" and
        // "---" with Enter to a scene break and a line
        {"editor.autoCorrect.dashBetweenWords", true},
        {"editor.autoCorrect.dashBetweenWordsType", "enDash"},
        {"editor.autoCorrect.dashInDialogue", true},
        {"editor.autoCorrect.doubleHyphen", true},
        {"editor.autoCorrect.replacementTable", true},
        {"editor.autoCorrect.sentenceCapitals", true},
        {"editor.autoCorrect.twoInitialCapitals", true},
        {"editor.autoCorrect.capsLock", true},
        {"editor.autoCorrect.sceneBreak", true},
        {"editor.autoCorrect.horizontalRule", true},
        // "* " and "1. " or "1) " starting a paragraph to an item of a bulleted and of a
        // numbered list
        {"editor.autoCorrect.bulletedLists", true},
        {"editor.autoCorrect.numberedLists", true},
        // An Internet address typed in the text ("www.example.com", "https://...", an e-mail
        // address) to a link, as a space or Enter ends it
        {"editor.autoCorrect.links", true},

        // The replacement table: pairs of a word and its replacement, each one on or off
        // (the program's own pairs too; "Restore Defaults" brings back these)
        {"editor.autoCorrect.replacements",
         json::array({json{{"from", "(c)"}, {"to", "©"}, {"enabled", true}},
                      json{{"from", "(r)"}, {"to", "®"}, {"enabled", true}},
                      json{{"from", "(tm)"}, {"to", "™"}, {"enabled", true}},
                      json{{"from", "1/2"}, {"to", "½"}, {"enabled", true}},
                      json{{"from", "1/4"}, {"to", "¼"}, {"enabled", true}},
                      json{{"from", "3/4"}, {"to", "¾"}, {"enabled", true}},
                      json{{"from", "->"}, {"to", "→"}, {"enabled", true}},
                      json{{"from", "<-"}, {"to", "←"}, {"enabled", true}},
                      json{{"from", "<->"}, {"to", "↔"}, {"enabled", true}},
                      json{{"from", "=>"}, {"to", "⇒"}, {"enabled", true}},
                      json{{"from", "+-"}, {"to", "±"}, {"enabled", true}}})},

        // The exceptions of the capitals: the abbreviations after which a period does not
        // end a sentence, a list for each language (another language has the English one;
        // abbreviations often at the end of a sentence, as "itd." and "etc.", are not in
        // them), the words that keep two capitals at their start, and whether undoing a
        // capital right after it adds the word to them, as in Word
        {"editor.autoCorrect.abbreviations.pl",
         json::array({"al.", "ang.", "art.", "bm.", "cz.", "dn.", "dot.", "dr", "gen.",
                      "godz.", "im.", "inż.", "jw.", "kpt.", "ks.", "m.in.", "mgr", "mjr",
                      "np.", "nr", "nt.", "ob.", "ok.", "pkt.", "pl.", "płd.", "płk", "płn.",
                      "por.", "prof.", "przyp.", "rozdz.", "ryc.", "rys.", "św.", "tab.",
                      "tel.", "tj.", "tys.", "tzn.", "tzw.", "ul.", "ust.", "wg", "wsch.",
                      "wyd.", "zach.", "zob."})},
        {"editor.autoCorrect.abbreviations.en",
         json::array({"approx.", "Ave.", "Blvd.", "ca.", "Capt.", "cf.", "Col.", "Dr.",
                      "e.g.", "fig.", "Fr.", "Gen.", "Gov.", "i.e.", "Jr.", "Lt.", "Mr.",
                      "Mrs.", "Ms.", "Mt.", "pp.", "Prof.", "Rev.", "Sen.", "Sgt.", "Sr.",
                      "St.", "vol.", "vs."})},
        {"editor.autoCorrect.twoCapitalsExceptions", json::array({"IDs", "PCs"})},
        {"editor.autoCorrect.learnExceptions", true},
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
