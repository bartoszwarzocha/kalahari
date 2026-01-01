/// @file height_tree.cpp
/// @brief Fenwick tree implementation for paragraph height queries

#include "kalahari/editor/height_tree.h"

#include <stdexcept>
#include <algorithm>

namespace kalahari::editor {

HeightTree::HeightTree(size_t size, double defaultHeight)
    : m_size(0)
{
    resize(size, defaultHeight);
}

void HeightTree::resize(size_t size, double defaultHeight)
{
    m_size = size;
    m_heights.assign(size, defaultHeight);
    // Fenwick tree is 1-indexed, size+1 elements
    m_tree.assign(size + 1, 0.0);
    rebuild();
}

size_t HeightTree::size() const
{
    return m_size;
}

bool HeightTree::empty() const
{
    return m_size == 0;
}

void HeightTree::clear()
{
    m_size = 0;
    m_heights.clear();
    m_tree.clear();
}

void HeightTree::setHeight(size_t index, double height)
{
    if (index >= m_size) {
        throw std::out_of_range("HeightTree::setHeight: index out of range");
    }

    double delta = height - m_heights[index];
    m_heights[index] = height;
    updateTree(index, delta);
}

double HeightTree::height(size_t index) const
{
    if (index >= m_size) {
        throw std::out_of_range("HeightTree::height: index out of range");
    }
    return m_heights[index];
}

double HeightTree::prefixSum(size_t index) const
{
    if (index == 0) {
        return 0.0;
    }
    if (index > m_size) {
        index = m_size;
    }
    // prefixSum(index) = sum of heights[0..index-1]
    // This equals queryTree(index) because tree is 1-indexed
    return queryTree(index);
}

double HeightTree::totalHeight() const
{
    return prefixSum(m_size);
}

size_t HeightTree::findIndexForY(double y) const
{
    if (empty()) {
        return 0;
    }

    if (y < 0.0) {
        return 0;
    }

    double total = totalHeight();
    if (y >= total) {
        return m_size;
    }

    // Binary search: find smallest index where prefixSum(index+1) > y
    // This means the paragraph at 'index' contains position y
    //
    // We search for the paragraph where:
    //   prefixSum(index) <= y < prefixSum(index+1)
    //
    // Using binary search on Fenwick tree for O(log N) complexity
    size_t pos = 0;
    double sum = 0.0;

    // Find the largest power of 2 <= m_size
    size_t mask = 1;
    while (mask <= m_size) {
        mask <<= 1;
    }
    mask >>= 1;

    // Binary search through Fenwick tree
    while (mask > 0) {
        size_t next = pos + mask;
        if (next <= m_size && sum + m_tree[next] <= y) {
            pos = next;
            sum += m_tree[next];
        }
        mask >>= 1;
    }

    // pos is now 0-based index where y falls within paragraph pos
    return pos;
}

void HeightTree::insert(size_t index, double height)
{
    if (index > m_size) {
        throw std::out_of_range("HeightTree::insert: index out of range");
    }

    // Insert into heights array
    m_heights.insert(m_heights.begin() + static_cast<std::ptrdiff_t>(index), height);
    ++m_size;

    // Resize and rebuild tree
    m_tree.assign(m_size + 1, 0.0);
    rebuild();
}

void HeightTree::remove(size_t index)
{
    if (index >= m_size) {
        throw std::out_of_range("HeightTree::remove: index out of range");
    }

    // Remove from heights array
    m_heights.erase(m_heights.begin() + static_cast<std::ptrdiff_t>(index));
    --m_size;

    // Resize and rebuild tree
    if (m_size == 0) {
        m_tree.clear();
    } else {
        m_tree.assign(m_size + 1, 0.0);
        rebuild();
    }
}

size_t HeightTree::lowbit(size_t x)
{
    // For 1-indexed Fenwick tree traversal
    return x & (~x + 1);  // Equivalent to x & (-x) for unsigned
}

void HeightTree::rebuild()
{
    // Clear tree
    std::fill(m_tree.begin(), m_tree.end(), 0.0);

    // Build Fenwick tree from heights
    for (size_t i = 0; i < m_size; ++i) {
        updateTree(i, m_heights[i]);
    }
}

void HeightTree::updateTree(size_t index, double delta)
{
    // Convert to 1-indexed
    size_t i = index + 1;

    // Update all affected tree nodes
    while (i <= m_size) {
        m_tree[i] += delta;
        i += lowbit(i);
    }
}

double HeightTree::queryTree(size_t index) const
{
    // index is already the 1-based count of elements
    // Query prefix sum of first 'index' elements
    double sum = 0.0;
    size_t i = index;

    while (i > 0) {
        sum += m_tree[i];
        i -= lowbit(i);
    }

    return sum;
}

}  // namespace kalahari::editor
