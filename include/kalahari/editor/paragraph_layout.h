/// @file paragraph_layout.h
/// @brief Paragraph layout engine wrapping QTextLayout (OpenSpec #00042 Phase 2.1/2.2/2.5)
///
/// ParagraphLayout provides a clean interface for text layout operations,
/// wrapping Qt's QTextLayout with dirty state tracking for efficient
/// layout invalidation and caching.
///
/// Key responsibilities:
/// - Text and font management
/// - Layout computation (line breaking)
/// - Character format ranges (bold, italic, underline, etc.)
/// - Dirty state tracking for efficient updates
/// - Geometry queries (height, bounding rect)
/// - Drawing with selection highlighting and spell error underlines

#pragma once

#include <QString>
#include <QFont>
#include <QTextLayout>
#include <QTextCharFormat>
#include <QRectF>
#include <QList>
#include <QColor>
#include <QPainter>
#include <memory>
#include <vector>

namespace kalahari::editor {

/// @brief Represents a spelling error range in text
struct SpellError {
    int start;      ///< Start character position
    int length;     ///< Length of the error in characters

    SpellError() : start(0), length(0) {}
    SpellError(int s, int len) : start(s), length(len) {}

    bool operator==(const SpellError& other) const {
        return start == other.start && length == other.length;
    }
};

/// @brief Type of grammar issue for color coding (OpenSpec #00042 Phase 6.17)
enum class GrammarErrorType {
    Grammar,        ///< Grammar error (blue underline)
    Style,          ///< Style suggestion (green underline)
    Typography      ///< Typography issue (gray underline)
};

/// @brief Represents a grammar error range in text (OpenSpec #00042 Phase 6.17)
struct GrammarErrorRange {
    int start;                              ///< Start character position
    int length;                             ///< Length of the error in characters
    GrammarErrorType type;                  ///< Error type for color coding

    GrammarErrorRange() : start(0), length(0), type(GrammarErrorType::Grammar) {}
    GrammarErrorRange(int s, int len, GrammarErrorType t = GrammarErrorType::Grammar)
        : start(s), length(len), type(t) {}

    bool operator==(const GrammarErrorRange& other) const {
        return start == other.start && length == other.length && type == other.type;
    }
};

/// @brief Wrapper around QTextLayout with dirty state tracking
///
/// ParagraphLayout manages the layout of a single paragraph of text.
/// It wraps QTextLayout to provide:
/// - Simple API for common operations
/// - Dirty state tracking to avoid redundant layout calculations
/// - Caching of layout results
///
/// Usage:
/// @code
/// ParagraphLayout layout;
/// layout.setText("Hello, world!");
/// layout.setFont(QFont("Serif", 12));
/// qreal height = layout.doLayout(500.0);  // Layout at 500px width
/// // Later, draw with: layout.draw(painter, position);
/// @endcode
///
/// Thread safety: Not thread-safe. Use from GUI thread only.
class ParagraphLayout {
public:
    /// @brief Construct an empty paragraph layout
    ParagraphLayout();

    /// @brief Construct with initial text
    /// @param text The text to layout
    explicit ParagraphLayout(const QString& text);

    /// @brief Construct with text and font
    /// @param text The text to layout
    /// @param font The font to use
    ParagraphLayout(const QString& text, const QFont& font);

    /// @brief Destructor
    ~ParagraphLayout();

    /// @brief Copy constructor
    ParagraphLayout(const ParagraphLayout& other);

    /// @brief Move constructor
    ParagraphLayout(ParagraphLayout&& other) noexcept;

    /// @brief Copy assignment
    ParagraphLayout& operator=(const ParagraphLayout& other);

    /// @brief Move assignment
    ParagraphLayout& operator=(ParagraphLayout&& other) noexcept;

    // =========================================================================
    // Text and Font
    // =========================================================================

    /// @brief Get the current text
    /// @return The text being laid out
    QString text() const;

    /// @brief Set the text to layout
    /// @param text The new text
    /// @note Marks the layout as dirty
    void setText(const QString& text);

    /// @brief Get the current font
    /// @return The font used for layout
    QFont font() const;

    /// @brief Set the font for layout
    /// @param font The new font
    /// @note Marks the layout as dirty
    void setFont(const QFont& font);

    /// @brief Get the current text alignment
    /// @return The alignment (Qt::AlignLeft, Qt::AlignHCenter, Qt::AlignRight, Qt::AlignJustify)
    Qt::Alignment alignment() const;

    /// @brief Set the text alignment
    /// @param alignment The new alignment
    /// @note Marks the layout as dirty
    void setAlignment(Qt::Alignment alignment);

    // =========================================================================
    // Formatting (Phase 2.2)
    // =========================================================================

    /// @brief Set character format ranges for styled text
    /// @param formats List of format ranges to apply
    /// @note Marks the layout as dirty
    ///
    /// FormatRange specifies a start position, length, and QTextCharFormat
    /// to apply to that range of text. Ranges can overlap - later ranges
    /// override earlier ones for overlapping properties.
    ///
    /// Example:
    /// @code
    /// QTextLayout::FormatRange boldRange;
    /// boldRange.start = 0;
    /// boldRange.length = 5;
    /// boldRange.format.setFontWeight(QFont::Bold);
    /// layout.setFormats({boldRange});
    /// @endcode
    void setFormats(const QList<QTextLayout::FormatRange>& formats);

    /// @brief Get the current format ranges
    /// @return List of format ranges applied to this layout
    QList<QTextLayout::FormatRange> formats() const;

    /// @brief Clear all format ranges
    /// @note Marks the layout as dirty
    void clearFormats();

    /// @brief Check if any formats are applied
    /// @return true if format ranges exist
    bool hasFormats() const;

    // =========================================================================
    // Layout Operations
    // =========================================================================

    /// @brief Perform the layout at a given width
    /// @param width The available width for text wrapping
    /// @return The total height of the laid-out text
    /// @note If not dirty and width matches, returns cached height
    qreal doLayout(qreal width);

    /// @brief Get the width used for the last layout
    /// @return The width passed to the last doLayout() call, or 0 if not laid out
    qreal layoutWidth() const;

    /// @brief Check if layout needs to be recalculated
    /// @return true if text or font changed since last doLayout()
    bool isDirty() const;

    /// @brief Mark the layout as needing recalculation
    /// @note Called automatically when text or font changes
    void invalidate();

    /// @brief Clear the layout and reset to empty state
    void clear();

    // =========================================================================
    // Geometry
    // =========================================================================

    /// @brief Get the height of the laid-out text
    /// @return The total height, or 0 if not laid out
    qreal height() const;

    /// @brief Get the number of lines after layout
    /// @return The line count, or 0 if not laid out
    int lineCount() const;

    /// @brief Get the bounding rectangle of the laid-out text
    /// @return The bounding rect, or empty rect if not laid out
    QRectF boundingRect() const;

    /// @brief Get the bounding rectangle of a specific line
    /// @param lineIndex The line index (0-based)
    /// @return The line's bounding rect, or empty rect if invalid index
    QRectF lineRect(int lineIndex) const;

    // =========================================================================
    // Hit Testing (Phase 2.4)
    // =========================================================================

    /// @brief Convert a point to a character position
    /// @param point The point in layout coordinates (relative to layout origin)
    /// @return The character index at the point, or -1 if layout is dirty
    ///
    /// This method finds the character position closest to the given point.
    /// It handles clicks between characters by returning the nearest valid
    /// cursor position. The point's y-coordinate determines which line is
    /// examined, and the x-coordinate determines the character within that line.
    ///
    /// Example:
    /// @code
    /// ParagraphLayout layout("Hello, world!");
    /// layout.doLayout(500.0);
    /// int pos = layout.positionAt(QPointF(50.0, 10.0));
    /// // pos is the character index at that point
    /// @endcode
    int positionAt(const QPointF& point) const;

    /// @brief Get the cursor rectangle for a character position
    /// @param position The character index (0 to text length)
    /// @return The cursor rectangle, or empty rect if invalid
    ///
    /// Returns a thin rectangle representing the cursor position.
    /// The rectangle's left edge is at the cursor x-position,
    /// the top/bottom span the line height at that position.
    /// Position 0 is before the first character, position == text.length()
    /// is after the last character.
    ///
    /// Example:
    /// @code
    /// ParagraphLayout layout("Hello");
    /// layout.doLayout(500.0);
    /// QRectF rect = layout.cursorRect(2);  // Cursor after "He"
    /// // rect.x() is the x-position of the cursor
    /// @endcode
    QRectF cursorRect(int position) const;

    /// @brief Find the line index containing a character position
    /// @param position The character index
    /// @return The line index, or -1 if invalid
    int lineForPosition(int position) const;

    // =========================================================================
    // Drawing (Phase 2.5)
    // =========================================================================

    /// @brief Draw the paragraph at the specified position
    /// @param painter The painter to draw with
    /// @param position Top-left position for drawing
    ///
    /// Draws the paragraph text including:
    /// - Text with applied character formats
    /// - Selection highlighting (if selection is set)
    /// - Spell error underlines (wavy red lines)
    ///
    /// The painter should have appropriate clip rect set if needed.
    /// Drawing respects the painter's current transform.
    ///
    /// Example:
    /// @code
    /// ParagraphLayout layout("Hello, world!");
    /// layout.doLayout(500.0);
    /// layout.setSelection(0, 5);
    /// layout.draw(&painter, QPointF(10, 20));
    /// @endcode
    void draw(QPainter* painter, const QPointF& position);

    /// @brief Set the selection range for highlighting
    /// @param start Start character position (inclusive)
    /// @param end End character position (exclusive)
    ///
    /// The selection will be highlighted when draw() is called.
    /// Set both to -1 or call clearSelection() to remove selection.
    void setSelection(int start, int end);

    /// @brief Clear the current selection
    void clearSelection();

    /// @brief Check if selection is set
    /// @return true if there is an active selection
    bool hasSelection() const;

    /// @brief Get the selection start position
    /// @return Start position, or -1 if no selection
    int selectionStart() const;

    /// @brief Get the selection end position
    /// @return End position, or -1 if no selection
    int selectionEnd() const;

    /// @brief Set colors for selection highlighting
    /// @param background Background color for selected text
    /// @param foreground Text color for selected text
    ///
    /// Default colors are platform-specific (QPalette::Highlight).
    void setSelectionColors(const QColor& background, const QColor& foreground);

    /// @brief Get selection background color
    QColor selectionBackgroundColor() const;

    /// @brief Get selection foreground color
    QColor selectionForegroundColor() const;

    /// @brief Add a spell error marker
    /// @param start Start character position
    /// @param length Length of the error
    ///
    /// Spell errors are rendered as wavy red underlines.
    void addSpellError(int start, int length);

    /// @brief Clear all spell error markers
    void clearSpellErrors();

    /// @brief Get the list of spell errors
    /// @return Vector of spell error ranges
    std::vector<SpellError> spellErrors() const;

    /// @brief Check if there are any spell errors
    bool hasSpellErrors() const;

    // =========================================================================
    // Grammar Errors (Phase 6.17)
    // =========================================================================

    /// @brief Add a grammar error marker
    /// @param start Start character position
    /// @param length Length of the error
    /// @param type Type of grammar error for color coding
    ///
    /// Grammar errors are rendered as wavy underlines with type-specific colors:
    /// - Grammar: blue
    /// - Style: green
    /// - Typography: gray
    void addGrammarError(int start, int length, GrammarErrorType type = GrammarErrorType::Grammar);

    /// @brief Clear all grammar error markers
    void clearGrammarErrors();

    /// @brief Get the list of grammar errors
    /// @return Vector of grammar error ranges
    std::vector<GrammarErrorRange> grammarErrors() const;

    /// @brief Check if there are any grammar errors
    bool hasGrammarErrors() const;

    // =========================================================================
    // Advanced Access
    // =========================================================================

    /// @brief Get direct access to the underlying QTextLayout
    /// @return Const reference to the QTextLayout
    /// @note Use for advanced operations not exposed by this wrapper
    const QTextLayout& textLayout() const;

    /// @brief Get mutable access to the underlying QTextLayout
    /// @return Reference to the QTextLayout
    /// @warning Modifying the layout directly may invalidate cached state
    QTextLayout& textLayout();

private:
    /// @brief Perform the actual layout operation
    /// @param width The available width
    void performLayout(qreal width);

    /// @brief Draw selection highlighting for a range
    /// @param painter The painter
    /// @param position Drawing offset
    void drawSelection(QPainter* painter, const QPointF& position);

    /// @brief Draw spell error underlines
    /// @param painter The painter
    /// @param position Drawing offset
    void drawSpellErrors(QPainter* painter, const QPointF& position);

    /// @brief Draw grammar error underlines
    /// @param painter The painter
    /// @param position Drawing offset
    void drawGrammarErrors(QPainter* painter, const QPointF& position);

    /// @brief Draw a wavy underline for a text range
    /// @param painter The painter
    /// @param startPos Start character position
    /// @param length Length in characters
    /// @param offset Drawing offset
    /// @param color Underline color
    void drawWavyUnderline(QPainter* painter, int startPos, int length,
                           const QPointF& offset, const QColor& color);

    std::unique_ptr<QTextLayout> m_layout;       ///< The underlying Qt layout
    QString m_text;                              ///< Current text
    QFont m_font;                                ///< Current font
    Qt::Alignment m_alignment = Qt::AlignLeft;   ///< Text alignment
    QList<QTextLayout::FormatRange> m_formats;   ///< Character format ranges
    qreal m_width;                               ///< Width used for last layout
    qreal m_height;                              ///< Cached height after layout
    bool m_dirty;                                ///< True if layout needs recalculation

    // Selection state (Phase 2.5)
    int m_selectionStart;                        ///< Selection start position (-1 if none)
    int m_selectionEnd;                          ///< Selection end position (-1 if none)
    QColor m_selectionBg;                        ///< Selection background color
    QColor m_selectionFg;                        ///< Selection foreground color

    // Spell errors (Phase 2.5)
    std::vector<SpellError> m_spellErrors;       ///< List of spell error ranges

    // Grammar errors (Phase 6.17)
    std::vector<GrammarErrorRange> m_grammarErrors;  ///< List of grammar error ranges
};

}  // namespace kalahari::editor
