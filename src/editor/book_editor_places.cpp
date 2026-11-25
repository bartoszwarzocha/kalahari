// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_editor_places.cpp
/// @brief BookEditor: Edit > Go To..., the pages, the headings, the notes, the images, the
///        tables and the annotations of the text

#include <kalahari/editor/book_editor.h>

#include <kalahari/editor/annotation.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_footnotes.h>
#include <kalahari/editor/text_images.h>
#include <kalahari/editor/text_places.h>

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextFrame>
#include <QTextTable>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace kalahari::editor {

namespace {

/// The layout of a document; nullptr: another one
const KalahariTextDocumentLayout* layoutOf(const QTextDocument* document)
{
    return document != nullptr
               ? qobject_cast<const KalahariTextDocumentLayout*>(document->documentLayout())
               : nullptr;
}

/// The positions of the tables of a frame and of the frames in it, in text order
std::vector<int> tablePositions(const QTextFrame& root)
{
    std::vector<int> places;
    std::vector<const QTextFrame*> frames{&root};
    while (!frames.empty()) {
        const QTextFrame* const frame = frames.back();
        frames.pop_back();
        for (const QTextFrame* child : frame->childFrames()) {
            if (const auto* table = qobject_cast<const QTextTable*>(child)) {
                places.push_back(table->cellAt(0, 0).firstPosition());
            }
            frames.push_back(child);
        }
    }
    std::ranges::sort(places);
    return places;
}

/// The positions of the headings of a document
std::vector<int> headingPositions(const QTextDocument& document)
{
    std::vector<int> places;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (headingLevelOf(paragraphStyleOf(block.blockFormat())) > 0) {
            places.push_back(block.position());
        }
    }
    return places;
}

/// The positions of the notes of a document: after their numbers, as when an endnote is
/// clicked
std::vector<int> notePositions(const QTextDocument& document)
{
    std::vector<int> places;
    if (const KalahariTextDocumentLayout* layout = layoutOf(&document)) {
        for (const NotePlace& note : layout->footnoteNumbers().notes()) {
            places.push_back(note.position + 1);
        }
    }
    return places;
}

/// The positions of the annotations of a document, in text order
std::vector<int> annotationPositions(const QTextDocument& document)
{
    std::vector<int> places;
    for (const AnnotationPlace& place : annotationsIn(document)) {
        places.push_back(place.start);
    }
    std::ranges::stable_sort(places);
    return places;
}

/// The outcome of a step of Go To that did not go: @p result, with the @p count places
GoToOutcome outcomeOf(GoToResult result, int count)
{
    GoToOutcome outcome;
    outcome.result = result;
    outcome.count = count;
    return outcome;
}

}  // namespace

PlaceNumber placeNumberOf(const QString& text)
{
    static const QRegularExpression number(QStringLiteral("^([+-]?)\\s*(\\d{1,9})$"));
    const QRegularExpressionMatch match = number.match(text.trimmed());
    PlaceNumber place;
    if (!match.hasMatch()) {
        return place;
    }
    const int value = match.captured(2).toInt();
    if (value == 0) {
        return place;
    }
    place.valid = true;
    place.relative = !match.captured(1).isEmpty();
    place.value = match.captured(1) == QLatin1String("-") ? -value : value;
    return place;
}

// =============================================================================
// The places
// =============================================================================

std::vector<int> BookEditor::placePositions(PlaceKind kind) const
{
    const QTextDocument* document = m_textBuffer.get();
    if (document == nullptr) {
        return {};
    }
    switch (kind) {
        case PlaceKind::Page:
            break;
        case PlaceKind::Heading:
            return headingPositions(*document);
        case PlaceKind::Note:
            return notePositions(*document);
        case PlaceKind::Image:
            return imagePositions(*document, 0, document->characterCount());
        case PlaceKind::Table:
            return tablePositions(*document->rootFrame());
        case PlaceKind::Annotation:
            return annotationPositions(*document);
    }
    return {};
}

int BookEditor::placeOfCursor() const
{
    return hasSelection() ? calculateAbsolutePosition(m_selection.normalized().start)
                          : calculateAbsolutePosition(m_cursorPosition);
}

int BookEditor::placesBeforeCursor(PlaceKind kind, const std::vector<int>& places) const
{
    const int cursor = placeOfCursor();
    const bool selects = kind == PlaceKind::Image || kind == PlaceKind::Annotation;
    const bool selected = kind == PlaceKind::Image ? selectedImage().has_value() : hasSelection();
    const auto after = selects && !selected ? std::ranges::lower_bound(places, cursor)
                                            : std::ranges::upper_bound(places, cursor);
    return static_cast<int>(after - places.begin());
}

// =============================================================================
// Go To
// =============================================================================

bool BookEditor::atLastPlace(PlaceKind kind, const std::vector<int>& places) const
{
    return m_lastPlace.kind == kind && m_lastPlace.index >= 0 &&
           std::cmp_less(m_lastPlace.index, places.size()) &&
           m_lastPlace.cursor == calculateAbsolutePosition(m_cursorPosition) &&
           m_lastPlace.anchor == calculateAbsolutePosition(m_selectionAnchor) &&
           places[static_cast<size_t>(m_lastPlace.index)] == placeOfCursor();
}

int BookEditor::adjacentPlace(PlaceKind kind, const std::vector<int>& places, bool next) const
{
    if (atLastPlace(kind, places)) {
        return m_lastPlace.index + (next ? 1 : -1);
    }
    if (next) {
        return placesBeforeCursor(kind, places);
    }
    const auto atCursor = std::ranges::lower_bound(places, placeOfCursor());
    return static_cast<int>(atCursor - places.begin()) - 1;
}

GoToOutcome BookEditor::goToPlace(PlaceKind kind, const QString& number)
{
    const PlaceNumber wanted = placeNumberOf(number);
    if (kind == PlaceKind::Page) {
        return goToPageNumber(wanted);
    }
    const std::vector<int> places = placePositions(kind);
    const int count = static_cast<int>(places.size());
    if (places.empty()) {
        return outcomeOf(GoToResult::NoPlaces, count);
    }
    // A note by the number it shows, also "iii" or "*"
    if (kind == PlaceKind::Note && !wanted.relative) {
        return goToNoteNumber(places, number.trimmed());
    }
    if (!wanted.valid) {
        return outcomeOf(GoToResult::Invalid, count);
    }
    // On or back from the place the cursor is at: the one Go To went to, else the last at or
    // before the cursor (-1: before the first)
    const int current =
        atLastPlace(kind, places) ? m_lastPlace.index : placesBeforeCursor(kind, places) - 1;
    const int index = wanted.relative ? current + wanted.value : wanted.value - 1;
    if (index < 0 || index >= count) {
        return outcomeOf(GoToResult::NoNumber, count);
    }
    return goToPlaceAt(kind, places, index);
}

GoToOutcome BookEditor::goToPageNumber(const PlaceNumber& wanted)
{
    if (!wanted.valid && totalPages() > 0) {
        GoToOutcome outcome = outcomeOf(GoToResult::Invalid, totalPages());
        outcome.firstNumber = firstPageNumber();
        return outcome;
    }
    // A number of its own is the one the page shows: its page in the whole book
    return goToPageAt(wanted.relative ? currentPage() + wanted.value
                                      : wanted.value - firstPageNumber() + 1,
                      GoToResult::NoNumber);
}

GoToOutcome BookEditor::goToPageAt(int page, GoToResult beyond)
{
    const int pages = totalPages();
    if (pages == 0) {
        return outcomeOf(GoToResult::NoPlaces, pages);
    }
    GoToOutcome outcome = outcomeOf(GoToResult::Done, pages);
    outcome.firstNumber = firstPageNumber();
    if (page < 1 || page > pages) {
        outcome.result = beyond;
        return outcome;
    }
    goToPage(page);
    outcome.ordinal = page;
    return outcome;
}

GoToOutcome BookEditor::goToNoteNumber(const std::vector<int>& places, const QString& shown)
{
    const int count = static_cast<int>(places.size());
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    if (shown.isEmpty() || layout == nullptr) {
        return outcomeOf(GoToResult::Invalid, count);
    }
    const FootnoteNumbers& numbers = layout->footnoteNumbers();
    for (size_t i = 0; i < numbers.notes().size(); ++i) {
        if (numbers.number(i).compare(shown, Qt::CaseInsensitive) == 0) {
            return goToPlaceAt(PlaceKind::Note, places, static_cast<int>(i));
        }
    }
    return outcomeOf(GoToResult::NoNumber, count);
}

GoToOutcome BookEditor::goToAdjacentPlace(PlaceKind kind, bool next)
{
    const GoToResult beyond = next ? GoToResult::NoFurther : GoToResult::NoEarlier;
    if (kind == PlaceKind::Page) {
        return goToPageAt(currentPage() + (next ? 1 : -1), beyond);
    }
    const std::vector<int> places = placePositions(kind);
    const int count = static_cast<int>(places.size());
    if (places.empty()) {
        return outcomeOf(GoToResult::NoPlaces, count);
    }
    // On from the place Go To went to, while the cursor stays there; else the first place after
    // the cursor, or the last before it
    const int index = adjacentPlace(kind, places, next);
    if (index < 0 || index >= count) {
        return outcomeOf(beyond, count);
    }
    return goToPlaceAt(kind, places, index);
}

GoToOutcome BookEditor::goToPlaceAt(PlaceKind kind, const std::vector<int>& places, int index)
{
    GoToOutcome outcome;
    outcome.count = static_cast<int>(places.size());
    outcome.ordinal = index + 1;
    const int position = places[static_cast<size_t>(index)];
    const QTextDocument* const document = m_textBuffer.get();
    const auto putCursor = [this, document](int at) {
        clearSelection();
        const CursorPosition place = absoluteToCursorPosition(document, at);
        m_selectionAnchor = place;
        setCursorPosition(place);
    };
    switch (kind) {
        case PlaceKind::Page:
            break;
        case PlaceKind::Heading:
        case PlaceKind::Table:
            putCursor(position);
            break;
        case PlaceKind::Note:
            if (const KalahariTextDocumentLayout* layout = layoutOf(document)) {
                outcome.number = layout->footnoteNumbers().number(static_cast<size_t>(index));
            }
            putCursor(position);
            break;
        case PlaceKind::Image:
            clearSelection();
            selectImage(position);
            break;
        case PlaceKind::Annotation: {
            // The one of this place among those that start there
            const std::vector<AnnotationPlace> annotations = annotationsIn(*document);
            std::vector<const AnnotationPlace*> ordered;
            ordered.reserve(annotations.size());
            for (const AnnotationPlace& place : annotations) {
                ordered.push_back(&place);
            }
            std::ranges::stable_sort(ordered, std::ranges::less{},
                                     [](const AnnotationPlace* place) { return place->start; });
            goToAnnotation(ordered[static_cast<size_t>(index)]->annotation.id);
            break;
        }
    }
    ensureCursorVisible();
    update();
    m_lastPlace = {.kind = kind,
                   .index = index,
                   .cursor = calculateAbsolutePosition(m_cursorPosition),
                   .anchor = calculateAbsolutePosition(m_selectionAnchor)};
    return outcome;
}

}  // namespace kalahari::editor
