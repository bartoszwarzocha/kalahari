/// @file kml_commands.h
/// @brief Undo/Redo command classes for BookEditor (OpenSpec #00042 Phase 4.8-4.12)
///
/// This header provides QUndoCommand-based classes for implementing undo/redo
/// functionality in the BookEditor. Each command encapsulates a reversible
/// editing operation.

#pragma once

#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kml_element.h>
#include <QUndoCommand>
#include <QString>
#include <chrono>

namespace kalahari::editor {

class KmlDocument;
class BookEditor;

// =============================================================================
// Command IDs for merging consecutive commands
// =============================================================================

/// @brief Command IDs for merging similar consecutive commands
enum class CommandId {
    InsertText = 1000,  ///< Character/text insertion
    DeleteText = 1001,  ///< Text deletion
    ApplyStyle = 1002,  ///< Style application
    ToggleFormat = 1003, ///< Inline formatting toggle
};

// =============================================================================
// Base Command
// =============================================================================

/// @brief Base class for all KML editing commands
///
/// Provides common functionality for tracking cursor positions and
/// document references.
class KmlCommand : public QUndoCommand {
public:
    /// @brief Constructor
    /// @param document The document being edited
    /// @param cursorBefore Cursor position before the command
    /// @param text Description text for the undo stack
    KmlCommand(KmlDocument* document,
               const CursorPosition& cursorBefore,
               const QString& text);

    /// @brief Get cursor position before command execution
    CursorPosition cursorBefore() const { return m_cursorBefore; }

    /// @brief Get cursor position after command execution
    CursorPosition cursorAfter() const { return m_cursorAfter; }

protected:
    KmlDocument* m_document;           ///< Document being edited (not owned)
    CursorPosition m_cursorBefore;     ///< Cursor position before command
    CursorPosition m_cursorAfter;      ///< Cursor position after command
};

// =============================================================================
// Insert Text Command
// =============================================================================

/// @brief Command for inserting text at cursor position
///
/// Supports merging consecutive typing into a single undo step.
/// Merging is limited by time (1 second) and paragraph boundaries.
class InsertTextCommand : public KmlCommand {
public:
    /// @brief Constructor
    /// @param document The document being edited
    /// @param position Position where text is inserted
    /// @param text The text to insert
    InsertTextCommand(KmlDocument* document,
                      const CursorPosition& position,
                      const QString& text);

    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    CursorPosition m_insertPosition;   ///< Position where text was inserted
    QString m_text;                    ///< The inserted text
    std::chrono::steady_clock::time_point m_timestamp;  ///< For merge timing
    static constexpr int MERGE_WINDOW_MS = 1000;  ///< Max ms between merges
};

// =============================================================================
// Delete Text Command
// =============================================================================

/// @brief Command for deleting text in a range
///
/// Stores the deleted content as KML for proper restoration with formatting.
class DeleteTextCommand : public KmlCommand {
public:
    /// @brief Constructor
    /// @param document The document being edited
    /// @param start Start of deletion range
    /// @param end End of deletion range
    /// @param deletedText The text that was deleted (for display)
    /// @param deletedKml The KML content that was deleted (for restoration)
    DeleteTextCommand(KmlDocument* document,
                      const CursorPosition& start,
                      const CursorPosition& end,
                      const QString& deletedText,
                      const QString& deletedKml);

    void undo() override;
    void redo() override;
    int id() const override;

private:
    CursorPosition m_start;            ///< Start of deletion range
    CursorPosition m_end;              ///< End of deletion range
    QString m_deletedText;             ///< Plain text that was deleted
    QString m_deletedKml;              ///< KML content for restoration
};

// =============================================================================
// Apply Style Command
// =============================================================================

/// @brief Command for applying a style to a selection range
class ApplyStyleCommand : public KmlCommand {
public:
    /// @brief Constructor
    /// @param document The document being edited
    /// @param range Selection range to apply style to
    /// @param styleId The style ID to apply
    /// @param oldStylesKml KML of the original content (for restoration)
    ApplyStyleCommand(KmlDocument* document,
                      const SelectionRange& range,
                      const QString& styleId,
                      const QString& oldStylesKml);

    void undo() override;
    void redo() override;
    int id() const override;

private:
    SelectionRange m_range;            ///< Range where style was applied
    QString m_styleId;                 ///< Style ID that was applied
    QString m_oldStylesKml;            ///< Original KML for undo
};

// =============================================================================
// Split Paragraph Command
// =============================================================================

/// @brief Command for splitting a paragraph (Enter key)
class SplitParagraphCommand : public KmlCommand {
public:
    /// @brief Constructor
    /// @param document The document being edited
    /// @param position Position where paragraph is split
    SplitParagraphCommand(KmlDocument* document,
                          const CursorPosition& position);

    void undo() override;
    void redo() override;

private:
    CursorPosition m_splitPosition;    ///< Where the split occurred
};

// =============================================================================
// Merge Paragraphs Command
// =============================================================================

/// @brief Command for merging paragraphs (Backspace at start or Delete at end)
class MergeParagraphsCommand : public KmlCommand {
public:
    /// @brief Constructor
    /// @param document The document being edited
    /// @param cursorPos Cursor position before merge
    /// @param mergeFromIndex Index of paragraph being merged (absorbed)
    MergeParagraphsCommand(KmlDocument* document,
                           const CursorPosition& cursorPos,
                           int mergeFromIndex);

    void undo() override;
    void redo() override;

private:
    int m_mergeFromIndex;              ///< Index of paragraph that was merged
    QString m_mergedParagraphKml;      ///< KML of merged paragraph for undo
    int m_splitOffset;                 ///< Offset where paragraphs were joined
};

// =============================================================================
// Toggle Format Command (Phase 7.2)
// =============================================================================

/// @brief Command for toggling inline formatting (bold, italic, etc.)
///
/// This command applies or removes inline formatting from a selection range.
/// It stores the KML before the operation for undo.
class ToggleFormatCommand : public KmlCommand {
public:
    /// @brief Constructor
    /// @param document The document being edited
    /// @param range Selection range to apply/remove format
    /// @param formatType The element type (Bold, Italic, Underline, Strikethrough)
    /// @param apply true to apply format, false to remove
    /// @param oldKml KML of affected paragraphs before the operation
    ToggleFormatCommand(KmlDocument* document,
                        const SelectionRange& range,
                        ElementType formatType,
                        bool apply,
                        const QString& oldKml);

    void undo() override;
    void redo() override;
    int id() const override;

private:
    SelectionRange m_range;            ///< Range where format was toggled
    ElementType m_formatType;          ///< Type of formatting
    bool m_apply;                      ///< true = apply, false = remove
    QString m_oldKml;                  ///< KML before operation for undo
};

}  // namespace kalahari::editor
