/// @file tag_detector.cpp
/// @brief Implementation of tag detection service (OpenSpec #00042 Task 7.10)

#include "kalahari/editor/tag_detector.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/core/logger.h"

namespace kalahari::editor {

// Static regex pattern for tag detection
// Matches: TODO, FIX, CHECK, NOTE, WARNING followed by optional colon and content
// Pattern: word boundary + keyword + optional whitespace + optional colon + rest of line
const QRegularExpression TagDetector::s_tagPattern(
    QStringLiteral("\\b(TODO|FIX|CHECK|NOTE|WARNING)\\s*:?\\s*(.*)"),
    QRegularExpression::CaseInsensitiveOption
);

TagDetector::TagDetector(QObject* parent)
    : QObject(parent)
    , m_editor(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("TagDetector constructor called");
}

TagDetector::~TagDetector()
{
    // Disconnect from editor before destruction
    if (m_editor) {
        disconnect(m_editor, nullptr, this, nullptr);
    }
}

void TagDetector::setBookEditor(BookEditor* editor)
{
    auto& logger = core::Logger::getInstance();

    // Disconnect from old editor
    if (m_editor) {
        disconnect(m_editor, nullptr, this, nullptr);
        logger.debug("TagDetector: Disconnected from previous editor");
    }

    m_editor = editor;
    m_tags.clear();

    // Connect to new editor
    if (m_editor) {
        connect(m_editor, &BookEditor::paragraphModified,
                this, &TagDetector::onParagraphModified);
        connect(m_editor, &BookEditor::paragraphInserted,
                this, &TagDetector::onParagraphInserted);
        connect(m_editor, &BookEditor::paragraphRemoved,
                this, &TagDetector::onParagraphRemoved);
        logger.debug("TagDetector: Connected to new editor");
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

    if (m_editor == nullptr) {
        emit tagsChanged();
        return;
    }

    logger.debug("TagDetector: Scanning {} paragraphs", m_editor->paragraphCount());

    for (size_t i = 0; i < m_editor->paragraphCount(); ++i) {
        QString text = m_editor->paragraphPlainText(i);
        detectTagsInText(text, static_cast<int>(i));
    }

    logger.debug("TagDetector: Found {} tags", m_tags.size());
    emit tagsChanged();
}

void TagDetector::scanParagraph(int index)
{
    if (m_editor == nullptr || index < 0 || static_cast<size_t>(index) >= m_editor->paragraphCount()) {
        return;
    }

    // Remove existing tags for this paragraph
    int removed = removeTagsForParagraph(index);

    // Scan the paragraph
    QString text = m_editor->paragraphPlainText(static_cast<size_t>(index));
    detectTagsInText(text, index);

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

// =============================================================================
// BookEditor Signal Handlers
// =============================================================================

void TagDetector::onParagraphModified(int paragraphIndex)
{
    // Rescan just the modified paragraph
    scanParagraph(paragraphIndex);
}

void TagDetector::onParagraphInserted(int paragraphIndex)
{
    Q_UNUSED(paragraphIndex);
    // Rescan all (line numbers shifted)
    scan();
}

void TagDetector::onParagraphRemoved(int paragraphIndex)
{
    // Remove tags for deleted paragraph and rescan (line numbers shifted)
    removeTagsForParagraph(paragraphIndex);
    scan();
}

}  // namespace kalahari::editor
