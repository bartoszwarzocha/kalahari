/// @file chapter_cursor.cpp
/// @brief A chapter opens where its cursor was when it was closed last

#include "kalahari/gui/utils/chapter_cursor.h"

#include "kalahari/core/project_database.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_types.h"

#include <QChar>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <cmath>
#include <limits>

namespace kalahari {
namespace gui {
namespace utils {

namespace {

/// The key of the place of the chapter's cursor in the book's settings
QString cursorKey(const QString& elementId) {
    return QStringLiteral("cursor.") + elementId;
}

}  // anonymous namespace

void rememberChapterCursor(core::ProjectDatabase& database, const QString& elementId,
                           const editor::BookEditor& editor) {
    // Its paragraph, the offset in it and the height of its line in the view, a part of the
    // view's height: "12,40,0.375"
    const editor::CursorPosition position = editor.cursorPosition();
    database.setSetting(cursorKey(elementId),
                        QStringLiteral("%1,%2,%3")
                            .arg(position.paragraph)
                            .arg(position.offset)
                            .arg(QString::number(editor.cursorLineViewHeight(), 'f', 3)));
}

bool restoreChapterCursor(const core::ProjectDatabase& database, const QString& elementId,
                          editor::BookEditor& editor) {
    const QStringList place =
        database.getSetting(cursorKey(elementId)).toString().split(QLatin1Char(','));
    if (place.size() != 3) {
        return false;
    }
    bool paragraphRead = false;
    bool offsetRead = false;
    bool heightRead = false;
    const int paragraph = place[0].toInt(&paragraphRead);
    const int offset = place[1].toInt(&offsetRead);
    const double height = place[2].toDouble(&heightRead);
    if (!paragraphRead || !offsetRead || !heightRead || paragraph < 0 || offset < 0 ||
        !std::isfinite(height)) {
        return false;
    }
    // A place past the end of the text, as after changes that were not saved: its end (the
    // editor keeps an offset within its paragraph)
    const int last = static_cast<int>(editor.paragraphCount()) - 1;
    editor.setCursorPosition(paragraph > last
                                 ? editor::CursorPosition{last, std::numeric_limits<int>::max()}
                                 : editor::CursorPosition{paragraph, offset});
    editor.ensureCursorVisible();
    editor.scrollCursorLineTo(height);
    return true;
}

} // namespace utils
} // namespace gui
} // namespace kalahari
