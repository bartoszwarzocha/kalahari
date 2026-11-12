/// @file kalahari_text_document_layout_notes.cpp
/// @brief KalahariTextDocumentLayout: the notes - their numbers in the text, their texts under
///        their paragraphs (the continuous views) and at the bottom of the pages (page flow)

#include "layout_notes_internal.h"

#include <kalahari/editor/text_link.h>
#include <kalahari/editor/text_tables.h>

#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <QTextDocument>
#include <QTextLine>
#include <QTextOption>
#include <QTextTable>

#include <algorithm>
#include <cmath>
#include <limits>

namespace kalahari::editor {

namespace {

/// As the page flow of the layout compares its positions
constexpr qreal NOTE_EPSILON = 0.01;

/// The width of a line laid out without a width of the text
constexpr qreal UNWRAPPED_NOTE_WIDTH = 10000.0;

/// A note's number in the text and in front of its note: so much smaller than the text
constexpr qreal NOTE_NUMBER_SCALE = 0.65;

/// The short line above the notes: this share of the text's width
constexpr qreal NOTE_SEPARATOR_SHARE = 1.0 / 3.0;

/// The room of the line above the notes, the gap under the notes of a paragraph and above
/// the endnotes, in lines of the notes
constexpr qreal NOTE_SEPARATOR_LINES = 0.8;
constexpr qreal NOTE_BOTTOM_GAP_LINES = 0.5;
constexpr qreal ENDNOTES_GAP_LINES = 1.0;

/// Between a note's number and its text
constexpr QChar NOTE_NUMBER_GAP(0x00A0);

/// A font so many times larger (smaller below 1)
QFont scaledFont(QFont font, qreal factor) {
    if (font.pointSizeF() > 0) {
        font.setPointSizeF(font.pointSizeF() * factor);
    } else if (font.pixelSize() > 0) {
        font.setPixelSize(std::max(1, static_cast<int>(std::lround(font.pixelSize() * factor))));
    }
    return font;
}

/// The width of a text in a font, as the lines measure it (the font's design metrics)
qreal naturalWidth(const QString& text, const QFont& font) {
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    option.setUseDesignMetrics(true);
    layout.setTextOption(option);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (line.isValid()) {
        line.setLineWidth(UNWRAPPED_NOTE_WIDTH);
    }
    layout.endLayout();
    return layout.lineCount() > 0 ? layout.lineAt(0).naturalTextWidth() : 0.0;
}

}  // namespace

// =============================================================================
// The numbers
// =============================================================================

void KalahariTextDocumentLayout::setFootnoteContext(const FootnoteContext& context) {
    if (context == m_footnoteContext) {
        return;
    }
    m_footnoteContext = context;
    m_captionNumbersDirty = true;
    if (m_fieldChars.empty()) {
        return;  // nothing in the text shows the numbers: they are counted when asked for
    }
    const std::vector<std::pair<int, QString>> old = m_fieldSignatures;
    refreshFields(0, -1, 0, old);
}

const FootnoteNumbers& KalahariTextDocumentLayout::footnoteNumbers() const {
    if (m_captionNumbersDirty) {
        countFields();
    }
    return m_footnoteNumbers;
}

qreal KalahariTextDocumentLayout::noteNumberWidth(int position, const QTextFormat& format,
                                                  qreal* ascent, qreal* descent) const {
    const QFont font = fieldFont(position, format);
    const QFontMetricsF metrics(font);
    if (ascent != nullptr) {
        *ascent = metrics.ascent();
    }
    if (descent != nullptr) {
        *descent = metrics.descent();
    }
    return naturalWidth(footnoteNumbers().numberAt(position), scaledFont(font, NOTE_NUMBER_SCALE));
}

void KalahariTextDocumentLayout::drawNoteNumber(QPainter* painter, const QRectF& rect,
                                                qreal ascent, int position,
                                                const QTextFormat& format) const {
    const QString number = footnoteNumbers().numberAt(position);
    if (number.isEmpty()) {
        return;
    }
    // Raised: its top where the top of the text around it is, in the color of the text
    const QFont font = fieldFont(position, format);
    const QFont small = scaledFont(font, NOTE_NUMBER_SCALE);
    const qreal textAscent = ascent >= 0.0 ? ascent : QFontMetricsF(font).ascent();
    const qreal top = rect.top() + std::max<qreal>(0.0, textAscent - QFontMetricsF(font).ascent());
    QTextLayout text(number, small);
    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    option.setUseDesignMetrics(true);
    text.setTextOption(option);
    text.beginLayout();
    QTextLine line = text.createLine();
    if (line.isValid()) {
        line.setLineWidth(UNWRAPPED_NOTE_WIDTH);
    }
    text.endLayout();
    if (text.lineCount() > 0) {
        text.draw(painter, QPointF(rect.left(), top));
    }
}

// =============================================================================
// The texts of the notes
// =============================================================================

const KalahariTextDocumentLayout::BlockMetrics& KalahariTextDocumentLayout::noteMetrics() const {
    const ParagraphStyle& style = m_styles.shownStyle(QLatin1String(FOOTNOTE_STYLE_ID));
    auto it = m_styleMetrics.find(style.id);
    if (it == m_styleMetrics.end()) {
        it = m_styleMetrics.emplace(style.id, styleMetrics(style)).first;
    }
    return it->second;
}

qreal KalahariTextDocumentLayout::noteSeparatorRoom() const {
    return std::round(noteMetrics().estimatedLineHeight * NOTE_SEPARATOR_LINES);
}

qreal KalahariTextDocumentLayout::noteBottomGap() const {
    return std::round(noteMetrics().estimatedLineHeight * NOTE_BOTTOM_GAP_LINES);
}

const KalahariTextDocumentLayout::NoteBox& KalahariTextDocumentLayout::noteBox(size_t index) const {
    const FootnoteNumbers& numbers = footnoteNumbers();
    const Footnote& note = numbers.notes().at(index).note;
    const QString& number = numbers.number(index);
    const qreal documentWidth = document()->textWidth();
    const qreal width = documentWidth > 0 ? documentWidth : UNWRAPPED_NOTE_WIDTH;
    const QString signature = (note.kind == NoteKind::Endnote ? QStringLiteral("e")
                                                              : QStringLiteral("f")) +
                              number + QLatin1Char('\x1f') + QString::number(width) +
                              QLatin1Char('\x1f') + note.content;
    std::unique_ptr<NoteBox>& slot = m_noteBoxes[note.id];
    if (slot && slot->signature == signature) {
        return *slot;
    }
    slot = std::make_unique<NoteBox>();
    NoteBox& box = *slot;
    box.signature = signature;

    // In the style Footnote: its font over the document's, its line spacing and margins
    const BlockMetrics& metrics = noteMetrics();
    QTextCharFormat base;
    base.setFont(document()->defaultFont());
    base.merge(metrics.charFormat);
    const QFont font = base.font();
    const qreal lineWidth = std::max<qreal>(1.0, width - metrics.leftMargin - metrics.rightMargin);
    QTextOption option;
    option.setAlignment(metrics.alignment);
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setUseDesignMetrics(true);

    const std::vector<NoteParagraph> paragraphs = noteParagraphs(note.content);
    qreal y = 0.0;
    for (size_t p = 0; p < paragraphs.size(); ++p) {
        const NoteParagraph& paragraph = paragraphs[p];
        QString text;
        QList<QTextLayout::FormatRange> formats;
        NoteBox::Paragraph shown;
        if (p == 0) {
            // The number in front, raised
            text = number + NOTE_NUMBER_GAP;
            QTextLayout::FormatRange raised;
            raised.start = 0;
            raised.length = static_cast<int>(number.size());
            raised.format.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
            formats.append(raised);
        }
        // The text without objects (a note has none): each run where its characters went
        const auto offset = static_cast<int>(text.size());
        std::vector<int> shownAt(static_cast<size_t>(paragraph.text.size()) + 1, 0);
        int kept = 0;
        for (qsizetype i = 0; i < paragraph.text.size(); ++i) {
            shownAt[static_cast<size_t>(i)] = kept;
            if (paragraph.text.at(i) != QChar::ObjectReplacementCharacter) {
                text += paragraph.text.at(i);
                ++kept;
            }
        }
        shownAt.back() = kept;
        for (const FormatRun& run : paragraph.formats) {
            const size_t end = std::min(run.end, shownAt.size() - 1);
            const size_t start = std::min(run.start, end);
            QTextLayout::FormatRange range;
            range.start = offset + shownAt[start];
            range.length = shownAt[end] - shownAt[start];
            range.format = run.format;
            if (range.length <= 0) {
                continue;
            }
            formats.append(range);
            if (!linkAddressOf(run.format).isEmpty()) {
                shown.links.append(range);
            }
        }

        shown.layout = std::make_unique<QTextLayout>(text, font);
        QTextLayout& layout = *shown.layout;
        layout.setTextOption(option);
        layout.setFormats(formats);
        layout.setCacheEnabled(true);
        const bool empty = text.isEmpty();
        layout.beginLayout();
        for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
            line.setLineWidth(lineWidth);
            line.setPosition(QPointF(metrics.leftMargin, y + lineTopInBox(line, metrics, empty)));
            const qreal height = lineBoxHeight(line, metrics, empty);
            box.lines.push_back({p, y, height});
            y += height;
        }
        layout.endLayout();
        if (p + 1 < paragraphs.size()) {
            y += metrics.spaceAfter;
        }
        box.paragraphs.push_back(std::move(shown));
    }
    box.height = box.heightOf(0, box.lines.size());
    return box;
}

std::vector<size_t> KalahariTextDocumentLayout::notesBetween(int from, int to,
                                                             NoteKind kind) const {
    const std::vector<NotePlace>& notes = footnoteNumbers().notes();
    const auto first = std::lower_bound(
        notes.begin(), notes.end(), from,
        [](const NotePlace& place, int position) { return place.position < position; });
    std::vector<size_t> found;
    for (auto it = first; it != notes.end() && it->position < to; ++it) {
        if (it->note.kind == kind) {
            found.push_back(static_cast<size_t>(std::distance(notes.begin(), it)));
        }
    }
    return found;
}

std::vector<size_t> KalahariTextDocumentLayout::footnotesUnder(const QTextBlock& block) const {
    if (!block.isValid() || footnoteNumbers().notes().empty()) {
        return {};
    }
    // The notes of a table under its last block: its cells are side by side
    if (const QTextTable* table = tableOf(block)) {
        if (document()->findBlock(table->lastPosition()) != block) {
            return {};
        }
        return notesBetween(table->firstPosition(), table->lastPosition() + 1, NoteKind::Footnote);
    }
    return notesBetween(block.position(), block.position() + block.length(), NoteKind::Footnote);
}

qreal KalahariTextDocumentLayout::askNotesBelow(const QTextBlock& block) const {
    if (m_pageFlow.enabled || !block.isValid() || footnoteNumbers().notes().empty()) {
        return 0.0;
    }
    const auto roomOf = [this](size_t index) {
        const NoteBox& box = noteBox(index);
        const auto own = m_noteRooms.find(footnoteNumbers().notes()[index].note.id);
        return own != m_noteRooms.end() ? std::max(box.height, own->second) : box.height;
    };
    qreal room = 0.0;
    const std::vector<size_t> footnotes = footnotesUnder(block);
    if (!footnotes.empty()) {
        room += noteSeparatorRoom() + noteBottomGap();
        for (const size_t index : footnotes) {
            room += roomOf(index);
        }
    }
    if (block == document()->lastBlock()) {
        const std::vector<size_t> endnotes =
            notesBetween(0, std::numeric_limits<int>::max(), NoteKind::Endnote);
        if (!endnotes.empty()) {
            room += std::round(noteMetrics().estimatedLineHeight * ENDNOTES_GAP_LINES) +
                    noteSeparatorRoom() + noteBottomGap();
            for (const size_t index : endnotes) {
                room += roomOf(index);
            }
        }
    }
    return std::ceil(room);
}

qreal KalahariTextDocumentLayout::notesBelow(int blockNumber) const {
    const auto index = static_cast<size_t>(blockNumber);
    return blockNumber >= 0 && index < m_blockNotesBelow.size() ? m_blockNotesBelow[index] : 0.0;
}

void KalahariTextDocumentLayout::setNoteRoom(const QString& id, qreal height) {
    const auto it = m_noteRooms.find(id);
    if (height <= 0.0) {
        if (it == m_noteRooms.end()) {
            return;
        }
        m_noteRooms.erase(it);
    } else if (it != m_noteRooms.end() && it->second == height) {
        return;
    } else {
        m_noteRooms[id] = height;
    }
    // The block whose room holds it: its paragraph's, a table's last, the last for an endnote
    const std::optional<NotePlace> place = findNote(*document(), id);
    if (!place) {
        return;
    }
    QTextBlock block = place->note.kind == NoteKind::Endnote
                           ? document()->lastBlock()
                           : document()->findBlock(place->position);
    if (const QTextTable* table = tableOf(block)) {
        block = document()->findBlock(table->lastPosition());
    }
    if (block.isValid()) {
        updateSpaceBelow(block.blockNumber(), block.blockNumber());
    }
}

// =============================================================================
// Drawing
// =============================================================================

void KalahariTextDocumentLayout::drawNoteSeparator(QPainter* painter, qreal top, bool continued,
                                                   const NoteColors& colors) const {
    const qreal documentWidth = document()->textWidth();
    const qreal width = documentWidth > 0 ? documentWidth : UNWRAPPED_NOTE_WIDTH;
    const qreal length = continued ? width : std::round(width * NOTE_SEPARATOR_SHARE);
    const qreal y = top + std::round(noteSeparatorRoom() / 2.0) + 0.5;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    QPen pen(colors.line.isValid() ? colors.line : colors.text, 1.0);
    pen.setCapStyle(Qt::FlatCap);
    painter->setPen(pen);
    painter->drawLine(QPointF(0.0, y), QPointF(length, y));
    painter->restore();
}

void KalahariTextDocumentLayout::drawNoteLines(QPainter* painter, size_t note, size_t first,
                                               size_t last, qreal top,
                                               const NoteColors& colors) const {
    const NoteBox& box = noteBox(note);
    last = std::min(last, box.lines.size());
    if (first >= last) {
        return;
    }
    // Only the lines of the part: the clip holds them whole and none of their neighbours
    const qreal shift = top - box.lines[first].top;
    const QRectF clip(-UNWRAPPED_NOTE_WIDTH, top + 0.5, 3.0 * UNWRAPPED_NOTE_WIDTH,
                      std::max<qreal>(0.0, box.heightOf(first, last) - 1.0));
    painter->save();
    painter->setPen(colors.text);
    for (size_t p = box.lines[first].paragraph; p <= box.lines[last - 1].paragraph; ++p) {
        const NoteBox::Paragraph& paragraph = box.paragraphs[p];
        QList<QTextLayout::FormatRange> links = paragraph.links;
        for (QTextLayout::FormatRange& link : links) {
            link.format = QTextCharFormat();
            link.format.setForeground(colors.link.isValid() ? colors.link : colors.text);
            link.format.setFontUnderline(true);
        }
        paragraph.layout->draw(painter, QPointF(0.0, shift), links, clip);
    }
    painter->restore();
}

void KalahariTextDocumentLayout::drawNotesBelow(QPainter* painter, const QTextBlock& block,
                                                const NoteColors& colors) const {
    const qreal room = notesBelow(block.blockNumber());
    if (painter == nullptr || room <= 0.0) {
        return;
    }
    const std::vector<NotePlace>& notes = footnoteNumbers().notes();
    const auto roomOf = [this, &notes](size_t index) {
        const NoteBox& box = noteBox(index);
        const auto own = m_noteRooms.find(notes[index].note.id);
        return own != m_noteRooms.end() ? std::max(box.height, own->second) : box.height;
    };
    // Below the block's own height (a table's: its last row's), in the room under it
    qreal y = m_blockHeights[static_cast<size_t>(block.blockNumber())];
    const std::vector<size_t> footnotes = footnotesUnder(block);
    if (!footnotes.empty()) {
        drawNoteSeparator(painter, y, false, colors);
        y += noteSeparatorRoom();
        for (const size_t index : footnotes) {
            drawNoteLines(painter, index, 0, noteBox(index).lines.size(), y, colors);
            y += roomOf(index);
        }
        y += noteBottomGap();
    }
    if (block == document()->lastBlock()) {
        const std::vector<size_t> endnotes =
            notesBetween(0, std::numeric_limits<int>::max(), NoteKind::Endnote);
        if (!endnotes.empty()) {
            y += std::round(noteMetrics().estimatedLineHeight * ENDNOTES_GAP_LINES);
            drawNoteSeparator(painter, y, false, colors);
            y += noteSeparatorRoom();
            for (const size_t index : endnotes) {
                drawNoteLines(painter, index, 0, noteBox(index).lines.size(), y, colors);
                y += roomOf(index);
            }
        }
    }
}

void KalahariTextDocumentLayout::drawPageNotes(QPainter* painter, int page,
                                               const NoteColors& colors) const {
    if (painter == nullptr || !m_pageFlow.enabled || page < 0) {
        return;
    }
    updateBlockPositions();

    // The footnotes at the bottom of the text area
    const auto index = static_cast<size_t>(page);
    if (index < m_pageNotes.size() && !m_pageNotes[index].parts.empty()) {
        const PageNotes& notes = m_pageNotes[index];
        qreal y = m_pageFlow.textHeight - notes.reserved;
        drawNoteSeparator(painter, y, notes.parts.front().continued, colors);
        y += noteSeparatorRoom();
        for (const PageNote& part : notes.parts) {
            drawNoteLines(painter, part.note, part.first, part.last, y, colors);
            y += noteBox(part.note).heightOf(part.first, part.last);
        }
    }

    // The endnotes placed on it, each note's lines on the page together
    const qreal pageTop = page * m_pageFlow.pitch;
    const qreal pageBottom = pageTop + m_pageFlow.textHeight;
    if (m_endnotesTop >= pageTop - NOTE_EPSILON && m_endnotesTop < pageBottom) {
        drawNoteSeparator(painter, m_endnotesTop - pageTop, false, colors);
    }
    for (size_t i = 0; i < m_endnoteLines.size();) {
        const EndnoteLine& line = m_endnoteLines[i];
        size_t end = i + 1;
        while (end < m_endnoteLines.size() && m_endnoteLines[end].note == line.note &&
               std::floor((m_endnoteLines[end].top + NOTE_EPSILON) / m_pageFlow.pitch) ==
                   std::floor((line.top + NOTE_EPSILON) / m_pageFlow.pitch)) {
            ++end;
        }
        if (line.top >= pageTop - NOTE_EPSILON && line.top < pageBottom) {
            drawNoteLines(painter, line.note, line.line, m_endnoteLines[end - 1].line + 1,
                          line.top - pageTop, colors);
        }
        i = end;
    }
}

// =============================================================================
// Where the notes are
// =============================================================================

std::optional<KalahariTextDocumentLayout::NoteArea> KalahariTextDocumentLayout::noteAreaAt(
    const QPointF& point) const {
    const std::vector<NotePlace>& notes = footnoteNumbers().notes();
    if (notes.empty()) {
        return std::nullopt;
    }
    const qreal documentWidth = document()->textWidth();
    const qreal width = documentWidth > 0 ? documentWidth : UNWRAPPED_NOTE_WIDTH;
    if (point.x() < 0.0 || point.x() > width) {
        return std::nullopt;
    }
    const auto hit = [&point, &notes, width](size_t index, qreal top,
                                             qreal height) -> std::optional<NoteArea> {
        if (point.y() >= top && point.y() < top + height) {
            return NoteArea{notes[index].note.id, QRectF(0.0, top, width, height)};
        }
        return std::nullopt;
    };

    if (m_pageFlow.enabled) {
        updateBlockPositions();
        const qreal pitch = m_pageFlow.pitch;
        const int page = static_cast<int>(std::floor(point.y() / pitch));
        const qreal pageTop = page * pitch;
        if (page < 0 || point.y() >= pageTop + m_pageFlow.textHeight) {
            return std::nullopt;
        }
        const auto index = static_cast<size_t>(page);
        if (index < m_pageNotes.size()) {
            const PageNotes& onPage = m_pageNotes[index];
            qreal y = pageTop + m_pageFlow.textHeight - onPage.reserved + noteSeparatorRoom();
            for (const PageNote& part : onPage.parts) {
                const qreal height = noteBox(part.note).heightOf(part.first, part.last);
                if (const std::optional<NoteArea> area = hit(part.note, y, height)) {
                    return area;
                }
                y += height;
            }
        }
        for (const EndnoteLine& line : m_endnoteLines) {
            const qreal height = noteBox(line.note).advance(line.line);
            if (const std::optional<NoteArea> area = hit(line.note, line.top, height)) {
                return area;
            }
        }
        return std::nullopt;
    }

    // The continuous views: in the room under the block
    const int number = blockNumberAtY(point.y());
    const QTextBlock block = document()->findBlockByNumber(number);
    if (!block.isValid() || notesBelow(number) <= 0.0) {
        return std::nullopt;
    }
    const auto roomOf = [this, &notes](size_t index) {
        const NoteBox& box = noteBox(index);
        const auto own = m_noteRooms.find(notes[index].note.id);
        return own != m_noteRooms.end() ? std::max(box.height, own->second) : box.height;
    };
    qreal y = blockY(number) + m_blockHeights[static_cast<size_t>(number)];
    const std::vector<size_t> footnotes = footnotesUnder(block);
    if (!footnotes.empty()) {
        y += noteSeparatorRoom();
        for (const size_t index : footnotes) {
            if (const std::optional<NoteArea> area = hit(index, y, roomOf(index))) {
                return area;
            }
            y += roomOf(index);
        }
        y += noteBottomGap();
    }
    if (block == document()->lastBlock()) {
        y += std::round(noteMetrics().estimatedLineHeight * ENDNOTES_GAP_LINES) +
             noteSeparatorRoom();
        for (const size_t index :
             notesBetween(0, std::numeric_limits<int>::max(), NoteKind::Endnote)) {
            if (const std::optional<NoteArea> area = hit(index, y, roomOf(index))) {
                return area;
            }
            y += roomOf(index);
        }
    }
    return std::nullopt;
}

std::optional<QRectF> KalahariTextDocumentLayout::noteRect(const QString& id) const {
    const std::vector<NotePlace>& notes = footnoteNumbers().notes();
    const auto found = std::find_if(notes.begin(), notes.end(),
                                    [&id](const NotePlace& place) { return place.note.id == id; });
    if (found == notes.end()) {
        return std::nullopt;
    }
    const auto index = static_cast<size_t>(std::distance(notes.begin(), found));
    const qreal documentWidth = document()->textWidth();
    const qreal width = documentWidth > 0 ? documentWidth : UNWRAPPED_NOTE_WIDTH;

    if (m_pageFlow.enabled) {
        updateBlockPositions();
        for (size_t page = 0; page < m_pageNotes.size(); ++page) {
            const PageNotes& onPage = m_pageNotes[page];
            qreal y = static_cast<qreal>(page) * m_pageFlow.pitch + m_pageFlow.textHeight -
                      onPage.reserved + noteSeparatorRoom();
            for (const PageNote& part : onPage.parts) {
                const qreal height = noteBox(part.note).heightOf(part.first, part.last);
                if (part.note == index) {
                    return QRectF(0.0, y, width, height);
                }
                y += height;
            }
        }
        for (size_t i = 0; i < m_endnoteLines.size(); ++i) {
            if (m_endnoteLines[i].note != index) {
                continue;
            }
            // Its lines on the page of its first
            const qreal top = m_endnoteLines[i].top;
            const qreal page = std::floor((top + NOTE_EPSILON) / m_pageFlow.pitch);
            qreal bottom = top;
            for (size_t k = i; k < m_endnoteLines.size() && m_endnoteLines[k].note == index &&
                               std::floor((m_endnoteLines[k].top + NOTE_EPSILON) /
                                          m_pageFlow.pitch) == page;
                 ++k) {
                bottom = m_endnoteLines[k].top + noteBox(index).advance(m_endnoteLines[k].line);
            }
            return QRectF(0.0, top, width, bottom - top);
        }
        return std::nullopt;
    }

    // The continuous views: in the room under its block
    QTextBlock block = found->note.kind == NoteKind::Endnote
                           ? document()->lastBlock()
                           : document()->findBlock(found->position);
    if (const QTextTable* table = tableOf(block)) {
        block = document()->findBlock(table->lastPosition());
    }
    const int number = block.blockNumber();
    if (!block.isValid() || notesBelow(number) <= 0.0) {
        return std::nullopt;
    }
    const auto roomOf = [this, &notes](size_t note) {
        const NoteBox& box = noteBox(note);
        const auto own = m_noteRooms.find(notes[note].note.id);
        return own != m_noteRooms.end() ? std::max(box.height, own->second) : box.height;
    };
    qreal y = blockY(number) + m_blockHeights[static_cast<size_t>(number)];
    const std::vector<size_t> footnotes = footnotesUnder(block);
    if (!footnotes.empty()) {
        y += noteSeparatorRoom();
        for (const size_t note : footnotes) {
            if (note == index) {
                return QRectF(0.0, y, width, roomOf(note));
            }
            y += roomOf(note);
        }
        y += noteBottomGap();
    }
    y += std::round(noteMetrics().estimatedLineHeight * ENDNOTES_GAP_LINES) + noteSeparatorRoom();
    for (const size_t note : notesBetween(0, std::numeric_limits<int>::max(), NoteKind::Endnote)) {
        if (note == index) {
            return QRectF(0.0, y, width, roomOf(note));
        }
        y += roomOf(note);
    }
    return std::nullopt;
}

// =============================================================================
// Page flow
// =============================================================================

qreal KalahariTextDocumentLayout::flowAreaHeight(qreal page) const {
    const auto index = static_cast<size_t>(std::max<qreal>(0.0, page));
    if (page < 0.0 || index >= m_pageNotes.size()) {
        return m_pageFlow.textHeight;
    }
    return std::max<qreal>(0.0, m_pageFlow.textHeight - m_pageNotes[index].reserved);
}

qreal KalahariTextDocumentLayout::notesNeeded(const std::vector<size_t>& notes) const {
    qreal needed = 0.0;
    for (size_t i = 0; i < notes.size(); ++i) {
        const NoteBox& box = noteBox(notes[i]);
        if (box.lines.empty()) {
            continue;
        }
        needed += i + 1 < notes.size() ? box.height : box.lines.front().height;
    }
    return needed;
}

void KalahariTextDocumentLayout::reserveNotes(qreal bottom,
                                              const std::vector<size_t>& notes) const {
    const qreal pitch = m_pageFlow.pitch;
    const qreal areaHeight = m_pageFlow.textHeight;
    const int linePage = static_cast<int>(std::floor((bottom - NOTE_EPSILON) / pitch));
    const qreal used = bottom - linePage * pitch;  // the text on the line's page down to it
    int page = linePage;
    for (const size_t index : notes) {
        const NoteBox& box = noteBox(index);
        size_t line = 0;
        bool continued = false;
        while (line < box.lines.size()) {
            if (static_cast<size_t>(page) >= m_pageNotes.size()) {
                m_pageNotes.resize(static_cast<size_t>(page) + 1);
            }
            PageNotes& onPage = m_pageNotes[static_cast<size_t>(page)];
            const qreal separator = onPage.parts.empty() ? noteSeparatorRoom() : 0.0;
            const qreal text = page == linePage ? used : 0.0;
            const qreal available = areaHeight - text - onPage.reserved - separator;
            size_t end = line;
            qreal height = 0.0;
            while (end < box.lines.size() &&
                   box.heightOf(line, end + 1) <= available + NOTE_EPSILON) {
                height = box.heightOf(line, end + 1);
                ++end;
            }
            if (end == line) {
                if (!onPage.parts.empty() || text > NOTE_EPSILON) {
                    ++page;  // nothing more fits on this page: the next one
                    continue;
                }
                // Not even a line on an empty page: one, though it does not fit
                end = line + 1;
                height = box.heightOf(line, end);
            }
            onPage.parts.push_back({index, line, end, continued});
            onPage.reserved += separator + height;
            m_notesLastPage = std::max(m_notesLastPage, page);
            line = end;
            if (line < box.lines.size()) {
                continued = true;
                ++page;
            }
        }
    }
}

qreal KalahariTextDocumentLayout::placeEndnotes(qreal top) const {
    m_endnoteLines.clear();
    m_endnotesTop = -1.0;
    const std::vector<size_t> endnotes =
        notesBetween(0, std::numeric_limits<int>::max(), NoteKind::Endnote);
    if (endnotes.empty()) {
        return top;
    }
    // The line above them with their first line, each line of them as a line of the text
    const qreal separator = noteSeparatorRoom();
    qreal y = top + std::round(noteMetrics().estimatedLineHeight * ENDNOTES_GAP_LINES);
    const NoteBox& first = noteBox(endnotes.front());
    const qreal firstLine = first.lines.empty() ? 0.0 : first.lines.front().height;
    y = flowLineTop(y, separator + firstLine);
    m_endnotesTop = y;
    y += separator;
    bool firstPlaced = false;
    for (const size_t index : endnotes) {
        const NoteBox& box = noteBox(index);
        for (size_t line = 0; line < box.lines.size(); ++line) {
            const qreal height = box.lines[line].height;
            const qreal placed = firstPlaced ? flowLineTop(y, height) : y;
            firstPlaced = true;
            m_endnoteLines.push_back({index, line, placed});
            y = placed + box.advance(line);
        }
    }
    return y;
}

}  // namespace kalahari::editor
