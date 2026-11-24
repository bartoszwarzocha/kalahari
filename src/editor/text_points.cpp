// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file text_points.cpp
/// @brief Fragments of a text for publishers linked to the points of its template

#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/editor/text_points.h>

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QVariantMap>

#include <algorithm>
#include <vector>

namespace kalahari::editor {

namespace {

/// @brief A piece of the text with one format
struct PointPiece {
    int start = 0;
    int end = 0;
    QTextCharFormat format;
};

/// @brief The format of the character at @p position (not a paragraph break)
QTextCharFormat formatOfCharacter(const QTextDocument& document, int position) {
    QTextCursor cursor(const_cast<QTextDocument*>(&document));
    cursor.setPosition(position + 1);
    return cursor.charFormat();  // of the character before the cursor
}

/// @brief Give the pieces their formats, as one undo step
void applyPointPieces(QTextDocument& document, const std::vector<PointPiece>& pieces) {
    QTextCursor cursor(&document);
    cursor.beginEditBlock();
    for (const PointPiece& piece : pieces) {
        cursor.setPosition(piece.start);
        cursor.setPosition(piece.end, QTextCursor::KeepAnchor);
        cursor.setCharFormat(piece.format);
    }
    cursor.endEditBlock();
}

/// @brief Calls @p visit(start, end, format) for each piece of the text from @p from to
///        @p to with one format, in the order of the text
template <typename Visit>
void forEachPiece(const QTextDocument& document, int from, int to, Visit visit) {
    for (QTextBlock block = document.findBlock(from); block.isValid() && block.position() < to;
         block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const int start = std::max(fragment.position(), from);
            const int end = std::min(fragment.position() + fragment.length(), to);
            if (start < end) {
                visit(start, end, fragment.charFormat());
            }
        }
    }
}

} // anonymous namespace

QStringList templatePointsOf(const QTextFormat& format) {
    const QVariant value = format.property(KmlPropTemplatePoints);
    if (!value.isValid()) {
        return {};
    }
    return value.toMap()
        .value(QString::fromLatin1(TEXT_POINTS_IDS))
        .toString()
        .split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

void setTemplatePoints(QTextFormat& format, const QStringList& ids) {
    if (ids.isEmpty()) {
        format.clearProperty(KmlPropTemplatePoints);
        return;
    }
    QVariantMap value;
    value.insert(QString::fromLatin1(TEXT_POINTS_IDS), ids.join(QLatin1Char(' ')));
    format.setProperty(KmlPropTemplatePoints, value);
}

QHash<QString, QList<PointFragment>> templatePointFragments(const QTextDocument& document) {
    QHash<QString, QList<PointFragment>> fragments;
    QHash<QString, PointFragment> open;  // the fragments the last piece belongs to
    const auto close = [&fragments, &open](const QString& id) {
        fragments[id].append(open.take(id));
    };
    forEachPiece(document, 0, document.characterCount(),
                 [&](int start, int end, const QTextCharFormat& format) {
                     const QStringList ids = templatePointsOf(format);
                     const QStringList openIds = open.keys();
                     for (const QString& id : openIds) {
                         if (!ids.contains(id)) {
                             close(id);
                         }
                     }
                     for (const QString& id : ids) {
                         auto it = open.find(id);
                         if (it == open.end()) {
                             it = open.insert(id, PointFragment{start, start, 0});
                         }
                         it->end = end;
                         it->characters += end - start;
                     }
                 });
    const QStringList openIds = open.keys();
    for (const QString& id : openIds) {
        close(id);
    }
    return fragments;
}

QStringList templatePointsAt(const QTextDocument& document, int position) {
    const QTextBlock block = document.findBlock(position);
    if (!block.isValid()) {
        return {};
    }
    QStringList ids;
    if (position > block.position()) {
        ids = templatePointsOf(formatOfCharacter(document, position - 1));
    }
    if (position < block.position() + block.length() - 1) {
        for (const QString& id : templatePointsOf(formatOfCharacter(document, position))) {
            if (!ids.contains(id)) {
                ids.append(id);
            }
        }
    }
    return ids;
}

QStringList templatePointsIn(const QTextDocument& document, int from, int to) {
    QStringList ids;
    forEachPiece(document, from, to, [&ids](int, int, const QTextCharFormat& format) {
        for (const QString& id : templatePointsOf(format)) {
            if (!ids.contains(id)) {
                ids.append(id);
            }
        }
    });
    return ids;
}

namespace {

/// @brief The pieces from @p from to @p to whose link to @p id changes, with their new formats
std::vector<PointPiece> linkPieces(const QTextDocument& document, int from, int to,
                                   const QString& id, bool linked) {
    std::vector<PointPiece> pieces;
    if (id.isEmpty() || from >= to) {
        return pieces;
    }
    forEachPiece(document, from, to,
                 [&pieces, &id, linked](int start, int end, const QTextCharFormat& format) {
                     QStringList ids = templatePointsOf(format);
                     if (ids.contains(id) == linked) {
                         return;
                     }
                     if (linked) {
                         ids.append(id);
                     } else {
                         ids.removeAll(id);
                     }
                     QTextCharFormat changed = format;
                     setTemplatePoints(changed, ids);
                     pieces.push_back({start, end, changed});
                 });
    return pieces;
}

} // anonymous namespace

bool changesTemplatePointLink(const QTextDocument& document, int from, int to,
                              const QString& id, bool linked) {
    return !linkPieces(document, from, to, id, linked).empty();
}

bool linkTemplatePoint(QTextDocument& document, int from, int to, const QString& id,
                       bool linked) {
    const std::vector<PointPiece> pieces = linkPieces(document, from, to, id, linked);
    if (pieces.empty()) {
        return false;
    }
    applyPointPieces(document, pieces);
    return true;
}

bool unlinkTemplatePoint(QTextDocument& document, const QString& id) {
    return linkTemplatePoint(document, 0, document.characterCount(), id, false);
}

void keepTemplatePointsBetween(QTextCharFormat& format, const QTextDocument& document,
                               int position) {
    QStringList kept;
    const QTextBlock block = document.findBlock(position);
    if (block.isValid() && position > block.position() &&
        position < block.position() + block.length() - 1) {
        const QStringList after = templatePointsOf(formatOfCharacter(document, position));
        for (const QString& id : templatePointsOf(formatOfCharacter(document, position - 1))) {
            if (after.contains(id)) {
                kept.append(id);
            }
        }
    }
    setTemplatePoints(format, kept);
}

} // namespace kalahari::editor
