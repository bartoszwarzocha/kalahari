/// @file issue_colors.h
/// @brief The colors of the misspelled words and the grammar issues, as the theme gives them

#pragma once

#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/render_context.h"
#include "kalahari/editor/text_issue.h"
#include "kalahari/gui/panels/annotation_colors.h"

#include <QColor>

namespace kalahari::gui {

/// @brief The theme's color of a kind of issue: of its waves in the editor and of the frames
///        of its cards in the Spelling and Grammar panel (the editor's own colors when the
///        theme has none)
inline QColor issueColor(editor::TextIssueKind kind) {
    const editor::RenderColors editorColors;
    return core::ThemeManager::getInstance().editorColor(
        editor::issueColorKey(kind),
        kind == editor::TextIssueKind::Spelling ? editorColors.spellError
                                                : editorColors.grammarWarning);
}

/// @brief The color of a kind of issue as it is shown on a background: as "Optimize colors in
///        themes" shows it (shownColor())
inline QColor issueColorOn(editor::TextIssueKind kind, const QColor& background) {
    return shownColor(issueColor(kind), background,
                      core::ThemeManager::getInstance().optimizesEditorColors());
}

}  // namespace kalahari::gui
