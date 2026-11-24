// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_editor_points.cpp
/// @brief BookEditor: the fragments of a text for publishers linked to the points of its
///        template, their fills and their bars on the margin

#include <kalahari/editor/book_editor.h>

#include "kalahari/editor/detail/book_editor_internal.h"

#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/text_points.h>

#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>
#include <vector>

namespace kalahari::editor {

namespace {

/// The opacity of the fill of a fragment: on a light paper and on a dark one, of a point and
/// of the point chosen in the list
constexpr float POINT_FILL_LIGHT = 0.17F;
constexpr float POINT_FILL_LIGHT_CHOSEN = 0.34F;
constexpr float POINT_FILL_DARK = 0.24F;
constexpr float POINT_FILL_DARK_CHOSEN = 0.45F;

/// Paper darker than this (QColor::lightness()) is dark
constexpr int DARK_PAPER_LIGHTNESS = 128;

}  // namespace

void BookEditor::setTemplatePointLook(const TemplatePointLook& look)
{
    if (m_pointLook == look) {
        return;
    }
    m_pointLook = look;
    syncTemplatePointMarks();
    update();
}

QHash<QString, QList<PointFragment>> BookEditor::templatePointFragments() const
{
    if (!m_textBuffer) {
        return {};
    }
    return editor::templatePointFragments(*m_textBuffer);
}

QStringList BookEditor::templatePointsAtCursor() const
{
    if (!m_textBuffer) {
        return {};
    }
    QTextDocument* document = m_textBuffer.get();
    if (hasSelection()) {
        const SelectionRange range = m_selection.normalized();
        return templatePointsIn(*document, createCursor(document, range.start).position(),
                                createCursor(document, range.end).position());
    }
    return templatePointsAt(*document, createCursor(document, m_cursorPosition).position());
}

bool BookEditor::linkToTemplatePoint(const QString& id, bool linked)
{
    if (!m_textBuffer || id.isEmpty()) {
        return false;
    }
    QTextDocument* document = m_textBuffer.get();
    if (hasSelection()) {
        if (refusesChange()) {
            return false;
        }
        const SelectionRange range = m_selection.normalized();
        return changeTemplatePointLink(createCursor(document, range.start).position(),
                                       createCursor(document, range.end).position(), id, linked);
    }
    // Without a selection only a link is taken away: of the fragment of the cursor
    if (linked || refusesChange()) {
        return false;
    }
    const int position = createCursor(document, m_cursorPosition).position();
    for (const PointFragment& fragment : editor::templatePointFragments(*document).value(id)) {
        if (fragment.start <= position && position <= fragment.end) {
            return changeTemplatePointLink(fragment.start, fragment.end, id, false);
        }
    }
    return false;
}

bool BookEditor::unlinkTemplatePointEverywhere(const QString& id)
{
    if (!m_textBuffer || refusesChange()) {
        return false;
    }
    return changeTemplatePointLink(0, m_textBuffer->characterCount(), id, false);
}

bool BookEditor::goToTemplatePointFragment(const QString& id, int index)
{
    if (!m_textBuffer) {
        return false;
    }
    const QList<PointFragment> fragments = editor::templatePointFragments(*m_textBuffer).value(id);
    if (index < 0 || index >= fragments.size()) {
        return false;
    }
    // The fragment is selected, with the cursor at its end, as the place of an annotation
    const CursorPosition start = calculateCursorPosition(fragments.at(index).start);
    const CursorPosition end = calculateCursorPosition(fragments.at(index).end);
    clearSelection();
    setCursorPosition(end);
    m_selectionAnchor = start;
    setSelection({start, end});
    ensureCursorVisible();
    return true;
}

bool BookEditor::changeTemplatePointLink(int from, int to, const QString& id, bool linked)
{
    QTextDocument* document = m_textBuffer.get();
    if (!changesTemplatePointLink(*document, from, to, id, linked)) {
        return false;
    }

    // One undo step, which brings back the cursor and the selection; the text stays as it is
    const StepCursor state{.cursor = m_cursorPosition, .selection = m_selection};
    QTextCursor step(document);
    step.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, state] { m_stepCursor = state; }));
    linkTemplatePoint(*document, from, to, id, linked);
    document->appendUndoItem(new CallbackUndoItem([this, state] { m_stepCursor = state; }));
    step.endEditBlock();
    m_learnableChange.reset();
    update();
    emit contentChanged();
    return true;
}

void BookEditor::syncTemplatePointMarks()
{
    if (!m_renderPipeline) {
        return;
    }
    std::vector<EditorRenderPipeline::PointMark> marks;
    const TemplatePointLook& look = m_pointLook;
    if (m_textBuffer && !look.order.isEmpty() && !look.colors.isEmpty() &&
        (look.shown || look.bars || !look.current.isEmpty())) {
        // The fragments of the points of the template, by their starts
        struct Piece {
            PointFragment fragment;
            qsizetype place = 0;  ///< Of its point in the template
        };
        std::vector<Piece> pieces;
        const auto fragments = editor::templatePointFragments(*m_textBuffer);
        for (qsizetype place = 0; place < look.order.size(); ++place) {
            for (const PointFragment& fragment : fragments.value(look.order.at(place))) {
                pieces.push_back({fragment, place});
            }
        }
        std::stable_sort(pieces.begin(), pieces.end(), [](const Piece& a, const Piece& b) {
            return a.fragment.start != b.fragment.start ? a.fragment.start < b.fragment.start
                                                        : a.place < b.place;
        });

        const bool dark = m_annotationCards.colors().paper.lightness() < DARK_PAPER_LIGHTNESS;
        std::vector<int> laneEnds;  // where the fragment of each lane of the bars ends
        for (const Piece& piece : pieces) {
            // The first lane free at its start: fragments side by side get bars side by side
            auto lane = std::find_if(laneEnds.begin(), laneEnds.end(),
                                     [&piece](int end) { return end <= piece.fragment.start; });
            if (lane == laneEnds.end()) {
                lane = laneEnds.insert(laneEnds.end(), piece.fragment.end);
            } else {
                *lane = piece.fragment.end;
            }
            const QString& id = look.order.at(piece.place);
            const QColor color = look.colors.at(piece.place % look.colors.size());
            EditorRenderPipeline::PointMark mark;
            mark.start = calculateCursorPosition(piece.fragment.start);
            mark.end = calculateCursorPosition(piece.fragment.end);
            const bool chosen = id == look.current;
            if (look.shown || chosen) {
                mark.fill = color;
                mark.fill.setAlphaF(chosen ? (dark ? POINT_FILL_DARK_CHOSEN : POINT_FILL_LIGHT_CHOSEN)
                                           : (dark ? POINT_FILL_DARK : POINT_FILL_LIGHT));
            }
            if (look.bars) {
                mark.bar = color;
            }
            mark.lane = static_cast<int>(lane - laneEnds.begin());
            marks.push_back(mark);
        }
    }
    m_renderPipeline->setPointMarks(std::move(marks));
}

}  // namespace kalahari::editor
