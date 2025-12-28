/// @file kml_converter.h
/// @brief KML Converter for TextBuffer + FormatLayer (OpenSpec #00043 Phase 7)
///
/// KmlConverter bridges the KML markup format with the new performance-optimized
/// TextBuffer and FormatLayer architecture. It provides bidirectional conversion:
/// - KML → TextBuffer + FormatLayer (parsing)
/// - TextBuffer + FormatLayer → KML (serialization)
///
/// Key features:
/// - Preserves all formatting (bold, italic, underline, strikethrough, sub/superscript)
/// - Handles nested formatting correctly
/// - Supports comments and metadata
/// - Round-trip safe (load → save produces equivalent output)

#pragma once

#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/format_layer.h>
#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <memory>
#include <optional>
#include <vector>

namespace kalahari::editor {

// =============================================================================
// Metadata Types
// =============================================================================

/// @brief Comment attached to text
struct TextComment {
    size_t anchorStart = 0;     ///< Start position in document
    size_t anchorEnd = 0;       ///< End position in document
    QString author;              ///< Comment author
    QString text;                ///< Comment content
    QString timestamp;           ///< ISO 8601 timestamp
    QString id;                  ///< Unique identifier
};

/// @brief Type of annotation marker
enum class MarkerType {
    Todo,   ///< Actionable item (checkbox-like)
    Note    ///< Informational annotation
};

/// @brief TODO/Note marker in text
struct TextTodo {
    size_t position = 0;         ///< Position in document
    QString text;                ///< Marker content/description
    MarkerType type = MarkerType::Todo;  ///< TODO or NOTE
    bool completed = false;      ///< Only meaningful for TODO
    QString priority;            ///< Priority level
    QString id;                  ///< Unique identifier
    QString timestamp;           ///< Creation timestamp
};

/// @brief Metadata layer for comments, TODOs, bookmarks
class MetadataLayer {
public:
    MetadataLayer() = default;
    ~MetadataLayer() = default;

    // Comments
    void addComment(const TextComment& comment);
    void removeComment(const QString& id);
    std::vector<TextComment> getCommentsAt(size_t position) const;
    std::vector<TextComment> getCommentsInRange(size_t start, size_t end) const;
    const std::vector<TextComment>& allComments() const { return m_comments; }
    void clearComments() { m_comments.clear(); }

    // TODOs
    void addTodo(const TextTodo& todo);
    void removeTodo(size_t index);
    void removeTodo(const QString& id);
    std::vector<TextTodo> getTodosAt(size_t position) const;
    std::vector<TextTodo> getTodosInRange(size_t start, size_t end) const;
    const std::vector<TextTodo>& allTodos() const { return m_todos; }
    void clearTodos() { m_todos.clear(); }

    // Marker query methods
    std::vector<TextTodo> getMarkersByType(MarkerType type) const;
    std::optional<TextTodo> getMarkerById(const QString& id) const;

    // Navigation methods
    std::optional<TextTodo> findNextMarker(size_t fromPosition,
        std::optional<MarkerType> typeFilter = std::nullopt) const;
    std::optional<TextTodo> findPreviousMarker(size_t fromPosition,
        std::optional<MarkerType> typeFilter = std::nullopt) const;

    // Update methods
    void updateTodo(const QString& id, const TextTodo& updated);
    void toggleTodoCompleted(const QString& id);

    // ID generation
    static QString generateMarkerId();

    // Position adjustment
    void onTextInserted(size_t position, size_t length);
    void onTextDeleted(size_t position, size_t length);

    // Clear all
    void clear();

private:
    std::vector<TextComment> m_comments;
    std::vector<TextTodo> m_todos;
};

// =============================================================================
// Conversion Result
// =============================================================================

/// @brief Result of KML parsing
struct KmlConversionResult {
    std::unique_ptr<TextBuffer> buffer;
    std::unique_ptr<FormatLayer> formatLayer;
    std::unique_ptr<MetadataLayer> metadataLayer;
    bool success = false;
    QString errorMessage;
    int errorLine = -1;
    int errorColumn = -1;

    explicit operator bool() const { return success; }

    static KmlConversionResult ok(
        std::unique_ptr<TextBuffer> buf,
        std::unique_ptr<FormatLayer> fmt,
        std::unique_ptr<MetadataLayer> meta = nullptr
    ) {
        KmlConversionResult result;
        result.buffer = std::move(buf);
        result.formatLayer = std::move(fmt);
        result.metadataLayer = std::move(meta);
        result.success = true;
        return result;
    }

    static KmlConversionResult error(const QString& msg, int line = -1, int col = -1) {
        KmlConversionResult result;
        result.errorMessage = msg;
        result.errorLine = line;
        result.errorColumn = col;
        return result;
    }
};

// =============================================================================
// KML Converter
// =============================================================================

/// @brief Converts between KML markup and TextBuffer + FormatLayer
///
/// The converter handles both directions:
/// - Parsing: KML string → TextBuffer (text) + FormatLayer (formatting)
/// - Serialization: TextBuffer + FormatLayer → KML string
///
/// Example usage:
/// @code
/// KmlConverter converter;
///
/// // Parse KML
/// auto result = converter.parseKml("<p><b>Hello</b> world</p>");
/// if (result) {
///     TextBuffer& buffer = *result.buffer;
///     FormatLayer& formats = *result.formatLayer;
///     // Use buffer and formats...
/// }
///
/// // Serialize back to KML
/// QString kml = converter.toKml(buffer, formats);
/// @endcode
class KmlConverter {
public:
    KmlConverter() = default;
    ~KmlConverter() = default;

    // =========================================================================
    // Parsing (KML → TextBuffer + FormatLayer)
    // =========================================================================

    /// @brief Parse KML markup into TextBuffer and FormatLayer
    /// @param kml The KML markup string
    /// @return Conversion result with buffer, format layer, and metadata
    KmlConversionResult parseKml(const QString& kml);

    /// @brief Parse KML with existing buffer (for incremental loading)
    /// @param kml The KML markup string
    /// @param buffer Existing buffer to append to
    /// @param formatLayer Existing format layer to add to
    /// @param metadataLayer Existing metadata layer to add to
    /// @return true if parsing succeeded
    bool parseKmlInto(const QString& kml,
                      TextBuffer& buffer,
                      FormatLayer& formatLayer,
                      MetadataLayer* metadataLayer = nullptr);

    // =========================================================================
    // Serialization (TextBuffer + FormatLayer → KML)
    // =========================================================================

    /// @brief Convert TextBuffer and FormatLayer to KML markup
    /// @param buffer The text buffer
    /// @param formatLayer The format layer
    /// @param metadataLayer Optional metadata layer for comments/TODOs
    /// @return KML markup string
    QString toKml(const TextBuffer& buffer,
                  const FormatLayer& formatLayer,
                  const MetadataLayer* metadataLayer = nullptr) const;

    /// @brief Convert a single paragraph to KML
    /// @param buffer The text buffer
    /// @param formatLayer The format layer
    /// @param paragraphIndex The paragraph index
    /// @return KML markup for the paragraph (without <p> wrapper)
    QString paragraphToKml(const TextBuffer& buffer,
                           const FormatLayer& formatLayer,
                           size_t paragraphIndex) const;

    // =========================================================================
    // Error Information
    // =========================================================================

    /// @brief Get the last error message
    const QString& lastError() const { return m_lastError; }

    /// @brief Get the last error line number
    int lastErrorLine() const { return m_lastErrorLine; }

    /// @brief Get the last error column number
    int lastErrorColumn() const { return m_lastErrorColumn; }

private:
    // =========================================================================
    // Parsing Helpers
    // =========================================================================

    /// @brief Parse document content (paragraphs)
    bool parseDocumentContent(QXmlStreamReader& reader,
                              TextBuffer& buffer,
                              FormatLayer& formatLayer,
                              MetadataLayer* metadataLayer);

    /// @brief Parse a paragraph element
    /// @param reader XML reader positioned at <p> start
    /// @param buffer Buffer to append text to
    /// @param formatLayer Format layer to add formats to
    /// @param baseOffset Character offset where this paragraph starts
    /// @return Character offset after this paragraph
    size_t parseParagraph(QXmlStreamReader& reader,
                          TextBuffer& buffer,
                          FormatLayer& formatLayer,
                          size_t baseOffset);

    /// @brief Parse inline content (text and formatting elements)
    /// @param reader XML reader
    /// @param text Text accumulator
    /// @param formatLayer Format layer
    /// @param baseOffset Document offset
    /// @param activeFormats Stack of active format types
    /// @param endTag Tag name to stop at
    /// @return Number of characters parsed
    size_t parseInlineContent(QXmlStreamReader& reader,
                              QString& text,
                              FormatLayer& formatLayer,
                              size_t baseOffset,
                              FormatType activeFormats,
                              const QString& endTag);

    /// @brief Parse comments element
    bool parseComments(QXmlStreamReader& reader, MetadataLayer& metadata);

    /// @brief Parse markers element (TODOs and Notes)
    bool parseMarkers(QXmlStreamReader& reader, MetadataLayer& metadata);

    /// @brief Get format type from tag name
    FormatType tagToFormatType(const QString& tag) const;

    /// @brief Set error from XML reader
    void setError(QXmlStreamReader& reader);

    /// @brief Set custom error
    void setError(const QString& message, int line = -1, int col = -1);

    // =========================================================================
    // Serialization Helpers
    // =========================================================================

    /// @brief Format event for serialization
    struct FormatEvent {
        size_t position;
        FormatType type;
        bool isStart;  ///< true = format starts, false = format ends

        bool operator<(const FormatEvent& other) const {
            if (position != other.position) return position < other.position;
            // End events before start events at same position
            if (isStart != other.isStart) return !isStart;
            return static_cast<uint32_t>(type) < static_cast<uint32_t>(other.type);
        }
    };

    /// @brief Build format events for a range
    std::vector<FormatEvent> buildFormatEvents(
        const FormatLayer& formatLayer,
        size_t start,
        size_t end) const;

    /// @brief Write formatted text with proper nesting
    void writeFormattedText(QXmlStreamWriter& writer,
                            const QString& text,
                            size_t textStart,
                            const std::vector<FormatEvent>& events) const;

    /// @brief Get KML tag name for format type
    QString formatTypeToTag(FormatType type) const;

    // =========================================================================
    // Members
    // =========================================================================

    QString m_lastError;
    int m_lastErrorLine = -1;
    int m_lastErrorColumn = -1;
};

}  // namespace kalahari::editor
