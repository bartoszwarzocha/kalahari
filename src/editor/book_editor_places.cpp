/// @file book_editor_places.cpp
/// @brief BookEditor: Edit > Go To..., the pages, the headings, the notes, the images, the
///        tables and the annotations of the text

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/editor/annotation.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_footnotes.h>
#include <kalahari/editor/text_images.h>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextFrame>
#include <QTextTable>

#include <algorithm>

namespace kalahari::editor {

namespace {

/// The layout of a document; nullptr: another one
const KalahariTextDocumentLayout* layoutOf(const QTextDocument* document)
{
    return document != nullptr
               ? qobject_cast<const KalahariTextDocumentLayout*>(document->documentLayout())
               : nullptr;
}

/// The tables of a frame and of the frames in it, in text order
void collectTables(const QTextFrame& frame, std::vector<int>& places)
{
    for (const QTextFrame* child : frame.childFrames()) {
        if (const auto* table = qobject_cast<const QTextTable*>(child)) {
            places.push_back(table->cellAt(0, 0).firstPosition());
        }
        collectTables(*child, places);
    }
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
    std::vector<int> places;
    const QTextDocument* document = m_textBuffer.get();
    if (document == nullptr) {
        return places;
    }
    switch (kind) {
        case PlaceKind::Page:
            break;
        case PlaceKind::Heading:
            for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
                if (headingLevelOf(paragraphStyleOf(block.blockFormat())) > 0) {
                    places.push_back(block.position());
                }
            }
            break;
        case PlaceKind::Note:
            // The cursor goes after the number, as when an endnote is clicked
            if (const KalahariTextDocumentLayout* layout = layoutOf(document)) {
                for (const NotePlace& note : layout->footnoteNumbers().notes()) {
                    places.push_back(note.position + 1);
                }
            }
            break;
        case PlaceKind::Image:
            places = imagePositions(*document, 0, document->characterCount());
            break;
        case PlaceKind::Table:
            collectTables(*document->rootFrame(), places);
            std::sort(places.begin(), places.end());
            break;
        case PlaceKind::Annotation:
            for (const AnnotationPlace& place : annotationsIn(*document)) {
                places.push_back(place.start);
            }
            std::stable_sort(places.begin(), places.end());
            break;
    }
    return places;
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
    const auto after = selects && !selected
                           ? std::lower_bound(places.begin(), places.end(), cursor)
                           : std::upper_bound(places.begin(), places.end(), cursor);
    return static_cast<int>(after - places.begin());
}

// =============================================================================
// Go To
// =============================================================================

GoToOutcome BookEditor::goToPlace(PlaceKind kind, const QString& number)
{
    GoToOutcome outcome;
    const PlaceNumber wanted = placeNumberOf(number);

    // The pages of the Page Layout view
    if (kind == PlaceKind::Page) {
        outcome.count = totalPages();
        if (outcome.count == 0) {
            outcome.result = GoToResult::NoPlaces;
            return outcome;
        }
        if (!wanted.valid) {
            outcome.result = GoToResult::Invalid;
            return outcome;
        }
        const int page = wanted.relative ? currentPage() + wanted.value : wanted.value;
        if (page < 1 || page > outcome.count) {
            outcome.result = GoToResult::NoNumber;
            return outcome;
        }
        goToPage(page);
        outcome.ordinal = page;
        return outcome;
    }

    const std::vector<int> places = placePositions(kind);
    outcome.count = static_cast<int>(places.size());
    if (places.empty()) {
        outcome.result = GoToResult::NoPlaces;
        return outcome;
    }
    // A note by the number it shows, also "iii" or "*"
    if (kind == PlaceKind::Note && !wanted.relative) {
        const QString shown = number.trimmed();
        const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
        if (shown.isEmpty() || layout == nullptr) {
            outcome.result = GoToResult::Invalid;
            return outcome;
        }
        const FootnoteNumbers& numbers = layout->footnoteNumbers();
        for (size_t i = 0; i < numbers.notes().size(); ++i) {
            if (numbers.number(i).compare(shown, Qt::CaseInsensitive) == 0) {
                return goToPlaceAt(kind, places, static_cast<int>(i));
            }
        }
        outcome.result = GoToResult::NoNumber;
        return outcome;
    }
    if (!wanted.valid) {
        outcome.result = GoToResult::Invalid;
        return outcome;
    }
    int index = wanted.value - 1;
    if (wanted.relative) {
        // From the place the cursor is at: the one Go To went to, else the last at or before
        // the cursor (-1: before the first)
        const int cursor = placeOfCursor();
        int current = placesBeforeCursor(kind, places) - 1;
        if (m_lastPlace.kind == kind && m_lastPlace.index >= 0 &&
            m_lastPlace.index < outcome.count &&
            m_lastPlace.cursor == calculateAbsolutePosition(m_cursorPosition) &&
            m_lastPlace.anchor == calculateAbsolutePosition(m_selectionAnchor) &&
            places[static_cast<size_t>(m_lastPlace.index)] == cursor) {
            current = m_lastPlace.index;
        }
        index = current + wanted.value;
    }
    if (index < 0 || index >= outcome.count) {
        outcome.result = GoToResult::NoNumber;
        return outcome;
    }
    return goToPlaceAt(kind, places, index);
}

GoToOutcome BookEditor::goToAdjacentPlace(PlaceKind kind, bool next)
{
    GoToOutcome outcome;
    if (kind == PlaceKind::Page) {
        outcome.count = totalPages();
        if (outcome.count == 0) {
            outcome.result = GoToResult::NoPlaces;
            return outcome;
        }
        const int page = currentPage() + (next ? 1 : -1);
        if (page < 1 || page > outcome.count) {
            outcome.result = next ? GoToResult::NoFurther : GoToResult::NoEarlier;
            return outcome;
        }
        goToPage(page);
        outcome.ordinal = page;
        return outcome;
    }

    const std::vector<int> places = placePositions(kind);
    outcome.count = static_cast<int>(places.size());
    if (places.empty()) {
        outcome.result = GoToResult::NoPlaces;
        return outcome;
    }
    // On from the place Go To went to, while the cursor stays there; else the first place after
    // the cursor, or the last before it
    const int cursor = placeOfCursor();
    int index = next ? placesBeforeCursor(kind, places)
                     : static_cast<int>(std::lower_bound(places.begin(), places.end(), cursor) -
                                        places.begin()) -
                           1;
    if (m_lastPlace.kind == kind && m_lastPlace.index >= 0 && m_lastPlace.index < outcome.count &&
        m_lastPlace.cursor == calculateAbsolutePosition(m_cursorPosition) &&
        m_lastPlace.anchor == calculateAbsolutePosition(m_selectionAnchor) &&
        places[static_cast<size_t>(m_lastPlace.index)] == cursor) {
        index = m_lastPlace.index + (next ? 1 : -1);
    }
    if (index < 0 || index >= outcome.count) {
        outcome.result = next ? GoToResult::NoFurther : GoToResult::NoEarlier;
        return outcome;
    }
    return goToPlaceAt(kind, places, index);
}

GoToOutcome BookEditor::goToPlaceAt(PlaceKind kind, const std::vector<int>& places, int index)
{
    GoToOutcome outcome;
    outcome.count = static_cast<int>(places.size());
    outcome.ordinal = index + 1;
    const int position = places[static_cast<size_t>(index)];
    QTextDocument* document = m_textBuffer.get();
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
            std::stable_sort(ordered.begin(), ordered.end(),
                             [](const AnnotationPlace* a, const AnnotationPlace* b) {
                                 return a->start < b->start;
                             });
            goToAnnotation(ordered[static_cast<size_t>(index)]->annotation.id);
            break;
        }
    }
    ensureCursorVisible();
    update();
    m_lastPlace = {kind, index, calculateAbsolutePosition(m_cursorPosition),
                   calculateAbsolutePosition(m_selectionAnchor)};
    return outcome;
}

}  // namespace kalahari::editor
