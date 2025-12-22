/// @file kml_comment.cpp
/// @brief Implementation of KML Comment element (OpenSpec #00042 Phase 7.8)

#include <kalahari/editor/kml_comment.h>
#include <QUuid>
#include <QDomElement>

namespace kalahari::editor {

// =============================================================================
// Constructors
// =============================================================================

KmlComment::KmlComment()
    : m_id()
    , m_startPos(0)
    , m_endPos(0)
    , m_text()
    , m_author()
    , m_createdAt(QDateTime::currentDateTime())
    , m_resolved(false)
{
    generateId();
}

KmlComment::KmlComment(int startPos, int endPos, const QString& text)
    : m_id()
    , m_startPos(startPos)
    , m_endPos(endPos)
    , m_text(text)
    , m_author()
    , m_createdAt(QDateTime::currentDateTime())
    , m_resolved(false)
{
    generateId();
}

// =============================================================================
// Identity
// =============================================================================

void KmlComment::generateId()
{
    // Generate UUID-based ID with "c-" prefix for clarity
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_id = QStringLiteral("c-") + uuid;
}

// =============================================================================
// Serialization
// =============================================================================

/// @brief Escape a string for use as XML text content
/// @param value The string to escape
/// @return The escaped string
static QString escapeXmlContent(const QString& value)
{
    QString result;
    result.reserve(value.size() + 16);

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
            default:
                result += ch;
                break;
        }
    }

    return result;
}

/// @brief Escape a string for use as an XML attribute value
/// @param value The string to escape
/// @return The escaped string
static QString escapeXmlAttribute(const QString& value)
{
    QString result;
    result.reserve(value.size() + 16);

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

QString KmlComment::toKml() const
{
    QString result;

    // Build the opening tag with attributes
    result = QStringLiteral("<comment");
    result += QStringLiteral(" id=\"") + escapeXmlAttribute(m_id) + QStringLiteral("\"");
    result += QStringLiteral(" start=\"") + QString::number(m_startPos) + QStringLiteral("\"");
    result += QStringLiteral(" end=\"") + QString::number(m_endPos) + QStringLiteral("\"");

    if (!m_author.isEmpty()) {
        result += QStringLiteral(" author=\"") + escapeXmlAttribute(m_author) + QStringLiteral("\"");
    }

    // Use ISO 8601 format for timestamp
    if (m_createdAt.isValid()) {
        result += QStringLiteral(" created=\"") +
                  m_createdAt.toString(Qt::ISODate) + QStringLiteral("\"");
    }

    result += QStringLiteral(" resolved=\"") +
              (m_resolved ? QStringLiteral("true") : QStringLiteral("false")) +
              QStringLiteral("\"");

    result += QStringLiteral(">");

    // Comment text content
    result += escapeXmlContent(m_text);

    // Closing tag
    result += QStringLiteral("</comment>");

    return result;
}

KmlComment KmlComment::fromKml(const QDomElement& element)
{
    KmlComment comment;

    // Parse ID attribute
    if (element.hasAttribute(QStringLiteral("id"))) {
        comment.setId(element.attribute(QStringLiteral("id")));
    }

    // Parse position attributes
    if (element.hasAttribute(QStringLiteral("start"))) {
        bool ok = false;
        int start = element.attribute(QStringLiteral("start")).toInt(&ok);
        if (ok) {
            comment.setStartPos(start);
        }
    }

    if (element.hasAttribute(QStringLiteral("end"))) {
        bool ok = false;
        int end = element.attribute(QStringLiteral("end")).toInt(&ok);
        if (ok) {
            comment.setEndPos(end);
        }
    }

    // Parse author attribute
    if (element.hasAttribute(QStringLiteral("author"))) {
        comment.setAuthor(element.attribute(QStringLiteral("author")));
    }

    // Parse created timestamp (ISO 8601 format)
    if (element.hasAttribute(QStringLiteral("created"))) {
        QString dateStr = element.attribute(QStringLiteral("created"));
        QDateTime dt = QDateTime::fromString(dateStr, Qt::ISODate);
        if (dt.isValid()) {
            comment.setCreatedAt(dt);
        }
    }

    // Parse resolved attribute
    if (element.hasAttribute(QStringLiteral("resolved"))) {
        QString resolvedStr = element.attribute(QStringLiteral("resolved")).toLower();
        comment.setResolved(resolvedStr == QStringLiteral("true") ||
                           resolvedStr == QStringLiteral("1"));
    }

    // Parse text content
    comment.setText(element.text());

    return comment;
}

}  // namespace kalahari::editor
