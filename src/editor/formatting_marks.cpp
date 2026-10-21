/// @file formatting_marks.cpp
/// @brief Implementation of the formatting marks of a paragraph

#include "kalahari/editor/formatting_marks.h"

#include "kalahari/editor/kalahari_text_document_layout.h"
#include "kalahari/editor/paragraph_styles.h"

#include <QFontMetricsF>
#include <QString>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextLayout>

#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

/// A narrow no-break space: a no-break space too (°)
constexpr char16_t NARROW_NO_BREAK_SPACE = u' ';

/// The mark of a character; 0 for a character without one
char16_t markOf(char16_t character) {
    switch (character) {
        case QChar::Space: return SPACE_MARK;
        case QChar::Nbsp:
        case NARROW_NO_BREAK_SPACE: return NO_BREAK_SPACE_MARK;
        case QChar::Tabulation: return TAB_MARK;
        case QChar::LineSeparator: return LINE_BREAK_MARK;
        case QChar::SoftHyphen: return OPTIONAL_HYPHEN_MARK;
        default: return 0;
    }
}

/// The font of a character format over the document's font, every property of it set: a
/// painter takes what a font leaves unset from its widget's font
QFont shownFont(const QTextCharFormat& format, const QFont& documentFont) {
    QFont font = format.font().resolve(documentFont);
    font.setResolveMask(QFont::AllPropertiesResolved);
    return font;
}

/// The font of a fragment as its line shows it: the document's font under the fragment's
/// format, with the format of the paragraph's style the layout puts over it
QFont fragmentFont(const QTextFragment& fragment, int offset, const QTextLayout& layout,
                   const QFont& documentFont) {
    QTextCharFormat format = fragment.charFormat();
    for (const QTextLayout::FormatRange& range : layout.formats()) {
        if (range.start <= offset && offset < range.start + range.length) {
            format.merge(range.format);
            break;
        }
    }
    return shownFont(format, documentFont);
}

/// The font of an empty paragraph: that of its style
QFont emptyParagraphFont(const QTextBlock& block, const QFont& documentFont) {
    QTextCharFormat format = block.charFormat();
    if (const auto* layout = qobject_cast<const KalahariTextDocumentLayout*>(
            block.document()->documentLayout())) {
        format.merge(layout->blockMetrics(block).charFormat);
    }
    return shownFont(format, documentFont);
}

/// A font this many times smaller or larger
QFont scaledFont(const QFont& font, qreal scale) {
    QFont scaled = font;
    if (font.pointSizeF() > 0) {
        scaled.setPointSizeF(font.pointSizeF() * scale);
    } else if (font.pixelSize() > 0) {
        scaled.setPixelSize(std::max(1, static_cast<int>(std::lround(font.pixelSize() * scale))));
    }
    return scaled;
}

/// The ¶ after a scene break, as it looks; none after the other objects (as wide as the text)
void addObjectMarks(const QTextBlock& block, const QTextLayout& layout, TextObject object,
                    std::vector<FormattingMark>& marks) {
    if (object != TextObject::SceneBreak) {
        return;
    }
    const QFont font = shownFont(QTextCharFormat(), block.document()->defaultFont());
    const auto* documentLayout =
        qobject_cast<const KalahariTextDocumentLayout*>(block.document()->documentLayout());
    const SceneBreakLook look =
        documentLayout != nullptr ? documentLayout->sceneBreakLook() : SceneBreakLook::Asterisks;

    // The layout draws the look in the middle of the object's box (drawInlineObject())
    const QTextLine line = layout.lineAt(0);
    const qreal left = line.cursorToX(0);
    const qreal width = line.cursorToX(1) - left;
    qreal shown = 0.0;
    if (const QString text = sceneBreakText(look); !text.isEmpty()) {
        shown = QFontMetricsF(font).horizontalAdvance(text);
    } else if (look == SceneBreakLook::Line) {
        shown = std::round(width * SCENE_BREAK_LINE_SHARE);
    }
    marks.push_back({QChar(PARAGRAPH_MARK),
                     QPointF(left + (width + shown) / 2.0, line.y() + line.ascent()), font});
}

}  // namespace

std::vector<FormattingMark> formattingMarksOf(const QTextBlock& block) {
    std::vector<FormattingMark> marks;
    const QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);
    if (layout == nullptr || layout->lineCount() == 0) {
        return marks;
    }
    if (const TextObject object = blockObject(block); object != TextObject::None) {
        addObjectMarks(block, *layout, object, marks);
        return marks;
    }

    const QFont documentFont = block.document()->defaultFont();
    const auto baselineOf = [](const QTextLine& line) { return line.y() + line.ascent(); };
    QFont lastFont = emptyParagraphFont(block, documentFont);

    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        const int start = fragment.position() - block.position();
        const QFont font = fragmentFont(fragment, start, *layout, documentFont);
        const QFontMetricsF metrics(font);
        lastFont = font;

        const QString text = fragment.text();
        for (int index = 0; index < text.size(); ++index) {
            const char16_t character = text.at(index).unicode();
            const char16_t glyph = markOf(character);
            if (glyph == 0) {
                continue;
            }
            const int position = start + index;
            const QTextLine line = layout->lineForTextPosition(position);
            if (!line.isValid()) {
                continue;
            }
            const qreal x1 = line.cursorToX(position);
            const qreal baseline = baselineOf(line);
            if (character == QChar::LineSeparator) {
                marks.push_back({QChar(glyph), QPointF(x1, baseline), font});
            } else if (character == QChar::SoftHyphen) {
                // At the end of a line the hyphen is there: the mark after it. Inside a line
                // it has no room: small, above the letters' middle
                const bool endsLine = position == line.textStart() + line.textLength() - 1 &&
                                      line.lineNumber() + 1 < layout->lineCount();
                if (endsLine) {
                    marks.push_back(
                        {QChar(glyph),
                         QPointF(x1 + metrics.horizontalAdvance(QLatin1Char('-')), baseline), font});
                } else {
                    const QFont small = scaledFont(font, INLINE_HYPHEN_MARK_SCALE);
                    const qreal glyphWidth = QFontMetricsF(small).horizontalAdvance(QChar(glyph));
                    marks.push_back({QChar(glyph),
                                     QPointF(x1 - glyphWidth / 2.0, baseline - metrics.xHeight()),
                                     small});
                }
            } else {
                // In the middle of the space; a tab narrower than its arrow from its start,
                // after the word before it
                const qreal x2 = line.cursorToX(position + 1);
                const qreal glyphWidth = metrics.horizontalAdvance(QChar(glyph));
                qreal x = (x1 + x2 - glyphWidth) / 2.0;
                if (character == QChar::Tabulation) {
                    x = std::max(x, std::min(x1, x2));
                }
                marks.push_back({QChar(glyph), QPointF(x, baseline), font});
            }
        }
    }

    // ¶ after the last character, in its font (an empty paragraph: in its style's font)
    const QTextLine last = layout->lineAt(layout->lineCount() - 1);
    marks.push_back({QChar(PARAGRAPH_MARK),
                     QPointF(last.cursorToX(block.length() - 1), baselineOf(last)), lastFont});
    return marks;
}

}  // namespace kalahari::editor
