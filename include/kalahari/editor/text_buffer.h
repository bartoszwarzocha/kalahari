/// @file text_buffer.h
/// @brief Text buffer using QTextDocument with Fenwick tree (OpenSpec #00043 Phase 2)
///
/// TextBuffer wraps QTextDocument to provide efficient text storage
/// with O(log N) paragraph height queries using a Fenwick tree.
///
/// Key performance characteristics (from Phase 1 benchmarks):
/// - O(1) block/paragraph access (QTextDocument internal optimization)
/// - O(log N) insert/remove operations (QTextDocument piece-table structure)
/// - O(log N) Y-to-paragraph mapping (Fenwick tree prefix sums)
/// - O(1) paragraph text retrieval
/// - Lazy height calculation (only visible paragraphs)

#pragma once

#include <QString>
#include <QTextDocument>
#include <QTextBlock>
#include <QFont>
#include <functional>
#include <memory>
#include <vector>

namespace kalahari::editor {

//=============================================================================
// Fenwick Tree for O(log N) prefix sums
//=============================================================================

/// @brief Fenwick tree for efficient cumulative height queries
///
/// Provides O(log N) operations for:
/// - Prefix sum queries (cumulative height up to paragraph N)
/// - Point updates (change height of paragraph N)
/// - Binary search (find paragraph at Y position)
class HeightTree {
public:
    HeightTree() = default;
    explicit HeightTree(size_t size);

    void resize(size_t size);
    size_t size() const { return m_size; }

    void update(size_t index, double delta);
    void setHeight(size_t index, double height);
    double get(size_t index) const;
    double prefixSum(size_t index) const;
    double totalHeight() const;
    size_t findParagraphAtY(double y) const;
    double getYPosition(size_t index) const;

    void insert(size_t index, double height);
    void remove(size_t index);
    void clear();

private:
    void rebuild();
    static size_t lowbit(size_t x) { return x & ((~x) + 1); }
    static size_t highestBit(size_t n);

    std::vector<double> m_tree;
    std::vector<double> m_heights;
    size_t m_size = 0;
};

//=============================================================================
// Height State
//=============================================================================

enum class HeightState {
    Estimated,   ///< Using estimated height
    Calculated,  ///< Actual height from layout
    Invalid      ///< Needs recalculation
};

struct ParagraphHeightInfo {
    double height = 0.0;
    double estimatedHeight = 0.0;
    HeightState state = HeightState::Estimated;

    bool isCalculated() const { return state == HeightState::Calculated; }
};

//=============================================================================
// Observer Interface
//=============================================================================

class ITextBufferObserver {
public:
    virtual ~ITextBufferObserver() = default;
    virtual void onTextChanged() = 0;
    virtual void onParagraphInserted(size_t index) = 0;
    virtual void onParagraphRemoved(size_t index) = 0;
    virtual void onParagraphChanged(size_t index) = 0;
    virtual void onHeightChanged(size_t index, double oldHeight, double newHeight) = 0;
};

//=============================================================================
// TextBuffer
//=============================================================================

/// @brief Text buffer wrapping QTextDocument with Fenwick tree for heights
///
/// Usage:
/// @code
/// TextBuffer buffer;
/// buffer.setPlainText("Hello\nWorld");
/// size_t para = buffer.getParagraphAtY(500.0);
/// buffer.setParagraphHeight(para, 45.0);
/// @endcode
class TextBuffer {
public:
    TextBuffer();
    ~TextBuffer();

    TextBuffer(TextBuffer&& other) noexcept;
    TextBuffer& operator=(TextBuffer&& other) noexcept;

    TextBuffer(const TextBuffer&) = delete;
    TextBuffer& operator=(const TextBuffer&) = delete;

    // =========================================================================
    // Observer Pattern
    // =========================================================================

    void addObserver(ITextBufferObserver* observer);
    void removeObserver(ITextBufferObserver* observer);

    // =========================================================================
    // Text Content
    // =========================================================================

    void setPlainText(const QString& text);
    QString plainText() const;
    bool isPlainTextCached() const { return m_plainTextCached; }
    void invalidatePlainTextCache();
    bool isEmpty() const;
    int characterCount() const;

    // =========================================================================
    // Paragraph Access
    // =========================================================================

    size_t paragraphCount() const;
    QString paragraphText(size_t index) const;
    int paragraphLength(size_t index) const;
    QTextBlock block(size_t index) const;

    // =========================================================================
    // Text Modification
    // =========================================================================

    void insert(int position, const QString& text);
    void remove(int position, int length);
    void replace(int position, int length, const QString& text);

    void insertParagraph(size_t index, const QString& text);
    void removeParagraph(size_t index);
    void setParagraphText(size_t index, const QString& text);

    // =========================================================================
    // Height Management (Fenwick Tree)
    // =========================================================================

    void setEstimatedLineHeight(double lineHeight);
    double estimatedLineHeight() const { return m_estimatedLineHeight; }

    void setEstimatedCharsPerLine(int charsPerLine);
    int estimatedCharsPerLine() const { return m_estimatedCharsPerLine; }

    double getParagraphY(size_t index) const;
    size_t getParagraphAtY(double y) const;
    double getParagraphHeight(size_t index) const;
    HeightState getHeightState(size_t index) const;

    void setParagraphHeight(size_t index, double height);
    void invalidateParagraphHeight(size_t index);

    double totalHeight() const;
    size_t calculatedParagraphCount() const;

    // =========================================================================
    // QTextDocument Access
    // =========================================================================

    QTextDocument* document() { return m_document.get(); }
    const QTextDocument* document() const { return m_document.get(); }

private:
    double estimateHeight(const QString& text) const;
    void initializeHeights();

    void notifyTextChanged();
    void notifyParagraphInserted(size_t index);
    void notifyParagraphRemoved(size_t index);
    void notifyParagraphChanged(size_t index);
    void notifyHeightChanged(size_t index, double oldHeight, double newHeight);

    void onDocumentContentsChanged();

    std::unique_ptr<QTextDocument> m_document;
    HeightTree m_heightTree;
    std::vector<ParagraphHeightInfo> m_heights;

    double m_estimatedLineHeight = 20.0;
    int m_estimatedCharsPerLine = 80;

    mutable QString m_plainTextCache;
    mutable bool m_plainTextCached = false;

    size_t m_calculatedCount = 0;

    std::vector<ITextBufferObserver*> m_observers;

    bool m_internalModification = false;
};

}  // namespace kalahari::editor
