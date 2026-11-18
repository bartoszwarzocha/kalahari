// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file text_skeleton.cpp
/// @brief The skeleton of a text for publishers: the headings of the points and their hints

#include "kalahari/editor/text_skeleton.h"

#include "kalahari/editor/kml_format_registry.h"
#include "kalahari/editor/paragraph_styles.h"

#include <QStringList>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

#include <optional>

namespace kalahari::editor {

namespace {

/// Whether @p block is in a cell of a table
bool inTable(const QTextBlock& block) {
    return QTextCursor(block).currentTable() != nullptr;
}

/// The format of the paragraph of the heading of @p point
QTextBlockFormat headingFormat(const SkeletonPoint& point) {
    QTextBlockFormat format = paragraphBlockFormat();
    setParagraphStyle(format, QString::fromLatin1(TEMPLATE_POINT_STYLE_ID));
    setTemplatePoint(format, point);
    return format;
}

/// Remove @p heading with the empty paragraph of its hint after it
/// @return The block to look at next
QTextBlock removeHeading(QTextCursor& cursor, const QTextBlock& heading) {
    QTextBlock last = heading;
    if (const QTextBlock next = heading.next();
        next.isValid() && next.length() == 1 && !isTemplatePointBlock(next) && !inTable(next)) {
        last = next;
    }
    const int end = last.position() + last.length() - 1;

    // From the end of the paragraph before it, which stays as it is
    if (const QTextBlock before = heading.previous(); before.isValid() && !inTable(before)) {
        cursor.setPosition(before.position() + before.length() - 1);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        return cursor.block().next();
    }
    // Up to the paragraph after it, which keeps its formats
    if (const QTextBlock after = last.next(); after.isValid() && !inTable(after)) {
        const QTextBlockFormat format = after.blockFormat();
        const QTextCharFormat charFormat = after.charFormat();
        cursor.setPosition(heading.position());
        cursor.setPosition(after.position(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.setBlockFormat(format);
        cursor.setBlockCharFormat(charFormat);
        return cursor.block();  // it may be a heading too
    }
    // Alone in the text or between tables: an empty paragraph stays in its place
    cursor.setPosition(heading.position());
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.setBlockFormat(paragraphBlockFormat());
    cursor.setBlockCharFormat(QTextCharFormat());
    return cursor.block().next();
}

}  // namespace

std::optional<SkeletonPoint> templatePointOf(const QTextBlockFormat& format) {
    const QString id = format.stringProperty(TEMPLATE_POINT_PROPERTY);
    if (id.isEmpty()) {
        return std::nullopt;
    }
    return SkeletonPoint{id, QString(), format.stringProperty(TEMPLATE_HINT_PROPERTY)};
}

void setTemplatePoint(QTextBlockFormat& format, const std::optional<SkeletonPoint>& point) {
    if (!point.has_value() || point->id.isEmpty()) {
        format.clearProperty(TEMPLATE_POINT_PROPERTY);
        format.clearProperty(TEMPLATE_HINT_PROPERTY);
        return;
    }
    format.setProperty(TEMPLATE_POINT_PROPERTY, point->id);
    if (point->hint.isEmpty()) {
        format.clearProperty(TEMPLATE_HINT_PROPERTY);
    } else {
        format.setProperty(TEMPLATE_HINT_PROPERTY, point->hint);
    }
}

bool isTemplatePointBlock(const QTextBlock& block) {
    return block.isValid() &&
           paragraphStyleOf(block.blockFormat()) == QLatin1String(TEMPLATE_POINT_STYLE_ID);
}

QString skeletonHintOf(const QTextBlock& block) {
    if (!block.isValid() || block.length() != 1 || isTemplatePointBlock(block)) {
        return {};
    }
    const QTextBlock heading = block.previous();
    if (!isTemplatePointBlock(heading)) {
        return {};
    }
    return heading.blockFormat().stringProperty(TEMPLATE_HINT_PROPERTY);
}

QString skeletonKml(const QList<SkeletonPoint>& points) {
    QString kml;
    for (const SkeletonPoint& point : points) {
        kml += QStringLiteral("<p style=\"") + QLatin1String(TEMPLATE_POINT_STYLE_ID) +
               QLatin1Char('"');
        if (!point.id.isEmpty()) {
            kml += QStringLiteral(" point=\"") +
                   KmlFormatRegistry::escapeXmlAttribute(point.id) + QLatin1Char('"');
            if (!point.hint.isEmpty()) {
                kml += QStringLiteral(" hint=\"") +
                       KmlFormatRegistry::escapeXmlAttribute(point.hint) + QLatin1Char('"');
            }
        }
        kml += QLatin1Char('>') + KmlFormatRegistry::escapeXml(point.name) +
               QStringLiteral("</p><p></p>");
    }
    return kml;
}

int insertSkeleton(QTextCursor& cursor, const QList<SkeletonPoint>& points) {
    if (points.isEmpty() || cursor.currentTable() != nullptr) {
        return -1;
    }
    cursor.clearSelection();
    leaveObject(cursor);
    if (cursor.block().length() > 1) {
        // A paragraph with text: the skeleton after it
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertBlock(paragraphBlockFormat(), QTextCharFormat());
    }
    int first = -1;
    for (const SkeletonPoint& point : points) {
        if (first >= 0) {
            cursor.insertBlock(paragraphBlockFormat(), QTextCharFormat());
        }
        cursor.setBlockFormat(headingFormat(point));
        cursor.setBlockCharFormat(QTextCharFormat());
        cursor.insertText(point.name, QTextCharFormat());
        cursor.insertBlock(paragraphBlockFormat(), QTextCharFormat());  // its hint
        if (first < 0) {
            first = cursor.position();
        }
    }
    return first;
}

int removeSkeleton(QTextCursor& cursor) {
    int removed = 0;
    QTextBlock block = cursor.document()->firstBlock();
    while (block.isValid()) {
        if (isTemplatePointBlock(block) && !inTable(block)) {
            block = removeHeading(cursor, block);
            ++removed;
        } else {
            block = block.next();
        }
    }
    return removed;
}

bool hasSkeleton(const QTextDocument& document) {
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (isTemplatePointBlock(block)) {
            return true;
        }
    }
    return false;
}

QString textWithoutSkeleton(const QTextDocument& document) {
    QStringList paragraphs;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (!isTemplatePointBlock(block)) {
            paragraphs << block.text().remove(QChar::ObjectReplacementCharacter);
        }
    }
    return paragraphs.join(QLatin1Char('\n'));
}

}  // namespace kalahari::editor
