/// @file chapter_cursor.h
/// @brief A chapter opens where its cursor was when it was closed last

#ifndef KALAHARI_GUI_UTILS_CHAPTER_CURSOR_H
#define KALAHARI_GUI_UTILS_CHAPTER_CURSOR_H

class QString;

namespace kalahari {
namespace core {
class ProjectDatabase;
}
namespace editor {
class BookEditor;
}
namespace gui {
namespace utils {

/// The setting of where a chapter opens: CHAPTER_OPENS_WHERE_LEFT, where its cursor was when it
/// was closed last (also before the program was closed), or CHAPTER_OPENS_AT_START
inline constexpr const char* CHAPTER_OPENS_AT_KEY = "editor.chapterOpensAt";
inline constexpr const char* CHAPTER_OPENS_WHERE_LEFT = "lastEdit";
inline constexpr const char* CHAPTER_OPENS_AT_START = "start";

/// @brief Keep in the book's database where the cursor of @p editor, the chapter @p elementId,
///        is now, and the height of its line in the view
///
/// The book keeps it, as Word and LibreOffice keep it in the document: a copy of the book
/// opens there too, also on another computer.
/// @param database The database of the book of the chapter
/// @param elementId The ID of the chapter
/// @param editor The editor of the chapter
void rememberChapterCursor(core::ProjectDatabase& database, const QString& elementId,
                           const editor::BookEditor& editor);

/// @brief Put the cursor of @p editor, the chapter @p elementId just opened, where the book's
///        database kept it, its line at the same height in the view as then
///
/// A place past the end of the text, as after changes that were not saved, is its end. The
/// line stays in the view, also when it was off the view then or the view is smaller now.
/// @param database The database of the book of the chapter
/// @param elementId The ID of the chapter
/// @param editor The editor of the chapter, with its text
/// @return Whether the database kept a place for the chapter; without one the cursor stays
bool restoreChapterCursor(const core::ProjectDatabase& database, const QString& elementId,
                          editor::BookEditor& editor);

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_CHAPTER_CURSOR_H
