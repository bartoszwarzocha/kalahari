// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file paragraph_marks_clippings.cpp
/// @brief The scissors beside the paragraphs clippings were cut from

#include "kalahari/gui/paragraph_marks_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/clipping.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_types.h"
#include "kalahari/gui/version_texts.h"

#include <QDateTime>
#include <QString>
#include <QTextDocument>

#include <algorithm>
#include <optional>
#include <vector>

namespace kalahari::gui {

namespace {

/// @brief The clippings of @p book cut from text @p elementId and not inserted since, the
///        newest first
std::vector<const core::ProjectElement*> clippingsCutFrom(const core::ProjectManager& book,
                                                          const QString& elementId) {
    std::vector<const core::ProjectElement*> clippings;
    for (const QString& id : book.textElementIds()) {
        const core::ProjectElement* element = book.findElement(id);
        if (element != nullptr && core::ProjectManager::isClipping(*element) &&
            element->clipping.origin.element == elementId &&
            element->clipping.inserted.isEmpty()) {
            clippings.push_back(element);
        }
    }
    std::ranges::stable_sort(clippings, [](const core::ProjectElement* first,
                                           const core::ProjectElement* second) {
        return first->clipping.origin.cut > second->clipping.origin.cut;
    });
    return clippings;
}

}  // namespace

QList<editor::ParagraphMark> ParagraphMarksCoordinator::clippingMarksOf(
    const core::ProjectManager& book, const QString& elementId,
    const editor::BookEditor& editor) {
    QList<editor::ParagraphMark> marks;
    const QTextDocument* document = editor.textDocument();
    if (elementId.isEmpty() || document == nullptr ||
        !core::SettingsManager::getInstance().get<bool>(core::Clippings::MARKS_SETTING, true)) {
        return marks;
    }
    const QStringList paragraphs = paragraphsOf(*document);
    for (const core::ProjectElement* clipping : clippingsCutFrom(book, elementId)) {
        const std::optional<core::ClippingPlace> place =
            core::Clippings::placeIn(paragraphs, clipping->clipping.origin);
        if (!place) {
            continue;
        }
        const QString words = wordCountText(book.wordCount(clipping->id));
        const QDateTime& cut = clipping->clipping.origin.cut;
        //: Beside the scissors at a paragraph: a clipping cut from it. %1: its title, %2: its
        //: words ("46 words"), %3: when it was cut ("today, 7:42"). In Polish: „%1” (%2, %3)
        const QString title =
            cut.isValid() ? tr("\"%1\" (%2, %3)").arg(clipping->title, words, versionTime(cut))
                          //: A clipping without the time of its cut. In Polish: „%1” (%2)
                          : tr("\"%1\" (%2)").arg(clipping->title, words);
        const auto same =
            std::ranges::find(marks, place->paragraph, &editor::ParagraphMark::paragraph);
        if (same != marks.end()) {
            same->ids << clipping->id;
            same->titles << title;
        } else {
            marks.append(editor::ParagraphMark{.paragraph = place->paragraph,
                                               .ids = {clipping->id},
                                               .titles = {title},
                                               .kind = editor::ParagraphMarkKind::Clippings});
        }
    }
    std::ranges::sort(marks, {}, &editor::ParagraphMark::paragraph);
    return marks;
}

}  // namespace kalahari::gui
