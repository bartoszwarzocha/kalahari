/// @file buffer_commands.cpp
/// @brief Cursor positions and TODO/Note markers in a QTextDocument

#include <kalahari/editor/buffer_commands.h>
#include <QDateTime>
#include <QStringList>
#include <QTextBlock>
#include <QUuid>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// TextMarker Implementation
// =============================================================================

namespace {

/// Attributes of the KML <todo> tag that TextMarker has fields for
const QStringList& markerFieldAttributes()
{
    static const QStringList attributes = {
        QStringLiteral("id"), QStringLiteral("text"), QStringLiteral("type"),
        QStringLiteral("completed"), QStringLiteral("priority"), QStringLiteral("created")};
    return attributes;
}

}  // anonymous namespace

QVariantMap TextMarker::toVariantMap() const
{
    QVariantMap map = otherAttributes;
    map[QStringLiteral("id")] = id;
    if (!text.isEmpty()) {
        map[QStringLiteral("text")] = text;
    }
    if (type == MarkerType::Note) {
        map[QStringLiteral("type")] = QStringLiteral("note");
    }
    map[QStringLiteral("completed")] = completed;
    if (!priority.isEmpty()) {
        map[QStringLiteral("priority")] = priority;
    }
    if (!timestamp.isEmpty()) {
        map[QStringLiteral("created")] = timestamp;
    }
    return map;
}

std::optional<TextMarker> TextMarker::fromVariant(const QVariant& value)
{
    if (value.typeId() != QMetaType::QVariantMap) {
        return std::nullopt;
    }

    const QVariantMap map = value.toMap();
    TextMarker marker;
    marker.id = map.value(QStringLiteral("id")).toString();
    marker.text = map.value(QStringLiteral("text")).toString();
    marker.type = (map.value(QStringLiteral("type")).toString() == QStringLiteral("note"))
        ? MarkerType::Note
        : MarkerType::Todo;
    marker.completed = map.value(QStringLiteral("completed")).toBool();
    marker.priority = map.value(QStringLiteral("priority")).toString();
    marker.timestamp = map.value(QStringLiteral("created")).toString();
    marker.otherAttributes = map;
    for (const QString& attribute : markerFieldAttributes()) {
        marker.otherAttributes.remove(attribute);
    }
    return marker;
}

QString TextMarker::generateId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// =============================================================================
// Helper Functions
// =============================================================================

int calculateAbsolutePosition(const QTextDocument* document, int blockNumber, int offset)
{
    if (!document) {
        return 0;
    }

    QTextBlock block = document->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        // Return end of document if block doesn't exist
        return document->characterCount() - 1;
    }

    return block.position() + offset;
}

int calculateAbsolutePosition(const QTextDocument* document, const CursorPosition& pos)
{
    return calculateAbsolutePosition(document, pos.paragraph, pos.offset);
}

CursorPosition absoluteToCursorPosition(const QTextDocument* document, int absolutePos)
{
    if (!document) {
        return CursorPosition{0, 0};
    }

    QTextBlock block = document->findBlock(absolutePos);
    if (!block.isValid()) {
        // Clamp to end of document
        QTextBlock lastBlock = document->lastBlock();
        return CursorPosition{
            lastBlock.blockNumber(),
            lastBlock.length() - 1  // -1 for block separator
        };
    }

    return CursorPosition{
        block.blockNumber(),
        absolutePos - block.position()
    };
}

QTextCursor createCursor(QTextDocument* document, const CursorPosition& pos)
{
    if (!document) {
        return QTextCursor();
    }

    int absPos = calculateAbsolutePosition(document, pos);
    QTextCursor cursor(document);
    cursor.setPosition(absPos);
    return cursor;
}

QTextCursor createCursor(QTextDocument* document, const CursorPosition& start, const CursorPosition& end)
{
    if (!document) {
        return QTextCursor();
    }

    int startPos = calculateAbsolutePosition(document, start);
    int endPos = calculateAbsolutePosition(document, end);

    QTextCursor cursor(document);
    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);
    return cursor;
}

// =============================================================================
// Marker Utility Functions
// =============================================================================

std::vector<TextMarker> findAllMarkers(const QTextDocument* document,
                                       std::optional<MarkerType> typeFilter)
{
    std::vector<TextMarker> markers;

    if (!document) {
        return markers;
    }

    // Iterate through all characters in the document
    QTextBlock block = document->begin();
    while (block.isValid()) {
        QTextBlock::iterator it;
        for (it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }

            auto markerOpt = TextMarker::fromVariant(fragment.charFormat().property(KmlPropTodo));
            if (!markerOpt || (typeFilter && markerOpt->type != *typeFilter)) {
                continue;
            }

            TextMarker marker = *markerOpt;
            marker.position = fragment.position();
            marker.length = fragment.length();

            // An anchor split into several fragments (e.g. partly bold) is one marker
            if (!markers.empty() && !marker.id.isEmpty() && markers.back().id == marker.id &&
                markers.back().position + markers.back().length == marker.position) {
                markers.back().length += marker.length;
                continue;
            }
            markers.push_back(marker);
        }
        block = block.next();
    }

    // Sort by position
    std::sort(markers.begin(), markers.end(),
              [](const TextMarker& a, const TextMarker& b) {
                  return a.position < b.position;
              });

    return markers;
}

std::optional<TextMarker> findMarkerById(const QTextDocument* document, const QString& markerId)
{
    auto markers = findAllMarkers(document);

    for (const auto& marker : markers) {
        if (marker.id == markerId) {
            return marker;
        }
    }

    return std::nullopt;
}

std::optional<TextMarker> findNextMarker(const QTextDocument* document,
                                         int fromPosition,
                                         std::optional<MarkerType> typeFilter)
{
    auto markers = findAllMarkers(document, typeFilter);

    for (const auto& marker : markers) {
        if (marker.position > fromPosition) {
            return marker;
        }
    }

    // Wrap around to beginning
    if (!markers.empty()) {
        return markers.front();
    }

    return std::nullopt;
}

std::optional<TextMarker> findPreviousMarker(const QTextDocument* document,
                                             int fromPosition,
                                             std::optional<MarkerType> typeFilter)
{
    auto markers = findAllMarkers(document, typeFilter);

    // Search in reverse order
    for (auto it = markers.rbegin(); it != markers.rend(); ++it) {
        if (it->position < fromPosition) {
            return *it;
        }
    }

    // Wrap around to end
    if (!markers.empty()) {
        return markers.back();
    }

    return std::nullopt;
}

void setMarkerInDocument(QTextDocument* document, const TextMarker& marker)
{
    if (!document) {
        return;
    }

    // Select the anchor text: one character for a new marker, the whole anchor for a
    // marker found in the document (e.g. a <todo> loaded from KML)
    const int lastPosition = document->characterCount() - 1;
    const int start = qBound(0, marker.position, lastPosition);
    const int end = qBound(start, marker.position + qMax(1, marker.length), lastPosition);

    QTextCursor cursor(document);
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);

    // Set the marker property
    QTextCharFormat format;
    format.setProperty(KmlPropTodo, marker.toVariantMap());
    cursor.mergeCharFormat(format);
}

void removeMarkerFromDocument(QTextDocument* document, int position, int length)
{
    if (!document) {
        return;
    }

    // Clear the property fragment by fragment, so each fragment of the anchor text keeps
    // the rest of its formatting (setCharFormat() over the whole range would flatten it).
    // Collect first: changing formats while iterating would invalidate the iterators.
    struct Range {
        int start;
        int end;
        QTextCharFormat format;
    };
    std::vector<Range> ranges;
    const int end = position + qMax(1, length);
    for (QTextBlock block = document->findBlock(position);
         block.isValid() && block.position() < end; block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const int fragmentStart = qMax(fragment.position(), position);
            const int fragmentEnd = qMin(fragment.position() + fragment.length(), end);
            QTextCharFormat format = fragment.charFormat();
            if (fragmentStart < fragmentEnd && format.hasProperty(KmlPropTodo)) {
                format.clearProperty(KmlPropTodo);
                ranges.push_back({fragmentStart, fragmentEnd, format});
            }
        }
    }

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    for (const Range& range : ranges) {
        cursor.setPosition(range.start);
        cursor.setPosition(range.end, QTextCursor::KeepAnchor);
        cursor.setCharFormat(range.format);
    }
    cursor.endEditBlock();
}

}  // namespace kalahari::editor
