/// @file kml_commands.cpp
/// @brief Undo/Redo command implementations (OpenSpec #00042 Phase 4.8-4.12)

#include <kalahari/editor/kml_commands.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>

namespace kalahari::editor {

// =============================================================================
// Base Command
// =============================================================================

KmlCommand::KmlCommand(KmlDocument* document,
                       const CursorPosition& cursorBefore,
                       const QString& text)
    : QUndoCommand(text)
    , m_document(document)
    , m_cursorBefore(cursorBefore)
    , m_cursorAfter(cursorBefore)
{
}

// =============================================================================
// Insert Text Command
// =============================================================================

InsertTextCommand::InsertTextCommand(KmlDocument* document,
                                     const CursorPosition& position,
                                     const QString& text)
    : KmlCommand(document, position, QObject::tr("Insert \"%1\"").arg(text.left(20)))
    , m_insertPosition(position)
    , m_text(text)
    , m_timestamp(std::chrono::steady_clock::now())
{
    // Calculate cursor after position
    m_cursorAfter = position;
    m_cursorAfter.offset += text.length();
}

void InsertTextCommand::undo()
{
    if (m_document == nullptr || m_text.isEmpty()) {
        return;
    }

    // Delete the inserted text
    CursorPosition deleteEnd = m_insertPosition;
    deleteEnd.offset += m_text.length();
    m_document->deleteText(m_insertPosition, deleteEnd);
}

void InsertTextCommand::redo()
{
    if (m_document == nullptr || m_text.isEmpty()) {
        return;
    }

    // Insert the text
    m_document->insertText(m_insertPosition, m_text);
}

int InsertTextCommand::id() const
{
    return static_cast<int>(CommandId::InsertText);
}

bool InsertTextCommand::mergeWith(const QUndoCommand* other)
{
    if (other->id() != id()) {
        return false;
    }

    const auto* otherInsert = static_cast<const InsertTextCommand*>(other);

    // Don't merge if different paragraphs
    if (otherInsert->m_insertPosition.paragraph != m_insertPosition.paragraph) {
        return false;
    }

    // Check time window
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_timestamp).count();
    if (elapsed > MERGE_WINDOW_MS) {
        return false;
    }

    // Check if the new insert is at the end of our current insert
    CursorPosition expectedPos = m_insertPosition;
    expectedPos.offset += m_text.length();
    if (otherInsert->m_insertPosition != expectedPos) {
        return false;
    }

    // Don't merge if either text contains newlines (those should be separate)
    if (m_text.contains('\n') || otherInsert->m_text.contains('\n')) {
        return false;
    }

    // Merge: append the new text
    m_text += otherInsert->m_text;
    m_cursorAfter = otherInsert->m_cursorAfter;
    m_timestamp = now;

    // Update the description
    setText(QObject::tr("Insert \"%1\"").arg(m_text.left(20)));

    return true;
}

// =============================================================================
// Delete Text Command
// =============================================================================

DeleteTextCommand::DeleteTextCommand(KmlDocument* document,
                                     const CursorPosition& start,
                                     const CursorPosition& end,
                                     const QString& deletedText,
                                     const QString& deletedKml)
    : KmlCommand(document, end, QObject::tr("Delete \"%1\"").arg(deletedText.left(20)))
    , m_start(start)
    , m_end(end)
    , m_deletedText(deletedText)
    , m_deletedKml(deletedKml)
{
    m_cursorAfter = start;
}

void DeleteTextCommand::undo()
{
    if (m_document == nullptr) {
        return;
    }

    // Restore the deleted text
    // For now, just insert plain text - full KML restoration would require
    // more complex logic with KmlParser
    m_document->insertText(m_start, m_deletedText);
}

void DeleteTextCommand::redo()
{
    if (m_document == nullptr) {
        return;
    }

    // Delete the text
    m_document->deleteText(m_start, m_end);
}

int DeleteTextCommand::id() const
{
    return static_cast<int>(CommandId::DeleteText);
}

// =============================================================================
// Apply Style Command
// =============================================================================

ApplyStyleCommand::ApplyStyleCommand(KmlDocument* document,
                                     const SelectionRange& range,
                                     const QString& styleId,
                                     const QString& oldStylesKml)
    : KmlCommand(document, range.start, QObject::tr("Apply style \"%1\"").arg(styleId))
    , m_range(range)
    , m_styleId(styleId)
    , m_oldStylesKml(oldStylesKml)
{
    m_cursorAfter = range.end;
}

void ApplyStyleCommand::undo()
{
    if (m_document == nullptr) {
        return;
    }

    // Restore the original styles - this is a simplified implementation
    // Full implementation would parse m_oldStylesKml and restore formatting
    // For now, we just clear the style
    m_document->applyStyle(m_range, QString());
}

void ApplyStyleCommand::redo()
{
    if (m_document == nullptr) {
        return;
    }

    m_document->applyStyle(m_range, m_styleId);
}

int ApplyStyleCommand::id() const
{
    return static_cast<int>(CommandId::ApplyStyle);
}

// =============================================================================
// Split Paragraph Command
// =============================================================================

SplitParagraphCommand::SplitParagraphCommand(KmlDocument* document,
                                             const CursorPosition& position)
    : KmlCommand(document, position, QObject::tr("New paragraph"))
    , m_splitPosition(position)
{
    m_cursorAfter.paragraph = position.paragraph + 1;
    m_cursorAfter.offset = 0;
}

void SplitParagraphCommand::undo()
{
    if (m_document == nullptr) {
        return;
    }

    // Merge the split paragraphs back together
    m_document->mergeParagraphWithPrevious(m_splitPosition.paragraph + 1);
}

void SplitParagraphCommand::redo()
{
    if (m_document == nullptr) {
        return;
    }

    m_document->splitParagraph(m_splitPosition);
}

// =============================================================================
// Merge Paragraphs Command
// =============================================================================

MergeParagraphsCommand::MergeParagraphsCommand(KmlDocument* document,
                                               const CursorPosition& cursorPos,
                                               int mergeFromIndex)
    : KmlCommand(document, cursorPos, QObject::tr("Merge paragraphs"))
    , m_mergeFromIndex(mergeFromIndex)
    , m_splitOffset(0)
{
    // Store the paragraph that will be merged for undo
    if (document != nullptr && mergeFromIndex > 0) {
        const KmlParagraph* prevPara = document->paragraph(mergeFromIndex - 1);
        if (prevPara != nullptr) {
            m_splitOffset = prevPara->characterCount();
        }

        const KmlParagraph* mergedPara = document->paragraph(mergeFromIndex);
        if (mergedPara != nullptr) {
            m_mergedParagraphKml = mergedPara->toKml();
        }
    }

    m_cursorAfter.paragraph = mergeFromIndex - 1;
    m_cursorAfter.offset = m_splitOffset;
}

void MergeParagraphsCommand::undo()
{
    if (m_document == nullptr) {
        return;
    }

    // Split the paragraph to restore the original two paragraphs
    CursorPosition splitPos;
    splitPos.paragraph = m_mergeFromIndex - 1;
    splitPos.offset = m_splitOffset;
    m_document->splitParagraph(splitPos);
}

void MergeParagraphsCommand::redo()
{
    if (m_document == nullptr) {
        return;
    }

    m_document->mergeParagraphWithPrevious(m_mergeFromIndex);
}

}  // namespace kalahari::editor
