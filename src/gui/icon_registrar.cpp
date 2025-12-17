/// @file icon_registrar.cpp
/// @brief Application icon registration with IconRegistry
///
/// This module is extracted from MainWindow to reduce its size (OpenSpec #00038).
/// Contains all icon registrations for the application.

#include "kalahari/gui/icon_registrar.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/logger.h"

namespace kalahari {
namespace gui {

void registerAllIcons() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Registering icons with IconRegistry");

    auto& iconRegistry = core::IconRegistry::getInstance();

    // -------------------------------------------------------------------------
    // FILE MENU ICONS (Material Design twotone style)
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("file.new", "resources/icons/twotone/note_add.svg", "New File");
    iconRegistry.registerIcon("file.new.project", "resources/icons/twotone/create_new_folder.svg", "New Book");
    iconRegistry.registerIcon("file.open", "resources/icons/twotone/folder_open.svg", "Open File");
    iconRegistry.registerIcon("file.close", "resources/icons/twotone/close.svg", "Close File");
    iconRegistry.registerIcon("file.save", "resources/icons/twotone/save.svg", "Save");
    iconRegistry.registerIcon("file.saveAs", "resources/icons/twotone/save_as.svg", "Save As");
    iconRegistry.registerIcon("file.saveAll", "resources/icons/twotone/layers.svg", "Save All");
    iconRegistry.registerIcon("file.exit", "resources/icons/twotone/logout.svg", "Exit");
    iconRegistry.registerIcon("file.open.file", "resources/icons/twotone/description.svg", "Open File");
    iconRegistry.registerIcon("file.import.docx", "resources/icons/twotone/upload_file.svg", "Import DOCX");
    iconRegistry.registerIcon("file.import.pdf", "resources/icons/twotone/picture_as_pdf.svg", "Import PDF");
    iconRegistry.registerIcon("file.import.text", "resources/icons/twotone/text_snippet.svg", "Import Text");
    iconRegistry.registerIcon("file.import.scrivener", "resources/icons/twotone/integration_instructions.svg", "Import Scrivener");
    iconRegistry.registerIcon("file.export.docx", "resources/icons/twotone/download.svg", "Export DOCX");
    iconRegistry.registerIcon("file.export.pdf", "resources/icons/twotone/picture_as_pdf.svg", "Export PDF");
    iconRegistry.registerIcon("file.export.markdown", "resources/icons/twotone/code.svg", "Export Markdown");
    iconRegistry.registerIcon("file.export.epub", "resources/icons/twotone/auto_stories.svg", "Export EPUB");
    iconRegistry.registerIcon("file.export.mobi", "resources/icons/twotone/smartphone.svg", "Export MOBI");
    iconRegistry.registerIcon("file.export.latex", "resources/icons/twotone/functions.svg", "Export LaTeX");

    // -------------------------------------------------------------------------
    // EDIT MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("edit.undo", "resources/icons/twotone/undo.svg", "Undo");
    iconRegistry.registerIcon("edit.redo", "resources/icons/twotone/redo.svg", "Redo");
    iconRegistry.registerIcon("edit.cut", "resources/icons/twotone/content_cut.svg", "Cut");
    iconRegistry.registerIcon("edit.copy", "resources/icons/twotone/content_copy.svg", "Copy");
    iconRegistry.registerIcon("edit.paste", "resources/icons/twotone/content_paste.svg", "Paste");
    iconRegistry.registerIcon("edit.pasteSpecial", "resources/icons/twotone/content_paste_go.svg", "Paste Special");
    iconRegistry.registerIcon("edit.delete", "resources/icons/twotone/delete.svg", "Delete");
    iconRegistry.registerIcon("edit.selectAll", "resources/icons/twotone/select_all.svg", "Select All");
    iconRegistry.registerIcon("edit.selectWord", "resources/icons/twotone/highlight_alt.svg", "Select Word");
    iconRegistry.registerIcon("edit.selectParagraph", "resources/icons/twotone/segment.svg", "Select Paragraph");
    iconRegistry.registerIcon("edit.find", "resources/icons/twotone/search.svg", "Find");
    iconRegistry.registerIcon("edit.findNext", "resources/icons/twotone/navigate_next.svg", "Find Next");
    iconRegistry.registerIcon("edit.findPrevious", "resources/icons/twotone/navigate_before.svg", "Find Previous");
    iconRegistry.registerIcon("edit.findReplace", "resources/icons/twotone/find_replace.svg", "Find & Replace");
    iconRegistry.registerIcon("edit.preferences", "resources/icons/twotone/settings.svg", "Preferences");
    iconRegistry.registerIcon("edit.settings", "resources/icons/twotone/settings.svg", "Settings");

    // -------------------------------------------------------------------------
    // BOOK MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("book.newChapter", "resources/icons/twotone/article.svg", "New Chapter");
    iconRegistry.registerIcon("book.newScene", "resources/icons/twotone/theaters.svg", "New Scene");
    iconRegistry.registerIcon("book.newCharacter", "resources/icons/twotone/person.svg", "New Character");
    iconRegistry.registerIcon("book.newLocation", "resources/icons/twotone/place.svg", "New Location");
    iconRegistry.registerIcon("book.newItem", "resources/icons/twotone/add_box.svg", "New Item");
    iconRegistry.registerIcon("book.newMindMap", "resources/icons/twotone/account_tree.svg", "New Mind Map");
    iconRegistry.registerIcon("book.newTimeline", "resources/icons/twotone/timeline.svg", "New Timeline");
    iconRegistry.registerIcon("book.chapterBreak", "resources/icons/twotone/horizontal_rule.svg", "Chapter Break");
    iconRegistry.registerIcon("book.sceneBreak", "resources/icons/twotone/more_horiz.svg", "Scene Break");
    iconRegistry.registerIcon("book.properties", "resources/icons/twotone/tune.svg", "Book Properties");

    // -------------------------------------------------------------------------
    // INSERT MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("insert.image", "resources/icons/twotone/add_photo_alternate.svg", "Insert Image");
    iconRegistry.registerIcon("insert.table", "resources/icons/twotone/table_chart.svg", "Insert Table");
    iconRegistry.registerIcon("insert.link", "resources/icons/twotone/link.svg", "Insert Link");
    iconRegistry.registerIcon("insert.footnote", "resources/icons/twotone/short_text.svg", "Insert Footnote");
    iconRegistry.registerIcon("insert.endnote", "resources/icons/twotone/notes.svg", "Insert Endnote");
    iconRegistry.registerIcon("insert.comment", "resources/icons/twotone/add_comment.svg", "Insert Comment");
    iconRegistry.registerIcon("insert.annotation", "resources/icons/twotone/edit_note.svg", "Insert Annotation");
    iconRegistry.registerIcon("insert.specialChar", "resources/icons/twotone/emoji_symbols.svg", "Insert Special Character");
    iconRegistry.registerIcon("insert.dateTime", "resources/icons/twotone/schedule.svg", "Insert Date/Time");
    iconRegistry.registerIcon("insert.field", "resources/icons/twotone/data_object.svg", "Insert Field");

    // -------------------------------------------------------------------------
    // FORMAT MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("format.font", "resources/icons/twotone/text_format.svg", "Font");
    iconRegistry.registerIcon("format.style.heading1", "resources/icons/twotone/looks_one.svg", "Heading 1");
    iconRegistry.registerIcon("format.style.heading2", "resources/icons/twotone/looks_two.svg", "Heading 2");
    iconRegistry.registerIcon("format.style.heading3", "resources/icons/twotone/looks_3.svg", "Heading 3");
    iconRegistry.registerIcon("format.style.body", "resources/icons/twotone/subject.svg", "Body Text");
    iconRegistry.registerIcon("format.style.quote", "resources/icons/twotone/format_quote.svg", "Block Quote");
    iconRegistry.registerIcon("format.style.code", "resources/icons/twotone/code.svg", "Code Block");
    iconRegistry.registerIcon("format.style.manage", "resources/icons/twotone/style.svg", "Manage Styles");
    iconRegistry.registerIcon("format.bold", "resources/icons/twotone/format_bold.svg", "Bold");
    iconRegistry.registerIcon("format.italic", "resources/icons/twotone/format_italic.svg", "Italic");
    iconRegistry.registerIcon("format.underline", "resources/icons/twotone/format_underlined.svg", "Underline");
    iconRegistry.registerIcon("format.strikethrough", "resources/icons/twotone/strikethrough_s.svg", "Strikethrough");
    iconRegistry.registerIcon("format.alignLeft", "resources/icons/twotone/format_align_left.svg", "Align Left");
    iconRegistry.registerIcon("format.alignCenter", "resources/icons/twotone/format_align_center.svg", "Align Center");
    iconRegistry.registerIcon("format.alignRight", "resources/icons/twotone/format_align_right.svg", "Align Right");
    iconRegistry.registerIcon("format.justify", "resources/icons/twotone/format_align_justify.svg", "Justify");
    iconRegistry.registerIcon("format.increaseIndent", "resources/icons/twotone/format_indent_increase.svg", "Increase Indent");
    iconRegistry.registerIcon("format.decreaseIndent", "resources/icons/twotone/format_indent_decrease.svg", "Decrease Indent");
    iconRegistry.registerIcon("format.bullets", "resources/icons/twotone/format_list_bulleted.svg", "Bullets");
    iconRegistry.registerIcon("format.numbering", "resources/icons/twotone/format_list_numbered.svg", "Numbering");
    iconRegistry.registerIcon("format.color", "resources/icons/twotone/format_color_text.svg", "Text Color");
    iconRegistry.registerIcon("format.clearFormatting", "resources/icons/twotone/format_clear.svg", "Clear Formatting");
    iconRegistry.registerIcon("format.paragraph", "resources/icons/twotone/notes.svg", "Paragraph");

    // -------------------------------------------------------------------------
    // TOOLS MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("tools.stats.full", "resources/icons/twotone/analytics.svg", "Full Statistics");
    iconRegistry.registerIcon("tools.stats.wordCount", "resources/icons/twotone/label.svg", "Word Count");
    iconRegistry.registerIcon("tools.spellcheck", "resources/icons/twotone/spellcheck.svg", "Spellcheck");
    iconRegistry.registerIcon("tools.grammar", "resources/icons/twotone/grading.svg", "Grammar Check");
    iconRegistry.registerIcon("tools.focus.normal", "resources/icons/twotone/visibility.svg", "Normal Mode");
    iconRegistry.registerIcon("tools.focus.focused", "resources/icons/twotone/center_focus_strong.svg", "Focus Mode");
    iconRegistry.registerIcon("tools.focus.distractionFree", "resources/icons/twotone/fullscreen.svg", "Distraction Free");
    iconRegistry.registerIcon("tools.backupNow", "resources/icons/twotone/backup.svg", "Backup Now");
    iconRegistry.registerIcon("tools.autoSaveSettings", "resources/icons/twotone/sync.svg", "Auto-Save Settings");
    iconRegistry.registerIcon("tools.versionHistory", "resources/icons/twotone/history.svg", "Version History");
    iconRegistry.registerIcon("tools.plugins.manager", "resources/icons/twotone/extension.svg", "Plugin Manager");
    iconRegistry.registerIcon("tools.plugins.marketplace", "resources/icons/twotone/storefront.svg", "Marketplace");
    iconRegistry.registerIcon("tools.plugins.updates", "resources/icons/twotone/system_update.svg", "Plugin Updates");
    iconRegistry.registerIcon("tools.plugins.reload", "resources/icons/twotone/refresh.svg", "Reload Plugins");
    iconRegistry.registerIcon("tools.challenges", "resources/icons/twotone/emoji_events.svg", "Writing Challenges");
    iconRegistry.registerIcon("tools.writingGoals", "resources/icons/twotone/track_changes.svg", "Writing Goals");
    iconRegistry.registerIcon("tools.cloudSync", "resources/icons/twotone/cloud_sync.svg", "Cloud Sync");
    iconRegistry.registerIcon("tools.collaboration", "resources/icons/twotone/groups.svg", "Collaboration");
    iconRegistry.registerIcon("tools.readability", "resources/icons/twotone/auto_stories.svg", "Readability Analysis");
    iconRegistry.registerIcon("tools.toolbarManager", "resources/icons/twotone/build.svg", "Toolbar Manager");

    // -------------------------------------------------------------------------
    // ASSISTANT MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("assistant", "resources/icons/twotone/pets.svg", "AI Assistant");
    iconRegistry.registerIcon("assistant.ask", "resources/icons/twotone/psychology.svg", "Ask Assistant");
    iconRegistry.registerIcon("assistant.switch", "resources/icons/twotone/swap_horiz.svg", "Switch Assistant");
    iconRegistry.registerIcon("assistant.action.style", "resources/icons/twotone/brush.svg", "Style Suggestions");
    iconRegistry.registerIcon("assistant.action.plot", "resources/icons/twotone/auto_graph.svg", "Plot Analysis");
    iconRegistry.registerIcon("assistant.action.research", "resources/icons/twotone/science.svg", "Research Help");
    iconRegistry.registerIcon("assistant.action.speedDraft", "resources/icons/twotone/flash_on.svg", "Speed Draft");
    iconRegistry.registerIcon("assistant.action.grammar", "resources/icons/twotone/fact_check.svg", "Grammar Assistant");
    iconRegistry.registerIcon("assistant.settings", "resources/icons/twotone/settings.svg", "Assistant Settings");

    // -------------------------------------------------------------------------
    // VIEW MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("view.dashboard", "resources/icons/twotone/home.svg", "Dashboard");
    iconRegistry.registerIcon("view.navigator", "resources/icons/twotone/import_contacts.svg", "Navigator");
    iconRegistry.registerIcon("view.search", "resources/icons/twotone/search.svg", "Search Panel");
    iconRegistry.registerIcon("view.properties", "resources/icons/twotone/tune.svg", "Properties");
    iconRegistry.registerIcon("view.assistant", "resources/icons/twotone/pets.svg", "Assistant");
    iconRegistry.registerIcon("view.log", "resources/icons/twotone/description.svg", "Log");
    iconRegistry.registerIcon("view.perspectives.writer", "resources/icons/twotone/edit.svg", "Writer Perspective");
    iconRegistry.registerIcon("view.perspectives.editor", "resources/icons/twotone/rate_review.svg", "Editor Perspective");
    iconRegistry.registerIcon("view.perspectives.researcher", "resources/icons/twotone/science.svg", "Researcher Perspective");
    iconRegistry.registerIcon("view.perspectives.planner", "resources/icons/twotone/event_note.svg", "Planner Perspective");
    iconRegistry.registerIcon("view.perspectives.manage", "resources/icons/twotone/dashboard.svg", "Manage Perspectives");
    iconRegistry.registerIcon("view.toolbars.standard", "resources/icons/twotone/view_headline.svg", "Standard Toolbar");
    iconRegistry.registerIcon("view.toolbars.format", "resources/icons/twotone/text_format.svg", "Format Toolbar");
    iconRegistry.registerIcon("view.toolbars.book", "resources/icons/twotone/menu_book.svg", "Book Toolbar");
    iconRegistry.registerIcon("view.toolbars.quickAccess", "resources/icons/twotone/bolt.svg", "Quick Access");
    iconRegistry.registerIcon("view.toolbars.customize", "resources/icons/twotone/build.svg", "Customize Toolbars");
    iconRegistry.registerIcon("view.showStatusBar", "resources/icons/twotone/horizontal_split.svg", "Status Bar");
    iconRegistry.registerIcon("view.showStatsBar", "resources/icons/twotone/insert_chart.svg", "Stats Bar");
    iconRegistry.registerIcon("view.showFormattingMarks", "resources/icons/twotone/notes.svg", "Formatting Marks");
    iconRegistry.registerIcon("view.zoomIn", "resources/icons/twotone/zoom_in.svg", "Zoom In");
    iconRegistry.registerIcon("view.zoomOut", "resources/icons/twotone/zoom_out.svg", "Zoom Out");
    iconRegistry.registerIcon("view.resetZoom", "resources/icons/twotone/fit_screen.svg", "Reset Zoom");
    iconRegistry.registerIcon("view.resetLayout", "resources/icons/twotone/dashboard.svg", "Reset Layout");
    iconRegistry.registerIcon("view.fullScreen", "resources/icons/twotone/fullscreen.svg", "Full Screen");

    // -------------------------------------------------------------------------
    // LOG PANEL TOOLBAR ICONS (OpenSpec #00024)
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("log.options", "resources/icons/twotone/settings.svg", "Log Options");
    iconRegistry.registerIcon("log.openFolder", "resources/icons/twotone/folder_open.svg", "Open Log Folder");
    iconRegistry.registerIcon("log.copy", "resources/icons/twotone/content_copy.svg", "Copy Log");
    iconRegistry.registerIcon("log.clear", "resources/icons/twotone/delete.svg", "Clear Log");

    // -------------------------------------------------------------------------
    // HELP MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("help.help", "resources/icons/twotone/help.svg", "Help");
    iconRegistry.registerIcon("help.manual", "resources/icons/twotone/help.svg", "Kalahari Help");  // OpenSpec #00037: help toolbar icon
    iconRegistry.registerIcon("help.about", "resources/icons/twotone/info.svg", "About");
    iconRegistry.registerIcon("help.tutorial", "resources/icons/twotone/school.svg", "Tutorial");
    iconRegistry.registerIcon("help.shortcuts", "resources/icons/twotone/keyboard.svg", "Keyboard Shortcuts");
    iconRegistry.registerIcon("help.tipsTricks", "resources/icons/twotone/tips_and_updates.svg", "Tips & Tricks");
    iconRegistry.registerIcon("help.whatsNew", "resources/icons/twotone/new_releases.svg", "What's New");
    iconRegistry.registerIcon("help.reportBug", "resources/icons/twotone/bug_report.svg", "Report Bug");
    iconRegistry.registerIcon("help.suggestFeature", "resources/icons/twotone/feedback.svg", "Suggest Feature");
    iconRegistry.registerIcon("help.communityForum", "resources/icons/twotone/forum.svg", "Community Forum");
    iconRegistry.registerIcon("help.checkUpdates", "resources/icons/twotone/update.svg", "Check for Updates");

    // -------------------------------------------------------------------------
    // COMMON/UTILITY ICONS (for UI elements, dialogs, etc.)
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("common.check", "resources/icons/twotone/check.svg", "Check");
    iconRegistry.registerIcon("common.checkCircle", "resources/icons/twotone/check_circle.svg", "Check Circle");
    iconRegistry.registerIcon("common.cancel", "resources/icons/twotone/cancel.svg", "Cancel");
    iconRegistry.registerIcon("common.error", "resources/icons/twotone/error.svg", "Error");
    iconRegistry.registerIcon("common.warning", "resources/icons/twotone/warning.svg", "Warning");
    iconRegistry.registerIcon("common.notifications", "resources/icons/twotone/notifications.svg", "Notifications");
    iconRegistry.registerIcon("common.home", "resources/icons/twotone/home.svg", "Home");
    iconRegistry.registerIcon("common.menu", "resources/icons/twotone/menu.svg", "Menu");
    iconRegistry.registerIcon("common.moreVert", "resources/icons/twotone/more_vert.svg", "More Options");
    iconRegistry.registerIcon("common.moreHoriz", "resources/icons/twotone/more_horiz.svg", "More Options Horizontal");
    iconRegistry.registerIcon("common.expandMore", "resources/icons/twotone/expand_more.svg", "Expand More");
    iconRegistry.registerIcon("common.expandLess", "resources/icons/twotone/expand_less.svg", "Expand Less");
    iconRegistry.registerIcon("common.chevronRight", "resources/icons/twotone/chevron_right.svg", "Chevron Right");
    iconRegistry.registerIcon("common.chevronLeft", "resources/icons/twotone/chevron_left.svg", "Chevron Left");
    iconRegistry.registerIcon("common.arrowBack", "resources/icons/twotone/arrow_back.svg", "Arrow Back");
    iconRegistry.registerIcon("common.arrowForward", "resources/icons/twotone/arrow_forward.svg", "Arrow Forward");
    iconRegistry.registerIcon("common.arrowUpward", "resources/icons/twotone/arrow_upward.svg", "Arrow Up");
    iconRegistry.registerIcon("common.arrowDownward", "resources/icons/twotone/arrow_downward.svg", "Arrow Down");
    iconRegistry.registerIcon("common.delete", "resources/icons/twotone/delete.svg", "Delete");
    // Aliases for dialog buttons (navigation.up, navigation.down, action.delete)
    iconRegistry.registerIcon("navigation.up", "resources/icons/twotone/arrow_upward.svg", "Move Up");
    iconRegistry.registerIcon("navigation.down", "resources/icons/twotone/arrow_downward.svg", "Move Down");
    iconRegistry.registerIcon("action.delete", "resources/icons/twotone/delete.svg", "Delete");
    iconRegistry.registerIcon("common.firstPage", "resources/icons/twotone/first_page.svg", "First Page");
    iconRegistry.registerIcon("common.lastPage", "resources/icons/twotone/last_page.svg", "Last Page");
    iconRegistry.registerIcon("common.sort", "resources/icons/twotone/sort.svg", "Sort");
    iconRegistry.registerIcon("common.filterList", "resources/icons/twotone/filter_list.svg", "Filter");
    iconRegistry.registerIcon("common.lock", "resources/icons/twotone/lock.svg", "Lock");
    iconRegistry.registerIcon("common.lockOpen", "resources/icons/twotone/lock_open.svg", "Unlock");
    iconRegistry.registerIcon("common.print", "resources/icons/twotone/print.svg", "Print");
    iconRegistry.registerIcon("common.share", "resources/icons/twotone/share.svg", "Share");
    iconRegistry.registerIcon("common.attachFile", "resources/icons/twotone/attach_file.svg", "Attach File");
    iconRegistry.registerIcon("common.filePresent", "resources/icons/twotone/file_present.svg", "File Present");
    iconRegistry.registerIcon("common.folder", "resources/icons/twotone/folder.svg", "Folder");
    iconRegistry.registerIcon("common.createNewFolder", "resources/icons/twotone/create_new_folder.svg", "Create Folder");
    iconRegistry.registerIcon("common.driveFileMove", "resources/icons/twotone/drive_file_move.svg", "Move File");
    iconRegistry.registerIcon("common.fileCopy", "resources/icons/twotone/file_copy.svg", "Copy File");
    // Dock title bar button icons (OpenSpec #00032)
    iconRegistry.registerIcon("dock.float", "resources/icons/twotone/fullscreen_exit.svg", "Float");
    iconRegistry.registerIcon("dock.close", "resources/icons/twotone/close.svg", "Close");

    // -------------------------------------------------------------------------
    // NEW ITEM DIALOG ICONS (OpenSpec #00033)
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("template.novel", "resources/icons/twotone/auto_stories.svg", "Novel");
    iconRegistry.registerIcon("template.shortStories", "resources/icons/twotone/book.svg", "Short Stories");
    iconRegistry.registerIcon("template.nonfiction", "resources/icons/twotone/menu_book.svg", "Non-fiction");
    iconRegistry.registerIcon("template.screenplay", "resources/icons/twotone/theaters.svg", "Screenplay");
    iconRegistry.registerIcon("template.poetry", "resources/icons/twotone/edit_note.svg", "Poetry");
    iconRegistry.registerIcon("template.empty", "resources/icons/twotone/folder_open.svg", "Empty Project");
    iconRegistry.registerIcon("template.chapter", "resources/icons/twotone/article.svg", "Chapter");
    iconRegistry.registerIcon("template.mindmap", "resources/icons/twotone/account_tree.svg", "Mind Map");
    iconRegistry.registerIcon("template.timeline", "resources/icons/twotone/timeline.svg", "Timeline");
    iconRegistry.registerIcon("template.note", "resources/icons/twotone/sticky_note_2.svg", "Note");
    iconRegistry.registerIcon("template.character", "resources/icons/twotone/person.svg", "Character");
    iconRegistry.registerIcon("template.location", "resources/icons/twotone/place.svg", "Location");

    // -------------------------------------------------------------------------
    // STRUCTURE ICONS (OpenSpec #00034)
    // Navigator panel section differentiation
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("structure.frontmatter", "resources/icons/twotone/first_page.svg", "Front Matter");
    iconRegistry.registerIcon("structure.body", "resources/icons/twotone/menu_book.svg", "Body");
    iconRegistry.registerIcon("structure.backmatter", "resources/icons/twotone/last_page.svg", "Back Matter");
    iconRegistry.registerIcon("structure.part", "resources/icons/twotone/folder_open.svg", "Part");
    iconRegistry.registerIcon("structure.otherfiles", "resources/icons/twotone/folder_open.svg", "Other Files");
    iconRegistry.registerIcon("project.book", "resources/icons/twotone/auto_stories.svg", "Book Project");

    logger.debug("Registered {} icons with IconRegistry", 184);
}

} // namespace gui
} // namespace kalahari
