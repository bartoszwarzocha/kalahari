// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file settings_schema.cpp
/// @brief Default values of all application settings

#include <kalahari/core/settings_schema.h>

#include <algorithm>
#include <set>

namespace kalahari::core::settings_schema {

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
        // The Statistics panel has its place in the saved layout: a layout saved before it
        // gets it once, in the tab after the Annotations panel (DockCoordinator)
        {"window.statisticsPanelPlaced", false},
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
        // The window of a paragraph style (New Style, Modify Style): all its settings on tabs
        // ("tabs") or the short one with a wide preview ("short")
        {"ui.styleWindow", "tabs"},
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

        // Statistics and goals of writing (Settings > Statistics and Goals, core::WritingRules).
        // A writing day ends at dayStart (minutes after midnight) of the next morning; the time
        // of writing stops after a pause of idleMinutes. The words pasted or dropped are kept
        // apart and count as written only with countPasted; any other change of more than
        // largeChangeWords at once (cutting a chapter, restoring a version) is not writing when
        // skipLargeChanges is on. The goals count the growth of the text ("net") or all the
        // words written ("written"), of all the books or of the open one. writingDays: bit 0 is
        // Monday, ... bit 6 Sunday. weeklyGoal 0: the daily goals of the writing days. A day
        // keeps the days in a row going with streakMinimumWords ("minimumWords") or with the
        // daily goal reached ("dailyGoal").
        {"stats.dayStart", 240},
        {"stats.idleMinutes", 5},
        {"stats.skipLargeChanges", true},
        {"stats.largeChangeWords", 500},
        {"stats.countPasted", false},
        // A cut to the clippings leaves the words of the day as they were (false: it lowers
        // them as a deletion does)
        {"stats.clippingsKeepWords", false},
        {"stats.goalCounting", "net"},
        {"stats.dailyGoalAllBooks", true},
        {"stats.writingDays", 31},
        {"stats.dailyGoal", 1000},
        {"stats.weeklyGoal", 0},
        {"stats.streakRule", "minimumWords"},
        {"stats.streakMinimumWords", 100},
        // The statistics bar above the tabs of the texts (Settings > Statistics and Goals > Bar
        // and Panel): a graph of the day ("graph", bar.height pixels high, at least 83) or one
        // line ("line"); its texts on the right or the left of the graph; the mini graph of the
        // line shows today ("day"), the last 7 or the last 30 days. The graph shows the hours
        // from bar.fromHour to bar.toHour (24: midnight), in pieces of bar.pieceMinutes: the day
        // from the left ("day"), or as many hours with now in the middle ("now"), as the button
        // on the graph switches it.
        {"stats.bar.show", true},
        {"stats.bar.mode", "graph"},
        {"stats.bar.height", 83},
        {"stats.bar.textSide", "right"},
        {"stats.bar.showWords", true},
        {"stats.bar.showTime", true},
        {"stats.bar.showStreak", true},
        {"stats.bar.miniGraph", "7"},
        {"stats.bar.graphMode", "day"},
        {"stats.bar.fromHour", 6},
        {"stats.bar.toHour", 24},
        {"stats.bar.pieceMinutes", 10},
        {"stats.bar.showPieces", true},
        {"stats.bar.showCumulative", true},
        // The views of the Statistics panel, and the time of writing and the days in a row in
        // the status bar
        {"stats.panel.today", true},
        {"stats.panel.week", true},
        {"stats.panel.month", true},
        {"stats.panel.book", true},
        {"stats.panel.hours", true},
        {"stats.panel.awards", true},
        {"stats.statusBar", true},
        // Messages in the bar at the bottom of the window (Settings > Statistics and Goals >
        // Notifications), the summary of the session and the badges and challenges
        {"stats.notify.dailyGoal", true},
        // The daily goal reached opens the Statistics panel on the view Today
        {"stats.notify.showPanel", true},
        {"stats.notify.weeklyGoal", true},
        {"stats.notify.deadline", true},
        {"stats.notify.deadlineDays", 3},
        {"stats.sessionSummary", true},
        {"stats.badges", true},
        {"stats.challenges", false},

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
        // With the option above: the books of the last work open, each in its window ("all"),
        // or only the book opened last ("last"); the books open when the program ended last
        // (their .klh files, the one in front last)
        {"startup.books", "all"},
        {"startup.openBooks", json::array()},
        // A book opened while another one is open in the window: the question Opening a Book
        // ("ask"), a new window ("new") or in place of the open book ("this")
        {"window.otherBook", "ask"},
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
        // The materials of the cover (Settings > Navigator > Cover): their lengths in
        // characters with spaces ("characters") or in "words", and the default target length
        // of a description, a tagline and an opinion in each unit (core::CoverMaterial)
        {"materials.cover.lengthUnit", "characters"},
        {"materials.cover.targets.blurb.characters", 600},
        {"materials.cover.targets.blurb.words", 100},
        {"materials.cover.targets.tagline.characters", 60},
        {"materials.cover.targets.tagline.words", 10},
        {"materials.cover.targets.opinion.characters", 200},
        {"materials.cover.targets.opinion.words", 35},
        // The kinds of materials the Navigator does not show, in every book: kinds with their
        // packages ("kalahari.base:cover"); their materials stay in the books (MaterialList)
        {"materials.hiddenKinds", json::array()},
        // The sources (Settings > Materials > Sources): the style of the bibliography of a book
        // without its own and the form of a reference in the text (core::Bibliography)
        {"materials.sources.style", "pn-iso-690"},
        {"materials.sources.reference", "authorYear"},
        // The description of a source from a catalog on the web by its ISBN or DOI: whether
        // the card of a source has From the Web by ISBN or DOI... (off: nothing goes out) and
        // the catalog of an ISBN, "googleBooks" or "openLibrary" (core::SourceCatalogs)
        {"materials.sources.lookup", false},
        {"materials.sources.isbnCatalog", "googleBooks"},
        // A PDF file opens on a preview tab of the program ("preview") or in the system program
        // ("system"); in a program built without the preview always in the system program
        // (gui::PdfFiles)
        {"materials.open.pdf", "preview"},
        // The preview of a PDF file (Settings > Materials > PDF Preview): the pages one under
        // another ("continuous"), one at a time ("single") or two side by side ("two"); in a
        // dark theme as in the file ("file"), darker ("dim") or with their colors turned
        // ("invert"); the zoom it opens with ("pageWidth", "wholePage", "actual" or "last", the
        // zoom shown last: "pageWidth", "wholePage" or a number); the side panel it opens with
        // ("bookmarks": the bookmarks, else the contents of the file, "contents", "thumbnails"
        // or "hidden"); whether it opens at the page shown last; whether text on the pages
        // can be highlighted, the highlights kept in the book (the file stays as it is)
        {"materials.pdf.layout", "continuous"},
        {"materials.pdf.dark", "file"},
        {"materials.pdf.zoom", "pageWidth"},
        {"materials.pdf.lastZoom", "pageWidth"},
        {"materials.pdf.side", "bookmarks"},
        {"materials.pdf.openAtLastPage", true},
        {"materials.pdf.highlights", false},

        // The commands that the menu of a node of the Navigator has for adding: "named" ("Add..."
        // and the commands named after the kinds most added there) or "single" ("Add..." only)
        {"navigator.addCommands", "named"},
        // The cards of the Workshop (characters, locations, items): the palette that new cards
        // take their colors from, in order (CardColors::next()), whether the Navigator shows
        // the color of a card beside its name, whether it puts a location under the one it
        // is part of ("tree") or all of them in alphabetical order ("list"), and what the
        // Properties panel shows of the cards at the cursor of a text: the card whose name it
        // is in and the cards of the paragraph ("names"), only those ("paragraphs") or none
        // ("none"), whether a paragraph linked with cards has the signs of their kinds
        // beside it, the fields of the cards that new books start with, by the kind of the
        // cards (card_fields::DEFAULTS_SETTING; none: the fields of the packages), the layout
        // of the full card (the data beside the description: "columns", or "tabs"), the shape
        // of the portraits ("square" or "circle") and what a portrait without a picture shows
        // (the initials on the color of the card, "initials", or the icon of its kind, "icon")
        {"cards.palette", json::array({"#c0504d", "#7e57c2", "#2e9e6a", "#d4a017", "#3a8fc9",
                                       "#8d7b6a", "#b04a8f", "#e07b39", "#4f6fd1", "#6b9e2e",
                                       "#1f9aa5", "#a35d3d"})},
        {"cards.navigatorColorDot", true},
        {"cards.navigatorTree", "tree"},
        {"cards.atCursor", "names"},
        {"cards.paragraphMarks", true},
        {"cards.fields", json::object()},
        {"cards.fullCardLayout", "columns"},
        {"cards.portraitShape", "square"},
        {"cards.portraitWithoutPicture", "initials"},

        // The plot board (gui::PlotBoard): the board of the cards ("board") or the outline
        // ("outline"), the one shown last, and the one a new tab opens on ("board", "outline"
        // or "last"); the parts side by side ("columns") or the cards in rows under the titles
        // of the parts ("grid"); the size of the cards ("small",
        // "medium", "large"); what colors them: the state ("status"), the point of view
        // ("pov"), the writer's own colors ("own") or nothing ("none"); whether the board
        // shows the scenes of the texts (in a grid: with their drawers open) and the outline
        // their rows; which scenes there are: those of the texts and those in their plans
        // ("both"), those of the texts ("text") or those in the plans only ("plan"); the color
        // of a card in a strip at its top ("strip") or as its background ("tint"); the
        // background of the board: the theme's ("theme"), cork ("cork") or the writer's own
        // ("own", the color plotBackground of the theme); what the cards show besides their
        // titles (fields) and the columns of the outline besides the title (outline); whether
        // the board shows the front and the back section too, as a preview (showSections);
        // what happens to the numbers in the titles the program gave after a move: offered on
        // the bar under the board ("ask"), changed at once ("auto") or never ("never")
        {"plotBoard.view", "board"},
        {"plotBoard.startView", "last"},
        {"plotBoard.layout", "columns"},
        {"plotBoard.cardSize", "medium"},
        {"plotBoard.colorBy", "status"},
        {"plotBoard.colorPlace", "strip"},
        {"plotBoard.background", "theme"},
        {"plotBoard.showScenes", true},
        {"plotBoard.scenes", "both"},
        {"plotBoard.showSections", false},
        {"plotBoard.renumber", "ask"},
        {"plotBoard.fields.description", true},
        {"plotBoard.fields.pov", true},
        {"plotBoard.fields.place", true},
        {"plotBoard.fields.time", false},
        {"plotBoard.fields.words", true},
        {"plotBoard.fields.status", true},
        {"plotBoard.fields.scenes", true},
        {"plotBoard.outline.number", true},
        {"plotBoard.outline.description", true},
        {"plotBoard.outline.pov", true},
        {"plotBoard.outline.place", true},
        {"plotBoard.outline.time", true},
        {"plotBoard.outline.words", true},
        {"plotBoard.outline.status", true},

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
        {"project.lastFolders.insertImage", ""},
        {"project.lastFolders.saveImage", ""},
        {"project.lastFolders.exportBook", ""},
        {"project.lastFolders.importText", ""},
        // File > Export > DOCX, PDF, Markdown and EPUB: the writer's last choices of the
        // window. What goes to the file: "book" (the whole book), "chosen" (the texts chosen
        // in the window) or "front" (the text in front); the titles of the Navigator as
        // headings; each text from a new page; page numbers; the font of the text (empty: the
        // editor's); the running head ("none", "authorAndTitle", "titleAndChapter",
        // "chapter"); the annotations as Word's comments; the images copied next to a
        // Markdown file
        {"export.content", "book"},
        {"export.titles", true},
        {"export.newPage", true},
        {"export.pageNumbers", true},
        {"export.font", ""},
        {"export.runningHead", "authorAndTitle"},
        {"export.comments", false},
        {"export.copyImages", true},
        // A PDF: the annotations as its comments; its purpose ("reading", "printShop" for a
        // print shop, PDF/X-4, "archive", PDF/A-1b); the bookmarks; the pages in pairs; the
        // shades of gray
        {"export.pdf.comments", false},
        {"export.pdf.purpose", "reading"},
        {"export.pdf.bookmarks", true},
        {"export.pdf.facingPages", true},
        {"export.pdf.grayscale", false},
        // An EPUB: its look ("book", the book's styles, or "simple"); the font of the text in
        // the file; its cover ("design", the design chosen in the materials of the cover, or
        // "none")
        {"export.epub.look", "book"},
        {"export.epub.embedFont", false},
        {"export.epub.cover", "design"},
        // File > Print: the writer's last choices of the window; its layout is the export's
        // (export.*). What goes to the printer, as export.content ("front" the first time, as
        // Ctrl+P in the editors of text); the copies whole one after another; the pages on a
        // sheet ("onePage", "twoPages" side by side, "booklet" folded in half); one page as
        // large as the sheet allows, else of its own size; the crop marks of a page of its own
        // size
        {"print.content", "front"},
        {"print.collate", true},
        {"print.sheet.layout", "onePage"},
        {"print.sheet.fit", true},
        {"print.sheet.cropMarks", false},
        // File > Import > DOCX, Markdown, Plain Text and Scrivener Project: the writer's last
        // choices of the window. Where the texts go: "book" (the open book), "newBook" or
        // "otherFiles"; more files each as one text
        {"import.target", "book"},
        {"import.wholeFiles", true},
        // Download the images that Markdown files and Word documents give by Internet
        // addresses: the program connects to them only when the import starts; off: the
        // descriptions of the images stay in their places
        {"import.webImages", false},
        // Of Word documents: how much of the file's formatting the texts keep, "emphasis"
        // (bold, italic... and centered or right-aligned paragraphs), "all" or "styles". Of
        // each format (docx, markdown, text, scrivener): the heading of a text its title only;
        // the paragraphs that start with a dash in the style Dialogue; the cleanups of the text
        // (the typography of autocorrect, double spaces, spaces at the start of a paragraph,
        // "* * *" as a scene break) and what the empty paragraphs become: "remove", "keep" or
        // "sceneBreak" (Settings > Files > Import shows them)
        {"import.docx.formatting", "emphasis"},
        {"import.docx.headingAsTitle", true},
        {"import.docx.dialogueStyle", false},
        {"import.docx.typography", true},
        {"import.docx.doubleSpaces", true},
        {"import.docx.leadingSpaces", true},
        {"import.docx.sceneMarkers", true},
        {"import.docx.emptyParagraphs", "remove"},
        // Word's comments as annotations; the tracked changes rejected (false: accepted)
        {"import.docx.comments", true},
        {"import.docx.rejectChanges", false},
        {"import.markdown.headingAsTitle", true},
        {"import.markdown.dialogueStyle", false},
        {"import.markdown.typography", true},
        {"import.markdown.doubleSpaces", true},
        {"import.markdown.leadingSpaces", true},
        {"import.markdown.sceneMarkers", true},
        {"import.markdown.emptyParagraphs", "remove"},
        {"import.text.headingAsTitle", true},
        {"import.text.dialogueStyle", false},
        {"import.text.typography", true},
        {"import.text.doubleSpaces", true},
        {"import.text.leadingSpaces", true},
        {"import.text.sceneMarkers", true},
        {"import.text.emptyParagraphs", "remove"},
        // Plain text: where a text starts, "titles", "emptyLines" or "whole"; the pattern of
        // the titles of the texts ("polish", "english", "number", "capitals", "prefix"; empty:
        // by the language of the book) and its start for "prefix"; the pattern of the titles
        // of the parts ("polish", "english", "none"; empty: by the language); how many empty
        // lines start a text; the style of the paragraphs
        {"import.text.split", "titles"},
        {"import.text.titles", ""},
        {"import.text.prefix", ""},
        {"import.text.parts", ""},
        {"import.text.emptyLines", 3},
        {"import.text.style", "normal"},
        // A project of Scrivener: the formatting and the cleanups as of the other formats; only
        // the documents Included in Compile; what divides the scenes of a text, "breaks",
        // "titles" (the scene's title as a heading) or "none"; the synopses at the start of the
        // notes, the notes as notes, the comments and inline annotations as annotations, the snapshots as
        // versions of the texts, the folder Research to the Workshop
        {"import.scrivener.formatting", "emphasis"},
        {"import.scrivener.headingAsTitle", true},
        {"import.scrivener.dialogueStyle", false},
        {"import.scrivener.typography", true},
        {"import.scrivener.doubleSpaces", true},
        {"import.scrivener.leadingSpaces", true},
        {"import.scrivener.sceneMarkers", true},
        {"import.scrivener.emptyParagraphs", "remove"},
        {"import.scrivener.compiledOnly", true},
        {"import.scrivener.scenes", "breaks"},
        {"import.scrivener.synopses", true},
        {"import.scrivener.notes", true},
        {"import.scrivener.comments", true},
        {"import.scrivener.snapshots", true},
        {"import.scrivener.research", true},
        // The writer's last choice of sections for a book type that shows them: a set of
        // names ("sections", "matter", "fragments", "arc"), own names ("custom") or "none"
        {"project.sectionSet", "sections"},
        // The writer's own names of the front, main and back section, for "custom"
        {"project.sectionNames", json::array()},

        // Plugins (only Kalahari's own, signed with its key: no setting loads others)
        // The ids of the plugins the writer turned off (the others load at the start)
        {"plugins.disabled", json::array()},
        // The values of the plugins' own settings: {plugin id: {setting id: value}}
        {"plugins.settings", json::object()},

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
        {"editor.images.keepSizeOnChange", true},
        // New images (Settings > Editor > Images): in a paragraph of their own ("inline"),
        // aligned "left", "center" or "right", as wide as their files and at most as the text
        // (0) or a share of the text in percent, with a numbered caption of a label (empty: the
        // first label of new books); copies of very large images made smaller above a number
        // of dots per inch at their size in the text; the manuscript for a publisher with the
        // images in the text ("inline") or markers in their places ("markers")
        {"editor.images.layout", "inline"},
        {"editor.images.alignment", "center"},
        {"editor.images.width", 0},
        {"editor.images.addCaption", true},
        {"editor.images.captionLabel", ""},
        {"editor.images.askCaption", true},
        {"editor.images.downscale", false},
        {"editor.images.maxDpi", 300},
        {"editor.images.manuscript", "inline"},
        // The captions of new books: their labels (an empty list: Figure, Photo, Illustration,
        // Map, Diagram in the user's language), the number format ("1", "a", "A", "i", "I"),
        // numbered anew in each chapter after its number, and what a cross-reference shows
        // ("number", "label", "caption", "position"). A book keeps its own from its first
        // caption on.
        {"editor.captions.labels", json::array()},
        {"editor.captions.format", "1"},
        {"editor.captions.withChapter", false},
        {"editor.captions.reference", "number"},
        // Clippings (core/clipping.h): Cut to Clippings cuts at once ("atOnce") or with the
        // small window first ("window"), without a selection the paragraph with the cursor
        // ("paragraph") or nothing ("nothing"); Insert > Clipping opens the window with the
        // preview ("window") or the list at the cursor ("list"); a clipping inserted stays
        // marked as inserted ("keep") or goes from the Workshop ("remove"); the editor shows
        // scissors beside the paragraphs clippings were cut from (true) or not
        {"editor.clippings.cut", "atOnce"},
        {"editor.clippings.noSelection", "paragraph"},
        {"editor.clippings.insert", "window"},
        {"editor.clippings.afterInsert", "keep"},
        {"editor.clippings.marks", false},
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

        // The book's dictionary: the places that go against its spelling rules are marked in
        // the chapters; Add of a misspelled word's card in the Spelling and Grammar panel puts
        // the word in the book's dictionary ("book") or in the writer's own words ("user")
        {"bookDictionary.markRules", true},
        {"bookDictionary.panelAddsTo", "book"},

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

        // Editor: the fields (the date, the book's title, the page, a caption's number) on a
        // gray background (Settings > Editor > General)
        {"editor.fieldShading", true},
        // Insert > Date && Time...: the format chosen last (editor::fieldFormats() of the
        // date: "long", "short", "full", "iso", "time", "date-time"), which Alt+Shift+D gives
        // when it is a date alone, and Update Automatically (a field), as it was last
        {"editor.fields.dateFormat", "long"},
        {"editor.fields.dateAsField", false},

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

        // Word count in the whole program: a dash standing alone (a dialogue dash) is a word;
        // the words of the footnotes and endnotes count (as in Microsoft Word)
        {"editor.wordCount.dashesAsWords", false},
        {"editor.wordCount.notes", true},

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

/// The table of the defaults, with the settings of the modules that addDefaults() added
std::map<std::string, json>& allDefaults() {
    static std::map<std::string, json> all = buildDefaults();
    return all;
}

} // namespace

const std::map<std::string, nlohmann::json>& defaults() {
    return allDefaults();
}

void addDefaults(const std::map<std::string, nlohmann::json>& moduleDefaults) {
    std::map<std::string, json>& all = allDefaults();
    for (const auto& [key, value] : moduleDefaults) {
        all.try_emplace(key, value);
    }
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

} // namespace kalahari::core::settings_schema
