/// @file buffer_commands.h
/// @brief Cursor positions and TODO/Note markers in a QTextDocument
///
/// Helpers for the editor's QTextDocument: conversion between absolute positions and
/// cursor positions (paragraph + offset), and TODO/Note markers, which are stored in the
/// KmlPropTodo character property. Edits go straight into the document, so its native
/// undo/redo records them.

#pragma once

#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kml_format_registry.h>  // For KmlPropertyId
#include <QTextCursor>
#include <QTextDocument>
#include <QTextCharFormat>
#include <QString>
#include <QVariantMap>
#include <vector>
#include <optional>

namespace kalahari::editor {

// =============================================================================
// Marker Types
// =============================================================================

/// @brief Type of annotation marker
enum class MarkerType {
    Todo,   ///< Actionable item (checkbox-like)
    Note    ///< Informational annotation
};

/// @brief TODO/Note marker in text
///
/// Stored in the document as a QVariantMap under the KmlPropTodo character property -
/// the same representation the KML <todo> tag is loaded into and saved from.
struct TextMarker {
    int position = 0;             ///< Position in document (absolute)
    int length = 1;               ///< Length of marker anchor text
    QString text;                 ///< Marker content/description
    MarkerType type = MarkerType::Todo;  ///< TODO or NOTE
    bool completed = false;       ///< Only meaningful for TODO
    QString priority;             ///< Priority level (high, normal, low)
    QString id;                   ///< Unique identifier (UUID)
    QString timestamp;            ///< Creation timestamp (ISO 8601)
    QVariantMap otherAttributes;  ///< Other attributes of the KML <todo> tag, kept as loaded

    /// @brief Map stored under KmlPropTodo (keys = attributes of the KML <todo> tag)
    /// @note position and length are not stored - they come from the anchor text
    QVariantMap toVariantMap() const;

    /// @brief Marker from a KmlPropTodo property value
    ///
    /// Attributes without a field go to otherAttributes, so a marker written back
    /// (e.g. after toggling it) keeps them.
    /// @return std::nullopt when @p value is not a marker map
    static std::optional<TextMarker> fromVariant(const QVariant& value);

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
/// @note This modifies the character format of the marker's anchor text
///       (marker.length characters from marker.position, at least one)
void setMarkerInDocument(QTextDocument* document, const TextMarker& marker);

/// @brief Remove marker from document
/// @param document The text document
/// @param position Position of the marker
/// @param length Length of the marker's anchor text
/// @note This clears the KmlPropTodo property of the anchor text
void removeMarkerFromDocument(QTextDocument* document, int position, int length = 1);

}  // namespace kalahari::editor
