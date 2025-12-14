/// @file chapter_document.cpp
/// @brief Implementation of ChapterDocument for .kchapter format
///
/// OpenSpec #00035: KChapter Document Format

#include <kalahari/core/chapter_document.h>
#include <kalahari/core/logger.h>

#include <QFile>
#include <QJsonDocument>
#include <QTextDocument>
#include <QRegularExpression>

namespace kalahari {
namespace core {

// =============================================================================
// Construction
// =============================================================================

ChapterDocument::ChapterDocument()
    : m_lastModified(QDateTime::currentDateTimeUtc())
{
}

ChapterDocument::ChapterDocument(const QString& html)
    : m_lastModified(QDateTime::currentDateTimeUtc())
{
    setHtml(html);
}

// =============================================================================
// Content Access
// =============================================================================

QString ChapterDocument::html() const
{
    return m_html;
}

QString ChapterDocument::plainText() const
{
    return m_plainText;
}

void ChapterDocument::setHtml(const QString& html)
{
    m_html = html;
    m_plainText = htmlToPlainText(html);
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
    return !m_html.isEmpty();
}

// =============================================================================
// Statistics
// =============================================================================

int ChapterDocument::wordCount() const
{
    return m_wordCount;
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
    m_wordCount = calculateWordCount(m_plainText);
    m_characterCount = calculateCharacterCount(m_plainText);
    m_paragraphCount = calculateParagraphCount(m_plainText);
}

int ChapterDocument::calculateWordCount(const QString& text)
{
    if (text.isEmpty()) {
        return 0;
    }

    // Split on whitespace and count non-empty tokens
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    const auto parts = text.split(whitespace, Qt::SkipEmptyParts);
    return static_cast<int>(parts.count());
}

int ChapterDocument::calculateCharacterCount(const QString& text)
{
    if (text.isEmpty()) {
        return 0;
    }

    // Count non-whitespace characters
    int count = 0;
    for (const QChar& ch : text) {
        if (!ch.isSpace()) {
            ++count;
        }
    }
    return count;
}

int ChapterDocument::calculateParagraphCount(const QString& text)
{
    if (text.isEmpty()) {
        return 0;
    }

    // Split on double newlines (paragraph separators)
    static const QRegularExpression paragraphSep(QStringLiteral("\n\\s*\n"));
    const auto parts = text.split(paragraphSep, Qt::SkipEmptyParts);

    // At least 1 paragraph if there's any content
    return qMax(1, static_cast<int>(parts.count()));
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
    content["html"] = m_html;
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
        doc.m_html = content["html"].toString();
        doc.m_plainText = content["plainText"].toString();

        // Regenerate plainText if missing
        if (doc.m_plainText.isEmpty() && !doc.m_html.isEmpty()) {
            doc.m_plainText = htmlToPlainText(doc.m_html);
        }
    }

    // Statistics
    if (json.contains("statistics")) {
        const QJsonObject stats = json["statistics"].toObject();
        doc.m_wordCount = stats["wordCount"].toInt();
        doc.m_characterCount = stats["characterCount"].toInt();
        doc.m_paragraphCount = stats["paragraphCount"].toInt();
        doc.m_lastModified = QDateTime::fromString(
            stats["lastModified"].toString(), Qt::ISODate);
    } else {
        // Recalculate if statistics missing
        doc.recalculateStatistics();
    }

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

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logger.error("ChapterDocument::save: Cannot open file for writing: {}",
                     path.toStdString());
        return false;
    }

    const QJsonDocument jsonDoc(toJson());
    const QByteArray data = jsonDoc.toJson(QJsonDocument::Indented);

    const qint64 written = file.write(data);
    file.close();

    if (written != data.size()) {
        logger.error("ChapterDocument::save: Write incomplete: {} of {} bytes",
                     written, data.size());
        return false;
    }

    logger.debug("ChapterDocument::save: Saved successfully: {} ({} bytes)",
                 path.toStdString(), written);

    return true;
}

// =============================================================================
// Migration Helpers
// =============================================================================

ChapterDocument ChapterDocument::fromHtmlContent(const QString& content,
                                                  const QString& title)
{
    ChapterDocument doc;
    doc.setHtml(content);
    if (!title.isEmpty()) {
        doc.setTitle(title);
    }
    return doc;
}

QString ChapterDocument::htmlToPlainText(const QString& html)
{
    if (html.isEmpty()) {
        return QString();
    }

    // Use QTextDocument for proper HTML-to-text conversion
    QTextDocument textDoc;
    textDoc.setHtml(html);
    return textDoc.toPlainText();
}

} // namespace core
} // namespace kalahari
