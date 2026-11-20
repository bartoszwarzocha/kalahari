// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_editor_tab_guides.cpp
/// @brief BookEditor: the tab guides of the text (Format > Align Text), dashed lines down the
///        text that a Tab aligns it to, set and moved from the keyboard

#include <kalahari/editor/book_editor.h>

#include "kalahari/editor/detail/book_editor_internal.h"

#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/tab_guides.h>

#include <QTextCursor>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace kalahari::editor {

namespace {

/// @brief The guide of @p guides nearest @p at; none without guides
std::optional<size_t> nearestGuide(const std::vector<double>& guides, double at) {
    if (guides.empty()) {
        return std::nullopt;
    }
    const auto nearest = std::min_element(guides.cbegin(), guides.cend(),
                                          [at](double first, double second) {
                                              return std::abs(first - at) < std::abs(second - at);
                                          });
    return static_cast<size_t>(nearest - guides.cbegin());
}

/// @brief Where the guide @p index of @p guides may go, the lowest and the highest place:
///        between the guides beside it, inside a text @p textWidth millimetres wide
std::pair<double, double> roomOf(const std::vector<double>& guides, size_t index,
                                 double textWidth) {
    const double lowest =
        index > 0 ? guides[index - 1] + TAB_GUIDE_MIN_GAP_MM : TAB_GUIDE_STEP_MM;
    const double highest = index + 1 < guides.size()
                               ? guides[index + 1] - TAB_GUIDE_MIN_GAP_MM
                               : std::min(TAB_GUIDE_MAX_MM, textWidth - TAB_GUIDE_STEP_MM);
    return {lowest, highest};
}

}  // namespace

std::vector<double> BookEditor::tabGuides() const {
    return m_textBuffer ? tabGuidesOf(*m_textBuffer) : std::vector<double>();
}

bool BookEditor::setTabGuides(const std::vector<double>& guides) {
    if (refusesChange()) {
        return false;
    }
    ensureDocument();
    const std::vector<double> before = tabGuidesOf(*m_textBuffer);
    const std::vector<double> after = normalizedTabGuides(guides);
    if (after == before) {
        return false;
    }

    // One undo step, which changes no text: undoing or redoing it brings back the cursor and
    // the selection it was made with, and the guides of its side
    QTextCursor step(m_textBuffer.get());
    step.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    m_textBuffer->appendUndoItem(new StateUndoItem([this, before] { applyTabGuides(before); },
                                                   [this, after] { applyTabGuides(after); }));
    applyTabGuides(after);
    step.endEditBlock();  // the text differs from the saved one now (checkModified())

    syncPipelineCursor();  // the text at the cursor may have moved to a guide
    update();
    emit contentChanged();
    return true;
}

void BookEditor::applyTabGuides(const std::vector<double>& guides) {
    setTabGuidesOf(*m_textBuffer, guides);
    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    update();
}

double BookEditor::textWidthMillimetres() const {
    const auto* layout = m_textBuffer ? qobject_cast<const KalahariTextDocumentLayout*>(
                                            m_textBuffer->documentLayout())
                                      : nullptr;
    const double perMillimetre = layout != nullptr ? layout->typography().pixelsPerMillimetre : 0.0;
    if (!m_renderPipeline || perMillimetre <= 0.0) {
        return 0.0;
    }
    return m_renderPipeline->context().computed.textWidth / perMillimetre;
}

std::optional<double> BookEditor::cursorMillimetres() const {
    if (!m_textBuffer || !m_renderPipeline) {
        return std::nullopt;
    }
    const auto* layout =
        qobject_cast<const KalahariTextDocumentLayout*>(m_textBuffer->documentLayout());
    const double perMillimetre = layout != nullptr ? layout->typography().pixelsPerMillimetre : 0.0;
    const QRectF caret = m_renderPipeline->caretRect(m_cursorPosition);
    if (perMillimetre <= 0.0 || caret.isNull()) {
        return std::nullopt;
    }
    // The caret in the view, back to the text: the zoom, the scroll and the page's side gone
    const QPointF place =
        m_renderPipeline->widgetToDocument(QPointF(caret.left(), caret.center().y()));
    return place.x() / perMillimetre;
}

TabGuideOutcome BookEditor::toggleTabGuideAtCursor() {
    if (refusesChange()) {
        return {TabGuideResult::Refused};
    }
    ensureDocument();
    std::vector<double> guides = tabGuides();
    const double at = cursorMillimetres().value_or(0.0);

    // The guide at the cursor goes
    if (const std::optional<size_t> nearest = nearestGuide(guides, at);
        nearest && std::abs(guides[*nearest] - at) < TAB_GUIDE_MIN_GAP_MM) {
        const double removed = guides[*nearest];
        guides.erase(guides.begin() + static_cast<std::ptrdiff_t>(*nearest));
        setTabGuides(guides);
        return {TabGuideResult::Removed, removed};
    }

    // else a new one comes, on the step nearest the cursor, inside the text
    const double place = std::round(at / TAB_GUIDE_STEP_MM) * TAB_GUIDE_STEP_MM;
    const double right = std::min(TAB_GUIDE_MAX_MM, textWidthMillimetres());
    if (place < TAB_GUIDE_STEP_MM || place >= right) {
        return {TabGuideResult::AtEdge, place};
    }
    if (static_cast<int>(guides.size()) >= MAX_TAB_GUIDES) {
        return {TabGuideResult::Full, place};
    }
    guides.push_back(place);
    setTabGuides(guides);
    const std::vector<double> kept = tabGuides();
    if (std::find(kept.cbegin(), kept.cend(), place) == kept.cend()) {
        return {TabGuideResult::Blocked, place};  // too close to another one (not on a step)
    }
    return {TabGuideResult::Added, place};
}

TabGuideOutcome BookEditor::moveTabGuide(int steps) {
    if (refusesChange()) {
        return {TabGuideResult::Refused};
    }
    std::vector<double> guides = tabGuides();
    const std::optional<size_t> chosen = tabGuideToMove(guides);
    if (!chosen || steps == 0) {
        return {chosen ? TabGuideResult::Blocked : TabGuideResult::NoGuides};
    }

    // Between the guides beside it, inside the text
    const size_t index = *chosen;
    m_movedTabGuideCursor = m_cursorPosition;
    m_movedTabGuide = guides[index];
    const double moved = guides[index] + steps * TAB_GUIDE_STEP_MM;
    const auto [lowest, highest] = roomOf(guides, index, textWidthMillimetres());
    // A step is a whole number of millimetres or a part of one: half of it is room enough
    const double tolerance = TAB_GUIDE_STEP_MM / 2.0;
    if (moved < lowest - tolerance || moved > highest + tolerance) {
        return {TabGuideResult::Blocked, guides[index]};
    }
    guides[index] = moved;
    setTabGuides(guides);
    m_movedTabGuide = moved;
    return {TabGuideResult::Moved, moved};
}

std::optional<size_t> BookEditor::tabGuideToMove(const std::vector<double>& guides) const {
    if (m_movedTabGuide && m_movedTabGuideCursor == m_cursorPosition) {
        if (const auto same = std::find(guides.cbegin(), guides.cend(), *m_movedTabGuide);
            same != guides.cend()) {
            return static_cast<size_t>(same - guides.cbegin());
        }
    }
    return nearestGuide(guides, cursorMillimetres().value_or(0.0));
}

TabGuideOutcome BookEditor::clearTabGuides() {
    if (refusesChange()) {
        return {TabGuideResult::Refused};
    }
    if (tabGuides().empty()) {
        return {TabGuideResult::NoGuides};
    }
    setTabGuides({});
    return {TabGuideResult::Cleared};
}

}  // namespace kalahari::editor
