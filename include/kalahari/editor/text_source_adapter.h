/// @file text_source_adapter.h
/// @brief Abstract text source interface for EditorRenderPipeline (OpenSpec #00043 Phase 12.1)
///
/// ITextSource is the interface through which the render pipeline reads the text and its
/// layout; QTextDocumentSource implements it for the editor's QTextDocument.

#pragma once

#include <QString>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextCharFormat>
#include <QFont>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/text_highlight.h>
#include <vector>
#include <memory>

class QTextDocument;

namespace kalahari::editor {

// Forward declarations
class KalahariTextDocumentLayout;

/// @brief Abstract interface for the text source of the render pipeline
///
/// The pipeline reads the paragraphs, their layouts and positions only through this
/// interface, so it does not depend on how the document stores them.
class ITextSource {
public:
    /// @brief Virtual destructor
    virtual ~ITextSource() = default;

    // =========================================================================
    // Content Access
    // =========================================================================

    /// @brief Get number of paragraphs/blocks
    /// @return Paragraph count
    virtual size_t paragraphCount() const = 0;

    /// @brief Get plain text of a paragraph
    /// @param index Paragraph index (0-based)
    /// @return Plain text content, or empty if index out of range
    virtual QString paragraphText(size_t index) const = 0;

    /// @brief Get character count in paragraph
    /// @param index Paragraph index (0-based)
    /// @return Number of characters
    virtual size_t paragraphLength(size_t index) const = 0;

    /// @brief Get full document plain text
    /// @return All paragraphs joined with newlines
    virtual QString plainText() const = 0;

    /// @brief Get total character count
    /// @return Total characters in document
    virtual size_t characterCount() const = 0;

    /// @brief Highlighted ranges of a paragraph: its annotations (comments, TODO and note
    ///        markers) and the results of the checks made for its current text
    /// @param index Paragraph index (0-based)
    /// @note Sources without annotations or checks have none
    virtual std::vector<TextHighlight> paragraphHighlights(size_t /*index*/) const { return {}; }

    // =========================================================================
    // Layout Access
    // =========================================================================

    /// @brief Get QTextLayout for paragraph (may create lazily)
    /// @param index Paragraph index (0-based)
    /// @return QTextLayout pointer, or nullptr if not available
    virtual QTextLayout* layout(size_t index) const = 0;

    /// @brief Check if paragraph has a valid layout
    /// @param index Paragraph index (0-based)
    /// @return true if layout exists and is valid
    virtual bool hasLayout(size_t index) const = 0;

    /// @brief Ensure paragraphs in range have layouts (for lazy sources)
    /// @param first First paragraph index
    /// @param last Last paragraph index (inclusive)
    virtual void ensureLayouted(size_t first, size_t last) = 0;

    // =========================================================================
    // Geometry Queries
    // =========================================================================

    /// @brief Get Y position of paragraph in document coordinates
    /// @param index Paragraph index (0-based)
    /// @return Y coordinate (cumulative height of previous paragraphs)
    virtual double paragraphY(size_t index) const = 0;

    /// @brief Get height of paragraph
    /// @param index Paragraph index (0-based)
    /// @return Height in pixels (estimated if not layouted)
    virtual double paragraphHeight(size_t index) const = 0;

    /// @brief Get total document height
    /// @return Total height in pixels
    virtual double totalHeight() const = 0;

    /// @brief Find paragraph at Y position
    /// @param y Y coordinate in document coordinates
    /// @return Paragraph index, or paragraphCount() if beyond end
    virtual size_t paragraphAtY(double y) const = 0;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// @brief Set text width for layout/wrapping
    /// @param width Width in pixels
    virtual void setTextWidth(double width) = 0;

    /// @brief Get current text width
    /// @return Width in pixels
    virtual double textWidth() const = 0;

    /// @brief Set font for layout
    /// @param font Font to use
    virtual void setFont(const QFont& font) = 0;

    /// @brief Get current font
    /// @return Current font
    virtual QFont font() const = 0;

    /// @brief Set the view typography (line and paragraph spacing, first-line indent)
    /// @note Sources without typography support ignore it
    virtual void setTypography(const LayoutTypography& /*typography*/) {}

    /// @brief Line spacing multiplier the layout uses
    ///
    /// Together with KalahariTextDocumentLayout::lineBox() it gives the full box of a
    /// line (the line plus its share of the extra spacing) for selection and hit tests.
    virtual double lineSpacing() const { return 1.0; }

    /// @brief Space the layout leaves below every paragraph (current font, pixels)
    virtual double paragraphSpacing() const { return 0.0; }

    /// @brief Set the page flow (page mode: lines only within the pages' text areas)
    /// @note Sources without page support ignore it and keep a single page
    virtual void setPageFlow(const PageFlow& /*flow*/) {}

    /// @brief Number of pages the text fills (1 without page flow)
    virtual int pageCount() const { return 1; }
};

// =============================================================================
// QTextDocument Adapter
// =============================================================================

/// @brief Adapter for QTextDocument as text source
///
/// QTextDocumentSource wraps a QTextDocument to implement ITextSource.
/// QTextDocument provides full editing capabilities, undo/redo, and cursor support.
class QTextDocumentSource : public ITextSource {
public:
    /// @brief Construct adapter for QTextDocument
    /// @param document Document to wrap (must outlive this adapter)
    explicit QTextDocumentSource(QTextDocument* document);

    /// @brief Destructor
    ~QTextDocumentSource() override = default;

    // ITextSource interface
    size_t paragraphCount() const override;
    QString paragraphText(size_t index) const override;
    size_t paragraphLength(size_t index) const override;
    QString plainText() const override;
    size_t characterCount() const override;
    std::vector<TextHighlight> paragraphHighlights(size_t index) const override;

    QTextLayout* layout(size_t index) const override;
    bool hasLayout(size_t index) const override;
    void ensureLayouted(size_t first, size_t last) override;

    double paragraphY(size_t index) const override;
    double paragraphHeight(size_t index) const override;
    double totalHeight() const override;
    size_t paragraphAtY(double y) const override;

    void setTextWidth(double width) override;
    double textWidth() const override;
    void setFont(const QFont& font) override;
    QFont font() const override;
    void setTypography(const LayoutTypography& typography) override;
    double lineSpacing() const override;
    double paragraphSpacing() const override;
    void setPageFlow(const PageFlow& flow) override;
    int pageCount() const override;

    /// @brief Get underlying QTextDocument
    /// @return Pointer to wrapped document
    QTextDocument* document() const { return m_document; }

    /// @brief Get block by index
    /// @param index Block index (0-based)
    /// @return QTextBlock (may be invalid if index out of range)
    QTextBlock blockAt(size_t index) const;

private:
    /// @brief The document's layout, when it is the editor's own layout
    KalahariTextDocumentLayout* kalahariLayout() const;

    QTextDocument* m_document;
};

}  // namespace kalahari::editor
