/**
 * @file lazy_layout_prototype.h
 * @brief Lazy layout prototype for OpenSpec #00043 - Phase 1 Research
 *
 * Demonstrates:
 * - Height estimation for off-screen paragraphs
 * - Layout calculation only for visible paragraphs
 * - Virtual scrolling with estimated total height
 * - Fenwick tree for O(log N) prefix sums
 */

#pragma once

#include <QString>
#include <QVector>
#include <QFont>
#include <QFontMetrics>
#include <QTextLayout>
#include <optional>
#include <cmath>

namespace prototype {

//=============================================================================
// Fenwick Tree (Binary Indexed Tree) for O(log N) prefix sums
//=============================================================================

/**
 * @brief Fenwick tree for efficient prefix sum queries
 *
 * Used to calculate cumulative paragraph heights in O(log N)
 * instead of O(N) linear scan.
 */
class FenwickTree {
public:
    explicit FenwickTree(size_t size) : m_tree(size + 1, 0.0) {}

    /**
     * @brief Add delta to value at index (0-based)
     * @param index 0-based index
     * @param delta Value to add
     */
    void update(size_t index, double delta) {
        for (size_t i = index + 1; i < m_tree.size(); i += lowbit(i)) {
            m_tree[i] += delta;
        }
    }

    /**
     * @brief Get prefix sum [0, index] (0-based, inclusive)
     * @param index 0-based index
     * @return Sum of values from 0 to index
     */
    double prefixSum(size_t index) const {
        double sum = 0.0;
        for (size_t i = index + 1; i > 0; i -= lowbit(i)) {
            sum += m_tree[i];
        }
        return sum;
    }

    /**
     * @brief Get value at specific index
     */
    double get(size_t index) const {
        if (index == 0) return prefixSum(0);
        return prefixSum(index) - prefixSum(index - 1);
    }

    /**
     * @brief Find index where prefix sum reaches target (binary search)
     * @param targetY Y coordinate to find
     * @return Paragraph index containing targetY
     */
    size_t findIndexForY(double targetY) const {
        size_t n = m_tree.size() - 1;
        size_t pos = 0;
        double sum = 0.0;

        // Binary search using Fenwick tree structure
        for (size_t bit = highestBit(n); bit > 0; bit >>= 1) {
            size_t next = pos + bit;
            if (next < m_tree.size() && sum + m_tree[next] <= targetY) {
                pos = next;
                sum += m_tree[next];
            }
        }
        return pos;
    }

private:
    std::vector<double> m_tree;

    // Extract lowest set bit (avoids unary minus on unsigned)
    static size_t lowbit(size_t x) {
        return x & ((~x) + 1);  // Equivalent to x & (-x) but safe for unsigned
    }

    static size_t highestBit(size_t n) {
        size_t bit = 1;
        while (bit <= n) bit <<= 1;
        return bit >> 1;
    }
};

//=============================================================================
// Paragraph Layout State
//=============================================================================

/**
 * @brief Layout state for a single paragraph
 */
struct ParagraphLayout {
    enum class State {
        NotCalculated,  // Using estimated height
        Calculated,     // Layout computed, height is accurate
        Invalid         // Content changed, needs recalculation
    };

    State state = State::NotCalculated;
    double height = 0.0;           // Actual or estimated height
    double estimatedHeight = 0.0;  // Height estimation (never changes)
    int lineCount = 0;             // Number of lines (if calculated)

    // Note: In production we would cache the layout, but QTextLayout is not
    // copyable so for this prototype we recalculate on demand. In the real
    // implementation we'd use unique_ptr<QTextLayout> or a cache map.

    bool isCalculated() const { return state == State::Calculated; }
};

//=============================================================================
// Lazy Layout Manager
//=============================================================================

/**
 * @brief Manages lazy layout calculation for paragraphs
 *
 * Key concepts:
 * - Uses estimated heights for all paragraphs initially
 * - Only calculates actual layout for visible paragraphs
 * - Fenwick tree provides O(log N) Y-to-paragraph mapping
 * - Buffer zone around viewport for smooth scrolling
 */
class LazyLayoutManager {
public:
    LazyLayoutManager() = default;

    /**
     * @brief Initialize with paragraphs
     * @param paragraphs List of paragraph texts
     * @param font Font to use for layout
     * @param viewportWidth Width available for text
     */
    void initialize(const QStringList& paragraphs, const QFont& /*font*/, int viewportWidth) {
        m_viewportWidth = viewportWidth;
        m_paragraphs = paragraphs;

        // Use fixed values to avoid Qt font subsystem (which triggers RNG crash)
        m_lineHeight = 20.0;  // Typical line height
        m_averageCharsPerLine = viewportWidth / 8;  // ~8px per char average

        // Initialize layouts with estimated heights
        m_layouts.clear();
        m_layouts.resize(paragraphs.size());

        m_heightTree = FenwickTree(static_cast<size_t>(paragraphs.size()));

        for (int i = 0; i < paragraphs.size(); ++i) {
            double estimated = estimateHeight(paragraphs[i]);
            m_layouts[static_cast<size_t>(i)].estimatedHeight = estimated;
            m_layouts[static_cast<size_t>(i)].height = estimated;
            m_layouts[static_cast<size_t>(i)].state = ParagraphLayout::State::NotCalculated;
            m_heightTree.update(static_cast<size_t>(i), estimated);
        }

        m_calculatedCount = 0;
    }

    /**
     * @brief Update visible range and calculate layouts as needed
     * @param scrollY Current scroll position
     * @param viewportHeight Visible area height
     * @param bufferZone Extra paragraphs to calculate above/below viewport
     */
    void updateVisibleRange(double scrollY, int viewportHeight, int bufferZone = 5) {
        if (m_paragraphs.isEmpty()) return;

        // Find first visible paragraph using Fenwick tree
        size_t firstVisible = m_heightTree.findIndexForY(scrollY);
        if (firstVisible > 0) --firstVisible; // Include partial paragraph above

        // Find last visible paragraph
        double bottomY = scrollY + viewportHeight;
        size_t lastVisible = m_heightTree.findIndexForY(bottomY);
        size_t paragraphCount = static_cast<size_t>(m_paragraphs.size());
        if (lastVisible < paragraphCount - 1) ++lastVisible;

        // Apply buffer zone
        size_t firstWithBuffer = (firstVisible > static_cast<size_t>(bufferZone))
            ? firstVisible - bufferZone : 0;
        size_t lastWithBuffer = std::min(lastVisible + static_cast<size_t>(bufferZone),
            paragraphCount - 1);

        m_visibleFirst = firstWithBuffer;
        m_visibleLast = lastWithBuffer;

        // Calculate layouts for visible + buffer paragraphs
        for (size_t i = firstWithBuffer; i <= lastWithBuffer; ++i) {
            ensureLayoutCalculated(i);
        }

        // Optionally: release layouts far outside viewport to save memory
        // (not implemented in this prototype)
    }

    /**
     * @brief Get Y position of paragraph start
     * @param index Paragraph index
     * @return Y coordinate
     */
    double getYPosition(size_t index) const {
        if (index == 0) return 0.0;
        return m_heightTree.prefixSum(index - 1);
    }

    /**
     * @brief Get total document height
     */
    double getTotalHeight() const {
        if (m_paragraphs.isEmpty()) return 0.0;
        return m_heightTree.prefixSum(m_paragraphs.size() - 1);
    }

    /**
     * @brief Get paragraph at Y position
     * @param y Y coordinate
     * @return Paragraph index
     */
    size_t getParagraphAtY(double y) const {
        return m_heightTree.findIndexForY(y);
    }

    /**
     * @brief Get layout for paragraph (may trigger calculation)
     */
    const ParagraphLayout& getLayout(size_t index) {
        ensureLayoutCalculated(index);
        return m_layouts[index];
    }

    /**
     * @brief Get number of paragraphs with calculated layouts
     */
    size_t getCalculatedCount() const { return m_calculatedCount; }

    /**
     * @brief Get total paragraph count
     */
    size_t getParagraphCount() const { return m_paragraphs.size(); }

    /**
     * @brief Get visible range
     */
    std::pair<size_t, size_t> getVisibleRange() const {
        return {m_visibleFirst, m_visibleLast};
    }

    /**
     * @brief Invalidate paragraph (content changed)
     */
    void invalidateParagraph(size_t index) {
        if (index >= m_layouts.size()) return;

        auto& layout = m_layouts[index];
        if (layout.state == ParagraphLayout::State::Calculated) {
            --m_calculatedCount;
        }

        // Update height tree: remove old height, add estimated
        double oldHeight = layout.height;
        double newEstimate = estimateHeight(m_paragraphs[index]);

        m_heightTree.update(index, newEstimate - oldHeight);
        layout.height = newEstimate;
        layout.estimatedHeight = newEstimate;
        layout.state = ParagraphLayout::State::Invalid;
    }

private:
    /**
     * @brief Estimate paragraph height based on text length
     */
    double estimateHeight(const QString& text) const {
        if (text.isEmpty()) return m_lineHeight;

        // Estimate line count based on character count
        qsizetype chars = text.length();
        qsizetype estimatedLines = std::max(static_cast<qsizetype>(1),
            (chars + m_averageCharsPerLine - 1) / m_averageCharsPerLine);

        return static_cast<double>(estimatedLines) * m_lineHeight;
    }

    /**
     * @brief Ensure layout is calculated for paragraph
     *
     * Note: In production this would use QTextLayout for accurate layout.
     * For this prototype we simulate the calculation to avoid Qt's RNG crash.
     * The key point is that this IS called on-demand, not upfront.
     */
    void ensureLayoutCalculated(size_t index) {
        if (index >= m_layouts.size()) return;

        auto& para = m_layouts[index];
        if (para.state == ParagraphLayout::State::Calculated) return;

        // Simulate actual layout calculation (more accurate than estimation)
        const QString& text = m_paragraphs[static_cast<int>(index)];
        double actualHeight = 0.0;
        int lineCount = 0;

        if (text.isEmpty()) {
            actualHeight = m_lineHeight;
            lineCount = 1;
        } else {
            // Simulate line-by-line processing
            qsizetype currentLineChars = 0;
            for (qsizetype i = 0; i < text.length(); ++i) {
                QChar c = text.at(i);
                if (c == '\n') {
                    lineCount++;
                    actualHeight += m_lineHeight;
                    currentLineChars = 0;
                } else {
                    currentLineChars++;
                    if (currentLineChars >= m_averageCharsPerLine) {
                        lineCount++;
                        actualHeight += m_lineHeight;
                        currentLineChars = 0;
                    }
                }
            }
            if (currentLineChars > 0) {
                lineCount++;
                actualHeight += m_lineHeight;
            }
        }

        // Update Fenwick tree with height difference
        double oldHeight = para.height;
        double heightDiff = actualHeight - oldHeight;

        if (std::abs(heightDiff) > 0.001) {
            m_heightTree.update(index, heightDiff);
        }

        para.height = actualHeight;
        para.lineCount = lineCount;
        para.state = ParagraphLayout::State::Calculated;

        ++m_calculatedCount;
    }

    int m_viewportWidth = 800;
    double m_lineHeight = 20.0;
    qsizetype m_averageCharsPerLine = 80;

    QStringList m_paragraphs;
    std::vector<ParagraphLayout> m_layouts;
    FenwickTree m_heightTree{0};

    size_t m_visibleFirst = 0;
    size_t m_visibleLast = 0;
    size_t m_calculatedCount = 0;
};

//=============================================================================
// Traditional Layout Manager (for comparison)
//=============================================================================

/**
 * @brief Traditional layout that calculates everything upfront
 *
 * Note: This uses estimation-based height calculation to avoid Qt6's
 * QRandomGenerator crash in QTextLayout. In production, this would use
 * actual QTextLayout, but for benchmarking purposes, we simulate the
 * O(N) cost of iterating through all paragraphs upfront.
 *
 * The key difference being demonstrated:
 * - Traditional: iterates ALL paragraphs at init time
 * - Lazy: only calculates visible paragraphs on demand
 */
class TraditionalLayoutManager {
public:
    void initialize(const QStringList& paragraphs, const QFont& /*font*/, int viewportWidth) {
        m_viewportWidth = viewportWidth;
        m_paragraphs = paragraphs;

        // Use fixed values to avoid Qt font subsystem (which triggers RNG crash)
        m_lineHeight = 20.0;  // Typical line height
        m_averageCharsPerLine = viewportWidth / 8;  // ~8px per char average

        m_heights.clear();
        m_heights.reserve(static_cast<size_t>(paragraphs.size()));

        m_cumulativeHeights.clear();
        m_cumulativeHeights.reserve(static_cast<size_t>(paragraphs.size()));

        double cumulative = 0.0;

        // Simulate calculating ALL layouts upfront
        // This demonstrates the O(N) initialization cost
        for (const QString& text : paragraphs) {
            double height = simulateLayoutCalculation(text);
            m_heights.push_back(height);
            cumulative += height;
            m_cumulativeHeights.push_back(cumulative);
        }
    }

    double getTotalHeight() const {
        return m_cumulativeHeights.empty() ? 0.0 : m_cumulativeHeights.back();
    }

    double getYPosition(size_t index) const {
        if (index == 0) return 0.0;
        return m_cumulativeHeights[index - 1];
    }

    size_t getParagraphAtY(double y) const {
        // Binary search for paragraph containing y
        auto it = std::lower_bound(m_cumulativeHeights.begin(),
            m_cumulativeHeights.end(), y);
        return static_cast<size_t>(std::distance(m_cumulativeHeights.begin(), it));
    }

private:
    /**
     * @brief Simulate expensive layout calculation
     *
     * This simulates the work that QTextLayout.beginLayout/createLine/endLayout
     * would do, without triggering Qt's internal QRandomGenerator usage.
     * The actual work is approximated by character-based estimation with
     * additional processing to simulate the O(text length) cost.
     */
    double simulateLayoutCalculation(const QString& text) {
        if (text.isEmpty()) return m_lineHeight;

        // Simulate line-by-line processing (like QTextLayout would do)
        qsizetype chars = text.length();
        qsizetype estimatedLines = 1;
        qsizetype currentLineChars = 0;

        // Process character-by-character to simulate layout work
        for (qsizetype i = 0; i < chars; ++i) {
            QChar c = text.at(i);
            if (c == '\n') {
                estimatedLines++;
                currentLineChars = 0;
            } else {
                currentLineChars++;
                if (currentLineChars >= m_averageCharsPerLine) {
                    estimatedLines++;
                    currentLineChars = 0;
                }
            }
        }

        return static_cast<double>(estimatedLines) * m_lineHeight;
    }

    int m_viewportWidth = 800;
    double m_lineHeight = 20.0;
    qsizetype m_averageCharsPerLine = 80;
    QStringList m_paragraphs;
    std::vector<double> m_heights;
    std::vector<double> m_cumulativeHeights;
};

} // namespace prototype
