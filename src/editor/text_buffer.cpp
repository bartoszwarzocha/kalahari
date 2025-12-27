/// @file text_buffer.cpp
/// @brief TextBuffer implementation (OpenSpec #00043 Phase 2)

#include <kalahari/editor/text_buffer.h>
#include <QTextCursor>
#include <algorithm>
#include <cmath>

namespace kalahari::editor {

//=============================================================================
// HeightTree Implementation
//=============================================================================

HeightTree::HeightTree(size_t size) {
    resize(size);
}

void HeightTree::resize(size_t size) {
    m_size = size;
    m_heights.resize(size, 0.0);
    m_tree.resize(size + 1, 0.0);
    rebuild();
}

void HeightTree::rebuild() {
    std::fill(m_tree.begin(), m_tree.end(), 0.0);
    for (size_t i = 0; i < m_size; ++i) {
        for (size_t j = i + 1; j < m_tree.size(); j += lowbit(j)) {
            m_tree[j] += m_heights[i];
        }
    }
}

void HeightTree::update(size_t index, double delta) {
    if (index >= m_size) return;
    m_heights[index] += delta;
    for (size_t i = index + 1; i < m_tree.size(); i += lowbit(i)) {
        m_tree[i] += delta;
    }
}

void HeightTree::setHeight(size_t index, double height) {
    if (index >= m_size) return;
    double delta = height - m_heights[index];
    update(index, delta);
}

double HeightTree::get(size_t index) const {
    if (index >= m_size) return 0.0;
    return m_heights[index];
}

double HeightTree::prefixSum(size_t index) const {
    if (index >= m_size) {
        index = m_size > 0 ? m_size - 1 : 0;
    }
    double sum = 0.0;
    for (size_t i = index + 1; i > 0; i -= lowbit(i)) {
        sum += m_tree[i];
    }
    return sum;
}

double HeightTree::totalHeight() const {
    if (m_size == 0) return 0.0;
    return prefixSum(m_size - 1);
}

size_t HeightTree::highestBit(size_t n) {
    size_t bit = 1;
    while (bit <= n) bit <<= 1;
    return bit >> 1;
}

size_t HeightTree::findParagraphAtY(double y) const {
    if (m_size == 0 || y <= 0) return 0;

    size_t pos = 0;
    double sum = 0.0;

    for (size_t bit = highestBit(m_size); bit > 0; bit >>= 1) {
        size_t next = pos + bit;
        if (next < m_tree.size() && sum + m_tree[next] <= y) {
            pos = next;
            sum += m_tree[next];
        }
    }

    // pos is now 1-based index, convert to 0-based
    return pos < m_size ? pos : m_size - 1;
}

double HeightTree::getYPosition(size_t index) const {
    if (index == 0) return 0.0;
    return prefixSum(index - 1);
}

void HeightTree::insert(size_t index, double height) {
    if (index > m_size) index = m_size;

    m_heights.insert(m_heights.begin() + static_cast<std::ptrdiff_t>(index), height);
    m_size++;
    m_tree.resize(m_size + 1);
    rebuild();
}

void HeightTree::remove(size_t index) {
    if (index >= m_size) return;

    m_heights.erase(m_heights.begin() + static_cast<std::ptrdiff_t>(index));
    m_size--;
    m_tree.resize(m_size + 1);
    rebuild();
}

void HeightTree::clear() {
    m_heights.clear();
    m_tree.clear();
    m_size = 0;
}

//=============================================================================
// TextBuffer Implementation
//=============================================================================

TextBuffer::TextBuffer()
    : m_document(std::make_unique<QTextDocument>()) {
    // Connect to document changes
    QObject::connect(m_document.get(), &QTextDocument::contentsChanged,
        [this]() { onDocumentContentsChanged(); });
}

TextBuffer::~TextBuffer() = default;

TextBuffer::TextBuffer(TextBuffer&& other) noexcept
    : m_document(std::move(other.m_document))
    , m_heightTree(std::move(other.m_heightTree))
    , m_heights(std::move(other.m_heights))
    , m_estimatedLineHeight(other.m_estimatedLineHeight)
    , m_estimatedCharsPerLine(other.m_estimatedCharsPerLine)
    , m_plainTextCache(std::move(other.m_plainTextCache))
    , m_plainTextCached(other.m_plainTextCached)
    , m_calculatedCount(other.m_calculatedCount)
    , m_observers(std::move(other.m_observers))
    , m_internalModification(other.m_internalModification) {
}

TextBuffer& TextBuffer::operator=(TextBuffer&& other) noexcept {
    if (this != &other) {
        m_document = std::move(other.m_document);
        m_heightTree = std::move(other.m_heightTree);
        m_heights = std::move(other.m_heights);
        m_estimatedLineHeight = other.m_estimatedLineHeight;
        m_estimatedCharsPerLine = other.m_estimatedCharsPerLine;
        m_plainTextCache = std::move(other.m_plainTextCache);
        m_plainTextCached = other.m_plainTextCached;
        m_calculatedCount = other.m_calculatedCount;
        m_observers = std::move(other.m_observers);
        m_internalModification = other.m_internalModification;
    }
    return *this;
}

// Observer Pattern

void TextBuffer::addObserver(ITextBufferObserver* observer) {
    if (observer) {
        m_observers.push_back(observer);
    }
}

void TextBuffer::removeObserver(ITextBufferObserver* observer) {
    m_observers.erase(
        std::remove(m_observers.begin(), m_observers.end(), observer),
        m_observers.end());
}

// Text Content

void TextBuffer::setPlainText(const QString& text) {
    m_internalModification = true;
    m_document->setPlainText(text);
    m_internalModification = false;

    initializeHeights();
    m_plainTextCache = text;
    m_plainTextCached = true;

    notifyTextChanged();
}

QString TextBuffer::plainText() const {
    if (!m_plainTextCached) {
        m_plainTextCache = m_document->toPlainText();
        m_plainTextCached = true;
    }
    return m_plainTextCache;
}

void TextBuffer::invalidatePlainTextCache() {
    m_plainTextCached = false;
}

bool TextBuffer::isEmpty() const {
    return m_document->isEmpty();
}

int TextBuffer::characterCount() const {
    return m_document->characterCount();
}

// Paragraph Access

size_t TextBuffer::paragraphCount() const {
    return static_cast<size_t>(m_document->blockCount());
}

QString TextBuffer::paragraphText(size_t index) const {
    QTextBlock blk = block(index);
    return blk.isValid() ? blk.text() : QString();
}

int TextBuffer::paragraphLength(size_t index) const {
    QTextBlock blk = block(index);
    return blk.isValid() ? blk.length() : 0;
}

QTextBlock TextBuffer::block(size_t index) const {
    if (index >= paragraphCount()) {
        return QTextBlock();
    }
    return m_document->findBlockByNumber(static_cast<int>(index));
}

// Text Modification

void TextBuffer::insert(int position, const QString& text) {
    QTextCursor cursor(m_document.get());
    cursor.setPosition(position);
    cursor.insertText(text);
    invalidatePlainTextCache();
}

void TextBuffer::remove(int position, int length) {
    QTextCursor cursor(m_document.get());
    cursor.setPosition(position);
    cursor.setPosition(position + length, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    invalidatePlainTextCache();
}

void TextBuffer::replace(int position, int length, const QString& text) {
    QTextCursor cursor(m_document.get());
    cursor.setPosition(position);
    cursor.setPosition(position + length, QTextCursor::KeepAnchor);
    cursor.insertText(text);
    invalidatePlainTextCache();
}

void TextBuffer::insertParagraph(size_t index, const QString& text) {
    size_t count = paragraphCount();
    if (index > count) index = count;

    QTextCursor cursor(m_document.get());

    if (index == 0) {
        cursor.movePosition(QTextCursor::Start);
        cursor.insertText(text + "\n");
    } else if (index >= count) {
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("\n" + text);
    } else {
        QTextBlock blk = block(index);
        cursor.setPosition(blk.position());
        cursor.insertText(text + "\n");
    }

    // Update height tracking
    double estimated = estimateHeight(text);
    m_heights.insert(m_heights.begin() + static_cast<std::ptrdiff_t>(index),
        ParagraphHeightInfo{estimated, estimated, HeightState::Estimated});
    m_heightTree.insert(index, estimated);

    invalidatePlainTextCache();
    notifyParagraphInserted(index);
}

void TextBuffer::removeParagraph(size_t index) {
    if (index >= paragraphCount()) return;

    QTextBlock blk = block(index);
    if (!blk.isValid()) return;

    QTextCursor cursor(m_document.get());
    cursor.setPosition(blk.position());

    // Select the entire block including newline
    if (index < paragraphCount() - 1) {
        cursor.setPosition(blk.next().position(), QTextCursor::KeepAnchor);
    } else {
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        if (index > 0) {
            // Also remove preceding newline for last block
            cursor.setPosition(blk.position() - 1, QTextCursor::KeepAnchor);
        }
    }
    cursor.removeSelectedText();

    // Update height tracking
    if (index < m_heights.size()) {
        if (m_heights[index].isCalculated()) {
            m_calculatedCount--;
        }
        m_heights.erase(m_heights.begin() + static_cast<std::ptrdiff_t>(index));
        m_heightTree.remove(index);
    }

    invalidatePlainTextCache();
    notifyParagraphRemoved(index);
}

void TextBuffer::setParagraphText(size_t index, const QString& text) {
    if (index >= paragraphCount()) return;

    QTextBlock blk = block(index);
    if (!blk.isValid()) return;

    QTextCursor cursor(m_document.get());
    cursor.setPosition(blk.position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(text);

    // Invalidate height for this paragraph
    invalidateParagraphHeight(index);
    invalidatePlainTextCache();
    notifyParagraphChanged(index);
}

// Height Management

void TextBuffer::setEstimatedLineHeight(double lineHeight) {
    m_estimatedLineHeight = lineHeight;
}

void TextBuffer::setEstimatedCharsPerLine(int charsPerLine) {
    m_estimatedCharsPerLine = charsPerLine;
}

double TextBuffer::getParagraphY(size_t index) const {
    return m_heightTree.getYPosition(index);
}

size_t TextBuffer::getParagraphAtY(double y) const {
    return m_heightTree.findParagraphAtY(y);
}

double TextBuffer::getParagraphHeight(size_t index) const {
    if (index >= m_heights.size()) return m_estimatedLineHeight;
    return m_heights[index].height;
}

HeightState TextBuffer::getHeightState(size_t index) const {
    if (index >= m_heights.size()) return HeightState::Estimated;
    return m_heights[index].state;
}

void TextBuffer::setParagraphHeight(size_t index, double height) {
    if (index >= m_heights.size()) return;

    auto& info = m_heights[index];
    double oldHeight = info.height;

    if (!info.isCalculated()) {
        m_calculatedCount++;
    }

    info.height = height;
    info.state = HeightState::Calculated;
    m_heightTree.setHeight(index, height);

    if (std::abs(oldHeight - height) > 0.001) {
        notifyHeightChanged(index, oldHeight, height);
    }
}

void TextBuffer::invalidateParagraphHeight(size_t index) {
    if (index >= m_heights.size()) return;

    auto& info = m_heights[index];
    double oldHeight = info.height;

    if (info.isCalculated()) {
        m_calculatedCount--;
    }

    // Recalculate estimate based on current text
    QString text = paragraphText(index);
    double estimated = estimateHeight(text);

    info.estimatedHeight = estimated;
    info.height = estimated;
    info.state = HeightState::Invalid;
    m_heightTree.setHeight(index, estimated);

    if (std::abs(oldHeight - estimated) > 0.001) {
        notifyHeightChanged(index, oldHeight, estimated);
    }
}

double TextBuffer::totalHeight() const {
    return m_heightTree.totalHeight();
}

size_t TextBuffer::calculatedParagraphCount() const {
    return m_calculatedCount;
}

// Private Methods

double TextBuffer::estimateHeight(const QString& text) const {
    if (text.isEmpty()) return m_estimatedLineHeight;

    // Count lines based on newlines and estimated wrapping
    int lineCount = 1;
    int currentLineChars = 0;

    for (QChar c : text) {
        if (c == '\n') {
            lineCount++;
            currentLineChars = 0;
        } else {
            currentLineChars++;
            if (currentLineChars >= m_estimatedCharsPerLine) {
                lineCount++;
                currentLineChars = 0;
            }
        }
    }

    return static_cast<double>(lineCount) * m_estimatedLineHeight;
}

void TextBuffer::initializeHeights() {
    size_t count = paragraphCount();
    m_heights.clear();
    m_heights.reserve(count);
    m_heightTree.resize(count);
    m_calculatedCount = 0;

    for (size_t i = 0; i < count; ++i) {
        QString text = paragraphText(i);
        double estimated = estimateHeight(text);
        m_heights.push_back(ParagraphHeightInfo{estimated, estimated, HeightState::Estimated});
        m_heightTree.setHeight(i, estimated);
    }
}

void TextBuffer::notifyTextChanged() {
    for (auto* observer : m_observers) {
        observer->onTextChanged();
    }
}

void TextBuffer::notifyParagraphInserted(size_t index) {
    for (auto* observer : m_observers) {
        observer->onParagraphInserted(index);
    }
}

void TextBuffer::notifyParagraphRemoved(size_t index) {
    for (auto* observer : m_observers) {
        observer->onParagraphRemoved(index);
    }
}

void TextBuffer::notifyParagraphChanged(size_t index) {
    for (auto* observer : m_observers) {
        observer->onParagraphChanged(index);
    }
}

void TextBuffer::notifyHeightChanged(size_t index, double oldHeight, double newHeight) {
    for (auto* observer : m_observers) {
        observer->onHeightChanged(index, oldHeight, newHeight);
    }
}

void TextBuffer::onDocumentContentsChanged() {
    if (m_internalModification) return;

    // External modification - invalidate cache and reinitialize heights
    invalidatePlainTextCache();
    initializeHeights();
    notifyTextChanged();
}

}  // namespace kalahari::editor
