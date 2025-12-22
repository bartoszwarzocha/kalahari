/// @file tag_detector.cpp
/// @brief Implementation of tag detection service (OpenSpec #00042 Task 7.10)

#include "kalahari/editor/tag_detector.h"
#include "kalahari/editor/kml_document.h"
#include "kalahari/editor/kml_paragraph.h"
#include "kalahari/core/logger.h"

namespace kalahari::editor {

// Static regex pattern for tag detection
// Matches: TODO, FIX, CHECK, NOTE, WARNING followed by optional colon and content
// Pattern: word boundary + keyword + optional whitespace + optional colon + rest of line
const QRegularExpression TagDetector::s_tagPattern(
    QStringLiteral("\\b(TODO|FIX|CHECK|NOTE|WARNING)\\s*:?\\s*(.*)"),
    QRegularExpression::CaseInsensitiveOption
);

/// @brief Document observer implementation for TagDetector
///
/// Implements IDocumentObserver to receive notifications when the document
/// changes, triggering automatic re-scanning of affected paragraphs.
class TagDetector::DocumentObserver : public IDocumentObserver {
public:
    explicit DocumentObserver(TagDetector* detector)
        : m_detector(detector)
    {}

    void onContentChanged() override {
        if (m_detector) {
            m_detector->onDocumentChanged();
        }
    }

    void onParagraphModified(int index) override {
        if (m_detector) {
            m_detector->onParagraphModified(index);
        }
    }

    void onParagraphInserted(int index) override {
        Q_UNUSED(index);
        if (m_detector) {
            // Rescan from this paragraph onwards (line numbers shifted)
            m_detector->scan();
        }
    }

    void onParagraphRemoved(int index) override {
        if (m_detector) {
            // Remove tags for deleted paragraph and rescan (line numbers shifted)
            m_detector->removeTagsForParagraph(index);
            m_detector->scan();
        }
    }

private:
    TagDetector* m_detector;
};

TagDetector::TagDetector(QObject* parent)
    : QObject(parent)
    , m_document(nullptr)
    , m_observer(std::make_unique<DocumentObserver>(this))
{
    auto& logger = core::Logger::getInstance();
    logger.debug("TagDetector constructor called");
}

TagDetector::~TagDetector()
{
    // Disconnect from document before destruction
    if (m_document) {
        m_document->removeObserver(m_observer.get());
    }
}

void TagDetector::setDocument(KmlDocument* document)
{
    auto& logger = core::Logger::getInstance();

    // Disconnect from old document
    if (m_document) {
        m_document->removeObserver(m_observer.get());
        logger.debug("TagDetector: Disconnected from previous document");
    }

    m_document = document;
    m_tags.clear();

    // Connect to new document
    if (m_document) {
        m_document->addObserver(m_observer.get());
        logger.debug("TagDetector: Connected to new document");
        // Initial scan
        scan();
    } else {
        emit tagsChanged();
    }
}

void TagDetector::scan()
{
    auto& logger = core::Logger::getInstance();

    m_tags.clear();

    if (m_document == nullptr) {
        emit tagsChanged();
        return;
    }

    logger.debug("TagDetector: Scanning {} paragraphs", m_document->paragraphCount());

    for (int i = 0; i < m_document->paragraphCount(); ++i) {
        const KmlParagraph* para = m_document->paragraph(i);
        if (para) {
            detectTagsInText(para->plainText(), i);
        }
    }

    logger.debug("TagDetector: Found {} tags", m_tags.size());
    emit tagsChanged();
}

void TagDetector::scanParagraph(int index)
{
    if (m_document == nullptr || index < 0 || index >= m_document->paragraphCount()) {
        return;
    }

    // Remove existing tags for this paragraph
    int removed = removeTagsForParagraph(index);

    // Scan the paragraph
    const KmlParagraph* para = m_document->paragraph(index);
    if (para) {
        detectTagsInText(para->plainText(), index);
    }

    // Only emit if something changed
    if (removed > 0 || !tagsInParagraph(index).isEmpty()) {
        emit tagsChanged();
    }
}

void TagDetector::clear()
{
    m_tags.clear();
    emit tagsChanged();
}

QList<DetectedTag> TagDetector::allTags() const
{
    return m_tags;
}

QList<DetectedTag> TagDetector::tagsOfType(TagType type) const
{
    QList<DetectedTag> result;
    for (const DetectedTag& tag : m_tags) {
        if (tag.type == type) {
            result.append(tag);
        }
    }
    return result;
}

QList<DetectedTag> TagDetector::tagsInParagraph(int index) const
{
    QList<DetectedTag> result;
    for (const DetectedTag& tag : m_tags) {
        if (tag.paragraphIndex == index) {
            result.append(tag);
        }
    }
    return result;
}

int TagDetector::tagCount(TagType type) const
{
    int count = 0;
    for (const DetectedTag& tag : m_tags) {
        if (tag.type == type) {
            ++count;
        }
    }
    return count;
}

QColor TagDetector::colorForType(TagType type)
{
    switch (type) {
        case TagType::Todo:
            return QColor(255, 165, 0);   // Orange
        case TagType::Fix:
            return QColor(255, 80, 80);   // Red
        case TagType::Check:
            return QColor(80, 160, 255);  // Blue
        case TagType::Note:
            return QColor(80, 200, 80);   // Green
        case TagType::Warning:
            return QColor(255, 200, 0);   // Yellow/Gold
        default:
            return QColor(128, 128, 128); // Gray
    }
}

QString TagDetector::nameForType(TagType type)
{
    switch (type) {
        case TagType::Todo:    return QStringLiteral("TODO");
        case TagType::Fix:     return QStringLiteral("FIX");
        case TagType::Check:   return QStringLiteral("CHECK");
        case TagType::Note:    return QStringLiteral("NOTE");
        case TagType::Warning: return QStringLiteral("WARNING");
        default:               return QStringLiteral("UNKNOWN");
    }
}

TagType TagDetector::typeFromKeyword(const QString& keyword)
{
    QString upper = keyword.toUpper();
    if (upper == QStringLiteral("TODO"))    return TagType::Todo;
    if (upper == QStringLiteral("FIX"))     return TagType::Fix;
    if (upper == QStringLiteral("CHECK"))   return TagType::Check;
    if (upper == QStringLiteral("NOTE"))    return TagType::Note;
    if (upper == QStringLiteral("WARNING")) return TagType::Warning;
    return TagType::Note;  // Default fallback
}

void TagDetector::detectTagsInText(const QString& text, int paragraphIndex)
{
    auto& logger = core::Logger::getInstance();

    QRegularExpressionMatchIterator it = s_tagPattern.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();

        QString keyword = match.captured(1).toUpper();
        QString content = match.captured(2).trimmed();
        int startPos = match.capturedStart(0);
        int keywordLength = match.captured(1).length();

        TagType type = typeFromKeyword(keyword);
        int lineNumber = calculateLineNumber(paragraphIndex);

        DetectedTag tag(type, paragraphIndex, startPos, keywordLength,
                        keyword, content, lineNumber);

        m_tags.append(tag);

        logger.debug("TagDetector: Found {} at paragraph {}, pos {}",
                     keyword.toStdString(), paragraphIndex, startPos);

        emit tagFound(tag);
    }
}

int TagDetector::removeTagsForParagraph(int paragraphIndex)
{
    int removed = 0;
    auto it = m_tags.begin();
    while (it != m_tags.end()) {
        if (it->paragraphIndex == paragraphIndex) {
            it = m_tags.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

int TagDetector::calculateLineNumber(int paragraphIndex) const
{
    // Simple: line number = paragraph index + 1 (1-based)
    // In a more complex implementation, this could count actual lines
    // within paragraphs for multi-line paragraphs
    return paragraphIndex + 1;
}

void TagDetector::onDocumentChanged()
{
    // Full rescan on document change
    // This is called for general content changes
    // For performance, we could debounce this
    scan();
}

void TagDetector::onParagraphModified(int index)
{
    // Rescan just the modified paragraph
    scanParagraph(index);
}

}  // namespace kalahari::editor
