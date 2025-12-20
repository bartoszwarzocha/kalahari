/// @file kml_text_run.cpp
/// @brief Implementation of KML Text Run element (OpenSpec #00042 Phase 1.4)

#include <kalahari/editor/kml_text_run.h>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace kalahari::editor {

// =============================================================================
// Constructors and Destructor
// =============================================================================

KmlTextRun::KmlTextRun()
    : m_text()
    , m_styleId()
{
}

KmlTextRun::KmlTextRun(const QString& text)
    : m_text(text)
    , m_styleId()
{
}

KmlTextRun::KmlTextRun(const QString& text, const QString& styleId)
    : m_text(text)
    , m_styleId(styleId)
{
}

KmlTextRun::KmlTextRun(const KmlTextRun& other)
    : KmlElement(other)
    , m_text(other.m_text)
    , m_styleId(other.m_styleId)
{
}

KmlTextRun::KmlTextRun(KmlTextRun&& other) noexcept
    : KmlElement(std::move(other))
    , m_text(std::move(other.m_text))
    , m_styleId(std::move(other.m_styleId))
{
}

KmlTextRun& KmlTextRun::operator=(const KmlTextRun& other)
{
    if (this != &other) {
        KmlElement::operator=(other);
        m_text = other.m_text;
        m_styleId = other.m_styleId;
    }
    return *this;
}

KmlTextRun& KmlTextRun::operator=(KmlTextRun&& other) noexcept
{
    if (this != &other) {
        KmlElement::operator=(std::move(other));
        m_text = std::move(other.m_text);
        m_styleId = std::move(other.m_styleId);
    }
    return *this;
}

// =============================================================================
// KmlElement interface implementation
// =============================================================================

ElementType KmlTextRun::type() const
{
    return ElementType::Text;
}

QString KmlTextRun::toKml() const
{
    QString result;
    QXmlStreamWriter writer(&result);

    writer.writeStartElement(QStringLiteral("t"));

    if (!m_styleId.isEmpty()) {
        writer.writeAttribute(QStringLiteral("style"), m_styleId);
    }

    writer.writeCharacters(m_text);
    writer.writeEndElement();

    return result;
}

std::unique_ptr<KmlElement> KmlTextRun::clone() const
{
    return std::make_unique<KmlTextRun>(*this);
}

QString KmlTextRun::plainText() const
{
    return m_text;
}

int KmlTextRun::length() const
{
    return m_text.length();
}

// =============================================================================
// KmlTextRun-specific methods
// =============================================================================

const QString& KmlTextRun::text() const
{
    return m_text;
}

void KmlTextRun::setText(const QString& text)
{
    m_text = text;
}

const QString& KmlTextRun::styleId() const
{
    return m_styleId;
}

void KmlTextRun::setStyleId(const QString& styleId)
{
    m_styleId = styleId;
}

bool KmlTextRun::hasStyle() const
{
    return !m_styleId.isEmpty();
}

// =============================================================================
// Static factory methods
// =============================================================================

std::unique_ptr<KmlTextRun> KmlTextRun::fromKml(const QString& kml)
{
    if (kml.isEmpty()) {
        return nullptr;
    }

    QXmlStreamReader reader(kml);

    // Skip to start element
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }

    if (reader.atEnd() || reader.hasError()) {
        return nullptr;
    }

    // Check element name
    if (reader.name() != QStringLiteral("t")) {
        return nullptr;
    }

    // Read style attribute if present
    QString styleId;
    QXmlStreamAttributes attrs = reader.attributes();
    if (attrs.hasAttribute(QStringLiteral("style"))) {
        styleId = attrs.value(QStringLiteral("style")).toString();
    }

    // Read text content
    QString text;
    reader.readNext();

    while (!reader.atEnd()) {
        if (reader.isCharacters()) {
            text += reader.text().toString();
        } else if (reader.isEndElement()) {
            break;
        }
        reader.readNext();
    }

    if (reader.hasError()) {
        return nullptr;
    }

    return std::make_unique<KmlTextRun>(text, styleId);
}

}  // namespace kalahari::editor
