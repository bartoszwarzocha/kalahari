/// @file kml_document_model.cpp
/// @brief KML reader: the paragraphs of a chapter with their text and formatting
///
/// Reads KML in a single streaming pass, by the format rules of KmlFormatRegistry.

#include "kalahari/editor/kml_document_model.h"
#include "kalahari/editor/kml_format_registry.h"
#include "kalahari/core/logger.h"

#include <QDateTime>
#include <QTimeZone>
#include <QXmlStreamReader>

#include <optional>

namespace kalahari {
namespace editor {

namespace {

/// @brief A time written in ISO 8601 (invalid when it is not); without a time zone, in UTC
QDateTime timeFromKml(const QString& value)
{
    QDateTime time = QDateTime::fromString(value, Qt::ISODate);
    if (time.isValid() && time.timeSpec() == Qt::LocalTime) {
        return QDateTime(time.date(), time.time(), QTimeZone::utc());
    }
    return time;
}

/// @brief Read an <annotation> element of the <annotations> section
/// @param reader XML reader positioned at the element's start; on return it is positioned
///               at its end element
/// @return The annotation; std::nullopt when it has no id or an unknown kind
std::optional<Annotation> readAnnotationElement(QXmlStreamReader& reader)
{
    Annotation annotation;
    QString kindName;
    const QXmlStreamAttributes attributes = reader.attributes();
    for (const QXmlStreamAttribute& attribute : attributes) {
        const QString name = attribute.name().toString();
        const QString value = attribute.value().toString();
        if (name == QStringLiteral("id")) {
            annotation.id = value;
        } else if (name == QStringLiteral("kind")) {
            kindName = value;
        } else if (name == QStringLiteral("author")) {
            annotation.author = value;
        } else if (name == QStringLiteral("done")) {
            const QString flag = value.toLower();
            annotation.done = flag == QStringLiteral("true") || flag == QStringLiteral("1");
        } else if (name == QStringLiteral("created")) {
            annotation.created = timeFromKml(value);
            if (!annotation.created.isValid()) {
                annotation.otherAttributes.insert(name, value);  // kept as it is
            }
        } else {
            annotation.otherAttributes.insert(name, value);
        }
    }
    annotation.text = reader.readElementText(QXmlStreamReader::SkipChildElements);

    const std::optional<AnnotationKind> kind = annotationKindFromName(kindName);
    if (annotation.id.isEmpty() || !kind) {
        core::Logger::getInstance().warn("KmlDocumentModel: annotation '{}' of kind '{}' left "
                                         "out (no id or an unknown kind)",
                                         annotation.id.toStdString(), kindName.toStdString());
        return std::nullopt;
    }
    annotation.kind = *kind;
    return annotation;
}

}  // anonymous namespace

// Static member initialization
const std::vector<FormatRun> KmlDocumentModel::s_emptyFormats;

// =============================================================================
// Document Loading
// =============================================================================

bool KmlDocumentModel::loadKml(const QString& kml)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("KmlDocumentModel::loadKml: Loading {} chars", kml.size());

    // Clear existing content
    clear();

    if (kml.isEmpty()) {
        return true;
    }

    // Wrap KML in root element if needed
    QXmlStreamReader reader(KmlFormatRegistry::withRootElement(kml));

    // Skip to first start element (root)
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }

    if (reader.atEnd()) {
        logger.warn("KmlDocumentModel::loadKml: No root element found");
        return true;
    }

    // Skip root element
    QString rootTag = reader.name().toString();
    reader.readNext();

    // Parse paragraphs
    while (!reader.atEnd()) {
        if (reader.isEndElement()) {
            QString tag = reader.name().toString();
            if (tag == rootTag) {
                break;
            }
            reader.readNext();
            continue;
        }

        if (reader.isStartElement()) {
            QString tag = reader.name().toString();

            if (tag == QStringLiteral("p") || tag == QStringLiteral("paragraph")) {
                // Parse the paragraph straight from the document reader (consumes </p>)
                Paragraph para;
                parseParagraphElement(reader, para);
                m_paragraphs.push_back(std::move(para));
            } else if (tag == QStringLiteral("annotations")) {
                parseAnnotations(reader);  // stops at </annotations>
            } else {
                // Skip unknown elements
                reader.skipCurrentElement();
            }
        } else {
            reader.readNext();
        }
    }

    const qsizetype unanchored = m_annotations.size() - m_anchoredIds.size();
    if (unanchored > 0) {
        logger.warn("KmlDocumentModel::loadKml: {} annotations have no place in the text, "
                    "they are left out", unanchored);
    }

    if (reader.hasError() &&
        reader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        logger.error("KmlDocumentModel::loadKml: XML error: {}",
                    reader.errorString().toStdString());
        return false;
    }

    logger.debug("KmlDocumentModel::loadKml: Loaded {} paragraphs", m_paragraphs.size());
    return true;
}

void KmlDocumentModel::clear()
{
    m_paragraphs.clear();
    m_annotations.clear();
    m_anchoredIds.clear();
}

AnnotationList KmlDocumentModel::readAnnotations(const QString& kml)
{
    AnnotationList annotations;
    if (kml.isEmpty()) {
        return annotations;
    }

    QXmlStreamReader reader(KmlFormatRegistry::withRootElement(kml));
    if (!reader.readNextStartElement()) {
        return annotations;  // no root element
    }

    // The section comes before the paragraphs (KmlSerializer writes it there): without one
    // before the first paragraph the chapter has no annotations
    while (reader.readNextStartElement()) {
        const QStringView tag = reader.name();
        if (tag == QStringLiteral("p") || tag == QStringLiteral("paragraph")) {
            break;
        }
        if (tag != QStringLiteral("annotations")) {
            reader.skipCurrentElement();
            continue;
        }

        QSet<QString> ids;
        while (reader.readNextStartElement()) {
            if (reader.name() != QStringLiteral("annotation")) {
                reader.skipCurrentElement();
                continue;
            }
            const std::optional<Annotation> annotation = readAnnotationElement(reader);
            if (annotation && !ids.contains(annotation->id)) {
                ids.insert(annotation->id);
                annotations.append(*annotation);
            }
        }
        break;
    }
    return annotations;
}

bool KmlDocumentModel::isEmpty() const
{
    return m_paragraphs.empty();
}

// =============================================================================
// Paragraph Access
// =============================================================================

size_t KmlDocumentModel::paragraphCount() const
{
    return m_paragraphs.size();
}

QString KmlDocumentModel::paragraphText(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return QString();
    }
    return m_paragraphs[index].text;
}

const std::vector<FormatRun>& KmlDocumentModel::paragraphFormats(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return s_emptyFormats;
    }
    return m_paragraphs[index].formats;
}

Qt::Alignment KmlDocumentModel::paragraphAlignment(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return {};
    }
    return m_paragraphs[index].alignment;
}

AnnotationList KmlDocumentModel::paragraphStartAnnotations(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return {};
    }
    return m_paragraphs[index].startAnnotations;
}

// =============================================================================
// Private Methods
// =============================================================================

void KmlDocumentModel::parseAnnotations(QXmlStreamReader& reader)
{
    while (reader.readNextStartElement()) {
        if (reader.name() != QStringLiteral("annotation")) {
            reader.skipCurrentElement();
            continue;
        }

        const std::optional<Annotation> annotation = readAnnotationElement(reader);
        if (!annotation) {
            continue;
        }
        if (m_annotations.contains(annotation->id)) {
            core::Logger::getInstance().warn("KmlDocumentModel::loadKml: annotation '{}' left "
                                             "out (a repeated id)",
                                             annotation->id.toStdString());
            continue;
        }
        m_annotations.insert(annotation->id, *annotation);
    }
}

void KmlDocumentModel::anchorAfterText(Paragraph& para, const Annotation& annotation)
{
    if (para.text.isEmpty()) {
        para.startAnnotations = withAnnotation(para.startAnnotations, annotation);
        return;
    }

    // The last character gets a run of its own (the runs are in text order); a character
    // written with a surrogate pair is both of its halves
    const auto end = static_cast<size_t>(para.text.size());
    size_t last = end - 1;
    if (last > 0 && para.text.at(static_cast<qsizetype>(last)).isLowSurrogate() &&
        para.text.at(static_cast<qsizetype>(last) - 1).isHighSurrogate()) {
        --last;
    }
    if (para.formats.empty() || para.formats.back().end != end) {
        FormatRun run;
        run.start = last;
        run.end = end;
        para.formats.push_back(run);
    } else if (para.formats.back().start < last) {
        FormatRun lastCharacter = para.formats.back();
        lastCharacter.start = last;
        para.formats.back().end = last;
        para.formats.push_back(lastCharacter);
    }
    QTextCharFormat& format = para.formats.back().format;
    setAnnotations(format, withAnnotation(annotationsOf(format), annotation));
}

void KmlDocumentModel::parseParagraphElement(QXmlStreamReader& reader, Paragraph& para)
{
    const QString tag = reader.name().toString();

    // Parse alignment attribute
    QXmlStreamAttributes attrs = reader.attributes();
    if (attrs.hasAttribute(QStringLiteral("align"))) {
        QString alignStr = attrs.value(QStringLiteral("align")).toString().toLower();
        if (alignStr == QStringLiteral("left")) {
            para.alignment = Qt::AlignLeft;
        } else if (alignStr == QStringLiteral("center")) {
            para.alignment = Qt::AlignHCenter;
        } else if (alignStr == QStringLiteral("right")) {
            para.alignment = Qt::AlignRight;
        } else if (alignStr == QStringLiteral("justify")) {
            para.alignment = Qt::AlignJustify;
        }
    }

    // Move past start element
    reader.readNext();

    // Parse inline content, up to and including the closing tag
    parseInlineContent(reader, para, QTextCharFormat(), tag);
}

void KmlDocumentModel::parseInlineContent(QXmlStreamReader& reader,
                                          Paragraph& para,
                                          QTextCharFormat currentFormat,
                                          const QString& endTag)
{
    while (!reader.atEnd()) {
        if (reader.isEndElement()) {
            QString tag = reader.name().toString();
            if (tag == endTag || tag == QStringLiteral("paragraph")) {
                reader.readNext();
                return;
            }
            return;
        }

        if (reader.isCharacters()) {
            QString chars = reader.text().toString();
            if (!chars.isEmpty()) {
                const auto start = static_cast<size_t>(para.text.size());
                para.text += chars;

                // Add a format run for any formatting, metadata or annotation (the base
                // format of a paragraph is empty, so any property marks a non-default run)
                if (currentFormat.propertyCount() > 0) {
                    FormatRun run;
                    run.start = start;
                    run.end = static_cast<size_t>(para.text.size());
                    run.format = currentFormat;
                    para.formats.push_back(run);
                }
            }
            reader.readNext();
        } else if (reader.isStartElement()) {
            QString tag = reader.name().toString();

            if (KmlFormatRegistry::isFormattingTag(tag)) {
                // Apply formatting tag
                QTextCharFormat newFormat = KmlFormatRegistry::applyTagFormat(tag, currentFormat);
                // Read inline style attributes (font, size, color, bg) off the current
                // start element. Must run BEFORE reader.readNext() while the reader is
                // still positioned on this tag, otherwise reader.attributes() no longer
                // refers to it. Covers injected-attribute carriers (b/i/u/s/sub/sup) and
                // the <span> carrier (span is a formatting tag whose applyTagFormat is a
                // no-op, so only the attributes take effect).
                KmlFormatRegistry::applyInlineStyleAttributes(reader.attributes(), newFormat);
                reader.readNext();
                parseInlineContent(reader, para, newFormat, tag);
            } else if (const MetadataTagDef* def = KmlFormatRegistry::getMetadataTagDef(tag)) {
                // Apply metadata with all of its attributes
                QTextCharFormat newFormat = currentFormat;
                newFormat.setProperty(def->propertyId,
                                      KmlFormatRegistry::readMetadataAttributes(reader.attributes()));

                reader.readNext();
                parseInlineContent(reader, para, newFormat, tag);
            } else if (tag == QStringLiteral("anchor")) {
                // An annotation: on the text inside the anchor, or on its place when it
                // holds none. The text of an anchor of an unknown annotation stays plain.
                const QString ref = reader.attributes().value(QStringLiteral("ref")).toString();
                const auto record = m_annotations.constFind(ref);
                const qsizetype start = para.text.size();
                reader.readNext();
                if (record == m_annotations.cend()) {
                    core::Logger::getInstance().warn(
                        "KmlDocumentModel::loadKml: anchor of an unknown annotation '{}'",
                        ref.toStdString());
                    parseInlineContent(reader, para, currentFormat, tag);
                    continue;
                }
                Annotation annotation = *record;
                QTextCharFormat newFormat = currentFormat;
                setAnnotations(newFormat, withAnnotation(annotationsOf(currentFormat), annotation));
                parseInlineContent(reader, para, newFormat, tag);
                if (para.text.size() == start) {
                    annotation.point = true;
                    anchorAfterText(para, annotation);
                }
                m_anchoredIds.insert(ref);
            } else if (tag == QStringLiteral("t") || tag == QStringLiteral("text")) {
                // Text run - just parse content
                reader.readNext();
                parseInlineContent(reader, para, currentFormat, tag);
            } else {
                // Unknown element - skip it, together with its end tag (where
                // skipCurrentElement() stops), so the text after it is still parsed
                reader.skipCurrentElement();
                reader.readNext();
            }
        } else {
            reader.readNext();
        }
    }
}

} // namespace editor
} // namespace kalahari
