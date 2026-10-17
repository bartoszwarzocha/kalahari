/// @file issue_colors.h
/// @brief The colors of the misspelled words and the grammar issues, as the theme gives them

#pragma once

#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/render_context.h"
#include "kalahari/editor/text_issue.h"

#include <QColor>

namespace kalahari::gui {

/// @brief The theme's color of a kind of issue on the light or the dark paper: the color
///        of its waves in the editor (the editor's own colors when the theme has none)
inline QColor issueColor(editor::TextIssueKind kind, bool darkPaper) {
    const editor::RenderColors editorColors;
    return core::ThemeManager::getInstance().editorColor(
        editor::issueColorKey(kind, darkPaper),
        kind == editor::TextIssueKind::Spelling ? editorColors.spellError
                                                : editorColors.grammarWarning);
}

}  // namespace kalahari::gui
