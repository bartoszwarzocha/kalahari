/// @file buffer_commands.cpp
/// @brief Simplified undo/redo commands for QTextDocument (OpenSpec #00043 Phase 11.5)
///
/// This file implements simplified QUndoCommand-based classes that work with
/// QTextDocument's native undo/redo system.

#include <kalahari/editor/buffer_commands.h>
#include <QDateTime>
#include <QStringList>
#include <QTextBlock>
#include <QUuid>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// TextMarker Implementation
// =============================================================================

namespace {

/// Attributes of the KML <todo> tag that TextMarker has fields for
const QStringList& markerFieldAttributes()
{
    static const QStringList attributes = {
        QStringLiteral("id"), QStringLiteral("text"), QStringLiteral("type"),
        QStringLiteral("completed"), QStringLiteral("priority"), QStringLiteral("created")};
    return attributes;
}

}  // anonymous namespace

QVariantMap TextMarker::toVariantMap() const
{
    QVariantMap map = otherAttributes;
    map[QStringLiteral("id")] = id;
    if (!text.isEmpty()) {
        map[QStringLiteral("text")] = text;
    }
    if (type == MarkerType::Note) {
        map[QStringLiteral("type")] = QStringLiteral("note");
    }
    map[QStringLiteral("completed")] = completed;
    if (!priority.isEmpty()) {
        map[QStringLiteral("priority")] = priority;
    }
    if (!timestamp.isEmpty()) {
        map[QStringLiteral("created")] = timestamp;
    }
    return map;
}

std::optional<TextMarker> TextMarker::fromVariant(const QVariant& value)
{
    if (value.typeId() != QMetaType::QVariantMap) {
        return std::nullopt;
    }

    const QVariantMap map = value.toMap();
    TextMarker marker;
    marker.id = map.value(QStringLiteral("id")).toString();
    marker.text = map.value(QStringLiteral("text")).toString();
    marker.type = (map.value(QStringLiteral("type")).toString() == QStringLiteral("note"))
        ? MarkerType::Note
        : MarkerType::Todo;
    marker.completed = map.value(QStringLiteral("completed")).toBool();
    marker.priority = map.value(QStringLiteral("priority")).toString();
    marker.timestamp = map.value(QStringLiteral("created")).toString();
    marker.otherAttributes = map;
    for (const QString& attribute : markerFieldAttributes()) {
        marker.otherAttributes.remove(attribute);
    }
    return marker;
}

QString TextMarker::generateId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// =============================================================================
// Helper Functions
// =============================================================================

int calculateAbsolutePosition(const QTextDocument* document, int blockNumber, int offset)
{
    if (!document) {
        return 0;
    }

    QTextBlock block = document->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        // Return end of document if block doesn't exist
        return document->characterCount() - 1;
    }

    return block.position() + offset;
}

int calculateAbsolutePosition(const QTextDocument* document, const CursorPosition& pos)
{
    return calculateAbsolutePosition(document, pos.paragraph, pos.offset);
}

CursorPosition absoluteToCursorPosition(const QTextDocument* document, int absolutePos)
{
    if (!document) {
        return CursorPosition{0, 0};
    }

    QTextBlock block = document->findBlock(absolutePos);
    if (!block.isValid()) {
        // Clamp to end of document
        QTextBlock lastBlock = document->lastBlock();
        return CursorPosition{
            lastBlock.blockNumber(),
            lastBlock.length() - 1  // -1 for block separator
        };
    }

    return CursorPosition{
        block.blockNumber(),
        absolutePos - block.position()
    };
}

QTextCursor createCursor(QTextDocument* document, const CursorPosition& pos)
{
    if (!document) {
        return QTextCursor();
    }

    int absPos = calculateAbsolutePosition(document, pos);
    QTextCursor cursor(document);
    cursor.setPosition(absPos);
    return cursor;
}

QTextCursor createCursor(QTextDocument* document, const CursorPosition& start, const CursorPosition& end)
{
    if (!document) {
        return QTextCursor();
    }

    int startPos = calculateAbsolutePosition(document, start);
    int endPos = calculateAbsolutePosition(document, end);

    QTextCursor cursor(document);
    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);
    return cursor;
}

// =============================================================================
// DocumentCommand Base Class
// =============================================================================

DocumentCommand::DocumentCommand(QTextDocument* document,
                                 const CursorPosition& cursorBefore,
                                 const QString& text)
    : QUndoCommand(text)
    , m_document(document)
    , m_cursorBefore(cursorBefore)
    , m_cursorAfter(cursorBefore)
{
}

// =============================================================================
// MarkerAddCommand
// =============================================================================

MarkerAddCommand::MarkerAddCommand(QTextDocument* document,
                                   const CursorPosition& cursorBefore,
                                   const TextMarker& marker)
    : DocumentCommand(document, cursorBefore,
                      marker.type == MarkerType::Todo ? QObject::tr("Add TODO") : QObject::tr("Add Note"))
    , m_marker(marker)
{
    // Cursor stays at current position after adding marker
    m_cursorAfter = cursorBefore;
}

void MarkerAddCommand::undo()
{
    if (!m_document) {
        return;
    }

    // Remove the marker by clearing the property
    removeMarkerFromDocument(m_document, m_marker.position, m_marker.length);
}

void MarkerAddCommand::redo()
{
    if (!m_document) {
        return;
    }

    // Add the marker by setting the property
    setMarkerInDocument(m_document, m_marker);
}

int MarkerAddCommand::id() const
{
    return static_cast<int>(BufferCommandId::MarkerAdd);
}

// =============================================================================
// MarkerRemoveCommand
// =============================================================================

MarkerRemoveCommand::MarkerRemoveCommand(QTextDocument* document,
                                         const CursorPosition& cursorBefore,
                                         const TextMarker& marker)
    : DocumentCommand(document, cursorBefore,
                      marker.type == MarkerType::Todo ? QObject::tr("Remove TODO") : QObject::tr("Remove Note"))
    , m_marker(marker)
{
    // Cursor stays at current position after removing marker
    m_cursorAfter = cursorBefore;
}

void MarkerRemoveCommand::undo()
{
    if (!m_document) {
        return;
    }

    // Restore the marker
    setMarkerInDocument(m_document, m_marker);
}

void MarkerRemoveCommand::redo()
{
    if (!m_document) {
        return;
    }

    // Remove the marker
    removeMarkerFromDocument(m_document, m_marker.position, m_marker.length);
}

int MarkerRemoveCommand::id() const
{
    return static_cast<int>(BufferCommandId::MarkerRemove);
}

// =============================================================================
// MarkerToggleCommand
// =============================================================================

MarkerToggleCommand::MarkerToggleCommand(QTextDocument* document,
                                         const CursorPosition& cursorBefore,
                                         const QString& markerId,
                                         int position)
    : DocumentCommand(document, cursorBefore, QObject::tr("Toggle TODO"))
    , m_markerId(markerId)
    , m_position(position)
{
    // Cursor stays at current position after toggling
    m_cursorAfter = cursorBefore;
}

void MarkerToggleCommand::undo()
{
    // Toggle again to restore previous state
    toggle();
}

void MarkerToggleCommand::redo()
{
    toggle();
}

void MarkerToggleCommand::toggle()
{
    if (!m_document) {
        return;
    }

    QTextCursor cursor(m_document);
    cursor.setPosition(m_position);
    cursor.setPosition(m_position + 1, QTextCursor::KeepAnchor);

    auto markerOpt = TextMarker::fromVariant(cursor.charFormat().property(KmlPropTodo));
    if (!markerOpt) {
        return;  // No marker at this position
    }

    // Toggle the completed state
    TextMarker marker = *markerOpt;
    marker.completed = !marker.completed;

    // Update the marker in the document
    QTextCharFormat newFormat;
    newFormat.setProperty(KmlPropTodo, marker.toVariantMap());
    cursor.mergeCharFormat(newFormat);
}

int MarkerToggleCommand::id() const
{
    return static_cast<int>(BufferCommandId::MarkerToggle);
}

// =============================================================================
// CompositeDocumentCommand
// =============================================================================

CompositeDocumentCommand::CompositeDocumentCommand(QTextDocument* document,
                                                   const CursorPosition& cursorBefore,
                                                   const QString& text)
    : DocumentCommand(document, cursorBefore, text)
{
}

CompositeDocumentCommand::~CompositeDocumentCommand() = default;

void CompositeDocumentCommand::addCommand(std::unique_ptr<DocumentCommand> command)
{
    if (command) {
        // Update cursor after to match the last command
        m_cursorAfter = command->cursorAfter();
        m_commands.push_back(std::move(command));
    }
}

void CompositeDocumentCommand::undo()
{
    // Undo in reverse order
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
        (*it)->undo();
    }
}

void CompositeDocumentCommand::redo()
{
    // Redo in forward order
    for (auto& cmd : m_commands) {
        cmd->redo();
    }
}

// =============================================================================
// Marker Utility Functions
// =============================================================================

std::vector<TextMarker> findAllMarkers(const QTextDocument* document,
                                       std::optional<MarkerType> typeFilter)
{
    std::vector<TextMarker> markers;

    if (!document) {
        return markers;
    }

    // Iterate through all characters in the document
    QTextBlock block = document->begin();
    while (block.isValid()) {
        QTextBlock::iterator it;
        for (it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }

            auto markerOpt = TextMarker::fromVariant(fragment.charFormat().property(KmlPropTodo));
            if (!markerOpt || (typeFilter && markerOpt->type != *typeFilter)) {
                continue;
            }

            TextMarker marker = *markerOpt;
            marker.position = fragment.position();
            marker.length = fragment.length();

            // An anchor split into several fragments (e.g. partly bold) is one marker
            if (!markers.empty() && !marker.id.isEmpty() && markers.back().id == marker.id &&
                markers.back().position + markers.back().length == marker.position) {
                markers.back().length += marker.length;
                continue;
            }
            markers.push_back(marker);
        }
        block = block.next();
    }

    // Sort by position
    std::sort(markers.begin(), markers.end(),
              [](const TextMarker& a, const TextMarker& b) {
                  return a.position < b.position;
              });

    return markers;
}

std::optional<TextMarker> findMarkerById(const QTextDocument* document, const QString& markerId)
{
    auto markers = findAllMarkers(document);

    for (const auto& marker : markers) {
        if (marker.id == markerId) {
            return marker;
        }
    }

    return std::nullopt;
}

std::optional<TextMarker> findNextMarker(const QTextDocument* document,
                                         int fromPosition,
                                         std::optional<MarkerType> typeFilter)
{
    auto markers = findAllMarkers(document, typeFilter);

    for (const auto& marker : markers) {
        if (marker.position > fromPosition) {
            return marker;
        }
    }

    // Wrap around to beginning
    if (!markers.empty()) {
        return markers.front();
    }

    return std::nullopt;
}

std::optional<TextMarker> findPreviousMarker(const QTextDocument* document,
                                             int fromPosition,
                                             std::optional<MarkerType> typeFilter)
{
    auto markers = findAllMarkers(document, typeFilter);

    // Search in reverse order
    for (auto it = markers.rbegin(); it != markers.rend(); ++it) {
        if (it->position < fromPosition) {
            return *it;
        }
    }

    // Wrap around to end
    if (!markers.empty()) {
        return markers.back();
    }

    return std::nullopt;
}

void setMarkerInDocument(QTextDocument* document, const TextMarker& marker)
{
    if (!document) {
        return;
    }

    // Select the anchor text: one character for a new marker, the whole anchor for a
    // marker found in the document (e.g. a <todo> loaded from KML)
    const int lastPosition = document->characterCount() - 1;
    const int start = qBound(0, marker.position, lastPosition);
    const int end = qBound(start, marker.position + qMax(1, marker.length), lastPosition);

    QTextCursor cursor(document);
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);

    // Set the marker property
    QTextCharFormat format;
    format.setProperty(KmlPropTodo, marker.toVariantMap());
    cursor.mergeCharFormat(format);
}

void removeMarkerFromDocument(QTextDocument* document, int position, int length)
{
    if (!document) {
        return;
    }

    // Clear the property fragment by fragment, so each fragment of the anchor text keeps
    // the rest of its formatting (setCharFormat() over the whole range would flatten it).
    // Collect first: changing formats while iterating would invalidate the iterators.
    struct Range {
        int start;
        int end;
        QTextCharFormat format;
    };
    std::vector<Range> ranges;
    const int end = position + qMax(1, length);
    for (QTextBlock block = document->findBlock(position);
         block.isValid() && block.position() < end; block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const int fragmentStart = qMax(fragment.position(), position);
            const int fragmentEnd = qMin(fragment.position() + fragment.length(), end);
            QTextCharFormat format = fragment.charFormat();
            if (fragmentStart < fragmentEnd && format.hasProperty(KmlPropTodo)) {
                format.clearProperty(KmlPropTodo);
                ranges.push_back({fragmentStart, fragmentEnd, format});
            }
        }
    }

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    for (const Range& range : ranges) {
        cursor.setPosition(range.start);
        cursor.setPosition(range.end, QTextCursor::KeepAnchor);
        cursor.setCharFormat(range.format);
    }
    cursor.endEditBlock();
}

// =============================================================================
// TextInsertCommand
// =============================================================================

TextInsertCommand::TextInsertCommand(QTextDocument* document,
                                     const CursorPosition& cursorPos,
                                     const QString& text)
    : DocumentCommand(document, cursorPos, QObject::tr("Insert Text"))
    , m_text(text)
    , m_timestamp(std::chrono::steady_clock::now())
{
    // Calculate cursor position after insert
    int newParagraph = cursorPos.paragraph;
    int newOffset = cursorPos.offset + text.length();

    // Count newlines in inserted text to adjust paragraph
    int newlines = text.count('\n');
    if (newlines > 0) {
        newParagraph += newlines;
        int lastNewline = text.lastIndexOf('\n');
        newOffset = text.length() - lastNewline - 1;
    }

    m_cursorAfter = CursorPosition{newParagraph, newOffset};
}

void TextInsertCommand::undo()
{
    if (!m_document || m_text.isEmpty()) {
        return;
    }

    // Delete the inserted text
    int startPos = calculateAbsolutePosition(m_document, m_cursorBefore);
    QTextCursor cursor(m_document);
    cursor.setPosition(startPos);
    cursor.setPosition(startPos + m_text.length(), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
}

void TextInsertCommand::redo()
{
    if (!m_document || m_text.isEmpty()) {
        return;
    }

    // Insert the text
    QTextCursor cursor = createCursor(m_document, m_cursorBefore);
    cursor.insertText(m_text);
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

    // Don't merge if time gap is too large
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_timestamp).count();
    if (elapsed > MERGE_WINDOW_MS) {
        return false;
    }

    // Don't merge if not consecutive
    if (otherInsert->m_cursorBefore != m_cursorAfter) {
        return false;
    }

    // Merge: append the new text
    m_text += otherInsert->m_text;
    m_cursorAfter = otherInsert->m_cursorAfter;
    m_timestamp = now;

    return true;
}

// =============================================================================
// TextDeleteCommand
// =============================================================================

TextDeleteCommand::TextDeleteCommand(QTextDocument* document,
                                     const CursorPosition& start,
                                     const CursorPosition& end,
                                     const QString& deletedText)
    : DocumentCommand(document, start, QObject::tr("Delete Text"))
    , m_startPos(start)
    , m_endPos(end)
    , m_deletedText(deletedText)
{
    m_cursorAfter = start;  // Cursor moves to start of deletion
}

void TextDeleteCommand::undo()
{
    if (!m_document || m_deletedText.isEmpty()) {
        return;
    }

    // Re-insert the deleted text
    QTextCursor cursor = createCursor(m_document, m_startPos);
    cursor.insertText(m_deletedText);
}

void TextDeleteCommand::redo()
{
    if (!m_document || m_deletedText.isEmpty()) {
        return;
    }

    // Delete the text
    QTextCursor cursor = createCursor(m_document, m_startPos, m_endPos);
    cursor.removeSelectedText();
}

int TextDeleteCommand::id() const
{
    return static_cast<int>(BufferCommandId::TextDelete);
}

// =============================================================================
// ParagraphSplitCommand
// =============================================================================

ParagraphSplitCommand::ParagraphSplitCommand(QTextDocument* document,
                                             const CursorPosition& position)
    : DocumentCommand(document, position, QObject::tr("Split Paragraph"))
    , m_splitPos(position)
{
    m_cursorAfter = CursorPosition{position.paragraph + 1, 0};
}

void ParagraphSplitCommand::undo()
{
    if (!m_document) {
        return;
    }

    // Delete the newline to merge paragraphs back
    // Position is at end of first paragraph (where newline was inserted)
    int absPos = calculateAbsolutePosition(m_document, m_splitPos);
    QTextCursor cursor(m_document);
    cursor.setPosition(absPos);
    cursor.setPosition(absPos + 1, QTextCursor::KeepAnchor);  // Select newline
    cursor.removeSelectedText();
}

void ParagraphSplitCommand::redo()
{
    if (!m_document) {
        return;
    }

    // Insert newline to split paragraph
    QTextCursor cursor = createCursor(m_document, m_splitPos);
    cursor.insertText(QStringLiteral("\n"));
}

int ParagraphSplitCommand::id() const
{
    return static_cast<int>(BufferCommandId::ParagraphSplit);
}

// =============================================================================
// ParagraphMergeCommand
// =============================================================================

ParagraphMergeCommand::ParagraphMergeCommand(QTextDocument* document,
                                             const CursorPosition& cursorPos,
                                             int paragraphIndex,
                                             const QString& mergedContent)
    : DocumentCommand(document, cursorPos, QObject::tr("Merge Paragraphs"))
    , m_paragraphIndex(paragraphIndex)
    , m_mergedContent(mergedContent)
    , m_splitOffset(0)
{
    // Calculate split offset (length of previous paragraph)
    if (document && paragraphIndex > 0) {
        QTextBlock prevBlock = document->findBlockByNumber(paragraphIndex - 1);
        if (prevBlock.isValid()) {
            m_splitOffset = prevBlock.text().length();
        }
    }
    m_cursorAfter = CursorPosition{paragraphIndex - 1, m_splitOffset};
}

void ParagraphMergeCommand::undo()
{
    if (!m_document || m_paragraphIndex <= 0) {
        return;
    }

    // Re-insert newline to split paragraphs
    CursorPosition splitPos{m_paragraphIndex - 1, m_splitOffset};
    QTextCursor cursor = createCursor(m_document, splitPos);
    cursor.insertText(QStringLiteral("\n"));
}

void ParagraphMergeCommand::redo()
{
    if (!m_document || m_paragraphIndex <= 0) {
        return;
    }

    // Delete the newline between paragraphs
    QTextBlock prevBlock = m_document->findBlockByNumber(m_paragraphIndex - 1);
    if (!prevBlock.isValid()) {
        return;
    }

    // Position at end of previous block (where newline is)
    int newlinePos = prevBlock.position() + prevBlock.length() - 1;
    QTextCursor cursor(m_document);
    cursor.setPosition(newlinePos);
    cursor.setPosition(newlinePos + 1, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
}

int ParagraphMergeCommand::id() const
{
    return static_cast<int>(BufferCommandId::ParagraphMerge);
}

// =============================================================================
// FormatApplyCommand
// =============================================================================

FormatApplyCommand::FormatApplyCommand(QTextDocument* document,
                                       const CursorPosition& start,
                                       const CursorPosition& end,
                                       const QTextCharFormat& format)
    : DocumentCommand(document, start, QObject::tr("Apply Format"))
    , m_startPos(start)
    , m_endPos(end)
    , m_format(format)
{
    m_cursorAfter = end;

    // Save previous format for undo
    if (document) {
        QTextCursor cursor = createCursor(document, start, end);
        m_previousFormat = cursor.charFormat();
    }
}

void FormatApplyCommand::undo()
{
    if (!m_document) {
        return;
    }

    // Restore previous format
    QTextCursor cursor = createCursor(m_document, m_startPos, m_endPos);
    cursor.setCharFormat(m_previousFormat);
}

void FormatApplyCommand::redo()
{
    if (!m_document) {
        return;
    }

    // Apply the new format
    QTextCursor cursor = createCursor(m_document, m_startPos, m_endPos);
    cursor.mergeCharFormat(m_format);
}

int FormatApplyCommand::id() const
{
    return static_cast<int>(BufferCommandId::FormatApply);
}

// =============================================================================
// FormatRemoveCommand
// =============================================================================

FormatRemoveCommand::FormatRemoveCommand(QTextDocument* document,
                                         const CursorPosition& start,
                                         const CursorPosition& end)
    : DocumentCommand(document, start, QObject::tr("Remove Format"))
    , m_startPos(start)
    , m_endPos(end)
{
    m_cursorAfter = end;

    // Save previous format for undo
    if (document) {
        QTextCursor cursor = createCursor(document, start, end);
        m_previousFormat = cursor.charFormat();
    }
}

void FormatRemoveCommand::undo()
{
    if (!m_document) {
        return;
    }

    // Restore previous format
    QTextCursor cursor = createCursor(m_document, m_startPos, m_endPos);
    cursor.setCharFormat(m_previousFormat);
}

void FormatRemoveCommand::redo()
{
    if (!m_document) {
        return;
    }

    // Clear formatting
    QTextCursor cursor = createCursor(m_document, m_startPos, m_endPos);
    QTextCharFormat clearFormat;
    cursor.setCharFormat(clearFormat);
}

int FormatRemoveCommand::id() const
{
    return static_cast<int>(BufferCommandId::FormatRemove);
}

// =============================================================================
// TextReplaceCommand
// =============================================================================

TextReplaceCommand::TextReplaceCommand(QTextDocument* document,
                                       const CursorPosition& start,
                                       const CursorPosition& end,
                                       const QString& oldText,
                                       const QString& newText)
    : DocumentCommand(document, start, QObject::tr("Replace"))
    , m_startPos(start)
    , m_endPos(end)
    , m_oldText(oldText)
    , m_newText(newText)
{
    // Calculate cursor after: at end of replaced text
    int newParagraph = start.paragraph;
    int newOffset = start.offset + newText.length();

    int newlines = newText.count('\n');
    if (newlines > 0) {
        newParagraph += newlines;
        int lastNewline = newText.lastIndexOf('\n');
        newOffset = newText.length() - lastNewline - 1;
    }

    m_cursorAfter = CursorPosition{newParagraph, newOffset};
}

void TextReplaceCommand::undo()
{
    if (!m_document) {
        return;
    }

    // Delete the new text and insert the old text
    int startPos = calculateAbsolutePosition(m_document, m_startPos);
    QTextCursor cursor(m_document);
    cursor.setPosition(startPos);
    cursor.setPosition(startPos + m_newText.length(), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(m_oldText);
}

void TextReplaceCommand::redo()
{
    if (!m_document) {
        return;
    }

    // Delete the old text and insert the new text
    QTextCursor cursor = createCursor(m_document, m_startPos, m_endPos);
    cursor.removeSelectedText();
    cursor.insertText(m_newText);
}

int TextReplaceCommand::id() const
{
    return static_cast<int>(BufferCommandId::TextReplace);
}

// =============================================================================
// ReplaceAllCommand
// =============================================================================

ReplaceAllCommand::ReplaceAllCommand(QTextDocument* document,
                                     const CursorPosition& cursorPos,
                                     const std::vector<Replacement>& replacements)
    : DocumentCommand(document, cursorPos, QObject::tr("Replace All"))
    , m_replacements(replacements)
{
    m_cursorAfter = cursorPos;  // Cursor stays in place
}

void ReplaceAllCommand::undo()
{
    if (!m_document || m_replacements.empty()) {
        return;
    }

    // Undo in forward order (from start to end) because after redo, the document
    // positions have changed. By undoing from start to end, each restoration
    // shifts subsequent positions back to their original locations.
    for (const auto& repl : m_replacements) {
        QTextCursor cursor(m_document);
        cursor.setPosition(repl.startPos);
        cursor.setPosition(repl.startPos + repl.newText.length(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(repl.oldText);
    }
}

void ReplaceAllCommand::redo()
{
    if (!m_document || m_replacements.empty()) {
        return;
    }

    // Apply in reverse order (from end to start) to maintain positions
    for (auto it = m_replacements.rbegin(); it != m_replacements.rend(); ++it) {
        const auto& repl = *it;

        QTextCursor cursor(m_document);
        cursor.setPosition(repl.startPos);
        cursor.setPosition(repl.endPos, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(repl.newText);
    }
}

int ReplaceAllCommand::id() const
{
    return static_cast<int>(BufferCommandId::ReplaceAll);
}

}  // namespace kalahari::editor
