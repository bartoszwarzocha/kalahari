/// @file kml_document_model.cpp
/// @brief KML reader: the paragraphs of a chapter with their text and formatting
///
/// Reads KML in a single streaming pass, by the format rules of KmlFormatRegistry.

#include "kalahari/editor/kml_document_model.h"
#include "kalahari/editor/kml_format_registry.h"
#include "kalahari/editor/text_link.h"
#include "kalahari/core/logger.h"

#include <QDateTime>
#include <QTimeZone>
#include <QXmlStreamReader>

#include <algorithm>
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

            if (const TextObject object = textObjectOfElement(tag); object != TextObject::None) {
                // A horizontal line or a break: a block of its own, without content
                Paragraph para;
                para.object = object;
                m_paragraphs.push_back(std::move(para));
                reader.skipCurrentElement();
                reader.readNext();
            } else if (tag == QStringLiteral("ol") || tag == QStringLiteral("ul")) {
                parseTopLevelList(reader);  // consumes </ol>
            } else if (isBlockElement(tag)) {
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
    m_lastListId = 0;
    m_lastListDefinition = ListDefinition();
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
        if (isBlockElement(tag)) {
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

QString KmlDocumentModel::paragraphStyle(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return {};
    }
    return m_paragraphs[index].style;
}

TextObject KmlDocumentModel::paragraphObject(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return TextObject::None;
    }
    return m_paragraphs[index].object;
}

ParagraphFormat KmlDocumentModel::paragraphOwnFormat(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return {};
    }
    return m_paragraphs[index].format;
}

std::optional<ListItem> KmlDocumentModel::paragraphList(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return std::nullopt;
    }
    return m_paragraphs[index].list;
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

bool KmlDocumentModel::isBlockElement(QStringView tag)
{
    if (tag == QStringLiteral("p") || tag == QStringLiteral("paragraph") ||
        tag == QStringLiteral("ol") || tag == QStringLiteral("ul")) {
        return true;
    }
    return headingLevelOfElement(tag) > 0 || textObjectOfElement(tag.toString()) != TextObject::None;
}

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

void KmlDocumentModel::appendText(Paragraph& para, const QString& source,
                                  const QTextCharFormat& format)
{
    // The object replacement character is the editor's mark of an object (its own element in
    // KML): in the text it would be an object without a kind
    QString text = source;
    text.remove(QChar::ObjectReplacementCharacter);
    if (text.isEmpty()) {
        return;
    }
    const auto start = static_cast<size_t>(para.text.size());
    para.text += text;

    // Add a format run for any formatting, metadata or annotation (the base format of a
    // paragraph is empty, so any property marks a non-default run)
    if (format.propertyCount() > 0) {
        FormatRun run;
        run.start = start;
        run.end = static_cast<size_t>(para.text.size());
        run.format = format;
        para.formats.push_back(run);
    }
}

void KmlDocumentModel::readParagraphAttributes(QXmlStreamReader& reader, Paragraph& para)
{
    const QString tag = reader.name().toString();

    // A heading is in the style of its level; a paragraph in the style it names (a style the
    // book does not have is kept: the editor shows it as Normal)
    const QXmlStreamAttributes attrs = reader.attributes();
    if (const int level = headingLevelOfElement(tag); level > 0) {
        para.style = headingStyleId(level);
    } else {
        para.style = attrs.value(QStringLiteral("style")).toString().trimmed();
        if (para.style == QLatin1String(NORMAL_STYLE_ID)) {
            para.style.clear();
        }
    }

    // Parse alignment attribute
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

    para.format = paragraphFormatFromKml(attrs);
}

void KmlDocumentModel::parseParagraphElement(QXmlStreamReader& reader, Paragraph& para)
{
    const QString tag = reader.name().toString();
    readParagraphAttributes(reader, para);

    // Move past start element
    reader.readNext();

    // Parse inline content, up to and including the closing tag
    parseInlineContent(reader, para, QTextCharFormat(), tag);
}

void KmlDocumentModel::parseTopLevelList(QXmlStreamReader& reader)
{
    // A list goes on with the number of the one before it, or gets a new one
    ListReading list;
    const QString continues =
        reader.attributes().value(QStringLiteral("continue")).toString().trimmed().toLower();
    if (m_lastListId > 0 &&
        (continues == QStringLiteral("true") || continues == QStringLiteral("1"))) {
        list.id = m_lastListId;
        list.definition = m_lastListDefinition;
    } else {
        list.id = m_lastListId + 1;
        list.definition = ListDefinition::ofKind(reader.name() == QStringLiteral("ul")
                                                     ? ListKind::Bulleted
                                                     : ListKind::Numbered);
    }
    parseList(reader, list, 0);

    // The items read before the element of a deeper level take its format
    for (const auto& [index, known] : list.items) {
        std::optional<ListItem>& listItem = m_paragraphs[index].list;
        if (!listItem) {
            continue;
        }
        ListItem& item = *listItem;
        for (int level = 0; level < LIST_LEVELS; ++level) {
            if ((known & (1U << level)) == 0) {
                item.definition.levels[static_cast<size_t>(level)] =
                    list.definition.levels[static_cast<size_t>(level)];
            }
        }
    }
    m_lastListId = std::max(m_lastListId, list.id);
    m_lastListDefinition = list.definition;
}

void KmlDocumentModel::parseList(QXmlStreamReader& reader, ListReading& list, int level)
{
    const QString tag = reader.name().toString();
    const int index = std::min(level, LIST_LEVELS - 1);
    list.definition.levels[static_cast<size_t>(index)] =
        listLevelFromKml(tag == QStringLiteral("ol"), reader.attributes(), index);
    list.knownLevels |= 1U << index;

    // Its items, and the lists of a deeper level without an item of their own; text between
    // them (the indentation of the KML) and other elements are skipped. Each of them stops
    // after its end element, at the next token.
    reader.readNext();
    while (!reader.atEnd()) {
        if (reader.isEndElement()) {
            reader.readNext();  // the list's own: its children are read whole
            return;
        }
        if (!reader.isStartElement()) {
            reader.readNext();
            continue;
        }
        const QStringView child = reader.name();
        if (child == QStringLiteral("li")) {
            parseListItem(reader, list, index);
        } else if (child == QStringLiteral("ol") || child == QStringLiteral("ul")) {
            parseList(reader, list, index + 1);
        } else {
            reader.skipCurrentElement();
            reader.readNext();
        }
    }
}

void KmlDocumentModel::appendListParagraph(Paragraph&& para, ListReading& list)
{
    list.items.emplace_back(m_paragraphs.size(), list.knownLevels);
    m_paragraphs.push_back(std::move(para));
}

namespace {

/// @brief Remove the indentation at the end of an item's text before a list inside it: white
///        space with a line end, which indented KML puts there
void trimIndentation(QString& text, std::vector<FormatRun>& formats)
{
    qsizetype end = text.size();
    bool lineEnd = false;
    while (end > 0 && text.at(end - 1).isSpace()) {
        lineEnd |= text.at(end - 1) == QLatin1Char('\n');
        --end;
    }
    if (!lineEnd) {
        return;
    }
    text.truncate(end);
    const auto length = static_cast<size_t>(end);
    for (FormatRun& run : formats) {
        run.end = std::min(run.end, length);
    }
    formats.erase(std::remove_if(formats.begin(), formats.end(),
                                 [](const FormatRun& run) { return run.start >= run.end; }),
                  formats.end());
}

/// @brief Remove the indentation at the start of the text after a list inside an item: white
///        space with a line end
void trimLeadingIndentation(QString& text, std::vector<FormatRun>& formats)
{
    qsizetype start = 0;
    bool lineEnd = false;
    while (start < text.size() && text.at(start).isSpace()) {
        lineEnd |= text.at(start) == QLatin1Char('\n');
        ++start;
    }
    if (!lineEnd) {
        return;
    }
    text.remove(0, start);
    const auto removed = static_cast<size_t>(start);
    for (FormatRun& run : formats) {
        run.start = run.start > removed ? run.start - removed : 0;
        run.end = run.end > removed ? run.end - removed : 0;
    }
    formats.erase(std::remove_if(formats.begin(), formats.end(),
                                 [](const FormatRun& run) { return run.start >= run.end; }),
                  formats.end());
}

}  // anonymous namespace

void KmlDocumentModel::parseListItem(QXmlStreamReader& reader, ListReading& list, int level)
{
    Paragraph item;
    readParagraphAttributes(reader, item);
    const QXmlStreamAttributes attrs = reader.attributes();
    ListItem place;
    place.id = list.id;
    place.level = level;
    place.definition = list.definition;
    bool ok = false;
    const int value = attrs.value(QStringLiteral("value")).toString().trimmed().toInt(&ok);
    if (ok) {
        place.value = std::clamp(value, MIN_LIST_NUMBER, MAX_LIST_NUMBER);
    }
    const QString numbered = attrs.value(QStringLiteral("numbered")).toString().trimmed().toLower();
    place.numbered = numbered != QStringLiteral("false") && numbered != QStringLiteral("0");
    item.list = place;
    reader.readNext();

    // The item's own paragraph; after a list or a paragraph inside the item, its text goes
    // on in a paragraph of the item's level without a number
    Paragraph current = std::move(item);
    bool first = true;
    for (;;) {
        const bool ended = parseInlineContent(reader, current, QTextCharFormat(),
                                              QStringLiteral("li"), true);
        if (!first) {
            trimLeadingIndentation(current.text, current.formats);
        }
        if (ended) {
            if (first || !current.text.trimmed().isEmpty() || !current.startAnnotations.isEmpty()) {
                appendListParagraph(std::move(current), list);
            }
            return;
        }

        const QString child = reader.name().toString();
        if (child == QStringLiteral("ol") || child == QStringLiteral("ul")) {
            trimIndentation(current.text, current.formats);
            if (first || !current.text.isEmpty() || !current.startAnnotations.isEmpty()) {
                appendListParagraph(std::move(current), list);
            }
            parseList(reader, list, std::min(level + 1, LIST_LEVELS - 1));
        } else {
            // A paragraph inside the item: the item's own text when it has none yet
            const bool own = first && current.text.trimmed().isEmpty() && current.formats.empty();
            Paragraph para;
            if (own) {
                para = std::move(current);
                para.text.clear();
            } else {
                trimIndentation(current.text, current.formats);
                if (first || !current.text.isEmpty() || !current.startAnnotations.isEmpty()) {
                    appendListParagraph(std::move(current), list);
                }
                para.list = place;
                para.list->definition = list.definition;
                para.list->value.reset();
                para.list->numbered = false;
            }
            std::optional<ListItem> keptPlace = para.list;
            readParagraphAttributes(reader, para);
            para.list = keptPlace;
            reader.readNext();
            parseInlineContent(reader, para, QTextCharFormat(), child);
            appendListParagraph(std::move(para), list);
        }

        // The rest of the item's text: a paragraph of its level without a number
        first = false;
        current = Paragraph();
        current.list = place;
        current.list->definition = list.definition;
        current.list->value.reset();
        current.list->numbered = false;
    }
}

bool KmlDocumentModel::parseInlineContent(QXmlStreamReader& reader,
                                          Paragraph& para,
                                          QTextCharFormat currentFormat,
                                          const QString& endTag,
                                          bool stopAtBlocks)
{
    while (!reader.atEnd()) {
        if (reader.isEndElement()) {
            QString tag = reader.name().toString();
            if (tag == endTag || tag == QStringLiteral("paragraph")) {
                reader.readNext();
                return true;
            }
            return true;
        }

        if (reader.isCharacters()) {
            appendText(para, reader.text().toString(), currentFormat);
            reader.readNext();
        } else if (reader.isStartElement()) {
            QString tag = reader.name().toString();

            if (stopAtBlocks && isBlockElement(tag) &&
                textObjectOfElement(tag) == TextObject::None) {
                return false;  // a paragraph or a list inside a list item
            }
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
            } else if (tag == QStringLiteral("a")) {
                // A link: its text with its address; without an address the text is plain
                QTextCharFormat newFormat = currentFormat;
                setLinkAddress(newFormat,
                               reader.attributes().value(QStringLiteral("href")).toString().trimmed());
                reader.readNext();
                parseInlineContent(reader, para, newFormat, tag);
            } else if (tag == QStringLiteral("t") || tag == QStringLiteral("text")) {
                // Text run - just parse content
                reader.readNext();
                parseInlineContent(reader, para, currentFormat, tag);
            } else if (tag == QStringLiteral("br")) {
                // A line break in the paragraph (Shift+Enter): a character of its text, in
                // the format of its place
                appendText(para, QString(QChar(QChar::LineSeparator)), currentFormat);
                reader.skipCurrentElement();
                reader.readNext();
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
    return true;
}

} // namespace editor
} // namespace kalahari
