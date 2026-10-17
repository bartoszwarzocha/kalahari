/// @file issue_colors.h
/// @brief The colors of the misspelled words and the grammar issues, as the theme gives them

#pragma once

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/render_context.h"
#include "kalahari/editor/text_issue.h"

#include <QColor>
#include <QString>

#include <string>

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

/// @brief The setting of the own color of a kind's cards in the Spelling and Grammar panel
///
/// Empty (the default): the cards take the color of the kind's underline.
/// @param kind The kind of issue
/// @param darkPanel true: the color in a dark panel
inline std::string issueFrameColorSetting(editor::TextIssueKind kind, bool darkPanel) {
    return std::string("spellingPanel.frameColors.") +
           (kind == editor::TextIssueKind::Spelling ? "spelling" : "grammar") +
           (darkPanel ? "Dark" : "Light");
}

/// @brief The color of a kind's cards in the Spelling and Grammar panel: their own one, or the
///        underline's on the paper as light or as dark as the panel
inline QColor issueFrameColor(editor::TextIssueKind kind, bool darkPanel) {
    const QColor own(QString::fromStdString(core::SettingsManager::getInstance().get<std::string>(
        issueFrameColorSetting(kind, darkPanel), std::string())));
    return own.isValid() ? own : issueColor(kind, darkPanel);
}

}  // namespace kalahari::gui
