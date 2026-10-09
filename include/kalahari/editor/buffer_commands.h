/// @file buffer_commands.h
/// @brief Cursor positions in a QTextDocument
///
/// Helpers for the editor's QTextDocument: conversion between absolute positions and
/// cursor positions (paragraph + offset).

#pragma once

#include <kalahari/editor/editor_types.h>
#include <QTextCursor>
#include <QTextDocument>

namespace kalahari::editor {

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

}  // namespace kalahari::editor
