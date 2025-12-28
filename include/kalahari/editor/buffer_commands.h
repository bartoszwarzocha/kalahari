/// @file buffer_commands.h
/// @brief Undo/Redo command classes for TextBuffer/FormatLayer (OpenSpec #00043 Phase 9)
///
/// This header provides QUndoCommand-based classes for implementing undo/redo
/// functionality using the new TextBuffer + FormatLayer architecture.
/// These commands operate on the performance-optimized data structures.

#pragma once

#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/format_layer.h>
#include <kalahari/editor/kml_converter.h>  // For TextTodo, MetadataLayer, MarkerType
#include <QUndoCommand>
#include <QString>
#include <chrono>
#include <memory>
#include <vector>

namespace kalahari::editor {

class TextBuffer;
class FormatLayer;
class MetadataLayer;

// =============================================================================
// Command IDs for merging consecutive commands
// =============================================================================

/// @brief Command IDs for buffer commands (starting at 2000 to avoid conflicts)
///
/// These IDs enable Qt's command merging feature for coalescing consecutive
/// similar operations into a single undo step.
enum class BufferCommandId {
    TextInsert = 2000,      ///< Character/text insertion
    TextDelete = 2001,      ///< Text deletion
    ParagraphSplit = 2002,  ///< Paragraph split (Enter key)
    ParagraphMerge = 2003,  ///< Paragraph merge (Backspace at start)
    FormatApply = 2004,     ///< Apply formatting to range
    FormatRemove = 2005,    ///< Remove formatting from range
    TextReplace = 2006,     ///< Text replacement (Find & Replace)
    ReplaceAll = 2007,      ///< Replace all matches at once
    MarkerAdd = 2010,       ///< Add TODO/Note marker
    MarkerRemove = 2011,    ///< Remove marker
    MarkerToggle = 2012,    ///< Toggle TODO completion state
};

// =============================================================================
// Helper Functions
// =============================================================================

/// @brief Calculate absolute character position from paragraph + offset
/// @param buffer The text buffer
/// @param paragraphIndex Paragraph index (0-based)
/// @param offset Character offset within paragraph
/// @return Absolute character position in the document (0-based)
size_t calculateAbsolutePosition(const TextBuffer& buffer, int paragraphIndex, int offset);

/// @brief Calculate absolute character position from cursor position
/// @param buffer The text buffer
/// @param pos Cursor position (paragraph + offset)
/// @return Absolute character position in the document (0-based)
size_t calculateAbsolutePosition(const TextBuffer& buffer, const CursorPosition& pos);

/// @brief Convert absolute position to cursor position
/// @param buffer The text buffer
/// @param absolutePos Absolute character offset (0-based)
/// @return Cursor position (paragraph + offset)
CursorPosition absoluteToCursorPosition(const TextBuffer& buffer, size_t absolutePos);

// =============================================================================
// Base Command
// =============================================================================

/// @brief Base class for all buffer editing commands
///
/// Provides common functionality for tracking cursor positions and
/// buffer/format layer references. Unlike KmlCommand, this works with
/// the new performance-optimized TextBuffer and FormatLayer.
class BufferCommand : public QUndoCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer being edited
    /// @param formatLayer The format layer for the buffer
    /// @param metadataLayer The metadata layer for comments/TODOs (can be nullptr)
    /// @param cursorBefore Cursor position before the command
    /// @param text Description text for the undo stack
    BufferCommand(TextBuffer* buffer,
                  FormatLayer* formatLayer,
                  MetadataLayer* metadataLayer,
                  const CursorPosition& cursorBefore,
                  const QString& text);

    /// @brief Virtual destructor
    ~BufferCommand() override = default;

    /// @brief Get cursor position before command execution
    /// @return Cursor position before this command
    CursorPosition cursorBefore() const { return m_cursorBefore; }

    /// @brief Get cursor position after command execution
    /// @return Cursor position after this command
    CursorPosition cursorAfter() const { return m_cursorAfter; }

protected:
    TextBuffer* m_buffer;              ///< Text buffer being edited (not owned)
    FormatLayer* m_formatLayer;        ///< Format layer (not owned)
    MetadataLayer* m_metadataLayer;    ///< Metadata layer for comments/TODOs (not owned)
    CursorPosition m_cursorBefore;     ///< Cursor position before command
    CursorPosition m_cursorAfter;      ///< Cursor position after command
};

// =============================================================================
// Task 9.1: Text Buffer Operations
// =============================================================================

/// @brief Command for inserting text at cursor position
///
/// Supports merging consecutive typing into a single undo step.
/// Merging is limited by:
/// - Time window (1 second between keystrokes)
/// - Paragraph boundaries (no merging across newlines)
/// - Maximum merge length (100 characters)
class TextInsertCommand : public BufferCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer being edited
    /// @param formatLayer The format layer for the buffer
    /// @param metadataLayer The metadata layer for comments/TODOs (can be nullptr)
    /// @param position Position where text is inserted
    /// @param text The text to insert
    TextInsertCommand(TextBuffer* buffer,
                      FormatLayer* formatLayer,
                      MetadataLayer* metadataLayer,
                      const CursorPosition& position,
                      const QString& text);

    /// @brief Undo the text insertion
    void undo() override;

    /// @brief Redo the text insertion
    void redo() override;

    /// @brief Get command ID for merging
    /// @return Command ID (BufferCommandId::TextInsert)
    int id() const override;

    /// @brief Merge with another insert command
    /// @param other The command to merge with
    /// @return true if merged successfully
    bool mergeWith(const QUndoCommand* other) override;

private:
    CursorPosition m_insertPosition;   ///< Position where text was inserted
    QString m_text;                    ///< The inserted text
    std::chrono::steady_clock::time_point m_timestamp;  ///< For merge timing

    static constexpr int MERGE_WINDOW_MS = 1000;  ///< Max ms between merges
    static constexpr int MAX_MERGE_LENGTH = 100;  ///< Max characters to merge
};

/// @brief Command for deleting text in a range
///
/// Stores the deleted text and any format ranges that were affected
/// for proper restoration on undo.
class TextDeleteCommand : public BufferCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer being edited
    /// @param formatLayer The format layer for the buffer
    /// @param metadataLayer The metadata layer for comments/TODOs (can be nullptr)
    /// @param start Start of deletion range
    /// @param end End of deletion range
    /// @param deletedText The text that was deleted
    /// @param deletedFormats Format ranges that were in the deleted area
    TextDeleteCommand(TextBuffer* buffer,
                      FormatLayer* formatLayer,
                      MetadataLayer* metadataLayer,
                      const CursorPosition& start,
                      const CursorPosition& end,
                      const QString& deletedText,
                      const std::vector<FormatRange>& deletedFormats);

    /// @brief Undo the text deletion (restore text and formats)
    void undo() override;

    /// @brief Redo the text deletion
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::TextDelete)
    int id() const override;

private:
    CursorPosition m_start;            ///< Start of deletion range
    CursorPosition m_end;              ///< End of deletion range
    QString m_deletedText;             ///< Plain text that was deleted
    std::vector<FormatRange> m_deletedFormats;  ///< Formats for restoration
};

/// @brief Command for splitting a paragraph (Enter key)
///
/// Stores any formats that were split between the two paragraphs
/// for proper restoration on undo.
class ParagraphSplitCommand : public BufferCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer being edited
    /// @param formatLayer The format layer for the buffer
    /// @param metadataLayer The metadata layer for comments/TODOs (can be nullptr)
    /// @param position Position where paragraph is split
    /// @param movedFormats Format ranges that moved to the new paragraph
    ParagraphSplitCommand(TextBuffer* buffer,
                          FormatLayer* formatLayer,
                          MetadataLayer* metadataLayer,
                          const CursorPosition& position,
                          const std::vector<FormatRange>& movedFormats = {});

    /// @brief Undo the paragraph split (merge back)
    void undo() override;

    /// @brief Redo the paragraph split
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::ParagraphSplit)
    int id() const override;

private:
    CursorPosition m_splitPosition;    ///< Where the split occurred
    std::vector<FormatRange> m_movedFormats;  ///< Formats moved to new paragraph
};

/// @brief Command for merging paragraphs (Backspace at start or Delete at end)
///
/// Stores the merged paragraph content and formats for restoration on undo.
class ParagraphMergeCommand : public BufferCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer being edited
    /// @param formatLayer The format layer for the buffer
    /// @param metadataLayer The metadata layer for comments/TODOs (can be nullptr)
    /// @param cursorPos Cursor position before merge
    /// @param mergeFromIndex Index of paragraph being merged (absorbed)
    /// @param mergedContent Content of the merged paragraph
    /// @param mergedFormats Formats from the merged paragraph
    ParagraphMergeCommand(TextBuffer* buffer,
                          FormatLayer* formatLayer,
                          MetadataLayer* metadataLayer,
                          const CursorPosition& cursorPos,
                          int mergeFromIndex,
                          const QString& mergedContent,
                          const std::vector<FormatRange>& mergedFormats);

    /// @brief Undo the paragraph merge (split apart)
    void undo() override;

    /// @brief Redo the paragraph merge
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::ParagraphMerge)
    int id() const override;

private:
    int m_mergeFromIndex;              ///< Index of paragraph that was merged
    QString m_mergedContent;           ///< Content of merged paragraph
    std::vector<FormatRange> m_mergedFormats;  ///< Formats from merged paragraph
    int m_splitOffset;                 ///< Offset where paragraphs were joined
};

/// @brief Command for grouping multiple buffer commands
///
/// Allows multiple operations to be undone/redone as a single unit.
/// Child commands are executed in forward order for redo,
/// reverse order for undo.
class CompositeBufferCommand : public BufferCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer being edited
    /// @param formatLayer The format layer for the buffer
    /// @param metadataLayer The metadata layer for comments/TODOs (can be nullptr)
    /// @param cursorBefore Cursor position before the composite operation
    /// @param text Description text for the undo stack
    CompositeBufferCommand(TextBuffer* buffer,
                           FormatLayer* formatLayer,
                           MetadataLayer* metadataLayer,
                           const CursorPosition& cursorBefore,
                           const QString& text);

    /// @brief Destructor
    ~CompositeBufferCommand() override;

    /// @brief Add a child command
    /// @param command The command to add (takes ownership)
    void addCommand(std::unique_ptr<BufferCommand> command);

    /// @brief Get number of child commands
    /// @return Number of child commands
    size_t commandCount() const { return m_commands.size(); }

    /// @brief Undo all child commands in reverse order
    void undo() override;

    /// @brief Redo all child commands in forward order
    void redo() override;

private:
    std::vector<std::unique_ptr<BufferCommand>> m_commands;  ///< Child commands
};

// =============================================================================
// Task 9.2: Format Operations
// =============================================================================

/// @brief Command for applying formatting to a range
///
/// Stores the previous format state for the affected range
/// to enable proper restoration on undo.
class FormatApplyCommand : public BufferCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer (for position calculation)
    /// @param formatLayer The format layer to modify
    /// @param metadataLayer The metadata layer for comments/TODOs (can be nullptr)
    /// @param start Start of formatting range (cursor position)
    /// @param end End of formatting range (cursor position)
    /// @param format The format to apply
    /// @param previousFormats Previous format ranges in the affected area
    FormatApplyCommand(TextBuffer* buffer,
                       FormatLayer* formatLayer,
                       MetadataLayer* metadataLayer,
                       const CursorPosition& start,
                       const CursorPosition& end,
                       const TextFormat& format,
                       const std::vector<FormatRange>& previousFormats);

    /// @brief Undo format application (restore previous formats)
    void undo() override;

    /// @brief Redo format application
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::FormatApply)
    int id() const override;

private:
    CursorPosition m_start;            ///< Start of format range
    CursorPosition m_end;              ///< End of format range
    TextFormat m_format;               ///< Format that was applied
    std::vector<FormatRange> m_previousFormats;  ///< Previous state for undo
};

/// @brief Command for removing formatting from a range
///
/// Stores the removed format ranges for restoration on undo.
class FormatRemoveCommand : public BufferCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer (for position calculation)
    /// @param formatLayer The format layer to modify
    /// @param metadataLayer The metadata layer for comments/TODOs (can be nullptr)
    /// @param start Start of range to clear
    /// @param end End of range to clear
    /// @param formatType Type of format to remove (or None for all)
    /// @param removedFormats Format ranges that were removed
    FormatRemoveCommand(TextBuffer* buffer,
                        FormatLayer* formatLayer,
                        MetadataLayer* metadataLayer,
                        const CursorPosition& start,
                        const CursorPosition& end,
                        FormatType formatType,
                        const std::vector<FormatRange>& removedFormats);

    /// @brief Undo format removal (restore removed formats)
    void undo() override;

    /// @brief Redo format removal
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::FormatRemove)
    int id() const override;

private:
    CursorPosition m_start;            ///< Start of cleared range
    CursorPosition m_end;              ///< End of cleared range
    FormatType m_formatType;           ///< Type that was removed
    std::vector<FormatRange> m_removedFormats;  ///< Removed formats for undo
};

// =============================================================================
// Task 9.5: Replace Operations
// =============================================================================

/// @brief Command for replacing text (Find & Replace single match)
///
/// Performs a text replacement at a specific position, storing the original
/// text and formats for undo. This is used for single "Replace" operations.
class TextReplaceCommand : public BufferCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer being edited
    /// @param formatLayer The format layer for the buffer
    /// @param metadataLayer The metadata layer for comments/TODOs (can be nullptr)
    /// @param cursorBefore Cursor position before the command
    /// @param position Absolute character position of the replacement
    /// @param originalText The text being replaced
    /// @param replacementText The new text to insert
    TextReplaceCommand(TextBuffer* buffer,
                       FormatLayer* formatLayer,
                       MetadataLayer* metadataLayer,
                       const CursorPosition& cursorBefore,
                       size_t position,
                       const QString& originalText,
                       const QString& replacementText);

    /// @brief Undo the replacement (restore original text)
    void undo() override;

    /// @brief Redo the replacement
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::TextReplace)
    int id() const override;

private:
    size_t m_position;                        ///< Absolute position of replacement
    QString m_originalText;                   ///< Original text that was replaced
    QString m_replacementText;                ///< New text that replaced the original
    std::vector<FormatRange> m_originalFormats;  ///< Formats for restoration on undo
};

/// @brief Command for replacing all matches at once (Find & Replace All)
///
/// Performs multiple text replacements in a single undoable operation.
/// Replacements are processed in reverse order (highest position first)
/// to maintain position validity during the operation.
class ReplaceAllCommand : public BufferCommand {
public:
    /// @brief Single replacement data
    struct Replacement {
        size_t position;                     ///< Absolute character position
        QString originalText;                ///< Text being replaced
        QString replacementText;             ///< Replacement text
        std::vector<FormatRange> formats;    ///< Original formats for undo
    };

    /// @brief Constructor
    /// @param buffer The text buffer being edited
    /// @param formatLayer The format layer for the buffer
    /// @param metadataLayer The metadata layer for comments/TODOs (can be nullptr)
    /// @param cursorBefore Cursor position before the command
    /// @param replacements List of all replacements to perform
    ReplaceAllCommand(TextBuffer* buffer,
                      FormatLayer* formatLayer,
                      MetadataLayer* metadataLayer,
                      const CursorPosition& cursorBefore,
                      const std::vector<Replacement>& replacements);

    /// @brief Undo all replacements (restore original text)
    void undo() override;

    /// @brief Redo all replacements
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::ReplaceAll)
    int id() const override;

private:
    std::vector<Replacement> m_replacements;  ///< All replacements to perform
};

// =============================================================================
// Task 9.12: Marker Operations
// =============================================================================

/// @brief Command for adding a TODO/Note marker
///
/// Creates a marker at the specified position. The marker can be either
/// a TODO (actionable item) or a Note (informational).
class MarkerAddCommand : public BufferCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer being edited
    /// @param formatLayer The format layer for the buffer
    /// @param metadataLayer The metadata layer for markers
    /// @param cursorBefore Cursor position before the command
    /// @param marker The marker to add
    MarkerAddCommand(TextBuffer* buffer,
                     FormatLayer* formatLayer,
                     MetadataLayer* metadataLayer,
                     const CursorPosition& cursorBefore,
                     const TextTodo& marker);

    /// @brief Undo marker addition (remove the marker)
    void undo() override;

    /// @brief Redo marker addition (add the marker)
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::MarkerAdd)
    int id() const override;

private:
    TextTodo m_marker;  ///< The marker to add/remove
};

/// @brief Command for removing a marker
///
/// Removes a marker from the metadata layer. Stores the full marker
/// data for restoration on undo.
class MarkerRemoveCommand : public BufferCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer being edited
    /// @param formatLayer The format layer for the buffer
    /// @param metadataLayer The metadata layer for markers
    /// @param cursorBefore Cursor position before the command
    /// @param marker The marker to remove (stored for undo)
    MarkerRemoveCommand(TextBuffer* buffer,
                        FormatLayer* formatLayer,
                        MetadataLayer* metadataLayer,
                        const CursorPosition& cursorBefore,
                        const TextTodo& marker);

    /// @brief Undo marker removal (restore the marker)
    void undo() override;

    /// @brief Redo marker removal (remove the marker)
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::MarkerRemove)
    int id() const override;

private:
    TextTodo m_marker;  ///< The marker to remove/restore
};

/// @brief Command for toggling TODO completion state
///
/// Toggles the completed flag of a TODO marker. The toggle operation
/// is its own inverse, so undo simply toggles again.
class MarkerToggleCommand : public BufferCommand {
public:
    /// @brief Constructor
    /// @param buffer The text buffer being edited
    /// @param formatLayer The format layer for the buffer
    /// @param metadataLayer The metadata layer for markers
    /// @param cursorBefore Cursor position before the command
    /// @param markerId ID of the marker to toggle
    MarkerToggleCommand(TextBuffer* buffer,
                        FormatLayer* formatLayer,
                        MetadataLayer* metadataLayer,
                        const CursorPosition& cursorBefore,
                        const QString& markerId);

    /// @brief Undo toggle (toggle again to restore state)
    void undo() override;

    /// @brief Redo toggle
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::MarkerToggle)
    int id() const override;

private:
    QString m_markerId;  ///< ID of the marker to toggle
};

}  // namespace kalahari::editor
