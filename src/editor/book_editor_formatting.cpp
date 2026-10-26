/// @file book_editor_formatting.cpp
/// @brief BookEditor: character formatting, paragraph alignment and fonts

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/core/logger.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/kml_format_registry.h>
#include <QTextBoundaryFinder>
#include <algorithm>
#include <array>
#include <vector>

namespace kalahari::editor {

namespace {

/// @brief The character properties Clear Formatting takes off text: the inline formats, the
/// font, the colors, the capitals and the letter spacing (KML <b>, <i>, <u>, <s>, <sub>, <sup>
/// and the attributes of <span>); Copy Formatting remembers them
constexpr std::array<int, 14> CLEARED_CHARACTER_PROPERTIES{
    QTextFormat::FontWeight,        QTextFormat::FontItalic,
    QTextFormat::FontUnderline,     QTextFormat::TextUnderlineStyle,
    QTextFormat::FontStrikeOut,     QTextFormat::TextVerticalAlignment,
    QTextFormat::FontFamilies,      QTextFormat::FontFamily,
    QTextFormat::FontPointSize,     QTextFormat::FontPixelSize,
    QTextFormat::ForegroundBrush,   QTextFormat::BackgroundBrush,
    QTextFormat::FontCapitalization, KmlPropLetterSpacing,
};

/// @brief @p format without the formatting Clear Formatting takes off; the annotations and
/// the references of the text stay
QTextCharFormat withoutClearedFormatting(QTextCharFormat format)
{
    for (const int property : CLEARED_CHARACTER_PROPERTIES) {
        format.clearProperty(property);
    }
    return format;
}

/// @brief Only the formatting of @p format Clear Formatting takes off
QTextCharFormat onlyClearedFormatting(const QTextCharFormat& format)
{
    QTextCharFormat only;
    for (const int property : CLEARED_CHARACTER_PROPERTIES) {
        if (format.hasProperty(property)) {
            only.setProperty(property, format.property(property));
        }
    }
    return only;
}

/// @brief What Copy Formatting remembers, for all the editors
struct CopiedFormatting {
    QTextCharFormat character;  ///< Only the formatting Clear Formatting takes off
    QString style;              ///< Empty when copied at an object
    ParagraphFormat paragraph;
    Qt::Alignment alignment;    ///< Its own; none: of the style
};

std::optional<CopiedFormatting>& copiedFormatting()
{
    static std::optional<CopiedFormatting> copied;
    return copied;
}

/// @brief Call @p visit with the format and the place in the document of each fragment of
/// text of a range (objects aside), cut to the range
template <typename Visit>
void forEachTextFragment(const QTextDocument& document, const SelectionRange& range, Visit visit)
{
    for (int i = range.start.paragraph; i <= range.end.paragraph; ++i) {
        const QTextBlock block = document.findBlockByNumber(i);
        if (!block.isValid()) {
            continue;
        }
        const int start = i == range.start.paragraph ? range.start.offset : 0;
        const int end = i == range.end.paragraph ? range.end.offset : block.length() - 1;
        const int from = block.position() + start;
        const int to = block.position() + end;
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const int fragmentFrom = fragment.position();
            const int fragmentTo = fragmentFrom + fragment.length();
            const QTextCharFormat format = fragment.charFormat();
            if (fragmentTo <= from || fragmentFrom >= to ||
                textObjectOf(format) != TextObject::None) {
                continue;
            }
            visit(format, std::max(fragmentFrom, from), std::min(fragmentTo, to));
        }
    }
}

/// @brief Keep in @p shown the parts @p other has the same
void keepCommonParts(CharacterFormat& shown, const CharacterFormat& other)
{
    const auto common = [](auto& part, const auto& otherPart) {
        if (part != otherPart) {
            part.reset();
        }
    };
    common(shown.family, other.family);
    common(shown.size, other.size);
    common(shown.bold, other.bold);
    common(shown.italic, other.italic);
    common(shown.underline, other.underline);
    common(shown.strikethrough, other.strikethrough);
    common(shown.position, other.position);
    common(shown.capitals, other.capitals);
    common(shown.letterSpacing, other.letterSpacing);
    common(shown.color, other.color);
    common(shown.highlight, other.highlight);
}

}  // namespace

// =============================================================================
// Formatting (Phase 7.2)
// =============================================================================

void BookEditor::toggleBold()
{
    toggleFormat(InlineFormat::Bold);
}

void BookEditor::toggleItalic()
{
    toggleFormat(InlineFormat::Italic);
}

void BookEditor::toggleUnderline()
{
    toggleFormat(InlineFormat::Underline);
}

void BookEditor::toggleStrikethrough()
{
    toggleFormat(InlineFormat::Strikethrough);
}

bool BookEditor::isBold() const
{
    return hasFormat(InlineFormat::Bold);
}

bool BookEditor::isItalic() const
{
    return hasFormat(InlineFormat::Italic);
}

bool BookEditor::isUnderline() const
{
    return hasFormat(InlineFormat::Underline);
}

bool BookEditor::isStrikethrough() const
{
    return hasFormat(InlineFormat::Strikethrough);
}

void BookEditor::toggleSuperscript()
{
    toggleFormat(InlineFormat::Superscript);
}

void BookEditor::toggleSubscript()
{
    toggleFormat(InlineFormat::Subscript);
}

void BookEditor::toggleSmallCaps()
{
    toggleFormat(InlineFormat::SmallCaps);
}

void BookEditor::toggleAllCaps()
{
    toggleFormat(InlineFormat::AllCaps);
}

bool BookEditor::isSuperscript() const
{
    return hasFormat(InlineFormat::Superscript);
}

bool BookEditor::isSubscript() const
{
    return hasFormat(InlineFormat::Subscript);
}

bool BookEditor::isSmallCaps() const
{
    return hasFormat(InlineFormat::SmallCaps);
}

bool BookEditor::isAllCaps() const
{
    return hasFormat(InlineFormat::AllCaps);
}

void BookEditor::clearFormatting()
{
    if (refusesChange()) {
        return;
    }
    if (!m_textBuffer) {
        return;
    }

    // The paragraphs of the selection, or the one at the cursor, lose their own alignments
    // and formatting (indents, spacing, line spacing, breaks); their styles and lists stay
    std::vector<std::pair<QTextBlock, QTextBlockFormat>> blocks;
    for (const QTextBlock& block : selectedTextBlocks()) {
        QTextBlockFormat format = block.blockFormat();
        format.clearProperty(QTextFormat::BlockAlignment);
        clearParagraphFormat(format);
        blocks.emplace_back(block, format);
    }

    // The selection or the word loses the formatting given by hand, objects aside: one undo
    // step with the paragraphs
    const std::optional<SelectionRange> range = formatRange();
    std::vector<FormatPiece> pieces;
    if (range) {
        pieces = changedFormatPieces(
            *range, [](QTextCharFormat& format) { format = withoutClearedFormatting(format); });
    }
    setFormats(pieces, blocks);

    // Without a selection or a word: the text typed next is without the inline formats, the
    // capitals and the letter spacing, whatever the text before it has (the pending format
    // adds to the place's, so what the place has is turned off). Its font and colors stay
    // those of the place: given here, the editor's own font would be saved with the text as
    // a font of its own.
    if (!range) {
        const QTextCharFormat place = placeFormat();
        QTextCharFormat pending;
        if (formatHas(place, InlineFormat::Bold)) {
            pending.setFontWeight(QFont::Normal);
        }
        if (formatHas(place, InlineFormat::Italic)) {
            pending.setFontItalic(false);
        }
        if (formatHas(place, InlineFormat::Underline)) {
            pending.setFontUnderline(false);
        }
        if (formatHas(place, InlineFormat::Strikethrough)) {
            pending.setFontStrikeOut(false);
        }
        if (place.verticalAlignment() != QTextCharFormat::AlignNormal) {
            pending.setVerticalAlignment(QTextCharFormat::AlignNormal);
        }
        if (place.fontCapitalization() != QFont::MixedCase) {
            pending.setFontCapitalization(QFont::MixedCase);
        }
        if (place.doubleProperty(KmlPropLetterSpacing) != 0.0) {
            pending.setProperty(KmlPropLetterSpacing, 0.0);
        }
        setPendingFormat(pending);
    }
}

// =============================================================================
// Paragraph Alignment
// =============================================================================

void BookEditor::setAlignLeft()
{
    setParagraphAlignment(Qt::AlignLeft);
}

void BookEditor::setAlignCenter()
{
    setParagraphAlignment(Qt::AlignHCenter);
}

void BookEditor::setAlignRight()
{
    setParagraphAlignment(Qt::AlignRight);
}

void BookEditor::setAlignJustify()
{
    setParagraphAlignment(Qt::AlignJustify);
}

void BookEditor::setParagraphAlignment(Qt::Alignment alignment)
{
    if (refusesChange()) {
        return;
    }
    if (!m_textBuffer) {
        return;
    }

    // Phase 11: Use QTextBlockFormat for paragraph alignment
    int startPara = m_cursorPosition.paragraph;
    int endPara = m_cursorPosition.paragraph;

    if (hasSelection()) {
        SelectionRange normRange = m_selection.normalized();
        startPara = normRange.start.paragraph;
        endPara = normRange.end.paragraph;
    }

    // All the paragraphs in one undo step. Undoing or redoing it brings back the cursor and
    // selection it was made with: QTextDocument would put the cursor after the last
    // paragraph changed, so the next paragraph's alignment would show.
    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    for (int i = startPara; i <= endPara; ++i) {
        QTextBlock block = m_textBuffer->findBlockByNumber(i);
        if (block.isValid()) {
            cursor.setPosition(block.position());
            QTextBlockFormat format = block.blockFormat();
            format.setAlignment(alignment);
            cursor.setBlockFormat(format);
        }
    }
    cursor.endEditBlock();

    // Phase 12.3: Mark pipeline dirty for relayout
    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }

    emit contentChanged();
    update();
}

void BookEditor::restoreStepCursor()
{
    // A step that changed no text has one cursor and selection for both sides of it; a
    // replacement recorded its own for each side
    if (m_stepCursor) {
        m_cursorPosition = validateCursorPosition(m_stepCursor->cursor);
        setSelection(m_stepCursor->selection);
        m_stepCursor.reset();
    }
}

Qt::Alignment BookEditor::currentAlignment() const
{
    if (!m_textBuffer) {
        return DEFAULT_PARAGRAPH_ALIGNMENT;
    }

    QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    if (!block.isValid()) {
        return DEFAULT_PARAGRAPH_ALIGNMENT;
    }
    // A heading is centred by its style
    if (const auto* layout =
            qobject_cast<const KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())) {
        return layout->shownAlignment(block);
    }
    return effectiveAlignment(ownAlignment(block.blockFormat()));
}

// =============================================================================
// Paragraph styles and the objects of the text
// =============================================================================

void BookEditor::setParagraphStyles(const ParagraphStyleSet& styles)
{
    if (styles == m_paragraphStyles) {
        return;
    }
    m_paragraphStyles = styles;
    if (!m_textBuffer) {
        return;
    }
    std::vector<QTextBlock> blocks;  // a style may now be checked or not
    for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
        blocks.push_back(block);
    }
    checkParagraphsAgain(blocks);
    if (auto* layout = qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())) {
        layout->setParagraphStyles(styles);
    }
    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    update();
}

QString BookEditor::currentParagraphStyle() const
{
    if (!m_textBuffer) {
        return QString::fromLatin1(NORMAL_STYLE_ID);
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    if (!block.isValid()) {
        return QString::fromLatin1(NORMAL_STYLE_ID);
    }
    if (blockObject(block) != TextObject::None) {
        return QString();
    }
    return paragraphStyleOf(block.blockFormat());
}

QString BookEditor::selectedParagraphStyle() const
{
    if (!hasSelection()) {
        return currentParagraphStyle();
    }
    if (!m_textBuffer) {
        return QString::fromLatin1(NORMAL_STYLE_ID);
    }
    const SelectionRange range = m_selection.normalized();
    QString common;
    for (QTextBlock block = m_textBuffer->findBlockByNumber(range.start.paragraph);
         block.isValid() && block.blockNumber() <= range.end.paragraph; block = block.next()) {
        if (blockObject(block) != TextObject::None) {
            continue;
        }
        const QString style = paragraphStyleOf(block.blockFormat());
        if (!common.isEmpty() && style != common) {
            return QString();
        }
        common = style;
    }
    return common;
}

TextObject BookEditor::currentObject() const
{
    if (!m_textBuffer) {
        return TextObject::None;
    }
    return blockObject(m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph));
}

void BookEditor::applyParagraphStyle(const QString& styleId)
{
    if (refusesChange()) {
        return;
    }
    if (!m_textBuffer) {
        return;
    }

    int startPara = m_cursorPosition.paragraph;
    int endPara = m_cursorPosition.paragraph;
    if (hasSelection()) {
        const SelectionRange range = m_selection.normalized();
        startPara = range.start.paragraph;
        endPara = range.end.paragraph;
    }

    // The paragraphs of text not in the style yet; nothing to change, no undo step
    const QString id = styleId.isEmpty() ? QString::fromLatin1(NORMAL_STYLE_ID) : styleId;
    std::vector<QTextBlock> changed;
    for (QTextBlock block = m_textBuffer->findBlockByNumber(startPara);
         block.isValid() && block.blockNumber() <= endPara; block = block.next()) {
        if (blockObject(block) == TextObject::None && paragraphStyleOf(block.blockFormat()) != id) {
            changed.push_back(block);
        }
    }
    if (changed.empty()) {
        return;
    }

    // One undo step that brings back the cursor and selection it was made with, as the
    // alignment does (setParagraphAlignment())
    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    for (const QTextBlock& block : changed) {
        QTextBlockFormat format = block.blockFormat();
        setParagraphStyle(format, id);
        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }
    cursor.endEditBlock();
    checkParagraphsAgain(changed);  // Code is not checked, the other styles are

    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    ensureCursorVisible();
    emit contentChanged();
    emit paragraphModified(m_cursorPosition.paragraph);
    update();
}

void BookEditor::insertTextObject(TextObject object)
{
    if (refusesChange()) {
        return;
    }
    if (object == TextObject::None) {
        return;
    }
    ensureDocument();

    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    if (hasSelection()) {
        const SelectionRange selection = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), selection.start, selection.end);
        clearSelection();
    }

    // The replaced selection and the object: one undo step. At the end of a paragraph the
    // one after the object is in the style Enter would start there.
    cursor.beginEditBlock();
    removeKeepingAnnotations(cursor);
    QTextBlockFormat nextFormat = paragraphBlockFormat();
    setParagraphStyle(nextFormat,
                      m_paragraphStyles.nextStyleOf(paragraphStyleOf(cursor.blockFormat())));
    insertObject(cursor, object, nextFormat);
    cursor.endEditBlock();
    finishEdit(cursor);
}

void BookEditor::toggleFormat(InlineFormat formatType)
{
    if (refusesChange()) {
        return;
    }
    const auto formatName = [formatType] {
        switch (formatType) {
            case InlineFormat::Bold:
                return "bold";
            case InlineFormat::Italic:
                return "italic";
            case InlineFormat::Underline:
                return "underline";
            case InlineFormat::Strikethrough:
                return "strikethrough";
            case InlineFormat::Superscript:
                return "superscript";
            case InlineFormat::Subscript:
                return "subscript";
            case InlineFormat::SmallCaps:
                return "small caps";
            case InlineFormat::AllCaps:
                return "all caps";
        }
        return "";
    };
    core::Logger::getInstance().debug("BookEditor::toggleFormat() called - "
        "type={}, hasSelection={}, cursor=({}, {})",
        formatName(), hasSelection(), m_cursorPosition.paragraph, m_cursorPosition.offset);

    if (!m_textBuffer) {
        return;
    }

    // A format with the inline format on (true), off (false), or without it (nullopt): as
    // the text's place gives it
    const auto withFormat = [formatType](QTextCharFormat format, std::optional<bool> on) {
        switch (formatType) {
            case InlineFormat::Bold:
                format.clearProperty(QTextFormat::FontWeight);
                if (on) {
                    format.setFontWeight(*on ? QFont::Bold : QFont::Normal);
                }
                break;
            case InlineFormat::Italic:
                format.clearProperty(QTextFormat::FontItalic);
                if (on) {
                    format.setFontItalic(*on);
                }
                break;
            case InlineFormat::Underline:
                format.clearProperty(QTextFormat::FontUnderline);
                format.clearProperty(QTextFormat::TextUnderlineStyle);
                if (on) {
                    format.setFontUnderline(*on);
                }
                break;
            case InlineFormat::Strikethrough:
                format.clearProperty(QTextFormat::FontStrikeOut);
                if (on) {
                    format.setFontStrikeOut(*on);
                }
                break;
            case InlineFormat::Superscript:
            case InlineFormat::Subscript:
                format.clearProperty(QTextFormat::TextVerticalAlignment);
                if (on) {
                    format.setVerticalAlignment(!*on ? QTextCharFormat::AlignNormal
                                                : formatType == InlineFormat::Superscript
                                                    ? QTextCharFormat::AlignSuperScript
                                                    : QTextCharFormat::AlignSubScript);
                }
                break;
            case InlineFormat::SmallCaps:
            case InlineFormat::AllCaps:
                format.clearProperty(QTextFormat::FontCapitalization);
                if (on) {
                    format.setFontCapitalization(!*on ? QFont::MixedCase
                                                 : formatType == InlineFormat::SmallCaps
                                                     ? QFont::SmallCaps
                                                     : QFont::AllUppercase);
                }
                break;
        }
        return format;
    };

    // Text: the selection, or the word with the cursor inside it. The format goes when all
    // of it has it, else all of it gets it.
    if (const std::optional<SelectionRange> range = formatRange()) {
        const bool on = !rangeHasFormat(*range, formatType);
        applyCharFormat(*range, withFormat(QTextCharFormat(), on));
        core::Logger::getInstance().debug("BookEditor::toggleFormat() - formatting {} {}",
            on ? "applied" : "removed", formatName());
        return;
    }

    // Elsewhere, the text typed next: it gets the format when it would be without it, and
    // the other way round. When the place gives it so, the format given goes: given twice,
    // the typed text is as without it.
    const bool on = !formatHas(typingFormat(), formatType);
    setPendingFormat(pendingFor(withFormat(typingFormat(), on)));
    core::Logger::getInstance().debug("BookEditor::toggleFormat() - text typed next: {} {}",
        formatName(), on ? "on" : "off");
}

bool BookEditor::hasFormat(InlineFormat formatType) const
{
    if (!m_textBuffer) {
        return false;
    }
    if (hasSelection()) {
        return rangeHasFormat(m_selection.normalized(), formatType);
    }
    return formatHas(typingFormat(), formatType);
}

bool BookEditor::formatHas(const QTextCharFormat& format, InlineFormat formatType)
{
    switch (formatType) {
        case InlineFormat::Bold:
            return format.fontWeight() >= QFont::Bold;
        case InlineFormat::Italic:
            return format.fontItalic();
        case InlineFormat::Underline:
            return format.fontUnderline();
        case InlineFormat::Strikethrough:
            return format.fontStrikeOut();
        case InlineFormat::Superscript:
            return format.verticalAlignment() == QTextCharFormat::AlignSuperScript;
        case InlineFormat::Subscript:
            return format.verticalAlignment() == QTextCharFormat::AlignSubScript;
        case InlineFormat::SmallCaps:
            return format.fontCapitalization() == QFont::SmallCaps;
        case InlineFormat::AllCaps:
            return format.fontCapitalization() == QFont::AllUppercase;
    }
    return false;
}

bool BookEditor::rangeHasFormat(const SelectionRange& range, InlineFormat formatType) const
{
    // Iterate the document's text FRAGMENTS (runs of uniform formatting) instead of
    // constructing one QTextCursor per character — the old per-character loop
    // was O(N) cursor allocations and made "select all + bold" take ~10 s on a
    // large chapter. Fragment iteration is O(runs), effectively instant.
    for (int i = range.start.paragraph; i <= range.end.paragraph; ++i) {
        QTextBlock block = m_textBuffer->findBlockByNumber(i);
        if (!block.isValid()) continue;

        int start = (i == range.start.paragraph) ? range.start.offset : 0;
        int end = (i == range.end.paragraph) ? range.end.offset : block.length() - 1;
        if (end < 0) end = 0;
        if (start >= end) continue;  // nothing selected in this block

        // Document-absolute bounds of the selected range within this block.
        const int selFrom = block.position() + start;
        const int selTo = block.position() + end;

        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;

            const int fragFrom = frag.position();
            const int fragTo = fragFrom + frag.length();
            // Skip fragments outside the selected range.
            if (fragTo <= selFrom || fragFrom >= selTo) continue;

            if (!formatHas(frag.charFormat(), formatType)) {
                return false;
            }
        }
    }
    return true;
}

std::optional<SelectionRange> BookEditor::formatRange() const
{
    if (hasSelection()) {
        return m_selection.normalized();
    }

    // The word the cursor is inside of, as in Word and LibreOffice: at its start or end the
    // format is for the text typed next
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    const QString text = block.isValid() ? block.text() : QString();
    const int offset = m_cursorPosition.offset;
    if (offset <= 0 || offset >= text.size()) {
        return std::nullopt;
    }
    QTextBoundaryFinder finder(QTextBoundaryFinder::Word, text);
    finder.setPosition(offset);
    if (finder.isAtBoundary()) {
        return std::nullopt;
    }
    const qsizetype start = finder.toPreviousBoundary();
    finder.setPosition(offset);
    const qsizetype end = finder.toNextBoundary();

    // Letters or digits, with what joins them (an apostrophe); not spaces kept together
    const QStringView word = QStringView(text).mid(start, end - start);
    if (start < 0 || end < 0 ||
        std::none_of(word.begin(), word.end(), [](QChar ch) { return ch.isLetterOrNumber(); })) {
        return std::nullopt;
    }
    const int paragraph = m_cursorPosition.paragraph;
    return SelectionRange{{paragraph, static_cast<int>(start)}, {paragraph, static_cast<int>(end)}};
}

void BookEditor::applyCharFormat(const SelectionRange& range, const QTextCharFormat& format)
{
    // One undo step. Undoing or redoing it brings back the cursor and selection it was made
    // with: QTextDocument would put the cursor at the end of the text it changed, with no
    // selection.
    QTextCursor cursor = createCursor(m_textBuffer.get(), range.start, range.end);
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    cursor.mergeCharFormat(format);
    cursor.endEditBlock();

    emit contentChanged();
    update();
}

QTextCharFormat BookEditor::pendingFormat() const
{
    // The cursor may have left without the pipeline having been told yet
    if (hasSelection() || m_cursorPosition != m_pendingFormatAt) {
        return {};
    }
    return m_pendingFormat;
}

void BookEditor::setPendingFormat(const QTextCharFormat& format)
{
    m_pendingFormat = format;
    m_pendingFormatAt = m_cursorPosition;
}

QTextCharFormat BookEditor::placeFormat() const
{
    if (!m_textBuffer) {
        return {};
    }
    return insertionFormat(createCursor(m_textBuffer.get(), m_cursorPosition));
}

QTextCharFormat BookEditor::typingFormat() const
{
    QTextCharFormat format = placeFormat();
    format.merge(pendingFormat());
    return format;
}

QString BookEditor::fontFamilyOf(const QTextCharFormat& format) const
{
    const QStringList families = format.fontFamilies().toStringList();
    return families.isEmpty() ? m_appearance.typography.textFont.family() : families.first();
}

int BookEditor::fontSizeOf(const QTextCharFormat& format) const
{
    const int size = static_cast<int>(format.fontPointSize());
    return size > 0 ? size : m_appearance.typography.textFont.pointSize();
}

qreal BookEditor::fontPointSizeOf(const QTextCharFormat& format) const
{
    const qreal size = format.fontPointSize();
    return size > 0 ? size : m_appearance.typography.textFont.pointSizeF();
}

CharacterFormat BookEditor::characterFormatOf(const QTextCharFormat& format) const
{
    CharacterFormat shown;
    shown.family = fontFamilyOf(format);
    shown.size = fontPointSizeOf(format);
    shown.bold = formatHas(format, InlineFormat::Bold);
    shown.italic = formatHas(format, InlineFormat::Italic);
    shown.underline = formatHas(format, InlineFormat::Underline);
    shown.strikethrough = formatHas(format, InlineFormat::Strikethrough);
    shown.position = formatHas(format, InlineFormat::Superscript) ? QTextCharFormat::AlignSuperScript
                     : formatHas(format, InlineFormat::Subscript) ? QTextCharFormat::AlignSubScript
                                                                   : QTextCharFormat::AlignNormal;
    shown.capitals = formatHas(format, InlineFormat::SmallCaps) ? QFont::SmallCaps
                     : formatHas(format, InlineFormat::AllCaps) ? QFont::AllUppercase
                                                                 : QFont::MixedCase;
    shown.letterSpacing = format.doubleProperty(KmlPropLetterSpacing);
    shown.color = textColorOf(format);
    shown.highlight = highlightColorOf(format);
    return shown;
}

std::vector<BookEditor::FormatPiece> BookEditor::changedFormatPieces(
    const SelectionRange& range, const std::function<void(QTextCharFormat&)>& change) const
{
    std::vector<FormatPiece> pieces;
    forEachTextFragment(*m_textBuffer, range,
                        [&pieces, &change](const QTextCharFormat& format, int from, int to) {
                            QTextCharFormat changed = format;
                            change(changed);
                            if (changed != format) {
                                pieces.push_back({from, to, changed});
                            }
                        });

    // An empty paragraph of a selection: the text typed there
    if (range.start.paragraph != range.end.paragraph) {
        for (QTextBlock block = m_textBuffer->findBlockByNumber(range.start.paragraph);
             block.isValid() && block.blockNumber() <= range.end.paragraph; block = block.next()) {
            if (block.length() == 1 && blockObject(block) == TextObject::None) {
                QTextCharFormat changed = block.charFormat();
                change(changed);
                if (changed != block.charFormat()) {
                    pieces.push_back({block.position(), block.position(), changed, true});
                }
            }
        }
    }
    return pieces;
}

void BookEditor::setFormats(const std::vector<FormatPiece>& pieces,
                            const std::vector<std::pair<QTextBlock, QTextBlockFormat>>& blocks)
{
    std::vector<std::pair<QTextBlock, QTextBlockFormat>> changed;
    std::copy_if(blocks.begin(), blocks.end(), std::back_inserter(changed),
                 [](const auto& entry) { return entry.first.blockFormat() != entry.second; });
    if (pieces.empty() && changed.empty()) {
        return;
    }

    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    for (const FormatPiece& piece : pieces) {
        cursor.setPosition(piece.from);
        if (piece.emptyParagraph) {
            cursor.setBlockCharFormat(piece.format);
            continue;
        }
        cursor.setPosition(piece.to, QTextCursor::KeepAnchor);
        cursor.setCharFormat(piece.format);
    }
    for (const auto& [block, format] : changed) {
        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }
    cursor.endEditBlock();

    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    emit contentChanged();
    update();
}

void BookEditor::changeCharFormat(const std::function<void(QTextCharFormat&)>& change)
{
    if (refusesChange() || !m_textBuffer) {
        return;
    }
    if (const std::optional<SelectionRange> range = formatRange()) {
        setFormats(changedFormatPieces(*range, change));
        return;
    }
    QTextCharFormat wanted = typingFormat();
    change(wanted);
    setPendingFormat(pendingFor(wanted));
}

QTextCharFormat BookEditor::pendingFor(const QTextCharFormat& wanted) const
{
    const QTextCharFormat place = placeFormat();
    QTextCharFormat pending;
    if (fontFamilyOf(wanted) != fontFamilyOf(place)) {
        pending.setFontFamilies({fontFamilyOf(wanted)});
    }
    if (fontPointSizeOf(wanted) != fontPointSizeOf(place)) {
        pending.setFontPointSize(fontPointSizeOf(wanted));
    }
    if (formatHas(wanted, InlineFormat::Bold) != formatHas(place, InlineFormat::Bold)) {
        pending.setFontWeight(formatHas(wanted, InlineFormat::Bold) ? QFont::Bold : QFont::Normal);
    }
    if (wanted.fontItalic() != place.fontItalic()) {
        pending.setFontItalic(wanted.fontItalic());
    }
    if (wanted.fontUnderline() != place.fontUnderline()) {
        pending.setFontUnderline(wanted.fontUnderline());
    }
    if (wanted.fontStrikeOut() != place.fontStrikeOut()) {
        pending.setFontStrikeOut(wanted.fontStrikeOut());
    }
    if (wanted.verticalAlignment() != place.verticalAlignment()) {
        pending.setVerticalAlignment(wanted.verticalAlignment());
    }
    if (wanted.fontCapitalization() != place.fontCapitalization()) {
        pending.setFontCapitalization(wanted.fontCapitalization());
    }
    if (wanted.doubleProperty(KmlPropLetterSpacing) != place.doubleProperty(KmlPropLetterSpacing)) {
        pending.setProperty(KmlPropLetterSpacing, wanted.doubleProperty(KmlPropLetterSpacing));
    }
    // No brush: the automatic color, no highlight (the pending format adds to the place's)
    if (const QColor color = textColorOf(wanted); color != textColorOf(place)) {
        pending.setForeground(color.isValid() ? QBrush(color) : QBrush());
    }
    if (const QColor color = highlightColorOf(wanted); color != highlightColorOf(place)) {
        pending.setBackground(color.isValid() ? QBrush(color) : QBrush());
    }
    return pending;
}

// =============================================================================
// More of the character formatting
// =============================================================================

CharacterFormat BookEditor::shownCharacterFormat() const
{
    if (!m_textBuffer || !hasSelection()) {
        return characterFormatOf(typingFormat());
    }
    std::optional<CharacterFormat> shown;
    forEachTextFragment(*m_textBuffer, m_selection.normalized(),
                        [this, &shown](const QTextCharFormat& format, int, int) {
                            const CharacterFormat part = characterFormatOf(format);
                            if (!shown) {
                                shown = part;
                            } else {
                                keepCommonParts(*shown, part);
                            }
                        });
    // A selection of objects or empty paragraphs only: as the text typed there
    return shown ? *shown : characterFormatOf(typingFormat());
}

void BookEditor::applyCharacterFormat(const CharacterFormat& changes)
{
    changeCharFormat([&changes](QTextCharFormat& format) { mergeCharacterFormat(format, changes); });
}

void BookEditor::changeCase(CaseChange change)
{
    if (refusesChange() || !m_textBuffer) {
        return;
    }
    const std::optional<SelectionRange> range = formatRange();
    if (!range) {
        return;
    }

    // The letters change fragment by fragment, so each keeps its format and annotations; the
    // text keeps its length (changedCase()), so the selection stays on it
    struct Replacement {
        int from;
        int to;
        QString text;
        QTextCharFormat format;
    };
    std::vector<Replacement> replacements;
    std::vector<int> paragraphs;
    for (int i = range->start.paragraph; i <= range->end.paragraph; ++i) {
        const QTextBlock block = m_textBuffer->findBlockByNumber(i);
        if (!block.isValid() || blockObject(block) != TextObject::None) {
            continue;
        }
        const int start = i == range->start.paragraph ? range->start.offset : 0;
        const int end = i == range->end.paragraph ? range->end.offset : block.length() - 1;
        const QString text = block.text();
        const QString changed = changedCase(text, start, end, change);
        if (changed == text) {
            continue;
        }
        paragraphs.push_back(i);
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const int from = std::max(fragment.position() - block.position(), start);
            const int to = std::min(fragment.position() - block.position() + fragment.length(), end);
            if (from >= to || textObjectOf(fragment.charFormat()) != TextObject::None) {
                continue;
            }
            const QString part = changed.mid(from, to - from);
            if (part != QStringView(text).mid(from, to - from)) {
                replacements.push_back(
                    {block.position() + from, block.position() + to, part, fragment.charFormat()});
            }
        }
    }
    if (replacements.empty()) {
        return;
    }

    // One undo step that brings back the cursor and selection it was made with
    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    for (const Replacement& replacement : replacements) {
        cursor.setPosition(replacement.from);
        cursor.setPosition(replacement.to, QTextCursor::KeepAnchor);
        cursor.insertText(replacement.text, replacement.format);
    }
    cursor.endEditBlock();
    m_learnableChange.reset();

    update();
    emit contentChanged();
    for (const int paragraph : paragraphs) {
        emit paragraphModified(paragraph);
    }
}

void BookEditor::copyFormatting()
{
    if (!m_textBuffer) {
        return;
    }
    // The first character of the selection, or the text typed at the cursor
    QTextCharFormat format = typingFormat();
    int paragraph = m_cursorPosition.paragraph;
    if (hasSelection()) {
        const SelectionRange range = m_selection.normalized();
        QTextCursor cursor = createCursor(m_textBuffer.get(), range.start);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        format = cursor.charFormat();
        paragraph = range.start.paragraph;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(paragraph);
    CopiedFormatting copied;
    copied.character = onlyClearedFormatting(format);
    if (block.isValid() && blockObject(block) == TextObject::None) {
        copied.style = paragraphStyleOf(block.blockFormat());
        copied.paragraph = paragraphFormatOf(block.blockFormat());
        copied.alignment = ownAlignment(block.blockFormat());
    }
    copiedFormatting() = copied;
}

bool BookEditor::hasCopiedFormatting()
{
    return copiedFormatting().has_value();
}

void BookEditor::pasteFormatting()
{
    const std::optional<CopiedFormatting> held = copiedFormatting();
    if (refusesChange() || !m_textBuffer || !held) {
        return;
    }
    const CopiedFormatting& copied = *held;
    const auto change = [&copied](QTextCharFormat& format) {
        format = withoutClearedFormatting(format);
        format.merge(copied.character);
    };

    // The paragraphs the selection holds whole get the style, own formatting and alignment;
    // a list item keeps the indents of its level
    std::vector<std::pair<QTextBlock, QTextBlockFormat>> blocks;
    std::vector<QTextBlock> restyled;
    if (hasSelection() && !copied.style.isEmpty()) {
        const SelectionRange range = m_selection.normalized();
        for (const QTextBlock& block : selectedTextBlocks()) {
            const int number = block.blockNumber();
            const bool whole = (number > range.start.paragraph || range.start.offset == 0) &&
                               (number < range.end.paragraph || range.end.offset >= block.length() - 1);
            if (!whole) {
                continue;
            }
            QTextBlockFormat format = block.blockFormat();
            if (paragraphStyleOf(format) != copied.style) {
                setParagraphStyle(format, copied.style);
                restyled.push_back(block);
            }
            ParagraphFormat paragraph = copied.paragraph;
            if (isListItem(format)) {
                const ParagraphFormat own = paragraphFormatOf(format);
                paragraph.leftIndent = own.leftIndent;
                paragraph.firstLineIndent = own.firstLineIndent;
            }
            setParagraphFormat(format, paragraph);
            if (copied.alignment) {
                format.setAlignment(copied.alignment);
            } else {
                format.clearProperty(QTextFormat::BlockAlignment);
            }
            blocks.emplace_back(block, format);
        }
    }

    if (const std::optional<SelectionRange> range = formatRange()) {
        setFormats(changedFormatPieces(*range, change), blocks);
    } else {
        QTextCharFormat wanted = typingFormat();
        change(wanted);
        setPendingFormat(pendingFor(wanted));
    }
    if (!restyled.empty()) {
        checkParagraphsAgain(restyled);  // Code is not checked, the other styles are
    }
}

// =============================================================================
// Font of the selection, the word with the cursor or the text typed next
// =============================================================================

void BookEditor::setSelectionFontFamily(const QString& family)
{
    if (refusesChange()) {
        return;
    }
    if (!m_textBuffer) {
        return;
    }

    QTextCharFormat format;
    format.setFontFamilies({family});
    if (const std::optional<SelectionRange> range = formatRange()) {
        applyCharFormat(*range, format);
        return;
    }

    // The text typed next. The editor's own font stays: the settings dialog owns it
    // (editor.fontFamily), and saving the settings would bring it back.
    QTextCharFormat pending = pendingFormat();
    if (family == fontFamilyOf(placeFormat())) {
        pending.clearProperty(QTextFormat::FontFamilies);
    } else {
        pending.setFontFamilies({family});
    }
    setPendingFormat(pending);
}

void BookEditor::setSelectionFontSize(int pointSize)
{
    if (refusesChange()) {
        return;
    }
    if (!m_textBuffer) {
        return;
    }

    QTextCharFormat format;
    format.setFontPointSize(pointSize);
    if (const std::optional<SelectionRange> range = formatRange()) {
        applyCharFormat(*range, format);
        return;
    }

    // The text typed next; the editor's own size stays (editor.fontSize, see above)
    QTextCharFormat pending = pendingFormat();
    if (pointSize == fontSizeOf(placeFormat())) {
        pending.clearProperty(QTextFormat::FontPointSize);
    } else {
        pending.setFontPointSize(pointSize);
    }
    setPendingFormat(pending);
}

QString BookEditor::currentFontFamily() const
{
    return fontFamilyOf(typingFormat());
}

int BookEditor::currentFontSize() const
{
    return fontSizeOf(typingFormat());
}

}  // namespace kalahari::editor
