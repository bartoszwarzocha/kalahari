/// @file layout_notes_internal.h
/// @brief KalahariTextDocumentLayout: a note's text laid out (inside the layout's sources)

#pragma once

#include <kalahari/editor/kalahari_text_document_layout.h>

#include <QList>
#include <QString>
#include <QTextLayout>

#include <memory>
#include <vector>

namespace kalahari::editor {

/// @brief A note's text laid out in the style Footnote, its number in front of its first line
///
/// Its lines are stacked from 0 as one paragraph's lines are; a part of them is drawn at
/// the bottom of a page and the rest on the next one.
struct KalahariTextDocumentLayout::NoteBox {
    /// What it was laid out from: its kind, number, width and text
    QString signature;

    /// A paragraph of the note: its lines, and the ranges of its links (drawn in the link color)
    struct Paragraph {
        std::unique_ptr<QTextLayout> layout;
        QList<QTextLayout::FormatRange> links;
    };
    std::vector<Paragraph> paragraphs;

    /// A line's box: its paragraph, its top and height from the top of the note
    struct Line {
        size_t paragraph = 0;
        qreal top = 0.0;
        qreal height = 0.0;
    };
    std::vector<Line> lines;

    /// The note's height: from the top of its first line to the bottom of its last
    qreal height = 0.0;

    /// From the top of line @p line to the top of the next one (to the end of the last)
    qreal advance(size_t line) const {
        return line + 1 < lines.size() ? lines[line + 1].top - lines[line].top
                                       : lines[line].height;
    }

    /// From the top of line @p first to the bottom of line @p last - 1
    qreal heightOf(size_t first, size_t last) const {
        return last > first && last <= lines.size()
                   ? lines[last - 1].top + lines[last - 1].height - lines[first].top
                   : 0.0;
    }
};

}  // namespace kalahari::editor
