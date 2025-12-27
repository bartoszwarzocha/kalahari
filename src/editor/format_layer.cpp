/// @file format_layer.cpp
/// @brief FormatLayer implementation (OpenSpec #00043 Phase 3)

#include <kalahari/editor/format_layer.h>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// IntervalTree Implementation
// =============================================================================

void IntervalTree::insert(const FormatRange& range) {
    if (range.isEmpty()) return;
    insertNode(m_root, range);
    m_size++;
}

void IntervalTree::insertNode(std::unique_ptr<IntervalTreeNode>& node, const FormatRange& range) {
    if (!node) {
        node = std::make_unique<IntervalTreeNode>(range);
        return;
    }

    // BST insert based on start position
    if (range.start < node->range.start) {
        insertNode(node->left, range);
    } else {
        insertNode(node->right, range);
    }

    // Update maxEnd
    updateMaxEnd(node.get());
}

void IntervalTree::updateMaxEnd(IntervalTreeNode* node) {
    if (!node) return;

    node->maxEnd = node->range.end;
    if (node->left && node->left->maxEnd > node->maxEnd) {
        node->maxEnd = node->left->maxEnd;
    }
    if (node->right && node->right->maxEnd > node->maxEnd) {
        node->maxEnd = node->right->maxEnd;
    }
}

size_t IntervalTree::removeIf(std::function<bool(const FormatRange&)> predicate) {
    size_t removedCount = 0;
    m_root = removeIfNode(std::move(m_root), predicate, removedCount);
    m_size -= removedCount;
    return removedCount;
}

std::unique_ptr<IntervalTreeNode> IntervalTree::removeIfNode(
    std::unique_ptr<IntervalTreeNode> node,
    std::function<bool(const FormatRange&)>& predicate,
    size_t& removedCount) {

    if (!node) return nullptr;

    // Recursively process children
    node->left = removeIfNode(std::move(node->left), predicate, removedCount);
    node->right = removeIfNode(std::move(node->right), predicate, removedCount);

    // Check if this node should be removed
    if (predicate(node->range)) {
        removedCount++;

        // Case 1: No children
        if (!node->left && !node->right) {
            return nullptr;
        }

        // Case 2: One child
        if (!node->left) {
            return std::move(node->right);
        }
        if (!node->right) {
            return std::move(node->left);
        }

        // Case 3: Two children - find successor
        IntervalTreeNode* successor = node->right.get();
        while (successor->left) {
            successor = successor->left.get();
        }
        node->range = successor->range;

        // Remove successor
        std::function<bool(const FormatRange&)> removeSuccessor =
            [&successor](const FormatRange& r) {
                return r.start == successor->range.start && r.end == successor->range.end;
            };
        size_t dummy = 0;
        node->right = removeIfNode(std::move(node->right), removeSuccessor, dummy);
    }

    updateMaxEnd(node.get());
    return node;
}

std::vector<FormatRange> IntervalTree::findAt(size_t position) const {
    std::vector<FormatRange> result;
    collectAt(m_root.get(), position, result);
    return result;
}

void IntervalTree::collectAt(const IntervalTreeNode* node, size_t position,
                              std::vector<FormatRange>& result) const {
    if (!node) return;

    // Check if this node's range contains position
    if (node->range.contains(position)) {
        result.push_back(node->range);
    }

    // Check left subtree if it might contain overlapping ranges
    if (node->left && node->left->maxEnd > position) {
        collectAt(node->left.get(), position, result);
    }

    // Check right subtree if the range starts before position
    if (node->right && node->range.start <= position) {
        collectAt(node->right.get(), position, result);
    }
}

std::vector<FormatRange> IntervalTree::findOverlapping(size_t start, size_t end) const {
    std::vector<FormatRange> result;
    collectOverlapping(m_root.get(), start, end, result);
    return result;
}

void IntervalTree::collectOverlapping(const IntervalTreeNode* node, size_t start, size_t end,
                                       std::vector<FormatRange>& result) const {
    if (!node) return;

    // Check if this node's range overlaps [start, end)
    if (node->range.start < end && node->range.end > start) {
        result.push_back(node->range);
    }

    // Check left subtree if it might contain overlapping ranges
    if (node->left && node->left->maxEnd > start) {
        collectOverlapping(node->left.get(), start, end, result);
    }

    // Check right subtree
    if (node->right && node->range.start < end) {
        collectOverlapping(node->right.get(), start, end, result);
    }
}

std::vector<FormatRange> IntervalTree::all() const {
    std::vector<FormatRange> result;
    result.reserve(m_size);
    collectAll(m_root.get(), result);
    return result;
}

void IntervalTree::collectAll(const IntervalTreeNode* node, std::vector<FormatRange>& result) const {
    if (!node) return;
    collectAll(node->left.get(), result);
    result.push_back(node->range);
    collectAll(node->right.get(), result);
}

void IntervalTree::clear() {
    m_root.reset();
    m_size = 0;
}

void IntervalTree::shiftRanges(size_t position, int delta) {
    shiftNode(m_root.get(), position, delta);
}

void IntervalTree::shiftNode(IntervalTreeNode* node, size_t position, int delta) {
    if (!node) return;

    // Shift ranges that start at or after position
    if (node->range.start >= position) {
        if (delta > 0) {
            node->range.start += static_cast<size_t>(delta);
            node->range.end += static_cast<size_t>(delta);
        } else {
            size_t absDelta = static_cast<size_t>(-delta);
            if (node->range.start >= absDelta) {
                node->range.start -= absDelta;
            } else {
                node->range.start = 0;
            }
            if (node->range.end >= absDelta) {
                node->range.end -= absDelta;
            } else {
                node->range.end = 0;
            }
        }
    }
    // Shift end of ranges that span the position
    else if (node->range.end > position) {
        if (delta > 0) {
            node->range.end += static_cast<size_t>(delta);
        } else {
            size_t absDelta = static_cast<size_t>(-delta);
            size_t deleteEnd = position + absDelta;

            // Shrink or remove range based on deletion
            if (node->range.end <= deleteEnd) {
                node->range.end = position;
            } else {
                node->range.end -= absDelta;
            }
        }
    }

    // Update maxEnd after shifting
    updateMaxEnd(node);

    // Recursively shift children
    shiftNode(node->left.get(), position, delta);
    shiftNode(node->right.get(), position, delta);
}

// =============================================================================
// FormatLayer Implementation
// =============================================================================

void FormatLayer::addFormat(size_t start, size_t end, const TextFormat& format) {
    if (start >= end || format.isEmpty()) return;

    FormatRange range;
    range.start = start;
    range.end = end;
    range.format = format;

    m_tree.insert(range);
}

void FormatLayer::removeFormat(size_t start, size_t end, FormatType type) {
    if (start >= end) return;

    // Get all overlapping ranges
    auto overlapping = m_tree.findOverlapping(start, end);

    // Remove them
    m_tree.removeIf([&](const FormatRange& r) {
        return r.start < end && r.end > start && r.format.hasFlag(type);
    });

    // Re-add parts outside the removal range with same format
    for (const auto& range : overlapping) {
        if (!range.format.hasFlag(type)) continue;

        // Part before removal range
        if (range.start < start) {
            FormatRange before;
            before.start = range.start;
            before.end = start;
            before.format = range.format;
            m_tree.insert(before);
        }

        // Part after removal range
        if (range.end > end) {
            FormatRange after;
            after.start = end;
            after.end = range.end;
            after.format = range.format;
            m_tree.insert(after);
        }
    }
}

void FormatLayer::clearFormats(size_t start, size_t end) {
    if (start >= end) return;

    // Get overlapping ranges first
    auto overlapping = m_tree.findOverlapping(start, end);

    // Remove all ranges that fully fall within [start, end)
    m_tree.removeIf([&](const FormatRange& r) {
        return r.start >= start && r.end <= end;
    });

    // For partially overlapping ranges, split them
    for (const auto& range : overlapping) {
        if (range.start >= start && range.end <= end) {
            continue;  // Already removed
        }

        // Remove original
        m_tree.removeIf([&](const FormatRange& r) {
            return r.start == range.start && r.end == range.end;
        });

        // Re-add part before
        if (range.start < start) {
            FormatRange before;
            before.start = range.start;
            before.end = start;
            before.format = range.format;
            m_tree.insert(before);
        }

        // Re-add part after
        if (range.end > end) {
            FormatRange after;
            after.start = end;
            after.end = range.end;
            after.format = range.format;
            m_tree.insert(after);
        }
    }
}

void FormatLayer::clearAll() {
    m_tree.clear();
}

bool FormatLayer::toggleFormat(size_t start, size_t end, FormatType type) {
    if (start >= end) return false;

    // Check if format is currently applied throughout the range
    bool hasFormat = hasFormatInRange(start, end, type);

    if (hasFormat) {
        removeFormat(start, end, type);
        return false;
    } else {
        TextFormat format;
        format.flags = type;
        addFormat(start, end, format);
        return true;
    }
}

std::vector<FormatRange> FormatLayer::getFormatsAt(size_t position) const {
    return m_tree.findAt(position);
}

TextFormat FormatLayer::getMergedFormatAt(size_t position) const {
    auto ranges = m_tree.findAt(position);
    TextFormat merged;

    for (const auto& range : ranges) {
        merged = merged.merged(range.format);
    }

    return merged;
}

std::vector<FormatRange> FormatLayer::getFormatsForParagraph(const TextBuffer& buffer,
                                                               size_t paragraphIndex) const {
    if (paragraphIndex >= buffer.paragraphCount()) {
        return {};
    }

    // Calculate character offset for paragraph start
    size_t start = 0;
    for (size_t i = 0; i < paragraphIndex; ++i) {
        start += static_cast<size_t>(buffer.paragraphLength(i));
    }

    size_t end = start + static_cast<size_t>(buffer.paragraphLength(paragraphIndex));

    return getFormatsInRange(start, end);
}

std::vector<FormatRange> FormatLayer::getFormatsInRange(size_t start, size_t end) const {
    return m_tree.findOverlapping(start, end);
}

bool FormatLayer::hasFormatAt(size_t position, FormatType type) const {
    auto ranges = m_tree.findAt(position);
    for (const auto& range : ranges) {
        if (range.format.hasFlag(type)) {
            return true;
        }
    }
    return false;
}

bool FormatLayer::hasFormatInRange(size_t start, size_t end, FormatType type) const {
    // Check each position in range (simplified - could be optimized)
    for (size_t pos = start; pos < end; ++pos) {
        if (!hasFormatAt(pos, type)) {
            return false;
        }
    }
    return true;
}

void FormatLayer::onTextInserted(size_t position, size_t length) {
    m_tree.shiftRanges(position, static_cast<int>(length));
}

void FormatLayer::onTextDeleted(size_t position, size_t length) {
    // Remove ranges completely within deleted region
    size_t deleteEnd = position + length;
    m_tree.removeIf([&](const FormatRange& r) {
        return r.start >= position && r.end <= deleteEnd;
    });

    // Shift remaining ranges
    m_tree.shiftRanges(position, -static_cast<int>(length));
}

// ITextBufferObserver implementation

void FormatLayer::onTextChanged() {
    // Full text change - clear all formats
    clearAll();
}

void FormatLayer::onParagraphInserted(size_t index) {
    // Calculate character position from paragraph index
    if (!m_buffer) return;

    size_t position = 0;
    for (size_t i = 0; i < index; ++i) {
        position += static_cast<size_t>(m_buffer->paragraphLength(i));
    }

    // Shift by paragraph length + newline
    size_t length = static_cast<size_t>(m_buffer->paragraphLength(index)) + 1;
    onTextInserted(position, length);
}

void FormatLayer::onParagraphRemoved(size_t index) {
    // Cannot determine exact position without buffer content before removal
    // This is a limitation - caller should use onTextDeleted directly
    (void)index;
}

void FormatLayer::onParagraphChanged(size_t index) {
    // Paragraph content changed - invalidate formats for this paragraph
    if (!m_buffer) return;

    size_t position = 0;
    for (size_t i = 0; i < index; ++i) {
        position += static_cast<size_t>(m_buffer->paragraphLength(i));
    }

    size_t length = static_cast<size_t>(m_buffer->paragraphLength(index));
    clearFormats(position, position + length);
}

void FormatLayer::onHeightChanged(size_t /*index*/, double /*oldHeight*/, double /*newHeight*/) {
    // Height changes don't affect formatting
}

void FormatLayer::attachToBuffer(TextBuffer* buffer) {
    if (m_buffer) {
        m_buffer->removeObserver(this);
    }
    m_buffer = buffer;
    if (m_buffer) {
        m_buffer->addObserver(this);
    }
}

void FormatLayer::detachFromBuffer() {
    if (m_buffer) {
        m_buffer->removeObserver(this);
        m_buffer = nullptr;
    }
}

void FormatLayer::coalesceRanges() {
    // Get all ranges sorted by start position
    auto ranges = m_tree.all();
    if (ranges.size() < 2) return;

    // Sort by start
    std::sort(ranges.begin(), ranges.end());

    // Rebuild tree with coalesced ranges
    m_tree.clear();

    FormatRange current = ranges[0];
    for (size_t i = 1; i < ranges.size(); ++i) {
        const auto& next = ranges[i];

        // Can coalesce if adjacent and same format
        if (current.end == next.start && current.format == next.format) {
            current.end = next.end;
        } else {
            m_tree.insert(current);
            current = next;
        }
    }
    m_tree.insert(current);
}

}  // namespace kalahari::editor
