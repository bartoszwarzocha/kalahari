/// @file chapter_document.cpp
/// @brief Implementation of ChapterDocument for .kchapter format
///
/// OpenSpec #00035: KChapter Document Format

#include <kalahari/core/chapter_document.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/text_statistics.h>
#include <kalahari/editor/kml_format_registry.h>

#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QXmlStreamReader>

namespace kalahari {
namespace core {

// =============================================================================
// Construction
// =============================================================================

ChapterDocument::ChapterDocument()
    : m_lastModified(QDateTime::currentDateTimeUtc())
{
}

ChapterDocument::ChapterDocument(const QString& kml)
    : m_lastModified(QDateTime::currentDateTimeUtc())
{
    setKml(kml);
}

// =============================================================================
// Content Access
// =============================================================================

QString ChapterDocument::kml() const
{
    return m_kml;
}

QString ChapterDocument::plainText() const
{
    return m_plainText;
}

void ChapterDocument::setKml(const QString& kml)
{
    m_kml = kml;
    m_plainText = kmlToPlainText(kml);
    recalculateStatistics();
    touch();
}

void ChapterDocument::setPlainText(const QString& text)
{
    m_plainText = text;
    recalculateStatistics();
}

bool ChapterDocument::hasContent() const
{
    return !m_kml.isEmpty();
}

// =============================================================================
// Statistics
// =============================================================================

int ChapterDocument::wordCount() const
{
    return m_wordCount;
}

WordTally ChapterDocument::wordTally() const
{
    // The text of the KML, as the editor has it: the plain text of a file is only a copy,
    // which an older version of the program may have made in another way
    return tallyWords(m_kml.isEmpty() ? m_plainText : kmlToPlainText(m_kml));
}

int ChapterDocument::characterCount() const
{
    return m_characterCount;
}

int ChapterDocument::paragraphCount() const
{
    return m_paragraphCount;
}

QDateTime ChapterDocument::lastModified() const
{
    return m_lastModified;
}

void ChapterDocument::touch()
{
    m_lastModified = QDateTime::currentDateTimeUtc();
}

void ChapterDocument::recalculateStatistics()
{
    const TextCounts counts = countText(m_plainText, wordCountRules());
    m_wordCount = counts.words;
    m_characterCount = counts.nonSpaceCharacters;
    m_paragraphCount = counts.paragraphs;
}

// =============================================================================
// Metadata
// =============================================================================

QString ChapterDocument::title() const
{
    return m_title;
}

void ChapterDocument::setTitle(const QString& title)
{
    m_title = title;
}

QString ChapterDocument::status() const
{
    return m_status;
}

void ChapterDocument::setStatus(const QString& status)
{
    m_status = status;
}

QString ChapterDocument::notes() const
{
    return m_notes;
}

void ChapterDocument::setNotes(const QString& notes)
{
    m_notes = notes;
}

std::optional<QColor> ChapterDocument::color() const
{
    return m_color;
}

void ChapterDocument::setColor(const QColor& color)
{
    m_color = color;
}

void ChapterDocument::clearColor()
{
    m_color = std::nullopt;
}

// =============================================================================
// Annotations
// =============================================================================

QJsonArray ChapterDocument::comments() const
{
    return m_comments;
}

void ChapterDocument::setComments(const QJsonArray& comments)
{
    m_comments = comments;
}

QJsonArray ChapterDocument::highlights() const
{
    return m_highlights;
}

void ChapterDocument::setHighlights(const QJsonArray& highlights)
{
    m_highlights = highlights;
}

// =============================================================================
// Serialization
// =============================================================================

QJsonObject ChapterDocument::toJson() const
{
    QJsonObject root;

    // Kalahari header
    QJsonObject kalahari;
    kalahari["version"] = QString::fromLatin1(FORMAT_VERSION);
    kalahari["type"] = QString::fromLatin1(FORMAT_TYPE);
    root["kalahari"] = kalahari;

    // Content
    QJsonObject content;
    content["kml"] = m_kml;
    content["plainText"] = m_plainText;
    root["content"] = content;

    // Statistics
    QJsonObject statistics;
    statistics["wordCount"] = m_wordCount;
    statistics["characterCount"] = m_characterCount;
    statistics["paragraphCount"] = m_paragraphCount;
    statistics["lastModified"] = m_lastModified.toString(Qt::ISODate);
    root["statistics"] = statistics;

    // Metadata
    QJsonObject metadata;
    metadata["title"] = m_title;
    metadata["status"] = m_status;
    metadata["notes"] = m_notes;
    if (m_color.has_value()) {
        metadata["color"] = m_color->name();
    }
    root["metadata"] = metadata;

    // Annotations
    QJsonObject annotations;
    annotations["comments"] = m_comments;
    annotations["highlights"] = m_highlights;
    root["annotations"] = annotations;

    return root;
}

ChapterDocument ChapterDocument::fromJson(const QJsonObject& json)
{
    ChapterDocument doc;

    // Content
    if (json.contains("content")) {
        const QJsonObject content = json["content"].toObject();
        doc.m_kml = content["kml"].toString();
        doc.m_plainText = content["plainText"].toString();

        // PERFORMANCE FIX: Don't regenerate plainText during loading
        // If plainText is missing (legacy file), it will be calculated on demand
        // by the editor when needed. Parsing KML here creates a redundant QTextDocument
        // that gets discarded immediately, causing O(n) overhead for large files.
        // The BookEditor::fromKml() will parse KML into QTextDocument anyway.
    }

    // Statistics
    if (json.contains("statistics")) {
        const QJsonObject stats = json["statistics"].toObject();
        doc.m_wordCount = stats["wordCount"].toInt();
        doc.m_characterCount = stats["characterCount"].toInt();
        doc.m_paragraphCount = stats["paragraphCount"].toInt();
        doc.m_lastModified = QDateTime::fromString(
            stats["lastModified"].toString(), Qt::ISODate);
    }
    // PERFORMANCE FIX: Don't recalculate statistics during loading
    // If stats are missing (legacy file), they'll be 0 until editor calculates them.
    // Calling recalculateStatistics() here requires plainText which might be empty.

    // Metadata
    if (json.contains("metadata")) {
        const QJsonObject meta = json["metadata"].toObject();
        doc.m_title = meta["title"].toString();
        doc.m_status = meta["status"].toString("draft");
        doc.m_notes = meta["notes"].toString();
        if (meta.contains("color")) {
            doc.m_color = QColor(meta["color"].toString());
        }
    }

    // Annotations
    if (json.contains("annotations")) {
        const QJsonObject annot = json["annotations"].toObject();
        doc.m_comments = annot["comments"].toArray();
        doc.m_highlights = annot["highlights"].toArray();
    }

    return doc;
}

// =============================================================================
// File I/O
// =============================================================================

std::optional<ChapterDocument> ChapterDocument::load(const QString& path)
{
    auto& logger = Logger::getInstance();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logger.error("ChapterDocument::load: Cannot open file: {}",
                     path.toStdString());
        return std::nullopt;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        logger.error("ChapterDocument::load: JSON parse error at {}: {}",
                     parseError.offset, parseError.errorString().toStdString());
        return std::nullopt;
    }

    if (!jsonDoc.isObject()) {
        logger.error("ChapterDocument::load: Root is not an object");
        return std::nullopt;
    }

    const QJsonObject root = jsonDoc.object();

    // Validate Kalahari header
    if (!root.contains("kalahari")) {
        logger.warn("ChapterDocument::load: Missing 'kalahari' header, "
                    "assuming legacy format");
    } else {
        const QJsonObject kalahari = root["kalahari"].toObject();
        const QString type = kalahari["type"].toString();
        if (type != QString::fromLatin1(FORMAT_TYPE)) {
            logger.error("ChapterDocument::load: Invalid type: {}, expected: {}",
                         type.toStdString(), FORMAT_TYPE);
            return std::nullopt;
        }
    }

    logger.debug("ChapterDocument::load: Loaded successfully: {}",
                 path.toStdString());

    return fromJson(root);
}

bool ChapterDocument::save(const QString& path) const
{
    auto& logger = Logger::getInstance();

    // QSaveFile writes a temporary file and renames it over the chapter only when all of it
    // is written, so a failed save (a full disk, a crash) leaves the previous chapter whole
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logger.error("ChapterDocument::save: Cannot open file for writing: {}",
                     path.toStdString());
        return false;
    }

    const QJsonDocument jsonDoc(toJson());
    const QByteArray data = jsonDoc.toJson(QJsonDocument::Indented);

    const qint64 written = file.write(data);
    if (written != data.size()) {
        logger.error("ChapterDocument::save: Write incomplete: {} of {} bytes",
                     written, data.size());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        logger.error("ChapterDocument::save: Cannot replace {}: {}", path.toStdString(),
                     file.errorString().toStdString());
        return false;
    }

    logger.debug("ChapterDocument::save: Saved successfully: {} ({} bytes)",
                 path.toStdString(), written);

    return true;
}

// =============================================================================
// Migration Helpers
// =============================================================================

ChapterDocument ChapterDocument::fromKmlContent(const QString& content,
                                                 const QString& title)
{
    ChapterDocument doc;
    doc.setKml(content);
    if (!title.isEmpty()) {
        doc.setTitle(title);
    }
    return doc;
}

namespace {

/// Whether an element is a paragraph of the text: <p>, <paragraph> or a heading
bool isParagraphElement(QStringView name)
{
    return name == u"p" || name == u"paragraph" || name == u"h1" || name == u"h2" || name == u"h3";
}

/// Whether an element is a list: <ol> or <ul>
bool isListElement(QStringView name)
{
    return name == u"ol" || name == u"ul";
}

/// Remove white space with a line end at the start and the end of a part of a list item's
/// text: the indentation of the KML before and after a list inside the item
QString withoutIndentation(QString text)
{
    qsizetype end = text.size();
    while (end > 0 && text.at(end - 1).isSpace()) {
        --end;
    }
    if (text.mid(end).contains(QLatin1Char('\n'))) {
        text.truncate(end);
    }
    qsizetype start = 0;
    while (start < text.size() && text.at(start).isSpace()) {
        ++start;
    }
    if (text.left(start).contains(QLatin1Char('\n'))) {
        text.remove(0, start);
    }
    return text;
}

/// The texts of the paragraphs of KML, one per line, as the editor reads them
class ParagraphTexts {
public:
    explicit ParagraphTexts(QString& result) : m_result(result) {}

    /// Add a paragraph and its text
    void add(const QString& text)
    {
        if (!m_first) {
            m_result += QLatin1Char('\n');
        }
        m_first = false;
        m_result += text;
    }

    /// Add the paragraph of the paragraph element the reader is on, up to its end tag
    void addParagraph(QXmlStreamReader& reader)
    {
        QString text;
        appendText(reader, text, false);
        add(text);
    }

    /// Add the items of the list element the reader is on, and of the lists inside them
    void addList(QXmlStreamReader& reader)
    {
        while (reader.readNextStartElement()) {
            if (reader.name() == u"li") {
                addItem(reader);
            } else if (isListElement(reader.name())) {
                addList(reader);
            } else {
                reader.skipCurrentElement();
            }
        }
    }

private:
    /// Add a list item: its own paragraph, the paragraphs and lists inside it, and a
    /// paragraph for its text after them (KmlDocumentModel::parseListItem())
    void addItem(QXmlStreamReader& reader)
    {
        bool first = true;
        for (;;) {
            QString text;
            const bool ended = appendText(reader, text, true);
            if (!first) {
                text = withoutIndentation(text);
            }
            if (ended) {
                if (first || !text.trimmed().isEmpty()) {
                    add(text);
                }
                return;
            }
            if (isListElement(reader.name())) {
                text = withoutIndentation(text);
                if (first || !text.isEmpty()) {
                    add(text);
                }
                addList(reader);
            } else {
                // A paragraph inside the item: the item's own text when it has none yet
                if (!first || !text.trimmed().isEmpty()) {
                    text = withoutIndentation(text);
                    if (first || !text.isEmpty()) {
                        add(text);
                    }
                }
                addParagraph(reader);
            }
            first = false;
        }
    }

    /// Append the text of the element the reader is on, up to its end tag
    /// @param stopAtBlocks Stop at a paragraph or list element inside it (a list item),
    ///        without reading it
    /// @return false when it stopped at such an element, else true
    static bool appendText(QXmlStreamReader& reader, QString& text, bool stopAtBlocks)
    {
        int depth = 1;  // open elements, the paragraph included
        while (depth > 0 && !reader.atEnd()) {
            switch (reader.readNext()) {
            case QXmlStreamReader::StartElement:
                if (stopAtBlocks && depth == 1 &&
                    (isParagraphElement(reader.name()) || isListElement(reader.name()))) {
                    return false;
                }
                if (editor::KmlFormatRegistry::isInlineTextTag(reader.name().toString())) {
                    ++depth;
                } else {
                    if (reader.name() == u"br") {
                        text += QChar(QChar::LineSeparator);  // a line break in the paragraph
                    }
                    reader.skipCurrentElement();  // with everything inside it
                }
                break;
            case QXmlStreamReader::EndElement:
                --depth;
                break;
            case QXmlStreamReader::Characters:
                text += reader.text();
                break;
            default:
                break;
            }
        }
        return true;
    }

    QString& m_result;
    bool m_first = true;
};

}  // anonymous namespace

QString ChapterDocument::kmlToPlainText(const QString& kml)
{
    // Collect the text of the paragraphs in a single streaming pass, by the editor's
    // rules (KmlDocumentModel): a fragment gets the same root element; the paragraphs are
    // the <p>, <paragraph> and heading children of the root and the items of its lists, with
    // the paragraphs inside them; <br/> is a line break in the paragraph
    // (QChar::LineSeparator, as in the editor's text), and elements other than formatting,
    // metadata and text runs - the horizontal line and the breaks too - are skipped with
    // everything inside them. Inline tags vanish without splitting words and entities are
    // decoded - so the statistics match the editor's.
    QString result;
    result.reserve(kml.size());
    QXmlStreamReader reader(editor::KmlFormatRegistry::withRootElement(kml));
    if (!reader.readNextStartElement()) {
        return result;  // no root element
    }
    ParagraphTexts paragraphs(result);
    while (reader.readNextStartElement()) {
        if (isParagraphElement(reader.name())) {
            paragraphs.addParagraph(reader);
        } else if (isListElement(reader.name())) {
            paragraphs.addList(reader);
        } else {
            reader.skipCurrentElement();
        }
    }
    return result;
}

} // namespace core
} // namespace kalahari
