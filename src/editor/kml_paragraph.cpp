/// @file kml_paragraph.cpp
/// @brief Implementation of KML Paragraph element (OpenSpec #00042 Phase 1.6)

#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/kml_text_run.h>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// Constructors / Destructor
// =============================================================================

KmlParagraph::KmlParagraph()
    : m_elements()
    , m_styleId()
{
}

KmlParagraph::KmlParagraph(const QString& text)
    : m_elements()
    , m_styleId()
{
    if (!text.isEmpty()) {
        m_elements.push_back(std::make_unique<KmlTextRun>(text));
    }
}

KmlParagraph::KmlParagraph(const QString& text, const QString& styleId)
    : m_elements()
    , m_styleId(styleId)
{
    if (!text.isEmpty()) {
        m_elements.push_back(std::make_unique<KmlTextRun>(text));
    }
}

KmlParagraph::~KmlParagraph() = default;

KmlParagraph::KmlParagraph(const KmlParagraph& other)
    : m_elements()
    , m_styleId(other.m_styleId)
{
    // Deep copy all elements
    m_elements.reserve(other.m_elements.size());
    for (const auto& element : other.m_elements) {
        if (element) {
            m_elements.push_back(element->clone());
        }
    }
}

KmlParagraph::KmlParagraph(KmlParagraph&& other) noexcept
    : m_elements(std::move(other.m_elements))
    , m_styleId(std::move(other.m_styleId))
{
}

KmlParagraph& KmlParagraph::operator=(const KmlParagraph& other)
{
    if (this != &other) {
        m_styleId = other.m_styleId;

        // Deep copy all elements
        m_elements.clear();
        m_elements.reserve(other.m_elements.size());
        for (const auto& element : other.m_elements) {
            if (element) {
                m_elements.push_back(element->clone());
            }
        }
    }
    return *this;
}

KmlParagraph& KmlParagraph::operator=(KmlParagraph&& other) noexcept
{
    if (this != &other) {
        m_elements = std::move(other.m_elements);
        m_styleId = std::move(other.m_styleId);
    }
    return *this;
}

// =============================================================================
// Element container methods
// =============================================================================

int KmlParagraph::elementCount() const
{
    return static_cast<int>(m_elements.size());
}

const KmlElement* KmlParagraph::elementAt(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= m_elements.size()) {
        return nullptr;
    }
    return m_elements[static_cast<size_t>(index)].get();
}

KmlElement* KmlParagraph::elementAt(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_elements.size()) {
        return nullptr;
    }
    return m_elements[static_cast<size_t>(index)].get();
}

void KmlParagraph::addElement(std::unique_ptr<KmlElement> element)
{
    if (element) {
        m_elements.push_back(std::move(element));
    }
}

void KmlParagraph::insertElement(int index, std::unique_ptr<KmlElement> element)
{
    if (element) {
        if (index < 0) {
            index = 0;
        }
        if (static_cast<size_t>(index) >= m_elements.size()) {
            m_elements.push_back(std::move(element));
        } else {
            m_elements.insert(m_elements.begin() + index, std::move(element));
        }
    }
}

std::unique_ptr<KmlElement> KmlParagraph::removeElement(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_elements.size()) {
        return nullptr;
    }
    auto element = std::move(m_elements[static_cast<size_t>(index)]);
    m_elements.erase(m_elements.begin() + index);
    return element;
}

void KmlParagraph::clearElements()
{
    m_elements.clear();
}

const std::vector<std::unique_ptr<KmlElement>>& KmlParagraph::elements() const
{
    return m_elements;
}

// =============================================================================
// Content methods
// =============================================================================

QString KmlParagraph::plainText() const
{
    QString result;
    for (const auto& element : m_elements) {
        if (element) {
            result += element->plainText();
        }
    }
    return result;
}

int KmlParagraph::length() const
{
    int total = 0;
    for (const auto& element : m_elements) {
        if (element) {
            total += element->length();
        }
    }
    return total;
}

bool KmlParagraph::isEmpty() const
{
    return length() == 0;
}

int KmlParagraph::characterCount() const
{
    return length();
}

// =============================================================================
// Text manipulation methods (Phase 1.7)
// =============================================================================

bool KmlParagraph::insertText(int offset, const QString& text)
{
    // Empty text is a no-op but still valid
    if (text.isEmpty()) {
        return true;
    }

    const int totalLength = length();

    // Validate offset (allow inserting at the end)
    if (offset < 0 || offset > totalLength) {
        return false;
    }

    // Handle empty paragraph - create new text run
    if (m_elements.empty()) {
        m_elements.push_back(std::make_unique<KmlTextRun>(text));
        return true;
    }

    // Find the element containing this offset
    int currentOffset = 0;
    for (auto& element : m_elements) {
        if (!element) {
            continue;
        }

        const int elementLength = element->length();
        const int elementEnd = currentOffset + elementLength;

        // Check if offset falls within this element (or right at its end)
        if (offset >= currentOffset && offset <= elementEnd) {
            const int localOffset = offset - currentOffset;

            // Try to insert into a KmlTextRun directly
            if (element->type() == ElementType::Text) {
                auto* textRun = static_cast<KmlTextRun*>(element.get());
                QString currentText = textRun->text();
                currentText.insert(localOffset, text);
                textRun->setText(currentText);
                return true;
            }

            // For formatted elements, we need to insert at the right position
            // For now, create a new text run at the appropriate position
            // This handles the case of inserting at element boundaries
            if (localOffset == 0) {
                // Insert before this element
                auto it = std::find_if(m_elements.begin(), m_elements.end(),
                    [&element](const auto& e) { return e.get() == element.get(); });
                if (it != m_elements.end()) {
                    m_elements.insert(it, std::make_unique<KmlTextRun>(text));
                    return true;
                }
            } else if (localOffset == elementLength) {
                // Insert after this element - handled in next iteration or at end
                currentOffset = elementEnd;
                continue;
            } else {
                // Insert in the middle of a non-text element
                // For inline formatting elements, we would need to split them
                // For now, insert after this element (simplified behavior)
                auto it = std::find_if(m_elements.begin(), m_elements.end(),
                    [&element](const auto& e) { return e.get() == element.get(); });
                if (it != m_elements.end()) {
                    ++it;  // Insert after
                    m_elements.insert(it, std::make_unique<KmlTextRun>(text));
                    return true;
                }
            }
        }

        currentOffset = elementEnd;
    }

    // Offset is at the very end - append new text run
    m_elements.push_back(std::make_unique<KmlTextRun>(text));
    return true;
}

bool KmlParagraph::deleteText(int start, int end)
{
    // Normalize the range
    if (start > end) {
        std::swap(start, end);
    }

    const int totalLength = length();

    // Validate range
    if (start < 0 || end > totalLength || start == end) {
        return start == end;  // Empty range is valid but no-op
    }

    // Collect elements to remove and track modifications
    std::vector<size_t> elementsToRemove;
    int currentOffset = 0;

    for (size_t i = 0; i < m_elements.size(); ++i) {
        auto& element = m_elements[i];
        if (!element) {
            continue;
        }

        const int elementLength = element->length();
        const int elementStart = currentOffset;
        const int elementEnd = currentOffset + elementLength;

        // Check if this element overlaps with the deletion range
        if (elementEnd > start && elementStart < end) {
            // Calculate the overlap
            const int deleteStart = std::max(start - elementStart, 0);
            const int deleteEnd = std::min(end - elementStart, elementLength);

            if (element->type() == ElementType::Text) {
                auto* textRun = static_cast<KmlTextRun*>(element.get());
                QString currentText = textRun->text();
                currentText.remove(deleteStart, deleteEnd - deleteStart);
                textRun->setText(currentText);

                // Mark for removal if empty
                if (textRun->length() == 0) {
                    elementsToRemove.push_back(i);
                }
            } else {
                // For formatted elements, if we're deleting the entire element
                if (deleteStart == 0 && deleteEnd == elementLength) {
                    elementsToRemove.push_back(i);
                }
                // Partial deletion of formatted elements is complex - skip for now
                // In a full implementation, we'd need to modify child elements
            }
        }

        currentOffset = elementEnd;
    }

    // Remove empty elements (in reverse order to maintain indices)
    for (auto it = elementsToRemove.rbegin(); it != elementsToRemove.rend(); ++it) {
        m_elements.erase(m_elements.begin() + static_cast<std::ptrdiff_t>(*it));
    }

    return true;
}

std::unique_ptr<KmlParagraph> KmlParagraph::splitAt(int offset)
{
    const int totalLength = length();

    // Cannot split at position 0 (nothing to split) or beyond the end
    if (offset <= 0 || offset > totalLength) {
        return nullptr;
    }

    // Splitting at the end creates an empty paragraph
    if (offset == totalLength) {
        auto newPara = std::make_unique<KmlParagraph>();
        newPara->setStyleId(m_styleId);
        return newPara;
    }

    // Create the new paragraph with same style
    auto newPara = std::make_unique<KmlParagraph>();
    newPara->setStyleId(m_styleId);

    // Find split point and split elements
    int currentOffset = 0;
    size_t splitElementIndex = 0;
    int localSplitOffset = 0;

    for (size_t i = 0; i < m_elements.size(); ++i) {
        auto& element = m_elements[i];
        if (!element) {
            continue;
        }

        const int elementLength = element->length();
        const int elementEnd = currentOffset + elementLength;

        if (offset >= currentOffset && offset < elementEnd) {
            // Split occurs within this element
            splitElementIndex = i;
            localSplitOffset = offset - currentOffset;
            break;
        } else if (offset == elementEnd) {
            // Split occurs at element boundary
            splitElementIndex = i + 1;
            localSplitOffset = 0;
            break;
        }

        currentOffset = elementEnd;
    }

    // Handle split within an element
    if (localSplitOffset > 0 && splitElementIndex < m_elements.size()) {
        auto& element = m_elements[splitElementIndex];

        if (element && element->type() == ElementType::Text) {
            auto* textRun = static_cast<KmlTextRun*>(element.get());
            QString fullText = textRun->text();
            QString afterText = fullText.mid(localSplitOffset);
            fullText.truncate(localSplitOffset);
            textRun->setText(fullText);

            // Add remaining text to new paragraph
            if (!afterText.isEmpty()) {
                newPara->addElement(std::make_unique<KmlTextRun>(afterText, textRun->styleId()));
            }
            splitElementIndex++;
        } else {
            // For formatted elements, clone the entire element to new paragraph
            // This is simplified - a full implementation would split child elements
            if (element) {
                newPara->addElement(element->clone());
            }
            splitElementIndex++;
        }
    }

    // Move remaining elements to new paragraph
    for (size_t i = splitElementIndex; i < m_elements.size(); ++i) {
        if (m_elements[i]) {
            newPara->addElement(std::move(m_elements[i]));
        }
    }

    // Remove moved elements from this paragraph
    if (splitElementIndex < m_elements.size()) {
        m_elements.erase(m_elements.begin() + static_cast<std::ptrdiff_t>(splitElementIndex),
                         m_elements.end());
    }

    return newPara;
}

void KmlParagraph::mergeWith(KmlParagraph& other)
{
    // Move all elements from other to this paragraph
    for (auto& element : other.m_elements) {
        if (element) {
            m_elements.push_back(std::move(element));
        }
    }

    // Clear the other paragraph
    other.m_elements.clear();
}

// =============================================================================
// Style methods
// =============================================================================

const QString& KmlParagraph::styleId() const
{
    return m_styleId;
}

void KmlParagraph::setStyleId(const QString& styleId)
{
    m_styleId = styleId;
}

bool KmlParagraph::hasStyle() const
{
    return !m_styleId.isEmpty();
}

// =============================================================================
// Serialization
// =============================================================================

/// @brief Escape a string for use as an XML attribute value
/// @param value The string to escape
/// @return The escaped string (with &, <, >, ", ' replaced by entities)
static QString escapeXmlAttribute(const QString& value)
{
    QString result;
    result.reserve(value.size() + 16);  // Small extra buffer for entities

    for (const QChar& ch : value) {
        switch (ch.unicode()) {
            case '&':
                result += QStringLiteral("&amp;");
                break;
            case '<':
                result += QStringLiteral("&lt;");
                break;
            case '>':
                result += QStringLiteral("&gt;");
                break;
            case '"':
                result += QStringLiteral("&quot;");
                break;
            case '\'':
                result += QStringLiteral("&apos;");
                break;
            default:
                result += ch;
                break;
        }
    }

    return result;
}

QString KmlParagraph::toKml() const
{
    QString result;

    // Opening tag with optional style attribute
    if (m_styleId.isEmpty()) {
        result = QStringLiteral("<p>");
    } else {
        // Escape special characters in style attribute
        result = QStringLiteral("<p style=\"") + escapeXmlAttribute(m_styleId) + QStringLiteral("\">");
    }

    // Serialize all child elements
    for (const auto& element : m_elements) {
        if (element) {
            result += element->toKml();
        }
    }

    // Closing tag
    result += QStringLiteral("</p>");

    return result;
}

std::unique_ptr<KmlParagraph> KmlParagraph::clone() const
{
    return std::make_unique<KmlParagraph>(*this);
}

}  // namespace kalahari::editor
