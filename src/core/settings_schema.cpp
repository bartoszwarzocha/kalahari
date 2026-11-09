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
        // The tabs in the middle of the window (Settings > Appearance > General): in one row
        // with arrows when they do not fit ("single") or in more rows ("wrap")
        {"window.tabRows", "single"},

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
        // The tabs of panels docked one over another (Settings > Appearance > General): icons
        // alone ("icons"), icons and names ("iconsAndNames") or names alone ("names"); in more
        // rows when they do not fit ("wrap") or in one row with shortened names ("single");
        // above the panels ("above") or under them ("below")
        {"panels.tabLook", "icons"},
        {"panels.tabRows", "wrap"},
        {"panels.tabSide", "above"},
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
        // A tab moved to a window of its own: the window shows the menus and the toolbars of the
        // book window ("full") or the Format toolbar ("writing"); closing it takes its texts back
        // to the book window ("return") or closes them ("close")
        {"window.detached.look", "full"},
        {"window.detached.close", "return"},

        // Annotations: the name new ones are made with. Empty: by no one named
        {"annotations.author", ""},

        // Whether a link of a material may name a paragraph of a text, which the editor marks
        // with a paperclip (core::LinkRoles::PARAGRAPHS_SETTING); false hides the paragraphs of
        // the links and keeps them
        {"materials.linkParagraphs", true},

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

        // Editor: the paper, light or dark whatever the theme, and the colors. The user's values
        // of the theme's editor colors (both papers and their text, the desk around them, the
        // selection, the search's hits, the underlines of the misspelled words and grammar
        // issues, the annotations) are kept for each theme, themes.<theme>.editor.<name in the
        // theme's "editor" section>, without a default here: the theme file gives it
        // (ThemeManager::editorColorSetting()). "Optimize colors in themes" shows the underlines
        // and the annotations darker on a light background and lighter on a dark one where they
        // are hard to see there
        {"editor.darkMode", true},
        {"editor.colors.optimize", true},

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
        // theme's colors of the papers
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

        // Editor: the cards of the annotations under their paragraphs, in the continuous view
        // (View > Annotation Cards Under Paragraphs, Settings > Annotations): shown or not, and
        // for which kinds; "full" (the kind, the author and the date over the text) or
        // "compact" (the text alone, the author and the date at its end); a collapsed one
        // "mark" (its mark alone) or "line" (a line with the start of its text); "expanded" or
        // "collapsed" after a chapter opens; a long text cut short after so many lines (0:
        // whole); the size of their text in percent of the chapter's; how much of the kind's
        // color they have, in percent of the usual; the fragment of the card the mouse is on
        // highlighted; "show" or "hide" them in Distraction-Free
        {"editor.annotationCards.show", false},
        {"editor.annotationCards.comments", true},
        {"editor.annotationCards.todos", true},
        {"editor.annotationCards.notes", true},
        {"editor.annotationCards.look", "full"},
        {"editor.annotationCards.collapsedLook", "mark"},
        {"editor.annotationCards.afterOpening", "expanded"},
        {"editor.annotationCards.longLines", 0},
        {"editor.annotationCards.fontSize", 90},
        {"editor.annotationCards.tint", 100},
        {"editor.annotationCards.highlightFragment", true},
        {"editor.annotationCards.distractionFree", "show"},

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

        // Editor: quick insert (Settings > Editor > Quick Insert, gui/quick_insert_items.h).
        // @ or # typed at the start of a word opens a list of the sources chosen for it:
        // "cards" (the names of the cards of the Workshop), "fragments" (the writer's text
        // fragments), "characters" (the typographic characters). The fragments and the
        // characters: [{"name", "text", "kind": "fragment" | "character"}]; null: the
        // characters the program has at first
        {"editor.quickInsert.enabled", true},
        {"editor.quickInsert.at", json::array({"cards"})},
        {"editor.quickInsert.hash", json::array({"fragments", "characters"})},
        {"editor.quickInsert.items", nullptr},

        // Editor: repetitions of words (Settings > Editor > Repetitions, RepetitionsCoordinator).
        // A word repeats when it comes back within distance words; shorter words than
        // minLength do not count. mergeForms: the forms of a word count as one (by the
        // spelling dictionary); skipCommon: the very common words of the language do not
        // count; skipped: the writer's words that do not count, in lower case. highlight: the
        // words too close are marked in the text while the panel shows, with a "background" or
        // a "wave"
        {"editor.repetitions.distance", 30},
        {"editor.repetitions.minLength", 3},
        {"editor.repetitions.mergeForms", true},
        {"editor.repetitions.skipCommon", true},
        {"editor.repetitions.skipped", json::array()},
        {"editor.repetitions.highlight", true},
        {"editor.repetitions.style", "background"},

        // Editor: versions of the texts (Settings > Editor > Versions, ChapterVersions). With
        // enabled off no new version is kept, neither the writer's nor the program's (except
        // the current text before another version is restored); those kept stay. Save
        // Version asks for a description; the program keeps a version by itself when a chapter
        // is saved, at most once in onSaveMinutes, and before Replace All in a chapter (and
        // always before another version is restored), keeping at most keepAutomatic of them
        // for each file. The writer's own versions stay until the writer deletes them. The
        // buttons of Save Version and Version History are on a toolbar of their own, with
        // Backup Now ("ownToolbar"), or on the Quick Actions toolbar ("quickActions")
        {"editor.versions.enabled", true},
        {"editor.versions.askName", true},
        {"editor.versions.onSave", true},
        {"editor.versions.onSaveMinutes", 60},
        {"editor.versions.beforeReplaceAll", true},
        {"editor.versions.keepAutomatic", 20},
        {"editor.versions.buttons", "ownToolbar"},
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
