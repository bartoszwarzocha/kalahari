/// @file kml_inline_elements.cpp
/// @brief Implementation of KML Inline formatting elements (OpenSpec #00042 Phase 1.5)

#include <kalahari/editor/kml_inline_elements.h>
#include <QXmlStreamWriter>

namespace kalahari::editor {

// =============================================================================
// KmlInlineContainer Implementation
// =============================================================================

KmlInlineContainer::KmlInlineContainer()
    : m_children()
{
}

KmlInlineContainer::KmlInlineContainer(const KmlInlineContainer& other)
    : KmlElement(other)
    , m_children()
{
    // Deep copy all children
    m_children.reserve(other.m_children.size());
    for (const auto& child : other.m_children) {
        if (child) {
            m_children.push_back(child->clone());
        }
    }
}

KmlInlineContainer::KmlInlineContainer(KmlInlineContainer&& other) noexcept
    : KmlElement(std::move(other))
    , m_children(std::move(other.m_children))
{
}

KmlInlineContainer& KmlInlineContainer::operator=(const KmlInlineContainer& other)
{
    if (this != &other) {
        KmlElement::operator=(other);

        // Deep copy all children
        m_children.clear();
        m_children.reserve(other.m_children.size());
        for (const auto& child : other.m_children) {
            if (child) {
                m_children.push_back(child->clone());
            }
        }
    }
    return *this;
}

KmlInlineContainer& KmlInlineContainer::operator=(KmlInlineContainer&& other) noexcept
{
    if (this != &other) {
        KmlElement::operator=(std::move(other));
        m_children = std::move(other.m_children);
    }
    return *this;
}

QString KmlInlineContainer::plainText() const
{
    QString result;
    for (const auto& child : m_children) {
        if (child) {
            result += child->plainText();
        }
    }
    return result;
}

int KmlInlineContainer::length() const
{
    int total = 0;
    for (const auto& child : m_children) {
        if (child) {
            total += child->length();
        }
    }
    return total;
}

int KmlInlineContainer::childCount() const
{
    return static_cast<int>(m_children.size());
}

const KmlElement* KmlInlineContainer::childAt(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= m_children.size()) {
        return nullptr;
    }
    return m_children[static_cast<size_t>(index)].get();
}

KmlElement* KmlInlineContainer::childAt(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_children.size()) {
        return nullptr;
    }
    return m_children[static_cast<size_t>(index)].get();
}

void KmlInlineContainer::appendChild(std::unique_ptr<KmlElement> child)
{
    if (child) {
        m_children.push_back(std::move(child));
    }
}

void KmlInlineContainer::insertChild(int index, std::unique_ptr<KmlElement> child)
{
    if (child) {
        if (index < 0) {
            index = 0;
        }
        if (static_cast<size_t>(index) >= m_children.size()) {
            m_children.push_back(std::move(child));
        } else {
            m_children.insert(m_children.begin() + index, std::move(child));
        }
    }
}

std::unique_ptr<KmlElement> KmlInlineContainer::removeChild(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_children.size()) {
        return nullptr;
    }
    auto child = std::move(m_children[static_cast<size_t>(index)]);
    m_children.erase(m_children.begin() + index);
    return child;
}

void KmlInlineContainer::clearChildren()
{
    m_children.clear();
}

const std::vector<std::unique_ptr<KmlElement>>& KmlInlineContainer::children() const
{
    return m_children;
}

QString KmlInlineContainer::childrenToKml() const
{
    QString result;
    for (const auto& child : m_children) {
        if (child) {
            result += child->toKml();
        }
    }
    return result;
}

void KmlInlineContainer::cloneChildrenTo(KmlInlineContainer& target) const
{
    for (const auto& child : m_children) {
        if (child) {
            target.m_children.push_back(child->clone());
        }
    }
}

// =============================================================================
// KmlBold Implementation
// =============================================================================

KmlBold::KmlBold()
    : KmlInlineContainer()
{
}

KmlBold::KmlBold(const KmlBold& other)
    : KmlInlineContainer(other)
{
}

KmlBold::KmlBold(KmlBold&& other) noexcept
    : KmlInlineContainer(std::move(other))
{
}

KmlBold& KmlBold::operator=(const KmlBold& other)
{
    if (this != &other) {
        KmlInlineContainer::operator=(other);
    }
    return *this;
}

KmlBold& KmlBold::operator=(KmlBold&& other) noexcept
{
    if (this != &other) {
        KmlInlineContainer::operator=(std::move(other));
    }
    return *this;
}

ElementType KmlBold::type() const
{
    return ElementType::Bold;
}

QString KmlBold::toKml() const
{
    return QStringLiteral("<b>") + childrenToKml() + QStringLiteral("</b>");
}

std::unique_ptr<KmlElement> KmlBold::clone() const
{
    return std::make_unique<KmlBold>(*this);
}

// =============================================================================
// KmlItalic Implementation
// =============================================================================

KmlItalic::KmlItalic()
    : KmlInlineContainer()
{
}

KmlItalic::KmlItalic(const KmlItalic& other)
    : KmlInlineContainer(other)
{
}

KmlItalic::KmlItalic(KmlItalic&& other) noexcept
    : KmlInlineContainer(std::move(other))
{
}

KmlItalic& KmlItalic::operator=(const KmlItalic& other)
{
    if (this != &other) {
        KmlInlineContainer::operator=(other);
    }
    return *this;
}

KmlItalic& KmlItalic::operator=(KmlItalic&& other) noexcept
{
    if (this != &other) {
        KmlInlineContainer::operator=(std::move(other));
    }
    return *this;
}

ElementType KmlItalic::type() const
{
    return ElementType::Italic;
}

QString KmlItalic::toKml() const
{
    return QStringLiteral("<i>") + childrenToKml() + QStringLiteral("</i>");
}

std::unique_ptr<KmlElement> KmlItalic::clone() const
{
    return std::make_unique<KmlItalic>(*this);
}

// =============================================================================
// KmlUnderline Implementation
// =============================================================================

KmlUnderline::KmlUnderline()
    : KmlInlineContainer()
{
}

KmlUnderline::KmlUnderline(const KmlUnderline& other)
    : KmlInlineContainer(other)
{
}

KmlUnderline::KmlUnderline(KmlUnderline&& other) noexcept
    : KmlInlineContainer(std::move(other))
{
}

KmlUnderline& KmlUnderline::operator=(const KmlUnderline& other)
{
    if (this != &other) {
        KmlInlineContainer::operator=(other);
    }
    return *this;
}

KmlUnderline& KmlUnderline::operator=(KmlUnderline&& other) noexcept
{
    if (this != &other) {
        KmlInlineContainer::operator=(std::move(other));
    }
    return *this;
}

ElementType KmlUnderline::type() const
{
    return ElementType::Underline;
}

QString KmlUnderline::toKml() const
{
    return QStringLiteral("<u>") + childrenToKml() + QStringLiteral("</u>");
}

std::unique_ptr<KmlElement> KmlUnderline::clone() const
{
    return std::make_unique<KmlUnderline>(*this);
}

// =============================================================================
// KmlStrikethrough Implementation
// =============================================================================

KmlStrikethrough::KmlStrikethrough()
    : KmlInlineContainer()
{
}

KmlStrikethrough::KmlStrikethrough(const KmlStrikethrough& other)
    : KmlInlineContainer(other)
{
}

KmlStrikethrough::KmlStrikethrough(KmlStrikethrough&& other) noexcept
    : KmlInlineContainer(std::move(other))
{
}

KmlStrikethrough& KmlStrikethrough::operator=(const KmlStrikethrough& other)
{
    if (this != &other) {
        KmlInlineContainer::operator=(other);
    }
    return *this;
}

KmlStrikethrough& KmlStrikethrough::operator=(KmlStrikethrough&& other) noexcept
{
    if (this != &other) {
        KmlInlineContainer::operator=(std::move(other));
    }
    return *this;
}

ElementType KmlStrikethrough::type() const
{
    return ElementType::Strikethrough;
}

QString KmlStrikethrough::toKml() const
{
    return QStringLiteral("<s>") + childrenToKml() + QStringLiteral("</s>");
}

std::unique_ptr<KmlElement> KmlStrikethrough::clone() const
{
    return std::make_unique<KmlStrikethrough>(*this);
}

// =============================================================================
// KmlSubscript Implementation
// =============================================================================

KmlSubscript::KmlSubscript()
    : KmlInlineContainer()
{
}

KmlSubscript::KmlSubscript(const KmlSubscript& other)
    : KmlInlineContainer(other)
{
}

KmlSubscript::KmlSubscript(KmlSubscript&& other) noexcept
    : KmlInlineContainer(std::move(other))
{
}

KmlSubscript& KmlSubscript::operator=(const KmlSubscript& other)
{
    if (this != &other) {
        KmlInlineContainer::operator=(other);
    }
    return *this;
}

KmlSubscript& KmlSubscript::operator=(KmlSubscript&& other) noexcept
{
    if (this != &other) {
        KmlInlineContainer::operator=(std::move(other));
    }
    return *this;
}

ElementType KmlSubscript::type() const
{
    return ElementType::Subscript;
}

QString KmlSubscript::toKml() const
{
    return QStringLiteral("<sub>") + childrenToKml() + QStringLiteral("</sub>");
}

std::unique_ptr<KmlElement> KmlSubscript::clone() const
{
    return std::make_unique<KmlSubscript>(*this);
}

// =============================================================================
// KmlSuperscript Implementation
// =============================================================================

KmlSuperscript::KmlSuperscript()
    : KmlInlineContainer()
{
}

KmlSuperscript::KmlSuperscript(const KmlSuperscript& other)
    : KmlInlineContainer(other)
{
}

KmlSuperscript::KmlSuperscript(KmlSuperscript&& other) noexcept
    : KmlInlineContainer(std::move(other))
{
}

KmlSuperscript& KmlSuperscript::operator=(const KmlSuperscript& other)
{
    if (this != &other) {
        KmlInlineContainer::operator=(other);
    }
    return *this;
}

KmlSuperscript& KmlSuperscript::operator=(KmlSuperscript&& other) noexcept
{
    if (this != &other) {
        KmlInlineContainer::operator=(std::move(other));
    }
    return *this;
}

ElementType KmlSuperscript::type() const
{
    return ElementType::Superscript;
}

QString KmlSuperscript::toKml() const
{
    return QStringLiteral("<sup>") + childrenToKml() + QStringLiteral("</sup>");
}

std::unique_ptr<KmlElement> KmlSuperscript::clone() const
{
    return std::make_unique<KmlSuperscript>(*this);
}

}  // namespace kalahari::editor
