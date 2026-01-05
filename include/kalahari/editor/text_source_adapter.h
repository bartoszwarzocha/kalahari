/// @file text_source_adapter.h
/// @brief Abstract text source interface for EditorRenderPipeline (OpenSpec #00043 Phase 12.1)
///
/// ITextSource provides a unified interface for accessing text content from different
/// document sources (QTextDocument for edit mode, KmlDocumentModel for view mode).
/// This abstraction allows the render pipeline to work with either source transparently.

#pragma once

#include <QString>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextCharFormat>
#include <QFont>
#include <vector>
#include <memory>

class QTextDocument;

namespace kalahari::editor {

// Forward declarations
class KmlDocumentModel;

/// @brief Abstract interface for text source (unified access to QTextDocument or KmlDocumentModel)
///
/// ITextSource abstracts away the differences between QTextDocument and KmlDocumentModel,
/// providing a common interface for the render pipeline. This allows the pipeline to
/// render content regardless of whether we're in edit mode (QTextDocument) or view mode
/// (KmlDocumentModel).
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
};

// =============================================================================
// QTextDocument Adapter
// =============================================================================

/// @brief Adapter for QTextDocument as text source
///
/// QTextDocumentSource wraps a QTextDocument to implement ITextSource.
/// Used in edit mode when user is actively editing the document.
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

    /// @brief Get underlying QTextDocument
    /// @return Pointer to wrapped document
    QTextDocument* document() const { return m_document; }

    /// @brief Get block by index
    /// @param index Block index (0-based)
    /// @return QTextBlock (may be invalid if index out of range)
    QTextBlock blockAt(size_t index) const;

private:
    QTextDocument* m_document;
    double m_textWidth = 800.0;
};

// =============================================================================
// KmlDocumentModel Adapter
// =============================================================================

/// @brief Adapter for KmlDocumentModel as text source
///
/// KmlDocumentModelSource wraps a KmlDocumentModel to implement ITextSource.
/// Used in view mode for efficient read-only rendering without full QTextDocument overhead.
/// KmlDocumentModel provides lazy layout creation for better performance with large documents.
class KmlDocumentModelSource : public ITextSource {
public:
    /// @brief Construct adapter for KmlDocumentModel
    /// @param model Model to wrap (must outlive this adapter)
    explicit KmlDocumentModelSource(KmlDocumentModel* model);

    /// @brief Destructor
    ~KmlDocumentModelSource() override = default;

    // ITextSource interface
    size_t paragraphCount() const override;
    QString paragraphText(size_t index) const override;
    size_t paragraphLength(size_t index) const override;
    QString plainText() const override;
    size_t characterCount() const override;

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

    /// @brief Get underlying KmlDocumentModel
    /// @return Pointer to wrapped model
    KmlDocumentModel* model() const { return m_model; }

private:
    KmlDocumentModel* m_model;
};

}  // namespace kalahari::editor
