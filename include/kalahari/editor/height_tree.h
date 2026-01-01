/// @file height_tree.h
/// @brief Fenwick tree for O(log N) paragraph height queries (OpenSpec #00043)
///
/// HeightTree provides efficient prefix sum queries and updates for paragraph
/// heights, enabling fast scroll position calculations in lazy loading scenarios.
/// Used by LazyKmlDocument and ViewportManager for viewport calculations.

#pragma once

#include <vector>
#include <cstddef>

namespace kalahari::editor {

/// @brief Fenwick tree (Binary Indexed Tree) for paragraph height management
///
/// Provides O(log N) operations for:
/// - prefixSum(index): Sum of heights [0, index)
/// - findIndexForY(y): Find paragraph at Y position
/// - setHeight(index, height): Update height at index
///
/// Insert/remove operations trigger full rebuild (O(N)) but are rare in practice.
///
/// Usage:
/// @code
/// HeightTree tree(1000, 24.0);  // 1000 paragraphs, 24px estimated height
///
/// // Get Y position of paragraph 500
/// double y = tree.prefixSum(500);
///
/// // Find paragraph at Y=5000
/// size_t idx = tree.findIndexForY(5000.0);
///
/// // Update height after layout
/// tree.setHeight(500, 32.0);
/// @endcode
///
/// Thread safety: Not thread-safe. Use from GUI thread only.
class HeightTree {
public:
    /// @brief Default constructor creates empty tree
    HeightTree() = default;

    /// @brief Construct tree with given size and default height
    /// @param size Number of elements
    /// @param defaultHeight Default height for all elements
    explicit HeightTree(size_t size, double defaultHeight = 0.0);

    /// @brief Resize tree with default height for all elements
    /// @param size New number of elements
    /// @param defaultHeight Height to assign to all elements
    void resize(size_t size, double defaultHeight = 0.0);

    /// @brief Get number of elements
    /// @return Element count
    size_t size() const;

    /// @brief Check if tree is empty
    /// @return True if no elements
    bool empty() const;

    /// @brief Clear all elements
    void clear();

    /// @brief Set height at index (replaces existing)
    /// @param index Element index (0-based)
    /// @param height New height value
    /// @throws std::out_of_range if index >= size()
    void setHeight(size_t index, double height);

    /// @brief Get height at index
    /// @param index Element index (0-based)
    /// @return Height value
    /// @throws std::out_of_range if index >= size()
    double height(size_t index) const;

    /// @brief Get prefix sum [0, index) - Y position of paragraph
    /// @param index End index (exclusive, 0-based)
    /// @return Sum of heights from 0 to index-1
    ///
    /// prefixSum(0) returns 0.0
    /// prefixSum(1) returns height(0)
    /// prefixSum(n) returns sum of all heights
    double prefixSum(size_t index) const;

    /// @brief Get total height (sum of all elements)
    /// @return Sum of all heights
    double totalHeight() const;

    /// @brief Find index of paragraph at Y position
    /// @param y Y position to search for
    /// @return Paragraph index containing Y, or size() if y >= totalHeight()
    ///
    /// For y=0, returns 0 (unless empty)
    /// Uses binary search on prefix sums for O(log N) complexity
    size_t findIndexForY(double y) const;

    /// @brief Insert element at index (shifts subsequent elements)
    /// @param index Insert position (0-based)
    /// @param height Height of new element
    /// @note O(N) operation - triggers full rebuild
    void insert(size_t index, double height);

    /// @brief Remove element at index (shifts subsequent elements)
    /// @param index Element to remove (0-based)
    /// @note O(N) operation - triggers full rebuild
    /// @throws std::out_of_range if index >= size()
    void remove(size_t index);

private:
    /// @brief Get lowest set bit (for tree traversal)
    /// @param x Index value
    /// @return x & (-x)
    static size_t lowbit(size_t x);

    /// @brief Rebuild Fenwick tree from heights array
    void rebuild();

    /// @brief Update Fenwick tree for delta at index
    /// @param index 0-based index
    /// @param delta Change in height
    void updateTree(size_t index, double delta);

    /// @brief Query Fenwick tree prefix sum [1, index]
    /// @param index 1-based index for tree query
    /// @return Prefix sum
    double queryTree(size_t index) const;

    std::vector<double> m_heights;  ///< Original heights (0-indexed)
    std::vector<double> m_tree;     ///< Fenwick tree (1-indexed)
    size_t m_size = 0;              ///< Number of elements
};

}  // namespace kalahari::editor
