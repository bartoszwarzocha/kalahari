/// @file book_editor_formatting.cpp
/// @brief BookEditor: character formatting, paragraph alignment and fonts

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/core/logger.h>
#include <kalahari/editor/buffer_commands.h>
#include <QTextBoundaryFinder>
#include <algorithm>

namespace kalahari::editor {

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
    if (block.isValid()) {
        return effectiveAlignment(ownAlignment(block.blockFormat()));
    }
    return DEFAULT_PARAGRAPH_ALIGNMENT;
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
    const bool placeHasIt = formatHas(placeFormat(), formatType);
    setPendingFormat(
        withFormat(pendingFormat(), placeHasIt == on ? std::nullopt : std::optional<bool>(on)));
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
