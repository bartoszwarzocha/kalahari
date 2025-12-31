/// @file buffer_commands.h
/// @brief Simplified undo/redo commands for QTextDocument (OpenSpec #00043 Phase 11.5)
///
/// This header provides QUndoCommand-based classes that work with QTextDocument.
/// Most text and format operations use QTextDocument's built-in undo/redo via
/// QTextCursor. Custom commands are provided only for marker operations and
/// composite actions that need special handling.
///
/// Key changes from previous version:
/// - TextBuffer, FormatLayer, MetadataLayer removed - use QTextDocument directly
/// - Text insert/delete/split/merge handled by QTextDocument's native undo/redo
/// - Format operations use QTextCursor::mergeCharFormat() with native undo
/// - Marker operations (TODO/Note) stored in QTextCharFormat properties (KmlProp*)

#pragma once

#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kml_format_registry.h>  // For KmlPropertyId
#include <QTextCursor>
#include <QTextDocument>
#include <QTextCharFormat>
#include <QUndoCommand>
#include <QString>
#include <chrono>
#include <memory>
#include <vector>
#include <optional>

namespace kalahari::editor {

// =============================================================================
// Command IDs for merging consecutive commands
// =============================================================================

/// @brief Command IDs for buffer commands (starting at 2000 to avoid conflicts)
///
/// These IDs enable Qt's command merging feature for coalescing consecutive
/// similar operations into a single undo step.
enum class BufferCommandId {
    TextInsert = 2000,      ///< Character/text insertion (unused - native undo)
    TextDelete = 2001,      ///< Text deletion (unused - native undo)
    ParagraphSplit = 2002,  ///< Paragraph split (unused - native undo)
    ParagraphMerge = 2003,  ///< Paragraph merge (unused - native undo)
    FormatApply = 2004,     ///< Apply formatting to range (unused - native undo)
    FormatRemove = 2005,    ///< Remove formatting from range (unused - native undo)
    TextReplace = 2006,     ///< Text replacement (unused - native undo)
    ReplaceAll = 2007,      ///< Replace all matches at once
    MarkerAdd = 2010,       ///< Add TODO/Note marker
    MarkerRemove = 2011,    ///< Remove marker
    MarkerToggle = 2012,    ///< Toggle TODO completion state
};

// =============================================================================
// Marker Types (moved from kml_converter.h to break dependency)
// =============================================================================

/// @brief Type of annotation marker
enum class MarkerType {
    Todo,   ///< Actionable item (checkbox-like)
    Note    ///< Informational annotation
};

/// @brief TODO/Note marker in text
struct TextMarker {
    int position = 0;             ///< Position in document (absolute)
    int length = 1;               ///< Length of marker anchor text
    QString text;                 ///< Marker content/description
    MarkerType type = MarkerType::Todo;  ///< TODO or NOTE
    bool completed = false;       ///< Only meaningful for TODO
    QString priority;             ///< Priority level (high, normal, low)
    QString id;                   ///< Unique identifier (UUID)
    QString timestamp;            ///< Creation timestamp (ISO 8601)

    /// @brief Serialize marker to JSON string for QTextCharFormat property
    QString toJson() const;

    /// @brief Deserialize marker from JSON string
    static std::optional<TextMarker> fromJson(const QString& json);

    /// @brief Generate a new unique marker ID
    static QString generateId();
};

// =============================================================================
// Helper Functions
// =============================================================================

/// @brief Calculate absolute character position from block + offset
/// @param document The text document
/// @param blockNumber Block number (0-based, equivalent to paragraph)
/// @param offset Character offset within block
/// @return Absolute character position in the document (0-based)
int calculateAbsolutePosition(const QTextDocument* document, int blockNumber, int offset);

/// @brief Calculate absolute character position from cursor position
/// @param document The text document
/// @param pos Cursor position (paragraph + offset)
/// @return Absolute character position in the document (0-based)
int calculateAbsolutePosition(const QTextDocument* document, const CursorPosition& pos);

/// @brief Convert absolute position to cursor position
/// @param document The text document
/// @param absolutePos Absolute character offset (0-based)
/// @return Cursor position (paragraph + offset)
CursorPosition absoluteToCursorPosition(const QTextDocument* document, int absolutePos);

/// @brief Create a QTextCursor positioned at the given cursor position
/// @param document The text document
/// @param pos Cursor position (paragraph + offset)
/// @return QTextCursor at the specified position
QTextCursor createCursor(QTextDocument* document, const CursorPosition& pos);

/// @brief Create a QTextCursor with selection from start to end
/// @param document The text document
/// @param start Start cursor position
/// @param end End cursor position
/// @return QTextCursor with selection
QTextCursor createCursor(QTextDocument* document, const CursorPosition& start, const CursorPosition& end);

// =============================================================================
// Base Command (simplified)
// =============================================================================

/// @brief Base class for document editing commands
///
/// Provides common functionality for tracking cursor positions.
/// Most text/format operations should use QTextDocument's native undo instead
/// of deriving from this class.
class DocumentCommand : public QUndoCommand {
public:
    /// @brief Constructor
    /// @param document The text document being edited
    /// @param cursorBefore Cursor position before the command
    /// @param text Description text for the undo stack
    DocumentCommand(QTextDocument* document,
                    const CursorPosition& cursorBefore,
                    const QString& text);

    /// @brief Virtual destructor
    ~DocumentCommand() override = default;

    /// @brief Get cursor position before command execution
    /// @return Cursor position before this command
    CursorPosition cursorBefore() const { return m_cursorBefore; }

    /// @brief Get cursor position after command execution
    /// @return Cursor position after this command
    CursorPosition cursorAfter() const { return m_cursorAfter; }

protected:
    QTextDocument* m_document;         ///< Text document being edited (not owned)
    CursorPosition m_cursorBefore;     ///< Cursor position before command
    CursorPosition m_cursorAfter;      ///< Cursor position after command
};

// =============================================================================
// Marker Commands (for TODO/Note operations)
// =============================================================================

/// @brief Command for adding a TODO/Note marker
///
/// Creates a marker at the specified position by setting a custom property
/// on the QTextCharFormat. The marker data is stored as JSON in the
/// KmlPropTodo property.
class MarkerAddCommand : public DocumentCommand {
public:
    /// @brief Constructor
    /// @param document The text document being edited
    /// @param cursorBefore Cursor position before the command
    /// @param marker The marker to add
    MarkerAddCommand(QTextDocument* document,
                     const CursorPosition& cursorBefore,
                     const TextMarker& marker);

    /// @brief Undo marker addition (remove the marker)
    void undo() override;

    /// @brief Redo marker addition (add the marker)
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::MarkerAdd)
    int id() const override;

private:
    TextMarker m_marker;  ///< The marker to add/remove
    QTextCharFormat m_previousFormat;  ///< Format before marker was added
};

/// @brief Command for removing a marker
///
/// Removes a marker from the document. Stores the full marker data
/// for restoration on undo.
class MarkerRemoveCommand : public DocumentCommand {
public:
    /// @brief Constructor
    /// @param document The text document being edited
    /// @param cursorBefore Cursor position before the command
    /// @param marker The marker to remove (stored for undo)
    MarkerRemoveCommand(QTextDocument* document,
                        const CursorPosition& cursorBefore,
                        const TextMarker& marker);

    /// @brief Undo marker removal (restore the marker)
    void undo() override;

    /// @brief Redo marker removal (remove the marker)
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::MarkerRemove)
    int id() const override;

private:
    TextMarker m_marker;  ///< The marker to remove/restore
};

/// @brief Command for toggling TODO completion state
///
/// Toggles the completed flag of a TODO marker. The toggle operation
/// is its own inverse, so undo simply toggles again.
class MarkerToggleCommand : public DocumentCommand {
public:
    /// @brief Constructor
    /// @param document The text document being edited
    /// @param cursorBefore Cursor position before the command
    /// @param markerId ID of the marker to toggle
    /// @param position Position of the marker in document
    MarkerToggleCommand(QTextDocument* document,
                        const CursorPosition& cursorBefore,
                        const QString& markerId,
                        int position);

    /// @brief Undo toggle (toggle again to restore state)
    void undo() override;

    /// @brief Redo toggle
    void redo() override;

    /// @brief Get command ID
    /// @return Command ID (BufferCommandId::MarkerToggle)
    int id() const override;

private:
    QString m_markerId;  ///< ID of the marker to toggle
    int m_position;      ///< Position of the marker

    /// @brief Toggle the marker's completed state
    void toggle();
};

// =============================================================================
// Composite Command (for grouping multiple operations)
// =============================================================================

/// @brief Command for grouping multiple document commands
///
/// Allows multiple operations to be undone/redone as a single unit.
/// Useful for complex operations like Replace All that modify multiple
/// locations in the document.
class CompositeDocumentCommand : public DocumentCommand {
public:
    /// @brief Constructor
    /// @param document The text document being edited
    /// @param cursorBefore Cursor position before the composite operation
    /// @param text Description text for the undo stack
    CompositeDocumentCommand(QTextDocument* document,
                             const CursorPosition& cursorBefore,
                             const QString& text);

    /// @brief Destructor
    ~CompositeDocumentCommand() override;

    /// @brief Add a child command
    /// @param command The command to add (takes ownership)
    void addCommand(std::unique_ptr<DocumentCommand> command);

    /// @brief Get number of child commands
    /// @return Number of child commands
    size_t commandCount() const { return m_commands.size(); }

    /// @brief Undo all child commands in reverse order
    void undo() override;

    /// @brief Redo all child commands in forward order
    void redo() override;

private:
    std::vector<std::unique_ptr<DocumentCommand>> m_commands;  ///< Child commands
};

// =============================================================================
// Marker Utility Functions
// =============================================================================

/// @brief Find all markers in a document
/// @param document The text document to search
/// @param typeFilter Optional filter by marker type
/// @return Vector of all markers found
std::vector<TextMarker> findAllMarkers(const QTextDocument* document,
                                       std::optional<MarkerType> typeFilter = std::nullopt);

/// @brief Find a marker by ID
/// @param document The text document to search
/// @param markerId The marker ID to find
/// @return The marker if found, nullopt otherwise
std::optional<TextMarker> findMarkerById(const QTextDocument* document, const QString& markerId);

/// @brief Find the next marker from a position
/// @param document The text document to search
/// @param fromPosition Position to search from
/// @param typeFilter Optional filter by marker type
/// @return The next marker if found, nullopt otherwise
std::optional<TextMarker> findNextMarker(const QTextDocument* document,
                                         int fromPosition,
                                         std::optional<MarkerType> typeFilter = std::nullopt);

/// @brief Find the previous marker from a position
/// @param document The text document to search
/// @param fromPosition Position to search from
/// @param typeFilter Optional filter by marker type
/// @return The previous marker if found, nullopt otherwise
std::optional<TextMarker> findPreviousMarker(const QTextDocument* document,
                                             int fromPosition,
                                             std::optional<MarkerType> typeFilter = std::nullopt);

/// @brief Set marker at position in document
/// @param document The text document
/// @param marker The marker to set
/// @note This modifies the character format at the marker position
void setMarkerInDocument(QTextDocument* document, const TextMarker& marker);

/// @brief Remove marker from document
/// @param document The text document
/// @param position Position of the marker
/// @note This clears the KmlPropTodo property at the position
void removeMarkerFromDocument(QTextDocument* document, int position);

// =============================================================================
// Text Editing Commands (QTextDocument-based)
// =============================================================================

/// @brief Command for inserting text at a position
///
/// Uses QTextCursor to insert text. Stores the inserted text for undo.
class TextInsertCommand : public DocumentCommand {
public:
    /// @brief Constructor
    /// @param document The text document
    /// @param cursorPos Position to insert at
    /// @param text Text to insert
    TextInsertCommand(QTextDocument* document,
                      const CursorPosition& cursorPos,
                      const QString& text);

    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    QString m_text;                ///< Text to insert
    std::chrono::steady_clock::time_point m_timestamp;  ///< For merge timing
    static constexpr int MERGE_WINDOW_MS = 1000;  ///< Merge window in milliseconds
};

/// @brief Command for deleting text in a range
///
/// Uses QTextCursor to delete text. Stores deleted text for undo.
class TextDeleteCommand : public DocumentCommand {
public:
    /// @brief Constructor
    /// @param document The text document
    /// @param start Start position
    /// @param end End position
    /// @param deletedText The text being deleted (for undo)
    TextDeleteCommand(QTextDocument* document,
                      const CursorPosition& start,
                      const CursorPosition& end,
                      const QString& deletedText);

    void undo() override;
    void redo() override;
    int id() const override;

private:
    CursorPosition m_startPos;     ///< Start of deleted range
    CursorPosition m_endPos;       ///< End of deleted range
    QString m_deletedText;         ///< Text that was deleted
};

/// @brief Command for splitting a paragraph (inserting newline)
class ParagraphSplitCommand : public DocumentCommand {
public:
    /// @brief Constructor
    /// @param document The text document
    /// @param position Position to split at
    ParagraphSplitCommand(QTextDocument* document,
                          const CursorPosition& position);

    void undo() override;
    void redo() override;
    int id() const override;

private:
    CursorPosition m_splitPos;     ///< Position where split occurs
};

/// @brief Command for merging two paragraphs (deleting newline)
class ParagraphMergeCommand : public DocumentCommand {
public:
    /// @brief Constructor
    /// @param document The text document
    /// @param cursorPos Cursor position (at start of second paragraph)
    /// @param paragraphIndex The paragraph being merged (1-based, merged into previous)
    /// @param mergedContent Content of the paragraph being merged
    ParagraphMergeCommand(QTextDocument* document,
                          const CursorPosition& cursorPos,
                          int paragraphIndex,
                          const QString& mergedContent);

    void undo() override;
    void redo() override;
    int id() const override;

private:
    int m_paragraphIndex;          ///< Paragraph index being merged
    QString m_mergedContent;       ///< Content of merged paragraph
    int m_splitOffset;             ///< Offset where split was in previous paragraph
};

/// @brief Command for applying text formatting
class FormatApplyCommand : public DocumentCommand {
public:
    /// @brief Constructor
    /// @param document The text document
    /// @param start Start position
    /// @param end End position
    /// @param format The format to apply
    FormatApplyCommand(QTextDocument* document,
                       const CursorPosition& start,
                       const CursorPosition& end,
                       const QTextCharFormat& format);

    void undo() override;
    void redo() override;
    int id() const override;

private:
    CursorPosition m_startPos;
    CursorPosition m_endPos;
    QTextCharFormat m_format;
    QTextCharFormat m_previousFormat;  ///< Format before application
};

/// @brief Command for removing text formatting
class FormatRemoveCommand : public DocumentCommand {
public:
    /// @brief Constructor
    /// @param document The text document
    /// @param start Start position
    /// @param end End position
    FormatRemoveCommand(QTextDocument* document,
                        const CursorPosition& start,
                        const CursorPosition& end);

    void undo() override;
    void redo() override;
    int id() const override;

private:
    CursorPosition m_startPos;
    CursorPosition m_endPos;
    QTextCharFormat m_previousFormat;  ///< Format before removal
};

/// @brief Command for replacing text (delete + insert as single operation)
class TextReplaceCommand : public DocumentCommand {
public:
    /// @brief Constructor
    /// @param document The text document
    /// @param start Start of text to replace
    /// @param end End of text to replace
    /// @param oldText The original text being replaced
    /// @param newText The replacement text
    TextReplaceCommand(QTextDocument* document,
                       const CursorPosition& start,
                       const CursorPosition& end,
                       const QString& oldText,
                       const QString& newText);

    void undo() override;
    void redo() override;
    int id() const override;

private:
    CursorPosition m_startPos;
    CursorPosition m_endPos;
    QString m_oldText;
    QString m_newText;
};

/// @brief Command for replacing all occurrences at once
class ReplaceAllCommand : public DocumentCommand {
public:
    /// @brief Single replacement entry
    struct Replacement {
        int startPos;              ///< Absolute start position
        int endPos;                ///< Absolute end position
        QString oldText;           ///< Original text
        QString newText;           ///< Replacement text
    };

    /// @brief Constructor
    /// @param document The text document
    /// @param cursorPos Initial cursor position
    /// @param replacements List of replacements to make
    ReplaceAllCommand(QTextDocument* document,
                      const CursorPosition& cursorPos,
                      const std::vector<Replacement>& replacements);

    void undo() override;
    void redo() override;
    int id() const override;

private:
    std::vector<Replacement> m_replacements;
};

// =============================================================================
// Backward Compatibility Aliases
// =============================================================================

/// @brief Alias for CompositeDocumentCommand (backward compatibility)
using CompositeBufferCommand = CompositeDocumentCommand;

}  // namespace kalahari::editor
