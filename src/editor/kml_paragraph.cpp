/// @file kml_paragraph.cpp
/// @brief Implementation of KML Paragraph element (OpenSpec #00042 Phase 1.6)

#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/kml_inline_elements.h>
#include <kalahari/editor/kml_text_run.h>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// Constructors / Destructor
// =============================================================================

KmlParagraph::KmlParagraph()
    : m_elements()
    , m_styleId()
    , m_comments()
{
}

KmlParagraph::KmlParagraph(const QString& text)
    : m_elements()
    , m_styleId()
    , m_comments()
{
    if (!text.isEmpty()) {
        m_elements.push_back(std::make_unique<KmlTextRun>(text));
    }
}

KmlParagraph::KmlParagraph(const QString& text, const QString& styleId)
    : m_elements()
    , m_styleId(styleId)
    , m_comments()
{
    if (!text.isEmpty()) {
        m_elements.push_back(std::make_unique<KmlTextRun>(text));
    }
}

KmlParagraph::~KmlParagraph() = default;

KmlParagraph::KmlParagraph(const KmlParagraph& other)
    : m_elements()
    , m_styleId(other.m_styleId)
    , m_comments(other.m_comments)
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
    , m_comments(std::move(other.m_comments))
{
}

KmlParagraph& KmlParagraph::operator=(const KmlParagraph& other)
{
    if (this != &other) {
        m_styleId = other.m_styleId;
        m_comments = other.m_comments;

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
        m_comments = std::move(other.m_comments);
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
// Inline Formatting (Phase 7.2)
// =============================================================================

namespace {

/// @brief Create an inline container element for the given type
/// @param type The element type (Bold, Italic, Underline, Strikethrough)
/// @return New inline container, or nullptr if type is not supported
std::unique_ptr<KmlInlineContainer> createInlineContainer(ElementType type) {
    switch (type) {
        case ElementType::Bold:
            return std::make_unique<KmlBold>();
        case ElementType::Italic:
            return std::make_unique<KmlItalic>();
        case ElementType::Underline:
            return std::make_unique<KmlUnderline>();
        case ElementType::Strikethrough:
            return std::make_unique<KmlStrikethrough>();
        default:
            return nullptr;
    }
}

/// @brief Check if an element or its ancestors have the specified format type
/// @param element The element to check
/// @param type The format type to check for
/// @return true if the element is wrapped in the specified format type
bool isElementType(const KmlElement* element, ElementType type) {
    return element && element->type() == type;
}

/// @brief Recursively check if element has formatting type at given offset
/// @param element The element to check
/// @param offset The character offset within the element
/// @param type The format type to check for
/// @param currentOffset Running offset counter
/// @param ancestorHasFormat true if an ancestor has the format
/// @return true if the character at offset has the specified formatting
bool hasFormatAtRecursive(const KmlElement* element, int offset, ElementType type,
                          int& currentOffset, bool ancestorHasFormat) {
    if (!element) {
        return false;
    }

    // Check if this element is the format type
    bool thisHasFormat = ancestorHasFormat || (element->type() == type);

    // If this is a text run, check if offset falls within it
    if (element->type() == ElementType::Text) {
        int len = element->length();
        if (offset >= currentOffset && offset < currentOffset + len) {
            return thisHasFormat;
        }
        currentOffset += len;
        return false;
    }

    // For inline containers, check children
    auto* container = dynamic_cast<const KmlInlineContainer*>(element);
    if (container) {
        for (int i = 0; i < container->childCount(); ++i) {
            if (hasFormatAtRecursive(container->childAt(i), offset, type,
                                     currentOffset, thisHasFormat)) {
                return true;
            }
        }
    }

    return false;
}

}  // namespace

bool KmlParagraph::applyInlineFormat(int start, int end, ElementType formatType)
{
    // Normalize range
    if (start > end) {
        std::swap(start, end);
    }

    const int totalLength = length();

    // Validate range
    if (start < 0 || end > totalLength || start == end) {
        return start == end;  // Empty range is valid but no-op
    }

    // For simplicity, we use a rebuild approach:
    // 1. Extract plain text for the affected range
    // 2. Split the range and wrap it in the format element
    // 3. Reconstruct elements

    // Get the text in the range
    QString rangeText = plainText().mid(start, end - start);

    // Build new element list:
    // - Elements before start (unchanged)
    // - Formatted element containing the range text
    // - Elements after end (unchanged)

    std::vector<std::unique_ptr<KmlElement>> newElements;
    int currentOffset = 0;

    for (size_t i = 0; i < m_elements.size(); ++i) {
        auto& element = m_elements[i];
        if (!element) {
            continue;
        }

        const int elemLen = element->length();
        const int elemStart = currentOffset;
        const int elemEnd = currentOffset + elemLen;

        // Case 1: Element is entirely before start - keep it
        if (elemEnd <= start) {
            newElements.push_back(std::move(element));
        }
        // Case 2: Element is entirely after end - keep it
        else if (elemStart >= end) {
            // On first element after end, insert the formatted element
            if (newElements.empty() || newElements.back()->type() != formatType ||
                dynamic_cast<KmlInlineContainer*>(newElements.back().get()) == nullptr) {
                // Check if we need to add the formatted element
                bool needFormatElement = true;
                for (const auto& e : newElements) {
                    if (e && e->type() == formatType) {
                        needFormatElement = false;
                        break;
                    }
                }
                if (needFormatElement && !rangeText.isEmpty()) {
                    auto formatElem = createInlineContainer(formatType);
                    if (formatElem) {
                        formatElem->appendChild(std::make_unique<KmlTextRun>(rangeText));
                        newElements.push_back(std::move(formatElem));
                    }
                    rangeText.clear();  // Mark as consumed
                }
            }
            newElements.push_back(std::move(element));
        }
        // Case 3: Element overlaps with range
        else {
            // Handle text runs specifically
            if (element->type() == ElementType::Text) {
                auto* textRun = static_cast<KmlTextRun*>(element.get());
                QString text = textRun->text();

                // Text before range
                if (elemStart < start) {
                    int beforeLen = start - elemStart;
                    newElements.push_back(
                        std::make_unique<KmlTextRun>(text.left(beforeLen), textRun->styleId()));
                }

                // The formatted part will be added after processing all overlapping elements

                // Text after range (if this element extends past end)
                if (elemEnd > end) {
                    // First add the formatted element
                    if (!rangeText.isEmpty()) {
                        auto formatElem = createInlineContainer(formatType);
                        if (formatElem) {
                            formatElem->appendChild(std::make_unique<KmlTextRun>(rangeText));
                            newElements.push_back(std::move(formatElem));
                        }
                        rangeText.clear();
                    }

                    int afterStart = end - elemStart;
                    newElements.push_back(
                        std::make_unique<KmlTextRun>(text.mid(afterStart), textRun->styleId()));
                }
            }
            // For other elements (formatted), we need more complex handling
            // For now, treat the content as plain text and re-wrap
            else {
                // Get text portions
                QString elemText = element->plainText();

                // Text before range in this element
                if (elemStart < start) {
                    int beforeLen = start - elemStart;
                    newElements.push_back(
                        std::make_unique<KmlTextRun>(elemText.left(beforeLen)));
                }

                // Text after range in this element
                if (elemEnd > end) {
                    // First add the formatted element
                    if (!rangeText.isEmpty()) {
                        auto formatElem = createInlineContainer(formatType);
                        if (formatElem) {
                            formatElem->appendChild(std::make_unique<KmlTextRun>(rangeText));
                            newElements.push_back(std::move(formatElem));
                        }
                        rangeText.clear();
                    }

                    int afterStart = end - elemStart;
                    newElements.push_back(
                        std::make_unique<KmlTextRun>(elemText.mid(afterStart)));
                }
            }
        }

        currentOffset = elemEnd;
    }

    // If formatted element wasn't added yet (range at end), add it now
    if (!rangeText.isEmpty()) {
        auto formatElem = createInlineContainer(formatType);
        if (formatElem) {
            formatElem->appendChild(std::make_unique<KmlTextRun>(rangeText));
            newElements.push_back(std::move(formatElem));
        }
    }

    // Replace elements
    m_elements = std::move(newElements);

    return true;
}

bool KmlParagraph::removeInlineFormat(int start, int end, ElementType formatType)
{
    // Normalize range
    if (start > end) {
        std::swap(start, end);
    }

    const int totalLength = length();

    // Validate range
    if (start < 0 || end > totalLength || start == end) {
        return start == end;
    }

    // For simplicity, flatten all formatting in the range to plain text
    // This is a simplified approach - a full implementation would preserve
    // nested formatting that isn't being removed

    std::vector<std::unique_ptr<KmlElement>> newElements;
    int currentOffset = 0;

    for (size_t i = 0; i < m_elements.size(); ++i) {
        auto& element = m_elements[i];
        if (!element) {
            continue;
        }

        const int elemLen = element->length();
        const int elemStart = currentOffset;
        const int elemEnd = currentOffset + elemLen;

        // Case 1: Element is entirely outside the range - keep it
        if (elemEnd <= start || elemStart >= end) {
            newElements.push_back(std::move(element));
        }
        // Case 2: Element overlaps with range
        else {
            // If this is the target format type, unwrap its content
            if (element->type() == formatType) {
                auto* container = dynamic_cast<KmlInlineContainer*>(element.get());
                if (container) {
                    // Move children out of the container
                    for (int j = 0; j < container->childCount(); ++j) {
                        auto child = container->removeChild(0);  // Always remove first
                        if (child) {
                            newElements.push_back(std::move(child));
                        }
                    }
                }
            }
            // For text runs or other format types, handle partial overlap
            else if (element->type() == ElementType::Text) {
                auto* textRun = static_cast<KmlTextRun*>(element.get());
                QString text = textRun->text();

                // Calculate overlap
                int overlapStart = std::max(start, elemStart);
                int overlapEnd = std::min(end, elemEnd);

                // Part before overlap
                if (elemStart < overlapStart) {
                    newElements.push_back(
                        std::make_unique<KmlTextRun>(text.left(overlapStart - elemStart)));
                }

                // Part within overlap - just plain text
                newElements.push_back(
                    std::make_unique<KmlTextRun>(
                        text.mid(overlapStart - elemStart, overlapEnd - overlapStart)));

                // Part after overlap
                if (elemEnd > overlapEnd) {
                    newElements.push_back(
                        std::make_unique<KmlTextRun>(text.mid(overlapEnd - elemStart)));
                }
            }
            // Other format types - keep as is for now
            else {
                newElements.push_back(std::move(element));
            }
        }

        currentOffset = elemEnd;
    }

    m_elements = std::move(newElements);
    return true;
}

bool KmlParagraph::hasFormatAt(int offset, ElementType formatType) const
{
    if (offset < 0 || offset >= length()) {
        return false;
    }

    int currentOffset = 0;
    for (const auto& element : m_elements) {
        if (!element) {
            continue;
        }

        // Check if this element contains the offset
        if (hasFormatAtRecursive(element.get(), offset, formatType, currentOffset, false)) {
            return true;
        }
    }

    return false;
}

bool KmlParagraph::hasFormatInRange(int start, int end, ElementType formatType) const
{
    // Normalize
    if (start > end) {
        std::swap(start, end);
    }

    if (start < 0 || end > length() || start == end) {
        return false;
    }

    // Check every character in the range
    for (int i = start; i < end; ++i) {
        if (!hasFormatAt(i, formatType)) {
            return false;
        }
    }

    return true;
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
// Comments (Phase 7.8)
// =============================================================================

const QList<KmlComment>& KmlParagraph::comments() const
{
    return m_comments;
}

int KmlParagraph::commentCount() const
{
    return m_comments.size();
}

void KmlParagraph::addComment(const KmlComment& comment)
{
    m_comments.append(comment);
}

bool KmlParagraph::removeComment(const QString& commentId)
{
    for (int i = 0; i < m_comments.size(); ++i) {
        if (m_comments[i].id() == commentId) {
            m_comments.removeAt(i);
            return true;
        }
    }
    return false;
}

KmlComment* KmlParagraph::commentById(const QString& id)
{
    for (auto& comment : m_comments) {
        if (comment.id() == id) {
            return &comment;
        }
    }
    return nullptr;
}

const KmlComment* KmlParagraph::commentById(const QString& id) const
{
    for (const auto& comment : m_comments) {
        if (comment.id() == id) {
            return &comment;
        }
    }
    return nullptr;
}

bool KmlParagraph::hasComments() const
{
    return !m_comments.isEmpty();
}

QList<KmlComment> KmlParagraph::commentsInRange(int start, int end) const
{
    QList<KmlComment> result;

    for (const auto& comment : m_comments) {
        // Check if comment range overlaps with query range
        // Overlap: comment.startPos < end && comment.endPos > start
        if (comment.startPos() < end && comment.endPos() > start) {
            result.append(comment);
        }
    }

    return result;
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

    // Serialize comments if any (Phase 7.8)
    if (!m_comments.isEmpty()) {
        result += QStringLiteral("<comments>");
        for (const auto& comment : m_comments) {
            result += comment.toKml();
        }
        result += QStringLiteral("</comments>");
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
