// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file clipping_texts.h
/// @brief What the program says of a clipping (core::ClippingData): where it comes from, the
///        place it was cut from and when it was inserted, alike in the list of the clippings,
///        the Properties panel and the window of Insert > Clipping

#ifndef KALAHARI_GUI_CLIPPING_TEXTS_H
#define KALAHARI_GUI_CLIPPING_TEXTS_H

#include <kalahari/core/clipping.h>

#include <QCoreApplication>
#include <QString>

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief Where a clipping comes from, as the writer reads it
struct ClippingFrom {
    /// The title of the text it was cut from; "(deleted)" after the title it had when that
    /// text is gone; empty: a clipping added by hand
    QString title;
    bool live = false;  ///< That text is in the book: the clipping can go back to its place
};

/// @brief The texts of a clipping
class ClippingTexts {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::ClippingTexts)

public:
    /// @brief Words of the text before the place that placeText() shows at most
    static constexpr int PLACE_TEXT_WORDS = 4;

    /// @brief Where clipping @p data comes from in the book open in @p book
    [[nodiscard]] static ClippingFrom fromOf(const core::ProjectManager& book,
                                             const core::ClippingData& data);

    /// @brief The place @p origin names in its text: "paragraph 3, after "…the horizon."";
    ///        "the start of paragraph 3"; empty: no place (a clipping added by hand)
    [[nodiscard]] static QString placeText(const core::ClippingOrigin& origin);

    /// @brief When clipping @p data was inserted last: "today, 7:50"; empty: never
    [[nodiscard]] static QString insertedText(const core::ClippingData& data);

    /// @brief What Restore to Its Place does with a clipping from @p from, or why it cannot:
    ///        it was added by hand, or its text is gone
    [[nodiscard]] static QString restoreToolTip(const ClippingFrom& from);

    /// @brief The last @p count words of @p text, with "…" before them when it has more
    [[nodiscard]] static QString lastWords(const QString& text, int count);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_CLIPPING_TEXTS_H
