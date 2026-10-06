/// @file book_editor_formatting.cpp
/// @brief BookEditor: character formatting, paragraph alignment and fonts

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/core/logger.h>

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
    // If no selection, check pending state or cursor position
    if (!hasSelection()) {
        if (m_pendingBold) {
            return true;
        }
    }
    return hasFormat(InlineFormat::Bold);
}

bool BookEditor::isItalic() const
{
    if (!hasSelection()) {
        if (m_pendingItalic) {
            return true;
        }
    }
    return hasFormat(InlineFormat::Italic);
}

bool BookEditor::isUnderline() const
{
    if (!hasSelection()) {
        if (m_pendingUnderline) {
            return true;
        }
    }
    return hasFormat(InlineFormat::Underline);
}

bool BookEditor::isStrikethrough() const
{
    if (!hasSelection()) {
        if (m_pendingStrikethrough) {
            return true;
        }
    }
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
    // The step changed no text, so its cursor and selection fit the text on both sides of it
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

    if (hasSelection()) {
        // Phase 11: Use QTextCursor for formatting selection
        SelectionRange normRange = m_selection.normalized();
        bool alreadyHasFormat = hasFormat(formatType);

        // Create QTextCursor with selection
        QTextBlock startBlock = m_textBuffer->findBlockByNumber(normRange.start.paragraph);
        QTextBlock endBlock = m_textBuffer->findBlockByNumber(normRange.end.paragraph);
        if (!startBlock.isValid() || !endBlock.isValid()) return;

        int startPos = startBlock.position() + normRange.start.offset;
        int endPos = endBlock.position() + normRange.end.offset;

        QTextCursor cursor(m_textBuffer.get());
        cursor.setPosition(startPos);
        cursor.setPosition(endPos, QTextCursor::KeepAnchor);

        // Create format to apply/remove
        QTextCharFormat fmt;
        switch (formatType) {
            case InlineFormat::Bold:
                fmt.setFontWeight(alreadyHasFormat ? QFont::Normal : QFont::Bold);
                break;
            case InlineFormat::Italic:
                fmt.setFontItalic(!alreadyHasFormat);
                break;
            case InlineFormat::Underline:
                fmt.setFontUnderline(!alreadyHasFormat);
                break;
            case InlineFormat::Strikethrough:
                fmt.setFontStrikeOut(!alreadyHasFormat);
                break;
        }

        // Apply format (QTextDocument handles undo/redo)
        cursor.mergeCharFormat(fmt);

        emit contentChanged();
        update();

        core::Logger::getInstance().debug("BookEditor::toggleFormat() - formatting {} {}",
            alreadyHasFormat ? "removed" : "applied", formatName());
    } else {
        // Toggle pending format for next typed characters
        switch (formatType) {
            case InlineFormat::Bold:
                m_pendingBold = !m_pendingBold;
                core::Logger::getInstance().debug("BookEditor::toggleFormat() - "
                    "pending bold={}", m_pendingBold);
                break;
            case InlineFormat::Italic:
                m_pendingItalic = !m_pendingItalic;
                core::Logger::getInstance().debug("BookEditor::toggleFormat() - "
                    "pending italic={}", m_pendingItalic);
                break;
            case InlineFormat::Underline:
                m_pendingUnderline = !m_pendingUnderline;
                core::Logger::getInstance().debug("BookEditor::toggleFormat() - "
                    "pending underline={}", m_pendingUnderline);
                break;
            case InlineFormat::Strikethrough:
                m_pendingStrikethrough = !m_pendingStrikethrough;
                core::Logger::getInstance().debug("BookEditor::toggleFormat() - "
                    "pending strikethrough={}", m_pendingStrikethrough);
                break;
        }
    }
}

bool BookEditor::hasFormat(InlineFormat formatType) const
{
    if (!m_textBuffer) {
        return false;
    }

    // Phase 11: Check QTextCharFormat for formatting
    auto checkCharFormat = [formatType](const QTextCharFormat& fmt) -> bool {
        switch (formatType) {
            case InlineFormat::Bold:
                return fmt.fontWeight() >= QFont::Bold;
            case InlineFormat::Italic:
                return fmt.fontItalic();
            case InlineFormat::Underline:
                return fmt.fontUnderline();
            case InlineFormat::Strikethrough:
                return fmt.fontStrikeOut();
        }
        return false;
    };

    if (hasSelection()) {
        // Check if ALL text in the selection carries this format. Iterate the
        // document's text FRAGMENTS (runs of uniform formatting) instead of
        // constructing one QTextCursor per character — the old per-character loop
        // was O(N) cursor allocations and made "select all + bold" take ~10 s on a
        // large chapter. Fragment iteration is O(runs), effectively instant.
        SelectionRange normRange = m_selection.normalized();

        for (int i = normRange.start.paragraph; i <= normRange.end.paragraph; ++i) {
            QTextBlock block = m_textBuffer->findBlockByNumber(i);
            if (!block.isValid()) continue;

            int start = (i == normRange.start.paragraph) ? normRange.start.offset : 0;
            int end = (i == normRange.end.paragraph) ? normRange.end.offset : block.length() - 1;
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

                if (!checkCharFormat(frag.charFormat())) {
                    return false;
                }
            }
        }
        return true;
    } else {
        // Check format at cursor position
        QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
        if (!block.isValid()) return false;

        // If cursor is at end of paragraph, check previous character
        int checkOffset = m_cursorPosition.offset;
        if (checkOffset > 0) {
            checkOffset--;
        }

        QTextCursor cursor(m_textBuffer.get());
        cursor.setPosition(block.position() + checkOffset);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        return checkCharFormat(cursor.charFormat());
    }
}

// =============================================================================
// Font Selection (applies to selection if any, otherwise default font)
// =============================================================================

void BookEditor::setSelectionFontFamily(const QString& family)
{
    if (!m_textBuffer) {
        return;
    }

    if (hasSelection()) {
        // Apply to selection
        SelectionRange normRange = m_selection.normalized();
        QTextBlock startBlock = m_textBuffer->findBlockByNumber(static_cast<int>(normRange.start.paragraph));
        QTextBlock endBlock = m_textBuffer->findBlockByNumber(static_cast<int>(normRange.end.paragraph));

        if (!startBlock.isValid() || !endBlock.isValid()) {
            return;
        }

        int startPos = startBlock.position() + normRange.start.offset;
        int endPos = endBlock.position() + normRange.end.offset;

        QTextCursor cursor(m_textBuffer.get());
        cursor.setPosition(startPos);
        cursor.setPosition(endPos, QTextCursor::KeepAnchor);

        QTextCharFormat fmt;
        fmt.setFontFamilies({family});
        cursor.mergeCharFormat(fmt);

        emit contentChanged();
        update();
    }
    // No selection: the toolbar font combo is a selection-only formatting control,
    // like a classic word processor. With no selection we intentionally do nothing.
    // The editor's global default font is owned solely by the settings dialog
    // (editor.fontFamily); the toolbar must never mutate it here, or saving settings
    // would appear to "revert" the font (it was only ever an unpersisted live change
    // on m_appearance, which applyEditorSettingsToAllPanels then overwrote).
}

void BookEditor::setSelectionFontSize(int pointSize)
{
    if (!m_textBuffer) {
        return;
    }

    if (hasSelection()) {
        // Apply to selection
        SelectionRange normRange = m_selection.normalized();
        QTextBlock startBlock = m_textBuffer->findBlockByNumber(static_cast<int>(normRange.start.paragraph));
        QTextBlock endBlock = m_textBuffer->findBlockByNumber(static_cast<int>(normRange.end.paragraph));

        if (!startBlock.isValid() || !endBlock.isValid()) {
            return;
        }

        int startPos = startBlock.position() + normRange.start.offset;
        int endPos = endBlock.position() + normRange.end.offset;

        QTextCursor cursor(m_textBuffer.get());
        cursor.setPosition(startPos);
        cursor.setPosition(endPos, QTextCursor::KeepAnchor);

        QTextCharFormat fmt;
        fmt.setFontPointSize(pointSize);
        cursor.mergeCharFormat(fmt);

        emit contentChanged();
        update();
    }
    // No selection: selection-only control, same as setSelectionFontFamily above.
    // The global default font size is owned by the settings dialog (editor.fontSize).
}

QString BookEditor::currentFontFamily() const
{
    if (!m_textBuffer) {
        return m_appearance.typography.textFont.family();
    }

    // Get font at cursor position
    QTextBlock block = m_textBuffer->findBlockByNumber(static_cast<int>(m_cursorPosition.paragraph));
    if (!block.isValid()) {
        return m_appearance.typography.textFont.family();
    }

    QTextCursor cursor(m_textBuffer.get());
    cursor.setPosition(block.position() + m_cursorPosition.offset);
    QVariant families = cursor.charFormat().fontFamilies();
    if (families.isValid() && families.toStringList().size() > 0) {
        return families.toStringList().first();
    }
    return m_appearance.typography.textFont.family();
}

int BookEditor::currentFontSize() const
{
    if (!m_textBuffer) {
        return m_appearance.typography.textFont.pointSize();
    }

    // Get font at cursor position
    QTextBlock block = m_textBuffer->findBlockByNumber(static_cast<int>(m_cursorPosition.paragraph));
    if (!block.isValid()) {
        return m_appearance.typography.textFont.pointSize();
    }

    QTextCursor cursor(m_textBuffer.get());
    cursor.setPosition(block.position() + m_cursorPosition.offset);
    int size = static_cast<int>(cursor.charFormat().fontPointSize());
    return size > 0 ? size : m_appearance.typography.textFont.pointSize();
}

}  // namespace kalahari::editor
