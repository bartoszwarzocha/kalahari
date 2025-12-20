/// @file kml_document.cpp
/// @brief Implementation of KML Document (OpenSpec #00042 Phase 1.8)

#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/editor_types.h>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// Constructor / Destructor
// =============================================================================

KmlDocument::KmlDocument()
    : m_paragraphs()
    , m_observers()
    , m_modified(false)
{
}

KmlDocument::~KmlDocument() = default;

KmlDocument::KmlDocument(const KmlDocument& other)
    : m_paragraphs()
    , m_observers()  // Observers are NOT copied
    , m_modified(other.m_modified)
{
    m_paragraphs.reserve(other.m_paragraphs.size());
    for (const auto& para : other.m_paragraphs) {
        if (para) {
            m_paragraphs.push_back(para->clone());
        }
    }
}

KmlDocument::KmlDocument(KmlDocument&& other) noexcept
    : m_paragraphs(std::move(other.m_paragraphs))
    , m_observers(std::move(other.m_observers))
    , m_modified(other.m_modified)
{
}

KmlDocument& KmlDocument::operator=(const KmlDocument& other)
{
    if (this != &other) {
        m_paragraphs.clear();
        m_paragraphs.reserve(other.m_paragraphs.size());
        for (const auto& para : other.m_paragraphs) {
            if (para) {
                m_paragraphs.push_back(para->clone());
            }
        }
        // Observers are NOT copied
        m_modified = other.m_modified;
    }
    return *this;
}

KmlDocument& KmlDocument::operator=(KmlDocument&& other) noexcept
{
    if (this != &other) {
        m_paragraphs = std::move(other.m_paragraphs);
        m_observers = std::move(other.m_observers);
        m_modified = other.m_modified;
    }
    return *this;
}

// =============================================================================
// Observer Management
// =============================================================================

void KmlDocument::addObserver(IDocumentObserver* observer)
{
    if (observer) {
        // Check if already added
        auto it = std::find(m_observers.begin(), m_observers.end(), observer);
        if (it == m_observers.end()) {
            m_observers.push_back(observer);
        }
    }
}

void KmlDocument::removeObserver(IDocumentObserver* observer)
{
    m_observers.erase(
        std::remove(m_observers.begin(), m_observers.end(), observer),
        m_observers.end()
    );
}

// =============================================================================
// Paragraph Container Methods
// =============================================================================

int KmlDocument::paragraphCount() const
{
    return static_cast<int>(m_paragraphs.size());
}

const KmlParagraph* KmlDocument::paragraph(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= m_paragraphs.size()) {
        return nullptr;
    }
    return m_paragraphs[static_cast<size_t>(index)].get();
}

KmlParagraph* KmlDocument::paragraph(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_paragraphs.size()) {
        return nullptr;
    }
    return m_paragraphs[static_cast<size_t>(index)].get();
}

void KmlDocument::addParagraph(std::unique_ptr<KmlParagraph> paragraph)
{
    if (paragraph) {
        int index = static_cast<int>(m_paragraphs.size());
        m_paragraphs.push_back(std::move(paragraph));
        m_modified = true;
        notifyParagraphInserted(index);
        notifyContentChanged();
    }
}

void KmlDocument::insertParagraph(int index, std::unique_ptr<KmlParagraph> paragraph)
{
    if (paragraph) {
        if (index < 0) {
            index = 0;
        }
        if (static_cast<size_t>(index) >= m_paragraphs.size()) {
            // Insert at end
            index = static_cast<int>(m_paragraphs.size());
            m_paragraphs.push_back(std::move(paragraph));
        } else {
            m_paragraphs.insert(m_paragraphs.begin() + index, std::move(paragraph));
        }
        m_modified = true;
        notifyParagraphInserted(index);
        notifyContentChanged();
    }
}

std::unique_ptr<KmlParagraph> KmlDocument::removeParagraph(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_paragraphs.size()) {
        return nullptr;
    }

    auto paragraph = std::move(m_paragraphs[static_cast<size_t>(index)]);
    m_paragraphs.erase(m_paragraphs.begin() + index);
    m_modified = true;
    notifyParagraphRemoved(index);
    notifyContentChanged();
    return paragraph;
}

void KmlDocument::clear()
{
    if (!m_paragraphs.empty()) {
        m_paragraphs.clear();
        m_modified = true;
        notifyContentChanged();
    }
}

bool KmlDocument::isEmpty() const
{
    return m_paragraphs.empty();
}

const std::vector<std::unique_ptr<KmlParagraph>>& KmlDocument::paragraphs() const
{
    return m_paragraphs;
}

// =============================================================================
// Content Methods
// =============================================================================

QString KmlDocument::plainText() const
{
    QString result;
    bool first = true;

    for (const auto& para : m_paragraphs) {
        if (para) {
            if (!first) {
                result += QChar('\n');
            }
            result += para->plainText();
            first = false;
        }
    }

    return result;
}

int KmlDocument::length() const
{
    int total = 0;
    for (const auto& para : m_paragraphs) {
        if (para) {
            total += para->length();
        }
    }
    return total;
}

// =============================================================================
// Text Operations (Phase 1.9)
// =============================================================================

bool KmlDocument::insertText(const CursorPosition& position, const QString& text)
{
    // Empty text is a no-op but valid
    if (text.isEmpty()) {
        return true;
    }

    // Validate paragraph index
    if (position.paragraph < 0 ||
        static_cast<size_t>(position.paragraph) >= m_paragraphs.size()) {
        return false;
    }

    auto* para = m_paragraphs[static_cast<size_t>(position.paragraph)].get();
    if (!para) {
        return false;
    }

    // Delegate to paragraph's insertText
    bool success = para->insertText(position.offset, text);
    if (success) {
        m_modified = true;
        notifyParagraphModified(position.paragraph);
    }

    return success;
}

bool KmlDocument::deleteText(const CursorPosition& start, const CursorPosition& end)
{
    // Normalize range so start <= end
    CursorPosition normStart = start;
    CursorPosition normEnd = end;
    if (start > end) {
        std::swap(normStart, normEnd);
    }

    // Empty range is a no-op
    if (normStart == normEnd) {
        return true;
    }

    // Validate paragraph indices
    if (normStart.paragraph < 0 || normEnd.paragraph < 0 ||
        static_cast<size_t>(normStart.paragraph) >= m_paragraphs.size() ||
        static_cast<size_t>(normEnd.paragraph) >= m_paragraphs.size()) {
        return false;
    }

    // Case 1: Single paragraph deletion
    if (normStart.paragraph == normEnd.paragraph) {
        auto* para = m_paragraphs[static_cast<size_t>(normStart.paragraph)].get();
        if (!para) {
            return false;
        }

        bool success = para->deleteText(normStart.offset, normEnd.offset);
        if (success) {
            m_modified = true;
            notifyParagraphModified(normStart.paragraph);
        }
        return success;
    }

    // Case 2: Multi-paragraph deletion
    // Strategy:
    // 1. Delete from start.offset to end of first paragraph
    // 2. Delete intermediate paragraphs entirely
    // 3. Delete from start of last paragraph to end.offset
    // 4. Merge the first and last paragraphs

    auto* firstPara = m_paragraphs[static_cast<size_t>(normStart.paragraph)].get();
    auto* lastPara = m_paragraphs[static_cast<size_t>(normEnd.paragraph)].get();
    if (!firstPara || !lastPara) {
        return false;
    }

    // Delete from start position to end of first paragraph
    int firstParaLength = firstPara->length();
    if (normStart.offset < firstParaLength) {
        firstPara->deleteText(normStart.offset, firstParaLength);
    }

    // Delete from start of last paragraph to end position
    if (normEnd.offset > 0) {
        lastPara->deleteText(0, normEnd.offset);
    }

    // Merge remaining content of last paragraph into first
    firstPara->mergeWith(*lastPara);

    // Remove intermediate paragraphs and the last paragraph (now empty)
    // Remove from back to front to maintain valid indices
    for (int i = normEnd.paragraph; i > normStart.paragraph; --i) {
        m_paragraphs.erase(m_paragraphs.begin() + i);
        notifyParagraphRemoved(i);
    }

    m_modified = true;
    notifyParagraphModified(normStart.paragraph);
    notifyContentChanged();

    return true;
}

bool KmlDocument::applyStyle(const SelectionRange& range, const QString& styleId)
{
    // Normalize range
    SelectionRange normRange = range.normalized();

    // Validate paragraph indices
    if (normRange.start.paragraph < 0 || normRange.end.paragraph < 0 ||
        static_cast<size_t>(normRange.start.paragraph) >= m_paragraphs.size() ||
        static_cast<size_t>(normRange.end.paragraph) >= m_paragraphs.size()) {
        return false;
    }

    // Apply style to all paragraphs in the range
    for (int i = normRange.start.paragraph; i <= normRange.end.paragraph; ++i) {
        auto* para = m_paragraphs[static_cast<size_t>(i)].get();
        if (para) {
            para->setStyleId(styleId);
            notifyParagraphModified(i);
        }
    }

    m_modified = true;
    return true;
}

bool KmlDocument::splitParagraph(const CursorPosition& position)
{
    // Validate paragraph index
    if (position.paragraph < 0 ||
        static_cast<size_t>(position.paragraph) >= m_paragraphs.size()) {
        return false;
    }

    auto* para = m_paragraphs[static_cast<size_t>(position.paragraph)].get();
    if (!para) {
        return false;
    }

    // Handle special case: split at position 0 creates empty paragraph before
    std::unique_ptr<KmlParagraph> newPara;
    if (position.offset == 0) {
        // Create empty paragraph before (with same style)
        newPara = std::make_unique<KmlParagraph>();
        newPara->setStyleId(para->styleId());
        // Insert the empty paragraph at current position
        m_paragraphs.insert(
            m_paragraphs.begin() + position.paragraph,
            std::move(newPara)
        );
        m_modified = true;
        notifyParagraphInserted(position.paragraph);
        notifyContentChanged();
        return true;
    }

    // Split at the specified offset
    newPara = para->splitAt(position.offset);
    if (!newPara) {
        // splitAt returns nullptr if offset is 0 or out of range
        // We handled offset 0 above, so this means offset is out of range
        // For offset at end (or past), create empty paragraph
        if (position.offset >= para->length()) {
            newPara = std::make_unique<KmlParagraph>();
            newPara->setStyleId(para->styleId());
        } else {
            return false;
        }
    }

    // Insert the new paragraph after the current one
    int newIndex = position.paragraph + 1;
    m_paragraphs.insert(
        m_paragraphs.begin() + newIndex,
        std::move(newPara)
    );

    m_modified = true;
    // Only notify paragraph modified (no content change since we'll notify for insert)
    for (auto* observer : m_observers) {
        if (observer) {
            observer->onParagraphModified(position.paragraph);
        }
    }
    notifyParagraphInserted(newIndex);
    notifyContentChanged();

    return true;
}

int KmlDocument::mergeParagraphWithPrevious(int paragraphIndex)
{
    // Cannot merge first paragraph (nothing before it)
    if (paragraphIndex <= 0) {
        return -1;
    }

    // Validate index
    if (static_cast<size_t>(paragraphIndex) >= m_paragraphs.size()) {
        return -1;
    }

    auto* prevPara = m_paragraphs[static_cast<size_t>(paragraphIndex - 1)].get();
    auto* currPara = m_paragraphs[static_cast<size_t>(paragraphIndex)].get();

    if (!prevPara || !currPara) {
        return -1;
    }

    // Remember the length of the previous paragraph (this is where the cursor should go)
    int cursorOffset = prevPara->length();

    // Merge current paragraph content into previous
    prevPara->mergeWith(*currPara);

    // Remove the now-empty paragraph
    m_paragraphs.erase(m_paragraphs.begin() + paragraphIndex);

    m_modified = true;
    // Only notify paragraph modified (no content change since we'll notify for remove)
    for (auto* observer : m_observers) {
        if (observer) {
            observer->onParagraphModified(paragraphIndex - 1);
        }
    }
    notifyParagraphRemoved(paragraphIndex);
    notifyContentChanged();

    return cursorOffset;
}

// =============================================================================
// Modification Tracking
// =============================================================================

bool KmlDocument::isModified() const
{
    return m_modified;
}

void KmlDocument::setModified(bool modified)
{
    m_modified = modified;
}

void KmlDocument::resetModified()
{
    m_modified = false;
}

// =============================================================================
// Serialization
// =============================================================================

QString KmlDocument::toKml() const
{
    QString result;
    result += QStringLiteral("<document>\n");

    for (const auto& para : m_paragraphs) {
        if (para) {
            result += para->toKml();
            result += QChar('\n');
        }
    }

    result += QStringLiteral("</document>");
    return result;
}

std::unique_ptr<KmlDocument> KmlDocument::clone() const
{
    auto cloned = std::make_unique<KmlDocument>(*this);
    // Observers are NOT cloned (already handled in copy constructor)
    return cloned;
}

// =============================================================================
// Notification Methods
// =============================================================================

void KmlDocument::notifyParagraphModified(int index)
{
    if (index >= 0 && static_cast<size_t>(index) < m_paragraphs.size()) {
        m_modified = true;
        for (auto* observer : m_observers) {
            if (observer) {
                observer->onParagraphModified(index);
            }
        }
        notifyContentChanged();
    }
}

void KmlDocument::notifyContentChanged()
{
    for (auto* observer : m_observers) {
        if (observer) {
            observer->onContentChanged();
        }
    }
}

void KmlDocument::notifyParagraphInserted(int index)
{
    for (auto* observer : m_observers) {
        if (observer) {
            observer->onParagraphInserted(index);
        }
    }
}

void KmlDocument::notifyParagraphRemoved(int index)
{
    for (auto* observer : m_observers) {
        if (observer) {
            observer->onParagraphRemoved(index);
        }
    }
}

}  // namespace kalahari::editor
