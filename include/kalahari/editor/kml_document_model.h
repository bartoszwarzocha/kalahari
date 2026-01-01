/// @file kml_document_model.h
/// @brief Lightweight document model with lazy rendering (OpenSpec #00043)
///
/// KmlDocumentModel stores the full document in memory (paragraphs + formats)
/// but creates QTextLayout only for visible paragraphs (lazy rendering).
/// This provides Word/Writer-like performance: fast loading, smooth scrolling.

#pragma once

#include <kalahari/editor/height_tree.h>
#include <kalahari/editor/format_run.h>

#include <QObject>
#include <QString>
#include <QColor>
#include <QFont>
#include <QTextLayout>
#include <QXmlStreamReader>

#include <memory>
#include <vector>

namespace kalahari {
namespace editor {

/// @brief Lightweight document model with lazy rendering
///
/// KmlDocumentModel stores the full document in memory (paragraphs + formats)
/// but creates QTextLayout only for visible paragraphs (lazy rendering).
/// This provides Word/Writer-like performance: fast loading, smooth scrolling.
///
/// Usage:
/// @code
/// KmlDocumentModel model;
/// model.loadKml(kmlString);  // Fast - just parses, no layout
///
/// // Get visible range from ViewportManager
/// model.ensureLayouted(first, last);  // Layout only visible
///
/// // Render
/// for (size_t i = first; i <= last; ++i) {
///     QTextLayout* layout = model.layout(i);
///     layout->draw(painter, pos);
/// }
/// @endcode
class KmlDocumentModel : public QObject {
    Q_OBJECT

public:
    /// @brief Default constructor
    /// @param parent Parent QObject
    explicit KmlDocumentModel(QObject* parent = nullptr);

    /// @brief Destructor
    ~KmlDocumentModel() override;

    // =========================================================================
    // Document Loading
    // =========================================================================

    /// @brief Load KML document (parses all, layouts none)
    /// @param kml KML markup string
    /// @return true if successful
    bool loadKml(const QString& kml);

    /// @brief Clear document
    void clear();

    /// @brief Check if document is empty
    /// @return true if no paragraphs
    bool isEmpty() const;

    // =========================================================================
    // Paragraph Access
    // =========================================================================

    /// @brief Get paragraph count
    /// @return Number of paragraphs
    size_t paragraphCount() const;

    /// @brief Get plain text of paragraph
    /// @param index Paragraph index (0-based)
    /// @return Plain text content
    QString paragraphText(size_t index) const;

    /// @brief Get format runs for paragraph
    /// @param index Paragraph index (0-based)
    /// @return Vector of format runs
    const std::vector<FormatRun>& paragraphFormats(size_t index) const;

    /// @brief Get full document plain text
    /// @return All paragraphs joined with newlines
    QString plainText() const;

    /// @brief Get character count in paragraph
    /// @param index Paragraph index (0-based)
    /// @return Number of characters
    size_t paragraphLength(size_t index) const;

    /// @brief Get total character count
    /// @return Total characters in document
    size_t characterCount() const;

    /// @brief Get total word count (cached, calculated during load)
    /// @return Total words in document
    size_t wordCount() const;

    /// @brief Get character count without spaces (cached, calculated during load)
    /// @return Total non-space characters in document
    size_t characterCountNoSpaces() const;

    // =========================================================================
    // Height Queries (for scrolling)
    // =========================================================================

    /// @brief Get Y position of paragraph
    /// @param index Paragraph index (0-based)
    /// @return Y coordinate in document coordinates
    double paragraphY(size_t index) const;

    /// @brief Get height of paragraph (estimated if not layouted)
    /// @param index Paragraph index (0-based)
    /// @return Height in pixels
    double paragraphHeight(size_t index) const;

    /// @brief Get total document height
    /// @return Total height in pixels
    double totalHeight() const;

    /// @brief Find paragraph at Y position
    /// @param y Y coordinate in document coordinates
    /// @return Paragraph index, or paragraphCount() if beyond end
    size_t paragraphAtY(double y) const;

    // =========================================================================
    // Lazy Layout
    // =========================================================================

    /// @brief Ensure paragraphs in range are layouted
    /// @param first First paragraph index
    /// @param last Last paragraph index (inclusive)
    void ensureLayouted(size_t first, size_t last);

    /// @brief Get layout for paragraph (nullptr if not layouted)
    /// @param index Paragraph index (0-based)
    /// @return QTextLayout pointer, or nullptr if not layouted
    QTextLayout* layout(size_t index) const;

    /// @brief Check if paragraph is layouted
    /// @param index Paragraph index (0-based)
    /// @return true if QTextLayout exists for this paragraph
    bool isLayouted(size_t index) const;

    /// @brief Invalidate layout for paragraph (e.g., after edit)
    /// @param index Paragraph index (0-based)
    void invalidateLayout(size_t index);

    /// @brief Invalidate all layouts
    void invalidateAllLayouts();

    /// @brief Evict layouts outside range (memory management)
    /// @param keepFirst First paragraph index to keep
    /// @param keepLast Last paragraph index to keep (inclusive)
    void evictLayouts(size_t keepFirst, size_t keepLast);

    // =========================================================================
    // Configuration
    // =========================================================================

    /// @brief Set font for layout
    /// @param font Font to use for text layout
    void setFont(const QFont& font);

    /// @brief Get current font
    /// @return Current font
    QFont font() const;

    /// @brief Set line width for layout
    /// @param width Width in pixels
    void setLineWidth(double width);

    /// @brief Get line width
    /// @return Line width in pixels
    double lineWidth() const;

    /// @brief Set estimated line height (for initial height estimation)
    /// @param height Estimated height per line in pixels
    void setEstimatedLineHeight(double height);

    /// @brief Set default text color for layout
    /// @param color Text color to use
    void setTextColor(const QColor& color);

    /// @brief Get current text color
    /// @return Current text color
    QColor textColor() const;

signals:
    /// @brief Emitted when document is loaded
    void documentLoaded();

    /// @brief Emitted when paragraph height changes
    /// @param index Paragraph index
    /// @param newHeight New height after layout
    void paragraphHeightChanged(size_t index, double newHeight);

    /// @brief Emitted when total height changes
    /// @param newHeight New total document height
    void totalHeightChanged(double newHeight);

    /// @brief Emitted when paragraph content changes
    /// @param index Paragraph index
    void paragraphChanged(size_t index);

private:
    /// @brief Internal paragraph storage
    struct Paragraph {
        QString text;                           ///< Plain text content
        std::vector<FormatRun> formats;         ///< Format runs within paragraph
        std::unique_ptr<QTextLayout> layout;    ///< QTextLayout (created lazily)
        bool layoutValid = false;               ///< Whether layout is valid
    };

    /// @brief Parse single paragraph from KML
    /// @param paraKml KML markup for single paragraph
    /// @param para Output paragraph structure
    /// @return true if parsing succeeded
    bool parseParagraph(const QString& paraKml, Paragraph& para);

    /// @brief Parse inline content recursively
    /// @param reader XML reader positioned at content
    /// @param text Output plain text (accumulated)
    /// @param formats Output format runs (accumulated)
    /// @param currentFormat Current active format
    /// @param currentPos Current position in text
    /// @param endTag Tag name to stop at
    void parseInlineContent(QXmlStreamReader& reader,
                            QString& text,
                            std::vector<FormatRun>& formats,
                            QTextCharFormat currentFormat,
                            size_t& currentPos,
                            const QString& endTag);

    /// @brief Create QTextLayout for paragraph
    /// @param index Paragraph index
    void createLayout(size_t index);

    /// @brief Estimate height for paragraph without layout
    /// @param text Paragraph text
    /// @return Estimated height in pixels
    double estimateHeight(const QString& text) const;

    /// @brief Apply formats to QTextLayout
    /// @param layout QTextLayout to configure
    /// @param formats Format runs to apply
    /// @param textLength Length of text (for base color format)
    void applyFormats(QTextLayout* layout, const std::vector<FormatRun>& formats, int textLength) const;

    std::vector<Paragraph> m_paragraphs;    ///< All paragraphs
    HeightTree m_heightTree;                ///< Fenwick tree for height queries

    QFont m_font;                           ///< Font for layout
    double m_lineWidth = 800.0;             ///< Line width for layout
    double m_estimatedLineHeight = 20.0;    ///< Estimated line height
    double m_charsPerLine = 80.0;           ///< Estimated characters per line
    QColor m_textColor{30, 30, 30};         ///< Default text color

    // Cached statistics (calculated during load)
    size_t m_cachedCharCount = 0;           ///< Total character count
    size_t m_cachedWordCount = 0;           ///< Total word count
    size_t m_cachedCharCountNoSpaces = 0;   ///< Characters without spaces

    static const std::vector<FormatRun> s_emptyFormats;  ///< Empty format vector
};

} // namespace editor
} // namespace kalahari
