/// @file format_layer.h
/// @brief Format layer with interval tree for O(log N) queries (OpenSpec #00043 Phase 3)
///
/// FormatLayer stores text formatting separately from text content.
/// This enables efficient O(log N) format queries using an interval tree.
/// Formats are stored as character ranges with formatting attributes.
///
/// Key features:
/// - Separate format storage from text (Word/Writer architecture)
/// - O(log N) range queries via interval tree
/// - Automatic range adjustment on text insert/delete
/// - Support for overlapping formats

#pragma once

#include <kalahari/editor/text_buffer.h>
#include <QColor>
#include <QFont>
#include <QString>
#include <memory>
#include <vector>
#include <set>
#include <optional>

namespace kalahari::editor {

// =============================================================================
// Format Types
// =============================================================================

/// @brief Format type flags (can be combined)
enum class FormatType : uint32_t {
    None        = 0,
    Bold        = 1 << 0,
    Italic      = 1 << 1,
    Underline   = 1 << 2,
    Strikethrough = 1 << 3,
    Subscript   = 1 << 4,
    Superscript = 1 << 5,
    FontFamily  = 1 << 6,
    FontSize    = 1 << 7,
    ForegroundColor = 1 << 8,
    BackgroundColor = 1 << 9,
    // Extended types for future
    SmallCaps   = 1 << 10,
    AllCaps     = 1 << 11,
};

/// @brief Enable bitwise operations on FormatType
inline FormatType operator|(FormatType a, FormatType b) {
    return static_cast<FormatType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline FormatType operator&(FormatType a, FormatType b) {
    return static_cast<FormatType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline FormatType& operator|=(FormatType& a, FormatType b) {
    a = a | b;
    return a;
}

inline FormatType& operator&=(FormatType& a, FormatType b) {
    a = a & b;
    return a;
}

inline FormatType operator~(FormatType a) {
    return static_cast<FormatType>(~static_cast<uint32_t>(a));
}

inline bool hasFlag(FormatType flags, FormatType flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

/// @brief Text format attributes
///
/// Contains all formatting attributes for a text range.
/// Only attributes corresponding to set flags are valid.
struct TextFormat {
    FormatType flags = FormatType::None;  ///< Which attributes are set
    QString fontFamily;                    ///< Font family (if FontFamily flag set)
    double fontSize = 0.0;                 ///< Font size in points (if FontSize flag set)
    QColor foregroundColor;                ///< Text color (if ForegroundColor flag set)
    QColor backgroundColor;                ///< Highlight color (if BackgroundColor flag set)

    /// @brief Check if format has any attributes set
    bool isEmpty() const {
        return flags == FormatType::None;
    }

    /// @brief Check if format has a specific flag
    bool hasFlag(FormatType flag) const {
        return kalahari::editor::hasFlag(flags, flag);
    }

    /// @brief Set bold
    void setBold(bool enabled = true) {
        if (enabled) flags |= FormatType::Bold;
        else flags = flags & ~FormatType::Bold;
    }

    /// @brief Set italic
    void setItalic(bool enabled = true) {
        if (enabled) flags |= FormatType::Italic;
        else flags = flags & ~FormatType::Italic;
    }

    /// @brief Set underline
    void setUnderline(bool enabled = true) {
        if (enabled) flags |= FormatType::Underline;
        else flags = flags & ~FormatType::Underline;
    }

    /// @brief Set strikethrough
    void setStrikethrough(bool enabled = true) {
        if (enabled) flags |= FormatType::Strikethrough;
        else flags = flags & ~FormatType::Strikethrough;
    }

    /// @brief Check equality
    bool operator==(const TextFormat& other) const {
        if (flags != other.flags) return false;
        if (hasFlag(FormatType::FontFamily) && fontFamily != other.fontFamily) return false;
        if (hasFlag(FormatType::FontSize) && fontSize != other.fontSize) return false;
        if (hasFlag(FormatType::ForegroundColor) && foregroundColor != other.foregroundColor) return false;
        if (hasFlag(FormatType::BackgroundColor) && backgroundColor != other.backgroundColor) return false;
        return true;
    }

    bool operator!=(const TextFormat& other) const {
        return !(*this == other);
    }

    /// @brief Merge another format (other takes precedence)
    TextFormat merged(const TextFormat& other) const {
        TextFormat result = *this;
        result.flags |= other.flags;
        if (other.hasFlag(FormatType::FontFamily)) result.fontFamily = other.fontFamily;
        if (other.hasFlag(FormatType::FontSize)) result.fontSize = other.fontSize;
        if (other.hasFlag(FormatType::ForegroundColor)) result.foregroundColor = other.foregroundColor;
        if (other.hasFlag(FormatType::BackgroundColor)) result.backgroundColor = other.backgroundColor;
        return result;
    }
};

/// @brief Format range in document
///
/// Represents a character range with associated formatting.
/// Ranges are half-open: [start, end)
struct FormatRange {
    size_t start = 0;      ///< Start character offset (inclusive)
    size_t end = 0;        ///< End character offset (exclusive)
    TextFormat format;     ///< Formatting attributes

    /// @brief Check if range is empty
    bool isEmpty() const { return start >= end; }

    /// @brief Get range length
    size_t length() const { return start < end ? end - start : 0; }

    /// @brief Check if position is within range
    bool contains(size_t pos) const { return pos >= start && pos < end; }

    /// @brief Check if ranges overlap
    bool overlaps(const FormatRange& other) const {
        return start < other.end && end > other.start;
    }

    /// @brief Check if ranges are adjacent
    bool isAdjacentTo(const FormatRange& other) const {
        return end == other.start || other.end == start;
    }

    /// @brief For ordering in interval tree
    bool operator<(const FormatRange& other) const {
        if (start != other.start) return start < other.start;
        return end < other.end;
    }
};

// =============================================================================
// Interval Tree for O(log N) Range Queries
// =============================================================================

/// @brief Interval tree node for efficient range queries
class IntervalTreeNode {
public:
    FormatRange range;
    size_t maxEnd = 0;  ///< Maximum end value in this subtree
    std::unique_ptr<IntervalTreeNode> left;
    std::unique_ptr<IntervalTreeNode> right;

    explicit IntervalTreeNode(const FormatRange& r)
        : range(r), maxEnd(r.end) {}
};

/// @brief Augmented interval tree for O(log N) range operations
///
/// Implements an augmented BST where each node stores the maximum
/// endpoint in its subtree. This enables O(log N) range queries.
class IntervalTree {
public:
    IntervalTree() = default;
    ~IntervalTree() = default;

    // Move-only
    IntervalTree(IntervalTree&&) noexcept = default;
    IntervalTree& operator=(IntervalTree&&) noexcept = default;
    IntervalTree(const IntervalTree&) = delete;
    IntervalTree& operator=(const IntervalTree&) = delete;

    /// @brief Insert a format range
    void insert(const FormatRange& range);

    /// @brief Remove ranges matching predicate
    /// @return Number of ranges removed
    size_t removeIf(std::function<bool(const FormatRange&)> predicate);

    /// @brief Find all ranges containing a position
    std::vector<FormatRange> findAt(size_t position) const;

    /// @brief Find all ranges overlapping with [start, end)
    std::vector<FormatRange> findOverlapping(size_t start, size_t end) const;

    /// @brief Get all ranges in order
    std::vector<FormatRange> all() const;

    /// @brief Clear all ranges
    void clear();

    /// @brief Get number of ranges
    size_t size() const { return m_size; }

    /// @brief Check if empty
    bool empty() const { return m_size == 0; }

    /// @brief Shift all ranges after position
    /// @param position Position where insert/delete occurred
    /// @param delta Amount to shift (+ve for insert, -ve for delete)
    void shiftRanges(size_t position, int delta);

private:
    void insertNode(std::unique_ptr<IntervalTreeNode>& node, const FormatRange& range);
    void collectAt(const IntervalTreeNode* node, size_t position, std::vector<FormatRange>& result) const;
    void collectOverlapping(const IntervalTreeNode* node, size_t start, size_t end, std::vector<FormatRange>& result) const;
    void collectAll(const IntervalTreeNode* node, std::vector<FormatRange>& result) const;
    void updateMaxEnd(IntervalTreeNode* node);
    std::unique_ptr<IntervalTreeNode> removeIfNode(std::unique_ptr<IntervalTreeNode> node,
                                                    std::function<bool(const FormatRange&)>& predicate,
                                                    size_t& removedCount);
    void shiftNode(IntervalTreeNode* node, size_t position, int delta);

    std::unique_ptr<IntervalTreeNode> m_root;
    size_t m_size = 0;
};

// =============================================================================
// Format Layer
// =============================================================================

/// @brief Format layer for separating formatting from text
///
/// FormatLayer manages text formatting as ranges separate from the text content.
/// This is a key part of the Word/Writer architecture that enables efficient
/// operations on large documents.
///
/// Usage:
/// @code
/// FormatLayer layer;
///
/// // Add bold formatting to characters 10-20
/// TextFormat bold;
/// bold.setBold();
/// layer.addFormat(10, 20, bold);
///
/// // Get formats at position 15
/// std::vector<FormatRange> formats = layer.getFormatsAt(15);
///
/// // Text inserted at position 5 - shift formats
/// layer.onTextInserted(5, 10);
/// @endcode
class FormatLayer : public ITextBufferObserver {
public:
    FormatLayer() = default;
    ~FormatLayer() override = default;

    // Move-only
    FormatLayer(FormatLayer&&) noexcept = default;
    FormatLayer& operator=(FormatLayer&&) noexcept = default;
    FormatLayer(const FormatLayer&) = delete;
    FormatLayer& operator=(const FormatLayer&) = delete;

    // =========================================================================
    // Format Operations
    // =========================================================================

    /// @brief Add format to a range
    /// @param start Start character offset (inclusive)
    /// @param end End character offset (exclusive)
    /// @param format Format to apply
    void addFormat(size_t start, size_t end, const TextFormat& format);

    /// @brief Remove specific format type from a range
    /// @param start Start character offset
    /// @param end End character offset
    /// @param type Format type to remove
    void removeFormat(size_t start, size_t end, FormatType type);

    /// @brief Clear all formatting from a range
    /// @param start Start character offset
    /// @param end End character offset
    void clearFormats(size_t start, size_t end);

    /// @brief Clear all formatting
    void clearAll();

    /// @brief Toggle a format type in a range
    /// @param start Start character offset
    /// @param end End character offset
    /// @param type Format type to toggle
    /// @return true if format was enabled, false if disabled
    bool toggleFormat(size_t start, size_t end, FormatType type);

    // =========================================================================
    // Format Queries
    // =========================================================================

    /// @brief Get all format ranges at a position
    /// @param position Character offset
    /// @return Vector of format ranges containing this position
    std::vector<FormatRange> getFormatsAt(size_t position) const;

    /// @brief Get merged format at a position
    /// @param position Character offset
    /// @return Combined format from all ranges at this position
    TextFormat getMergedFormatAt(size_t position) const;

    /// @brief Get formats for a paragraph
    /// @param buffer Text buffer to get paragraph positions
    /// @param paragraphIndex Paragraph index (0-based)
    /// @return Vector of format ranges within this paragraph
    std::vector<FormatRange> getFormatsForParagraph(const TextBuffer& buffer, size_t paragraphIndex) const;

    /// @brief Get formats for a range
    /// @param start Start character offset
    /// @param end End character offset
    /// @return Vector of format ranges overlapping this range
    std::vector<FormatRange> getFormatsInRange(size_t start, size_t end) const;

    /// @brief Check if a format type is active at position
    /// @param position Character offset
    /// @param type Format type to check
    /// @return true if format is active
    bool hasFormatAt(size_t position, FormatType type) const;

    /// @brief Check if a format type is active throughout a range
    /// @param start Start character offset
    /// @param end End character offset
    /// @param type Format type to check
    /// @return true if format is active at all positions in range
    bool hasFormatInRange(size_t start, size_t end, FormatType type) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /// @brief Get number of format ranges
    size_t rangeCount() const { return m_tree.size(); }

    /// @brief Check if any formats exist
    bool isEmpty() const { return m_tree.empty(); }

    /// @brief Get all format ranges
    std::vector<FormatRange> allRanges() const { return m_tree.all(); }

    // =========================================================================
    // Text Change Handling
    // =========================================================================

    /// @brief Adjust ranges after text insertion
    /// @param position Position where text was inserted
    /// @param length Length of inserted text
    void onTextInserted(size_t position, size_t length);

    /// @brief Adjust ranges after text deletion
    /// @param position Position where text was deleted
    /// @param length Length of deleted text
    void onTextDeleted(size_t position, size_t length);

    // =========================================================================
    // ITextBufferObserver Implementation
    // =========================================================================

    void onTextChanged() override;
    void onParagraphInserted(size_t index) override;
    void onParagraphRemoved(size_t index) override;
    void onParagraphChanged(size_t index) override;
    void onHeightChanged(size_t index, double oldHeight, double newHeight) override;

    /// @brief Attach to a text buffer for automatic range adjustment
    void attachToBuffer(TextBuffer* buffer);

    /// @brief Detach from current buffer
    void detachFromBuffer();

private:
    /// @brief Optimize overlapping ranges with same format
    void coalesceRanges();

    IntervalTree m_tree;
    TextBuffer* m_buffer = nullptr;
};

}  // namespace kalahari::editor
