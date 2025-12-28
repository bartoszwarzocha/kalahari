/// @file buffer_commands.cpp
/// @brief Implementation of undo/redo commands for TextBuffer/FormatLayer
///
/// This file implements QUndoCommand-based classes for the new
/// TextBuffer + FormatLayer architecture (OpenSpec #00043 Phase 9).

#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/kml_converter.h>  // For MetadataLayer
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// Helper Functions
// =============================================================================

size_t calculateAbsolutePosition(const TextBuffer& buffer, int paragraphIndex, int offset)
{
    size_t absolutePos = 0;

    // Sum lengths of all paragraphs before this one
    // Each paragraph is followed by a newline character (except the last)
    for (int i = 0; i < paragraphIndex && i < static_cast<int>(buffer.paragraphCount()); ++i) {
        absolutePos += static_cast<size_t>(buffer.paragraphLength(static_cast<size_t>(i)));
        // Add 1 for the newline separator between paragraphs
        absolutePos += 1;
    }

    // Add the offset within the current paragraph
    absolutePos += static_cast<size_t>(offset);

    return absolutePos;
}

size_t calculateAbsolutePosition(const TextBuffer& buffer, const CursorPosition& pos)
{
    return calculateAbsolutePosition(buffer, pos.paragraph, pos.offset);
}

CursorPosition absoluteToCursorPosition(const TextBuffer& buffer, size_t absolutePos)
{
    size_t currentPos = 0;
    const size_t paraCount = buffer.paragraphCount();

    for (size_t i = 0; i < paraCount; ++i) {
        const size_t paraLength = static_cast<size_t>(buffer.paragraphLength(i));
        const size_t paraEnd = currentPos + paraLength;

        // Check if position is within this paragraph
        if (absolutePos <= paraEnd) {
            return CursorPosition{
                static_cast<int>(i),
                static_cast<int>(absolutePos - currentPos)
            };
        }

        // Move past this paragraph and its newline
        currentPos = paraEnd + 1;
    }

    // Position is past end of document - clamp to end
    if (paraCount > 0) {
        return CursorPosition{
            static_cast<int>(paraCount - 1),
            buffer.paragraphLength(paraCount - 1)
        };
    }

    return CursorPosition{0, 0};
}

// =============================================================================
// BufferCommand Base Class
// =============================================================================

BufferCommand::BufferCommand(TextBuffer* buffer,
                             FormatLayer* formatLayer,
                             MetadataLayer* metadataLayer,
                             const CursorPosition& cursorBefore,
                             const QString& text)
    : QUndoCommand(text)
    , m_buffer(buffer)
    , m_formatLayer(formatLayer)
    , m_metadataLayer(metadataLayer)
    , m_cursorBefore(cursorBefore)
    , m_cursorAfter(cursorBefore)
{
}

// =============================================================================
// TextInsertCommand
// =============================================================================

TextInsertCommand::TextInsertCommand(TextBuffer* buffer,
                                     FormatLayer* formatLayer,
                                     MetadataLayer* metadataLayer,
                                     const CursorPosition& position,
                                     const QString& text)
    : BufferCommand(buffer, formatLayer, metadataLayer, position, QObject::tr("Insert Text"))
    , m_insertPosition(position)
    , m_text(text)
    , m_timestamp(std::chrono::steady_clock::now())
{
    // Calculate cursor position after insertion
    // Count newlines in text to adjust paragraph
    int newParagraph = position.paragraph;
    int newOffset = position.offset;

    for (const QChar& ch : text) {
        if (ch == '\n') {
            ++newParagraph;
            newOffset = 0;
        } else {
            ++newOffset;
        }
    }

    m_cursorAfter = CursorPosition{newParagraph, newOffset};
}

void TextInsertCommand::undo()
{
    if (!m_buffer) {
        return;
    }

    // Calculate absolute position for format layer
    const size_t absPos = calculateAbsolutePosition(*m_buffer, m_insertPosition);

    // Handle text with newlines - need to merge paragraphs
    const int newlineCount = m_text.count('\n');

    if (newlineCount > 0) {
        // Remove the inserted paragraphs by merging them back
        // Work from the last inserted paragraph back to the first
        for (int i = newlineCount; i > 0; --i) {
            const size_t paraToMerge = static_cast<size_t>(m_insertPosition.paragraph) + 1;
            if (paraToMerge < m_buffer->paragraphCount()) {
                // Get content of paragraph to merge
                const QString nextContent = m_buffer->paragraphText(paraToMerge);

                // Remove that paragraph
                m_buffer->removeParagraph(paraToMerge);

                // Append its content to the current paragraph
                const size_t currentPara = static_cast<size_t>(m_insertPosition.paragraph);
                const QString currentContent = m_buffer->paragraphText(currentPara);
                m_buffer->setParagraphText(currentPara, currentContent + nextContent);
            }
        }

        // Now remove the remaining inserted text from the first paragraph
        const QStringList lines = m_text.split('\n');
        const QString firstLineInserted = lines.first();
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(m_insertPosition.paragraph));

        // Remove the first line portion
        QString newContent = currentContent;
        newContent.remove(m_insertPosition.offset, firstLineInserted.length());
        m_buffer->setParagraphText(static_cast<size_t>(m_insertPosition.paragraph), newContent);
    } else {
        // Simple case: text within single paragraph
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(m_insertPosition.paragraph));
        QString newContent = currentContent;
        newContent.remove(m_insertPosition.offset, m_text.length());
        m_buffer->setParagraphText(static_cast<size_t>(m_insertPosition.paragraph), newContent);
    }

    // Notify format layer about text deletion
    if (m_formatLayer) {
        m_formatLayer->onTextDeleted(absPos, static_cast<size_t>(m_text.length()));
    }

    // Notify metadata layer about text deletion
    if (m_metadataLayer) {
        m_metadataLayer->onTextDeleted(absPos, static_cast<size_t>(m_text.length()));
    }
}

void TextInsertCommand::redo()
{
    if (!m_buffer) {
        return;
    }

    // Calculate absolute position for format layer
    const size_t absPos = calculateAbsolutePosition(*m_buffer, m_insertPosition);

    // Handle text with newlines - need to split paragraphs
    if (m_text.contains('\n')) {
        const QStringList lines = m_text.split('\n');

        // Get current paragraph content
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(m_insertPosition.paragraph));

        // Split at insertion point
        const QString before = currentContent.left(m_insertPosition.offset);
        const QString after = currentContent.mid(m_insertPosition.offset);

        // Update first paragraph with text before first newline
        m_buffer->setParagraphText(
            static_cast<size_t>(m_insertPosition.paragraph),
            before + lines.first());

        // Insert new paragraphs for each subsequent line
        for (int i = 1; i < lines.size() - 1; ++i) {
            m_buffer->insertParagraph(
                static_cast<size_t>(m_insertPosition.paragraph) + static_cast<size_t>(i),
                lines[i]);
        }

        // Last line gets the content after the insertion point
        const size_t lastParaIndex = static_cast<size_t>(m_insertPosition.paragraph) +
                                     static_cast<size_t>(lines.size()) - 1;
        m_buffer->insertParagraph(lastParaIndex, lines.last() + after);
    } else {
        // Simple case: text within single paragraph
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(m_insertPosition.paragraph));
        const QString newContent = currentContent.left(m_insertPosition.offset) +
                                   m_text +
                                   currentContent.mid(m_insertPosition.offset);
        m_buffer->setParagraphText(static_cast<size_t>(m_insertPosition.paragraph), newContent);
    }

    // Notify format layer about text insertion
    if (m_formatLayer) {
        m_formatLayer->onTextInserted(absPos, static_cast<size_t>(m_text.length()));
    }

    // Notify metadata layer about text insertion
    if (m_metadataLayer) {
        m_metadataLayer->onTextInserted(absPos, static_cast<size_t>(m_text.length()));
    }
}

int TextInsertCommand::id() const
{
    return static_cast<int>(BufferCommandId::TextInsert);
}

bool TextInsertCommand::mergeWith(const QUndoCommand* other)
{
    if (other->id() != id()) {
        return false;
    }

    const auto* otherInsert = static_cast<const TextInsertCommand*>(other);

    // Don't merge if text contains newlines (paragraph boundaries)
    if (m_text.contains('\n') || otherInsert->m_text.contains('\n')) {
        return false;
    }

    // Check time window
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        otherInsert->m_timestamp - m_timestamp);
    if (elapsed.count() > MERGE_WINDOW_MS) {
        return false;
    }

    // Check if the new insertion is at the end of our inserted text
    if (otherInsert->m_insertPosition.paragraph != m_cursorAfter.paragraph ||
        otherInsert->m_insertPosition.offset != m_cursorAfter.offset) {
        return false;
    }

    // Check maximum merge length
    if (m_text.length() + otherInsert->m_text.length() > MAX_MERGE_LENGTH) {
        return false;
    }

    // Merge the text
    m_text += otherInsert->m_text;
    m_cursorAfter = otherInsert->m_cursorAfter;
    m_timestamp = otherInsert->m_timestamp;

    return true;
}

// =============================================================================
// TextDeleteCommand
// =============================================================================

TextDeleteCommand::TextDeleteCommand(TextBuffer* buffer,
                                     FormatLayer* formatLayer,
                                     MetadataLayer* metadataLayer,
                                     const CursorPosition& start,
                                     const CursorPosition& end,
                                     const QString& deletedText,
                                     const std::vector<FormatRange>& deletedFormats)
    : BufferCommand(buffer, formatLayer, metadataLayer, start, QObject::tr("Delete Text"))
    , m_start(start)
    , m_end(end)
    , m_deletedText(deletedText)
    , m_deletedFormats(deletedFormats)
{
    m_cursorAfter = start;  // Cursor moves to start of deletion
}

void TextDeleteCommand::undo()
{
    if (!m_buffer) {
        return;
    }

    // Calculate absolute position
    const size_t absPos = calculateAbsolutePosition(*m_buffer, m_start);

    // Re-insert the deleted text
    if (m_deletedText.contains('\n')) {
        const QStringList lines = m_deletedText.split('\n');

        // Get current paragraph content
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(m_start.paragraph));

        // Split at insertion point
        const QString before = currentContent.left(m_start.offset);
        const QString after = currentContent.mid(m_start.offset);

        // Update first paragraph
        m_buffer->setParagraphText(
            static_cast<size_t>(m_start.paragraph),
            before + lines.first());

        // Insert new paragraphs
        for (int i = 1; i < lines.size() - 1; ++i) {
            m_buffer->insertParagraph(
                static_cast<size_t>(m_start.paragraph) + static_cast<size_t>(i),
                lines[i]);
        }

        // Last line
        const size_t lastParaIndex = static_cast<size_t>(m_start.paragraph) +
                                     static_cast<size_t>(lines.size()) - 1;
        m_buffer->insertParagraph(lastParaIndex, lines.last() + after);
    } else {
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(m_start.paragraph));
        const QString newContent = currentContent.left(m_start.offset) +
                                   m_deletedText +
                                   currentContent.mid(m_start.offset);
        m_buffer->setParagraphText(static_cast<size_t>(m_start.paragraph), newContent);
    }

    // Notify format layer about text insertion
    if (m_formatLayer) {
        m_formatLayer->onTextInserted(absPos, static_cast<size_t>(m_deletedText.length()));

        // Restore deleted format ranges
        for (const auto& range : m_deletedFormats) {
            m_formatLayer->addFormat(range.start, range.end, range.format);
        }
    }

    // Notify metadata layer about text insertion
    if (m_metadataLayer) {
        m_metadataLayer->onTextInserted(absPos, static_cast<size_t>(m_deletedText.length()));
    }
}

void TextDeleteCommand::redo()
{
    if (!m_buffer) {
        return;
    }

    // Calculate absolute position
    const size_t absPos = calculateAbsolutePosition(*m_buffer, m_start);

    // Handle multi-paragraph deletion
    if (m_start.paragraph != m_end.paragraph) {
        // Get content to keep from first and last paragraphs
        const QString firstContent = m_buffer->paragraphText(
            static_cast<size_t>(m_start.paragraph));
        const QString lastContent = m_buffer->paragraphText(
            static_cast<size_t>(m_end.paragraph));

        const QString keepBefore = firstContent.left(m_start.offset);
        const QString keepAfter = lastContent.mid(m_end.offset);

        // Remove paragraphs from end to start+1 (to preserve first)
        for (int i = m_end.paragraph; i > m_start.paragraph; --i) {
            m_buffer->removeParagraph(static_cast<size_t>(i));
        }

        // Update first paragraph with merged content
        m_buffer->setParagraphText(
            static_cast<size_t>(m_start.paragraph),
            keepBefore + keepAfter);
    } else {
        // Single paragraph deletion
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(m_start.paragraph));
        const QString newContent = currentContent.left(m_start.offset) +
                                   currentContent.mid(m_end.offset);
        m_buffer->setParagraphText(static_cast<size_t>(m_start.paragraph), newContent);
    }

    // Notify format layer about text deletion
    if (m_formatLayer) {
        m_formatLayer->onTextDeleted(absPos, static_cast<size_t>(m_deletedText.length()));
    }

    // Notify metadata layer about text deletion
    if (m_metadataLayer) {
        m_metadataLayer->onTextDeleted(absPos, static_cast<size_t>(m_deletedText.length()));
    }
}

int TextDeleteCommand::id() const
{
    return static_cast<int>(BufferCommandId::TextDelete);
}

// =============================================================================
// ParagraphSplitCommand
// =============================================================================

ParagraphSplitCommand::ParagraphSplitCommand(TextBuffer* buffer,
                                             FormatLayer* formatLayer,
                                             MetadataLayer* metadataLayer,
                                             const CursorPosition& position,
                                             const std::vector<FormatRange>& movedFormats)
    : BufferCommand(buffer, formatLayer, metadataLayer, position, QObject::tr("Split Paragraph"))
    , m_splitPosition(position)
    , m_movedFormats(movedFormats)
{
    // After split, cursor is at start of new paragraph
    m_cursorAfter = CursorPosition{position.paragraph + 1, 0};
}

void ParagraphSplitCommand::undo()
{
    if (!m_buffer) {
        return;
    }

    // Merge the two paragraphs back together
    const size_t currentPara = static_cast<size_t>(m_splitPosition.paragraph);
    const size_t nextPara = currentPara + 1;

    if (nextPara >= m_buffer->paragraphCount()) {
        return;
    }

    const QString firstContent = m_buffer->paragraphText(currentPara);
    const QString secondContent = m_buffer->paragraphText(nextPara);

    // Remove the second paragraph
    m_buffer->removeParagraph(nextPara);

    // Merge content
    m_buffer->setParagraphText(currentPara, firstContent + secondContent);

    // Calculate absolute position of the split point
    const size_t absPos = calculateAbsolutePosition(*m_buffer, m_splitPosition);

    // Notify format layer about newline deletion
    if (m_formatLayer) {
        // A paragraph split is essentially inserting a newline character
        m_formatLayer->onTextDeleted(absPos, 1);
    }

    // Notify metadata layer about newline deletion
    if (m_metadataLayer) {
        m_metadataLayer->onTextDeleted(absPos, 1);
    }
}

void ParagraphSplitCommand::redo()
{
    if (!m_buffer) {
        return;
    }

    const size_t currentPara = static_cast<size_t>(m_splitPosition.paragraph);
    const QString currentContent = m_buffer->paragraphText(currentPara);

    // Split the content
    const QString beforeSplit = currentContent.left(m_splitPosition.offset);
    const QString afterSplit = currentContent.mid(m_splitPosition.offset);

    // Update current paragraph
    m_buffer->setParagraphText(currentPara, beforeSplit);

    // Insert new paragraph with remaining content
    m_buffer->insertParagraph(currentPara + 1, afterSplit);

    // Calculate absolute position of the split point
    const size_t absPos = calculateAbsolutePosition(*m_buffer, m_splitPosition);

    // Notify format layer about newline insertion
    if (m_formatLayer) {
        m_formatLayer->onTextInserted(absPos, 1);
    }

    // Notify metadata layer about newline insertion
    if (m_metadataLayer) {
        m_metadataLayer->onTextInserted(absPos, 1);
    }
}

int ParagraphSplitCommand::id() const
{
    return static_cast<int>(BufferCommandId::ParagraphSplit);
}

// =============================================================================
// ParagraphMergeCommand
// =============================================================================

ParagraphMergeCommand::ParagraphMergeCommand(TextBuffer* buffer,
                                             FormatLayer* formatLayer,
                                             MetadataLayer* metadataLayer,
                                             const CursorPosition& cursorPos,
                                             int mergeFromIndex,
                                             const QString& mergedContent,
                                             const std::vector<FormatRange>& mergedFormats)
    : BufferCommand(buffer, formatLayer, metadataLayer, cursorPos, QObject::tr("Merge Paragraphs"))
    , m_mergeFromIndex(mergeFromIndex)
    , m_mergedContent(mergedContent)
    , m_mergedFormats(mergedFormats)
    , m_splitOffset(0)
{
    // Calculate split offset (where paragraphs were joined)
    if (buffer && m_mergeFromIndex > 0) {
        const size_t targetPara = static_cast<size_t>(m_mergeFromIndex - 1);
        m_splitOffset = buffer->paragraphLength(targetPara);
    }

    // After merge, cursor is at the join point
    m_cursorAfter = CursorPosition{m_mergeFromIndex - 1, m_splitOffset};
}

void ParagraphMergeCommand::undo()
{
    if (!m_buffer || m_mergeFromIndex <= 0) {
        return;
    }

    const size_t targetPara = static_cast<size_t>(m_mergeFromIndex - 1);
    const QString mergedParagraph = m_buffer->paragraphText(targetPara);

    // Split back into two paragraphs
    const QString firstPart = mergedParagraph.left(m_splitOffset);
    const QString secondPart = mergedParagraph.mid(m_splitOffset);

    // Update the first paragraph
    m_buffer->setParagraphText(targetPara, firstPart);

    // Re-insert the merged paragraph
    m_buffer->insertParagraph(static_cast<size_t>(m_mergeFromIndex), secondPart);

    // Calculate absolute position for format layer
    const CursorPosition splitPos{m_mergeFromIndex - 1, m_splitOffset};
    const size_t absPos = calculateAbsolutePosition(*m_buffer, splitPos);

    // Notify format layer about newline insertion
    if (m_formatLayer) {
        m_formatLayer->onTextInserted(absPos, 1);

        // Restore merged format ranges
        for (const auto& range : m_mergedFormats) {
            m_formatLayer->addFormat(range.start, range.end, range.format);
        }
    }

    // Notify metadata layer about newline insertion
    if (m_metadataLayer) {
        m_metadataLayer->onTextInserted(absPos, 1);
    }
}

void ParagraphMergeCommand::redo()
{
    if (!m_buffer || m_mergeFromIndex <= 0) {
        return;
    }

    const size_t targetPara = static_cast<size_t>(m_mergeFromIndex - 1);
    const size_t sourcePara = static_cast<size_t>(m_mergeFromIndex);

    if (sourcePara >= m_buffer->paragraphCount()) {
        return;
    }

    const QString firstContent = m_buffer->paragraphText(targetPara);
    const QString secondContent = m_buffer->paragraphText(sourcePara);

    // Store the split offset for undo
    m_splitOffset = firstContent.length();

    // Calculate absolute position before merge
    const CursorPosition splitPos{m_mergeFromIndex - 1, m_splitOffset};
    const size_t absPos = calculateAbsolutePosition(*m_buffer, splitPos);

    // Remove the source paragraph
    m_buffer->removeParagraph(sourcePara);

    // Merge content
    m_buffer->setParagraphText(targetPara, firstContent + secondContent);

    // Notify format layer about newline deletion
    if (m_formatLayer) {
        m_formatLayer->onTextDeleted(absPos, 1);
    }

    // Notify metadata layer about newline deletion
    if (m_metadataLayer) {
        m_metadataLayer->onTextDeleted(absPos, 1);
    }
}

int ParagraphMergeCommand::id() const
{
    return static_cast<int>(BufferCommandId::ParagraphMerge);
}

// =============================================================================
// CompositeBufferCommand
// =============================================================================

CompositeBufferCommand::CompositeBufferCommand(TextBuffer* buffer,
                                               FormatLayer* formatLayer,
                                               MetadataLayer* metadataLayer,
                                               const CursorPosition& cursorBefore,
                                               const QString& text)
    : BufferCommand(buffer, formatLayer, metadataLayer, cursorBefore, text)
{
}

CompositeBufferCommand::~CompositeBufferCommand() = default;

void CompositeBufferCommand::addCommand(std::unique_ptr<BufferCommand> command)
{
    if (command) {
        // Update cursor after to match the last command
        m_cursorAfter = command->cursorAfter();
        m_commands.push_back(std::move(command));
    }
}

void CompositeBufferCommand::undo()
{
    // Undo in reverse order
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
        (*it)->undo();
    }
}

void CompositeBufferCommand::redo()
{
    // Redo in forward order
    for (auto& cmd : m_commands) {
        cmd->redo();
    }
}

// =============================================================================
// FormatApplyCommand
// =============================================================================

FormatApplyCommand::FormatApplyCommand(TextBuffer* buffer,
                                       FormatLayer* formatLayer,
                                       MetadataLayer* metadataLayer,
                                       const CursorPosition& start,
                                       const CursorPosition& end,
                                       const TextFormat& format,
                                       const std::vector<FormatRange>& previousFormats)
    : BufferCommand(buffer, formatLayer, metadataLayer, start, QObject::tr("Apply Formatting"))
    , m_start(start)
    , m_end(end)
    , m_format(format)
    , m_previousFormats(previousFormats)
{
    m_cursorAfter = end;  // Cursor stays at end of formatted range
}

void FormatApplyCommand::undo()
{
    if (!m_formatLayer || !m_buffer) {
        return;
    }

    // Calculate absolute positions
    const size_t absStart = calculateAbsolutePosition(*m_buffer, m_start);
    const size_t absEnd = calculateAbsolutePosition(*m_buffer, m_end);

    // Clear the formatting we applied
    m_formatLayer->clearFormats(absStart, absEnd);

    // Restore previous format ranges
    for (const auto& range : m_previousFormats) {
        m_formatLayer->addFormat(range.start, range.end, range.format);
    }
}

void FormatApplyCommand::redo()
{
    if (!m_formatLayer || !m_buffer) {
        return;
    }

    // Calculate absolute positions
    const size_t absStart = calculateAbsolutePosition(*m_buffer, m_start);
    const size_t absEnd = calculateAbsolutePosition(*m_buffer, m_end);

    // Apply the format
    m_formatLayer->addFormat(absStart, absEnd, m_format);
}

int FormatApplyCommand::id() const
{
    return static_cast<int>(BufferCommandId::FormatApply);
}

// =============================================================================
// FormatRemoveCommand
// =============================================================================

FormatRemoveCommand::FormatRemoveCommand(TextBuffer* buffer,
                                         FormatLayer* formatLayer,
                                         MetadataLayer* metadataLayer,
                                         const CursorPosition& start,
                                         const CursorPosition& end,
                                         FormatType formatType,
                                         const std::vector<FormatRange>& removedFormats)
    : BufferCommand(buffer, formatLayer, metadataLayer, start, QObject::tr("Remove Formatting"))
    , m_start(start)
    , m_end(end)
    , m_formatType(formatType)
    , m_removedFormats(removedFormats)
{
    m_cursorAfter = end;  // Cursor stays at end of range
}

void FormatRemoveCommand::undo()
{
    if (!m_formatLayer) {
        return;
    }

    // Restore removed format ranges
    for (const auto& range : m_removedFormats) {
        m_formatLayer->addFormat(range.start, range.end, range.format);
    }
}

void FormatRemoveCommand::redo()
{
    if (!m_formatLayer || !m_buffer) {
        return;
    }

    // Calculate absolute positions
    const size_t absStart = calculateAbsolutePosition(*m_buffer, m_start);
    const size_t absEnd = calculateAbsolutePosition(*m_buffer, m_end);

    // Remove the format(s)
    if (m_formatType == FormatType::None) {
        m_formatLayer->clearFormats(absStart, absEnd);
    } else {
        m_formatLayer->removeFormat(absStart, absEnd, m_formatType);
    }
}

int FormatRemoveCommand::id() const
{
    return static_cast<int>(BufferCommandId::FormatRemove);
}

// =============================================================================
// TextReplaceCommand
// =============================================================================

TextReplaceCommand::TextReplaceCommand(TextBuffer* buffer,
                                       FormatLayer* formatLayer,
                                       MetadataLayer* metadataLayer,
                                       const CursorPosition& cursorBefore,
                                       size_t position,
                                       const QString& originalText,
                                       const QString& replacementText)
    : BufferCommand(buffer, formatLayer, metadataLayer, cursorBefore, QObject::tr("Replace"))
    , m_position(position)
    , m_originalText(originalText)
    , m_replacementText(replacementText)
{
    // Capture original formats at the replacement position
    if (m_formatLayer) {
        m_originalFormats = m_formatLayer->getFormatsInRange(
            m_position, m_position + static_cast<size_t>(m_originalText.length()));
    }

    // Calculate cursor position after replacement
    CursorPosition afterPos = absoluteToCursorPosition(*buffer,
        m_position + static_cast<size_t>(m_replacementText.length()));
    m_cursorAfter = afterPos;
}

void TextReplaceCommand::undo()
{
    if (!m_buffer) {
        return;
    }

    // Convert absolute position to cursor position for buffer operations
    CursorPosition cursorPos = absoluteToCursorPosition(*m_buffer, m_position);

    // First, delete the replacement text
    if (m_replacementText.contains('\n')) {
        // Handle multi-paragraph replacement text
        const int newlineCount = m_replacementText.count('\n');

        // Remove the inserted paragraphs by merging them back
        for (int i = newlineCount; i > 0; --i) {
            const size_t paraToMerge = static_cast<size_t>(cursorPos.paragraph) + 1;
            if (paraToMerge < m_buffer->paragraphCount()) {
                const QString nextContent = m_buffer->paragraphText(paraToMerge);
                m_buffer->removeParagraph(paraToMerge);

                const size_t currentPara = static_cast<size_t>(cursorPos.paragraph);
                const QString currentContent = m_buffer->paragraphText(currentPara);
                m_buffer->setParagraphText(currentPara, currentContent + nextContent);
            }
        }

        // Remove the remaining replacement text from the first paragraph
        const QStringList lines = m_replacementText.split('\n');
        const QString firstLineInserted = lines.first();
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(cursorPos.paragraph));

        QString newContent = currentContent;
        newContent.remove(cursorPos.offset, firstLineInserted.length());
        m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
    } else {
        // Simple single-paragraph replacement text
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(cursorPos.paragraph));
        QString newContent = currentContent;
        newContent.remove(cursorPos.offset, m_replacementText.length());
        m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
    }

    // Notify format layer about text deletion
    if (m_formatLayer) {
        m_formatLayer->onTextDeleted(m_position, static_cast<size_t>(m_replacementText.length()));
    }

    // Notify metadata layer about text deletion
    if (m_metadataLayer) {
        m_metadataLayer->onTextDeleted(m_position, static_cast<size_t>(m_replacementText.length()));
    }

    // Now insert the original text back
    if (m_originalText.contains('\n')) {
        const QStringList lines = m_originalText.split('\n');

        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(cursorPos.paragraph));

        const QString before = currentContent.left(cursorPos.offset);
        const QString after = currentContent.mid(cursorPos.offset);

        m_buffer->setParagraphText(
            static_cast<size_t>(cursorPos.paragraph),
            before + lines.first());

        for (int i = 1; i < lines.size() - 1; ++i) {
            m_buffer->insertParagraph(
                static_cast<size_t>(cursorPos.paragraph) + static_cast<size_t>(i),
                lines[i]);
        }

        const size_t lastParaIndex = static_cast<size_t>(cursorPos.paragraph) +
                                     static_cast<size_t>(lines.size()) - 1;
        m_buffer->insertParagraph(lastParaIndex, lines.last() + after);
    } else {
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(cursorPos.paragraph));
        const QString newContent = currentContent.left(cursorPos.offset) +
                                   m_originalText +
                                   currentContent.mid(cursorPos.offset);
        m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
    }

    // Notify format layer about text insertion and restore formats
    if (m_formatLayer) {
        m_formatLayer->onTextInserted(m_position, static_cast<size_t>(m_originalText.length()));

        // Restore original format ranges
        for (const auto& range : m_originalFormats) {
            m_formatLayer->addFormat(range.start, range.end, range.format);
        }
    }

    // Notify metadata layer about text insertion
    if (m_metadataLayer) {
        m_metadataLayer->onTextInserted(m_position, static_cast<size_t>(m_originalText.length()));
    }
}

void TextReplaceCommand::redo()
{
    if (!m_buffer) {
        return;
    }

    // Convert absolute position to cursor position for buffer operations
    CursorPosition cursorPos = absoluteToCursorPosition(*m_buffer, m_position);

    // First, delete the original text
    if (m_originalText.contains('\n')) {
        // Handle multi-paragraph original text
        const int newlineCount = m_originalText.count('\n');

        for (int i = newlineCount; i > 0; --i) {
            const size_t paraToMerge = static_cast<size_t>(cursorPos.paragraph) + 1;
            if (paraToMerge < m_buffer->paragraphCount()) {
                const QString nextContent = m_buffer->paragraphText(paraToMerge);
                m_buffer->removeParagraph(paraToMerge);

                const size_t currentPara = static_cast<size_t>(cursorPos.paragraph);
                const QString currentContent = m_buffer->paragraphText(currentPara);
                m_buffer->setParagraphText(currentPara, currentContent + nextContent);
            }
        }

        const QStringList lines = m_originalText.split('\n');
        const QString firstLineOriginal = lines.first();
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(cursorPos.paragraph));

        QString newContent = currentContent;
        newContent.remove(cursorPos.offset, firstLineOriginal.length());
        m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
    } else {
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(cursorPos.paragraph));
        QString newContent = currentContent;
        newContent.remove(cursorPos.offset, m_originalText.length());
        m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
    }

    // Notify format layer about text deletion
    if (m_formatLayer) {
        m_formatLayer->onTextDeleted(m_position, static_cast<size_t>(m_originalText.length()));
    }

    // Notify metadata layer about text deletion
    if (m_metadataLayer) {
        m_metadataLayer->onTextDeleted(m_position, static_cast<size_t>(m_originalText.length()));
    }

    // Now insert the replacement text
    if (m_replacementText.contains('\n')) {
        const QStringList lines = m_replacementText.split('\n');

        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(cursorPos.paragraph));

        const QString before = currentContent.left(cursorPos.offset);
        const QString after = currentContent.mid(cursorPos.offset);

        m_buffer->setParagraphText(
            static_cast<size_t>(cursorPos.paragraph),
            before + lines.first());

        for (int i = 1; i < lines.size() - 1; ++i) {
            m_buffer->insertParagraph(
                static_cast<size_t>(cursorPos.paragraph) + static_cast<size_t>(i),
                lines[i]);
        }

        const size_t lastParaIndex = static_cast<size_t>(cursorPos.paragraph) +
                                     static_cast<size_t>(lines.size()) - 1;
        m_buffer->insertParagraph(lastParaIndex, lines.last() + after);
    } else {
        const QString currentContent = m_buffer->paragraphText(
            static_cast<size_t>(cursorPos.paragraph));
        const QString newContent = currentContent.left(cursorPos.offset) +
                                   m_replacementText +
                                   currentContent.mid(cursorPos.offset);
        m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
    }

    // Notify format layer about text insertion
    if (m_formatLayer) {
        m_formatLayer->onTextInserted(m_position, static_cast<size_t>(m_replacementText.length()));
    }

    // Notify metadata layer about text insertion
    if (m_metadataLayer) {
        m_metadataLayer->onTextInserted(m_position, static_cast<size_t>(m_replacementText.length()));
    }
}

int TextReplaceCommand::id() const
{
    return static_cast<int>(BufferCommandId::TextReplace);
}

// =============================================================================
// ReplaceAllCommand
// =============================================================================

ReplaceAllCommand::ReplaceAllCommand(TextBuffer* buffer,
                                     FormatLayer* formatLayer,
                                     MetadataLayer* metadataLayer,
                                     const CursorPosition& cursorBefore,
                                     const std::vector<Replacement>& replacements)
    : BufferCommand(buffer, formatLayer, metadataLayer, cursorBefore, QObject::tr("Replace All"))
    , m_replacements(replacements)
{
    // Sort replacements by position in descending order (highest first)
    // This ensures positions remain valid during replacement
    std::sort(m_replacements.begin(), m_replacements.end(),
              [](const Replacement& a, const Replacement& b) {
                  return a.position > b.position;
              });

    // Cursor stays at original position after replace all
    m_cursorAfter = cursorBefore;
}

void ReplaceAllCommand::undo()
{
    if (!m_buffer) {
        return;
    }

    // Process in forward order (lowest position first) for undo
    // Since m_replacements is sorted descending, iterate in reverse
    for (auto it = m_replacements.rbegin(); it != m_replacements.rend(); ++it) {
        const Replacement& repl = *it;

        // Convert absolute position to cursor position
        CursorPosition cursorPos = absoluteToCursorPosition(*m_buffer, repl.position);

        // Delete the replacement text
        if (repl.replacementText.contains('\n')) {
            const int newlineCount = repl.replacementText.count('\n');

            for (int i = newlineCount; i > 0; --i) {
                const size_t paraToMerge = static_cast<size_t>(cursorPos.paragraph) + 1;
                if (paraToMerge < m_buffer->paragraphCount()) {
                    const QString nextContent = m_buffer->paragraphText(paraToMerge);
                    m_buffer->removeParagraph(paraToMerge);

                    const size_t currentPara = static_cast<size_t>(cursorPos.paragraph);
                    const QString currentContent = m_buffer->paragraphText(currentPara);
                    m_buffer->setParagraphText(currentPara, currentContent + nextContent);
                }
            }

            const QStringList lines = repl.replacementText.split('\n');
            const QString firstLine = lines.first();
            const QString currentContent = m_buffer->paragraphText(
                static_cast<size_t>(cursorPos.paragraph));

            QString newContent = currentContent;
            newContent.remove(cursorPos.offset, firstLine.length());
            m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
        } else {
            const QString currentContent = m_buffer->paragraphText(
                static_cast<size_t>(cursorPos.paragraph));
            QString newContent = currentContent;
            newContent.remove(cursorPos.offset, repl.replacementText.length());
            m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
        }

        // Notify format layer about deletion
        if (m_formatLayer) {
            m_formatLayer->onTextDeleted(repl.position,
                static_cast<size_t>(repl.replacementText.length()));
        }

        // Notify metadata layer about deletion
        if (m_metadataLayer) {
            m_metadataLayer->onTextDeleted(repl.position,
                static_cast<size_t>(repl.replacementText.length()));
        }

        // Insert the original text back
        if (repl.originalText.contains('\n')) {
            const QStringList lines = repl.originalText.split('\n');

            const QString currentContent = m_buffer->paragraphText(
                static_cast<size_t>(cursorPos.paragraph));

            const QString before = currentContent.left(cursorPos.offset);
            const QString after = currentContent.mid(cursorPos.offset);

            m_buffer->setParagraphText(
                static_cast<size_t>(cursorPos.paragraph),
                before + lines.first());

            for (int i = 1; i < lines.size() - 1; ++i) {
                m_buffer->insertParagraph(
                    static_cast<size_t>(cursorPos.paragraph) + static_cast<size_t>(i),
                    lines[i]);
            }

            const size_t lastParaIndex = static_cast<size_t>(cursorPos.paragraph) +
                                         static_cast<size_t>(lines.size()) - 1;
            m_buffer->insertParagraph(lastParaIndex, lines.last() + after);
        } else {
            const QString currentContent = m_buffer->paragraphText(
                static_cast<size_t>(cursorPos.paragraph));
            const QString newContent = currentContent.left(cursorPos.offset) +
                                       repl.originalText +
                                       currentContent.mid(cursorPos.offset);
            m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
        }

        // Notify format layer and restore formats
        if (m_formatLayer) {
            m_formatLayer->onTextInserted(repl.position,
                static_cast<size_t>(repl.originalText.length()));

            for (const auto& range : repl.formats) {
                m_formatLayer->addFormat(range.start, range.end, range.format);
            }
        }

        // Notify metadata layer about insertion
        if (m_metadataLayer) {
            m_metadataLayer->onTextInserted(repl.position,
                static_cast<size_t>(repl.originalText.length()));
        }
    }
}

void ReplaceAllCommand::redo()
{
    if (!m_buffer) {
        return;
    }

    // Process in descending position order (already sorted)
    for (const Replacement& repl : m_replacements) {
        // Convert absolute position to cursor position
        CursorPosition cursorPos = absoluteToCursorPosition(*m_buffer, repl.position);

        // Delete the original text
        if (repl.originalText.contains('\n')) {
            const int newlineCount = repl.originalText.count('\n');

            for (int i = newlineCount; i > 0; --i) {
                const size_t paraToMerge = static_cast<size_t>(cursorPos.paragraph) + 1;
                if (paraToMerge < m_buffer->paragraphCount()) {
                    const QString nextContent = m_buffer->paragraphText(paraToMerge);
                    m_buffer->removeParagraph(paraToMerge);

                    const size_t currentPara = static_cast<size_t>(cursorPos.paragraph);
                    const QString currentContent = m_buffer->paragraphText(currentPara);
                    m_buffer->setParagraphText(currentPara, currentContent + nextContent);
                }
            }

            const QStringList lines = repl.originalText.split('\n');
            const QString firstLine = lines.first();
            const QString currentContent = m_buffer->paragraphText(
                static_cast<size_t>(cursorPos.paragraph));

            QString newContent = currentContent;
            newContent.remove(cursorPos.offset, firstLine.length());
            m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
        } else {
            const QString currentContent = m_buffer->paragraphText(
                static_cast<size_t>(cursorPos.paragraph));
            QString newContent = currentContent;
            newContent.remove(cursorPos.offset, repl.originalText.length());
            m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
        }

        // Notify format layer about deletion
        if (m_formatLayer) {
            m_formatLayer->onTextDeleted(repl.position,
                static_cast<size_t>(repl.originalText.length()));
        }

        // Notify metadata layer about deletion
        if (m_metadataLayer) {
            m_metadataLayer->onTextDeleted(repl.position,
                static_cast<size_t>(repl.originalText.length()));
        }

        // Insert the replacement text
        if (repl.replacementText.contains('\n')) {
            const QStringList lines = repl.replacementText.split('\n');

            const QString currentContent = m_buffer->paragraphText(
                static_cast<size_t>(cursorPos.paragraph));

            const QString before = currentContent.left(cursorPos.offset);
            const QString after = currentContent.mid(cursorPos.offset);

            m_buffer->setParagraphText(
                static_cast<size_t>(cursorPos.paragraph),
                before + lines.first());

            for (int i = 1; i < lines.size() - 1; ++i) {
                m_buffer->insertParagraph(
                    static_cast<size_t>(cursorPos.paragraph) + static_cast<size_t>(i),
                    lines[i]);
            }

            const size_t lastParaIndex = static_cast<size_t>(cursorPos.paragraph) +
                                         static_cast<size_t>(lines.size()) - 1;
            m_buffer->insertParagraph(lastParaIndex, lines.last() + after);
        } else {
            const QString currentContent = m_buffer->paragraphText(
                static_cast<size_t>(cursorPos.paragraph));
            const QString newContent = currentContent.left(cursorPos.offset) +
                                       repl.replacementText +
                                       currentContent.mid(cursorPos.offset);
            m_buffer->setParagraphText(static_cast<size_t>(cursorPos.paragraph), newContent);
        }

        // Notify format layer about insertion
        if (m_formatLayer) {
            m_formatLayer->onTextInserted(repl.position,
                static_cast<size_t>(repl.replacementText.length()));
        }

        // Notify metadata layer about insertion
        if (m_metadataLayer) {
            m_metadataLayer->onTextInserted(repl.position,
                static_cast<size_t>(repl.replacementText.length()));
        }
    }
}

int ReplaceAllCommand::id() const
{
    return static_cast<int>(BufferCommandId::ReplaceAll);
}

// =============================================================================
// MarkerAddCommand
// =============================================================================

MarkerAddCommand::MarkerAddCommand(TextBuffer* buffer,
                                   FormatLayer* formatLayer,
                                   MetadataLayer* metadataLayer,
                                   const CursorPosition& cursorBefore,
                                   const TextTodo& marker)
    : BufferCommand(buffer, formatLayer, metadataLayer, cursorBefore,
                    marker.type == MarkerType::Todo ? QObject::tr("Add TODO") : QObject::tr("Add Note"))
    , m_marker(marker)
{
    // Cursor stays at current position after adding marker
    m_cursorAfter = cursorBefore;
}

void MarkerAddCommand::redo()
{
    if (m_metadataLayer) {
        m_metadataLayer->addTodo(m_marker);
    }
}

void MarkerAddCommand::undo()
{
    if (m_metadataLayer) {
        m_metadataLayer->removeTodo(m_marker.id);
    }
}

int MarkerAddCommand::id() const
{
    return static_cast<int>(BufferCommandId::MarkerAdd);
}

// =============================================================================
// MarkerRemoveCommand
// =============================================================================

MarkerRemoveCommand::MarkerRemoveCommand(TextBuffer* buffer,
                                         FormatLayer* formatLayer,
                                         MetadataLayer* metadataLayer,
                                         const CursorPosition& cursorBefore,
                                         const TextTodo& marker)
    : BufferCommand(buffer, formatLayer, metadataLayer, cursorBefore,
                    marker.type == MarkerType::Todo ? QObject::tr("Remove TODO") : QObject::tr("Remove Note"))
    , m_marker(marker)
{
    // Cursor stays at current position after removing marker
    m_cursorAfter = cursorBefore;
}

void MarkerRemoveCommand::redo()
{
    if (m_metadataLayer) {
        m_metadataLayer->removeTodo(m_marker.id);
    }
}

void MarkerRemoveCommand::undo()
{
    if (m_metadataLayer) {
        m_metadataLayer->addTodo(m_marker);
    }
}

int MarkerRemoveCommand::id() const
{
    return static_cast<int>(BufferCommandId::MarkerRemove);
}

// =============================================================================
// MarkerToggleCommand
// =============================================================================

MarkerToggleCommand::MarkerToggleCommand(TextBuffer* buffer,
                                         FormatLayer* formatLayer,
                                         MetadataLayer* metadataLayer,
                                         const CursorPosition& cursorBefore,
                                         const QString& markerId)
    : BufferCommand(buffer, formatLayer, metadataLayer, cursorBefore, QObject::tr("Toggle TODO"))
    , m_markerId(markerId)
{
    // Cursor stays at current position after toggling
    m_cursorAfter = cursorBefore;
}

void MarkerToggleCommand::redo()
{
    if (m_metadataLayer) {
        m_metadataLayer->toggleTodoCompleted(m_markerId);
    }
}

void MarkerToggleCommand::undo()
{
    // Toggle again to restore previous state
    if (m_metadataLayer) {
        m_metadataLayer->toggleTodoCompleted(m_markerId);
    }
}

int MarkerToggleCommand::id() const
{
    return static_cast<int>(BufferCommandId::MarkerToggle);
}

}  // namespace kalahari::editor
