/// @file annotation.cpp
/// @brief Annotations on the text: comments, TODOs and notes

#include <kalahari/editor/annotation.h>
#include <kalahari/editor/kml_format_registry.h>

#include <QTextBlock>
#include <QTextBoundaryFinder>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QUuid>

#include <algorithm>

namespace kalahari::editor {

namespace {

/// @brief Whether a list has the annotation with this id on a fragment (not on a place)
bool hasFragment(const AnnotationList& list, const QString& id) {
    return std::any_of(list.cbegin(), list.cend(),
                       [&id](const Annotation& a) { return a.id == id && !a.point; });
}

/// @brief The format of the character at @p position (a character of the text, not a
/// paragraph break)
QTextCharFormat characterFormat(const QTextDocument& document, int position) {
    QTextCursor cursor(const_cast<QTextDocument*>(&document));
    cursor.setPosition(position + 1);
    return cursor.charFormat();  // of the character before the cursor
}

/// @brief The format of the nearest character of the text before @p position
///
/// Paragraph breaks and empty paragraphs are passed over. An empty format at the start of
/// the text.
QTextCharFormat textFormatBefore(const QTextDocument& document, int position) {
    int place = position;
    for (QTextBlock block = document.findBlock(position); block.isValid();
         block = block.previous()) {
        if (place > block.position()) {
            return characterFormat(document, place - 1);
        }
        if (block.previous().isValid()) {
            place = block.previous().position() + block.previous().length() - 1;
        }
    }
    return {};
}

/// @brief The format of the nearest character of the text from @p position on
///
/// Paragraph breaks and empty paragraphs are passed over. An empty format at the end of
/// the text.
QTextCharFormat textFormatFrom(const QTextDocument& document, int position) {
    int place = position;
    for (QTextBlock block = document.findBlock(position); block.isValid(); block = block.next()) {
        if (place < block.position() + block.length() - 1) {
            return characterFormat(document, place);
        }
        if (block.next().isValid()) {
            place = block.next().position();
        }
    }
    return {};
}

/// @brief A piece of the text with one format: part of a fragment, or a paragraph's own
/// format (its block character format)
struct FormatPiece {
    int start = 0;           ///< First character (for a paragraph: where it starts)
    int end = 0;             ///< After the last character (for a paragraph: -1)
    QTextCharFormat format;  ///< The format of the piece
    bool isParagraph() const { return end < 0; }
};

/// @brief The pieces of the text from @p from to @p to with annotations, and the
/// paragraphs that start inside the range (@p from excluded) with annotations on their start
std::vector<FormatPiece> annotatedPieces(const QTextDocument& document, int from, int to,
                                         bool paragraphAtFrom) {
    std::vector<FormatPiece> pieces;
    for (QTextBlock block = document.findBlock(from); block.isValid() && block.position() <= to;
         block = block.next()) {
        const bool startsInside =
            block.position() > from || (paragraphAtFrom && block.position() == from);
        if (startsInside && !annotationsOf(block.charFormat()).isEmpty()) {
            pieces.push_back({block.position(), -1, block.charFormat()});
        }
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const int start = std::max(fragment.position(), from);
            const int end = std::min(fragment.position() + fragment.length(), to);
            if (start < end && !annotationsOf(fragment.charFormat()).isEmpty()) {
                pieces.push_back({start, end, fragment.charFormat()});
            }
        }
    }
    return pieces;
}

/// @brief Give the pieces of the text new formats, as one undo step
///
/// The format of a piece is replaced as a whole: the new one is the piece's format with
/// other annotations, so nothing else changes.
void applyPieces(QTextDocument& document, const std::vector<FormatPiece>& pieces,
                 bool joinPreviousStep = false) {
    if (pieces.empty()) {
        return;
    }
    QTextCursor cursor(&document);
    if (joinPreviousStep) {
        cursor.joinPreviousEditBlock();
    } else {
        cursor.beginEditBlock();
    }
    for (const FormatPiece& piece : pieces) {
        cursor.setPosition(piece.start);
        if (piece.isParagraph()) {
            cursor.setBlockCharFormat(piece.format);
        } else {
            cursor.setPosition(piece.end, QTextCursor::KeepAnchor);
            cursor.setCharFormat(piece.format);
        }
    }
    cursor.endEditBlock();
}

/// @brief The pieces of the whole text with the annotation @p id, with new formats
///
/// @p change gives the new annotations of a piece from the annotation with @p id and its
/// current annotations.
template <typename Change>
std::vector<FormatPiece> changedPieces(const QTextDocument& document, const QString& id,
                                       Change change) {
    std::vector<FormatPiece> pieces;
    for (FormatPiece piece : annotatedPieces(document, 0, document.characterCount() - 1, true)) {
        const AnnotationList annotations = annotationsOf(piece.format);
        const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
                                     [&id](const Annotation& a) { return a.id == id; });
        if (it == annotations.cend() || (piece.isParagraph() && !it->point)) {
            continue;
        }
        setAnnotations(piece.format, change(*it, annotations));
        pieces.push_back(std::move(piece));
    }
    return pieces;
}

/// @brief Insert text with a format; a line break starts a new paragraph
///
/// The new paragraphs carry no annotations of their own.
void insertLines(QTextCursor& cursor, const QString& text, const QTextCharFormat& format) {
    QStringList lines;
    qsizetype lineStart = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == u'\n' || ch == u'\r' || ch == QChar::ParagraphSeparator) {
            lines.append(text.mid(lineStart, i - lineStart));
            if (ch == u'\r' && i + 1 < text.size() && text.at(i + 1) == u'\n') {
                ++i;
            }
            lineStart = i + 1;
        }
    }
    if (lines.isEmpty()) {
        cursor.insertText(text, format);  // typing: one undo step with the text typed before
        return;
    }
    lines.append(text.mid(lineStart));

    QTextCharFormat paragraphFormat = format;
    setAnnotations(paragraphFormat, {});
    cursor.beginEditBlock();
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            cursor.insertBlock(cursor.blockFormat(), paragraphFormat);
        }
        if (!lines.at(i).isEmpty()) {
            cursor.insertText(lines.at(i), format);
        }
    }
    cursor.endEditBlock();
}

}  // namespace

// =============================================================================
// Annotations
// =============================================================================

QString annotationKindName(AnnotationKind kind) {
    switch (kind) {
    case AnnotationKind::Comment:
        return QStringLiteral("comment");
    case AnnotationKind::Todo:
        return QStringLiteral("todo");
    case AnnotationKind::Note:
        return QStringLiteral("note");
    }
    return QStringLiteral("comment");
}

std::optional<AnnotationKind> annotationKindFromName(QStringView name) {
    for (const AnnotationKind kind :
         {AnnotationKind::Comment, AnnotationKind::Todo, AnnotationKind::Note}) {
        if (name == annotationKindName(kind)) {
            return kind;
        }
    }
    return std::nullopt;
}

std::string annotationColorKey(AnnotationKind kind, bool darkPaper) {
    std::string key;
    switch (kind) {
    case AnnotationKind::Comment:
        key = "annotationComment";
        break;
    case AnnotationKind::Todo:
        key = "annotationTodo";
        break;
    case AnnotationKind::Note:
        key = "annotationNote";
        break;
    }
    return key + (darkPaper ? "DarkPaper" : "LightPaper");
}

QString newAnnotationId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

AnnotationList annotationsOf(const QTextFormat& format) {
    const QVariant value = format.property(KmlPropAnnotations);
    return value.canConvert<AnnotationList>() ? value.value<AnnotationList>() : AnnotationList();
}

void setAnnotations(QTextFormat& format, const AnnotationList& annotations) {
    if (annotations.isEmpty()) {
        format.clearProperty(KmlPropAnnotations);
    } else {
        format.setProperty(KmlPropAnnotations, QVariant::fromValue(annotations));
    }
}

AnnotationList withAnnotation(AnnotationList list, const Annotation& annotation) {
    for (Annotation& existing : list) {
        if (existing.id == annotation.id) {
            existing = annotation;
            return list;
        }
    }
    list.append(annotation);
    return list;
}

// =============================================================================
// Annotations in a document
// =============================================================================

std::vector<AnnotationPlace> annotationsIn(const QTextDocument& document) {
    std::vector<AnnotationPlace> places;
    QHash<QString, size_t> placeOf;  // id -> index in places
    auto note = [&places, &placeOf](const Annotation& annotation, int start, int end) {
        const auto it = placeOf.constFind(annotation.id);
        if (it == placeOf.cend()) {
            placeOf.insert(annotation.id, places.size());
            places.push_back({annotation, start, end});
        } else {
            AnnotationPlace& place = places[*it];
            place.start = std::min(place.start, start);
            place.end = std::max(place.end, end);
        }
    };

    for (const FormatPiece& piece :
         annotatedPieces(document, 0, document.characterCount() - 1, true)) {
        for (const Annotation& annotation : annotationsOf(piece.format)) {
            if (piece.isParagraph()) {
                if (annotation.point) {  // the paragraph's start
                    note(annotation, piece.start, piece.start);
                }
            } else if (annotation.point) {  // after the piece's last character
                note(annotation, piece.end, piece.end);
            } else {
                note(annotation, piece.start, piece.end);
            }
        }
    }

    std::stable_sort(places.begin(), places.end(),
                     [](const AnnotationPlace& a, const AnnotationPlace& b) {
                         return a.end != b.end ? a.end < b.end : a.start < b.start;
                     });
    return places;
}

std::optional<AnnotationPlace> findAnnotation(const QTextDocument& document, const QString& id) {
    for (const AnnotationPlace& place : annotationsIn(document)) {
        if (place.annotation.id == id) {
            return place;
        }
    }
    return std::nullopt;
}

QSet<QString> annotationIdsIn(const QTextDocument& document) {
    QSet<QString> ids;
    for (const AnnotationPlace& place : annotationsIn(document)) {
        ids.insert(place.annotation.id);
    }
    return ids;
}

Annotation addAnnotation(const QTextCursor& cursor, Annotation annotation) {
    QTextDocument& document = *cursor.document();
    if (annotation.id.isEmpty()) {
        annotation.id = newAnnotationId();
    }

    // Every character of the selection - the paragraph breaks in it excluded
    std::vector<FormatPiece> pieces;
    const int from = cursor.selectionStart();
    const int to = cursor.selectionEnd();
    if (from < to) {
        annotation.point = false;
        for (QTextBlock block = document.findBlock(from); block.isValid() && block.position() < to;
             block = block.next()) {
            for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
                const QTextFragment fragment = it.fragment();
                const int start = std::max(fragment.position(), from);
                const int end = std::min(fragment.position() + fragment.length(), to);
                if (start < end) {
                    QTextCharFormat format = fragment.charFormat();
                    setAnnotations(format, withAnnotation(annotationsOf(format), annotation));
                    pieces.push_back({start, end, format});
                }
            }
        }
    }

    if (pieces.empty()) {
        annotation.point = true;
        anchorToPlace(document, from, {annotation});
    } else {
        applyPieces(document, pieces);
    }
    return annotation;
}

bool updateAnnotation(QTextDocument& document, const Annotation& annotation,
                      bool joinPreviousStep) {
    const std::vector<FormatPiece> pieces = changedPieces(
        document, annotation.id, [&annotation](const Annotation& old, const AnnotationList& list) {
            Annotation updated = annotation;
            updated.point = old.point;  // the anchor stays as it is
            return withAnnotation(list, updated);
        });
    if (pieces.empty()) {
        return false;
    }
    applyPieces(document, pieces, joinPreviousStep);
    return true;
}

bool removeAnnotation(QTextDocument& document, const QString& id) {
    const std::vector<FormatPiece> pieces =
        changedPieces(document, id, [&id](const Annotation&, AnnotationList list) {
            list.removeIf([&id](const Annotation& a) { return a.id == id; });
            return list;
        });
    if (pieces.empty()) {
        return false;
    }
    applyPieces(document, pieces);
    return true;
}

// =============================================================================
// Editing with annotations
// =============================================================================

QTextCharFormat insertionFormat(const QTextCursor& cursor) {
    QTextCharFormat format = cursor.charFormat();

    // Inside a fragment: between two of its characters, in one paragraph
    AnnotationList inside;
    const QTextDocument& document = *cursor.document();
    const int position = cursor.position();
    const QTextBlock block = cursor.block();
    if (position > block.position() && position < block.position() + block.length() - 1) {
        const AnnotationList after = annotationsOf(characterFormat(document, position));
        for (const Annotation& annotation : annotationsOf(characterFormat(document, position - 1))) {
            if (!annotation.point && hasFragment(after, annotation.id)) {
                inside.append(annotation);
            }
        }
    }
    setAnnotations(format, inside);
    return format;
}

AnnotationList enclosingFragments(const QTextDocument& document, int from, int to) {
    std::optional<AnnotationList> common;
    for (QTextBlock block = document.findBlock(from); block.isValid() && block.position() < to;
         block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (std::max(fragment.position(), from) >=
                std::min(fragment.position() + fragment.length(), to)) {
                continue;
            }
            const AnnotationList annotations = annotationsOf(fragment.charFormat());
            if (!common) {
                common = annotations;
                common->removeIf([](const Annotation& a) { return a.point; });
            } else {
                common->removeIf([&annotations](const Annotation& a) {
                    return !hasFragment(annotations, a.id);
                });
            }
            if (common->isEmpty()) {
                return {};
            }
        }
    }
    return common.value_or(AnnotationList());
}

AnnotationList annotationsLeavingWith(const QTextDocument& document, int from, int to) {
    if (from >= to) {
        return {};
    }

    // The annotations on the text that goes, with where they end
    std::vector<AnnotationPlace> leaving;
    std::vector<AnnotationPlace> fragments;  // annotations on a fragment with characters going
    for (const FormatPiece& piece : annotatedPieces(document, from, to, false)) {
        for (const Annotation& annotation : annotationsOf(piece.format)) {
            if (piece.isParagraph()) {
                // A paragraph that starts inside merges into the one before it
                if (annotation.point) {
                    leaving.push_back({annotation, piece.start, piece.start});
                }
            } else if (annotation.point) {  // on a character that goes
                leaving.push_back({annotation, piece.end, piece.end});
            } else {
                fragments.push_back({annotation, piece.start, piece.end});
            }
        }
    }

    // A fragment goes when no character of it stays. Its characters follow one another,
    // so the nearest characters on both sides of the text that goes tell.
    if (!fragments.empty()) {
        const AnnotationList before = annotationsOf(textFormatBefore(document, from));
        const AnnotationList after = annotationsOf(textFormatFrom(document, to));
        for (const AnnotationPlace& place : fragments) {
            if (!hasFragment(before, place.annotation.id) &&
                !hasFragment(after, place.annotation.id)) {
                leaving.push_back(place);
            }
        }
    }

    std::stable_sort(leaving.begin(), leaving.end(),
                     [](const AnnotationPlace& a, const AnnotationPlace& b) { return a.end < b.end; });
    AnnotationList result;
    for (AnnotationPlace& place : leaving) {
        place.annotation.point = true;
        result = withAnnotation(result, place.annotation);
    }
    return result;
}

void anchorToPlace(QTextDocument& document, int position, const AnnotationList& annotations) {
    if (annotations.isEmpty()) {
        return;
    }
    const QTextBlock block = document.findBlock(position);
    FormatPiece piece;
    if (position > block.position()) {
        // The character before the place: both halves of a surrogate pair
        int start = position - 1;
        if (start > block.position() && document.characterAt(start).isLowSurrogate() &&
            document.characterAt(start - 1).isHighSurrogate()) {
            --start;
        }
        piece = {start, position, characterFormat(document, position - 1)};
    } else {
        piece = {block.position(), -1, block.charFormat()};
    }
    AnnotationList list = annotationsOf(piece.format);
    for (Annotation annotation : annotations) {
        annotation.point = true;
        list = withAnnotation(list, annotation);
    }
    setAnnotations(piece.format, list);
    applyPieces(document, {piece});
}

void removeKeepingAnnotations(QTextCursor& cursor) {
    if (!cursor.hasSelection()) {
        return;
    }
    QTextDocument& document = *cursor.document();
    const AnnotationList staying =
        annotationsLeavingWith(document, cursor.selectionStart(), cursor.selectionEnd());
    cursor.beginEditBlock();
    cursor.removeSelectedText();
    anchorToPlace(document, cursor.position(), staying);
    cursor.endEditBlock();
}

AnnotationList removeForReplacement(QTextCursor& cursor) {
    QTextDocument& document = *cursor.document();
    AnnotationList fragments;
    if (cursor.hasSelection()) {
        const int from = cursor.selectionStart();
        const int to = cursor.selectionEnd();
        fragments = enclosingFragments(document, from, to);
        AnnotationList staying = annotationsLeavingWith(document, from, to);
        staying.removeIf([&fragments](const Annotation& a) { return hasFragment(fragments, a.id); });

        cursor.beginEditBlock();
        cursor.removeSelectedText();
        anchorToPlace(document, cursor.position(), staying);
        cursor.endEditBlock();
    }
    for (const Annotation& annotation : annotationsOf(insertionFormat(cursor))) {
        fragments = withAnnotation(fragments, annotation);
    }
    return fragments;
}

void insertKeepingAnnotations(QTextCursor& cursor, const QString& text) {
    if (!cursor.hasSelection()) {
        insertLines(cursor, text, insertionFormat(cursor));
        return;
    }
    if (text.isEmpty()) {
        removeKeepingAnnotations(cursor);
        return;
    }

    cursor.beginEditBlock();
    const AnnotationList fragments = removeForReplacement(cursor);
    QTextCharFormat format = insertionFormat(cursor);
    setAnnotations(format, fragments);
    insertLines(cursor, text, format);
    cursor.endEditBlock();
}

void insertParagraphKeepingAnnotations(QTextCursor& cursor) {
    cursor.beginEditBlock();
    removeKeepingAnnotations(cursor);
    QTextCharFormat format = insertionFormat(cursor);  // the new paragraph's own format
    setAnnotations(format, {});
    cursor.insertBlock(cursor.blockFormat(), format);
    cursor.endEditBlock();
}

void deleteCharacterKeepingAnnotations(QTextCursor& cursor, bool backward) {
    if (cursor.hasSelection()) {
        removeKeepingAnnotations(cursor);
        return;
    }

    // What QTextCursor deletes: a character (both halves of a surrogate pair) before the
    // cursor; the character after it, with its combining marks; or the paragraph break
    QTextDocument& document = *cursor.document();
    const int position = cursor.position();
    int from = position;
    int to = position;
    const QTextBlock block = cursor.block();
    if (backward) {
        if (position == 0) {
            return;
        }
        from = position - 1;
        if (from > block.position() && document.characterAt(from).isLowSurrogate() &&
            document.characterAt(from - 1).isHighSurrogate()) {
            --from;
        }
    } else {
        const int blockEnd = block.position() + block.length() - 1;
        if (position < blockEnd) {
            QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, block.text());
            finder.setPosition(position - block.position());
            const qsizetype next = finder.toNextBoundary();
            to = next < 0 ? blockEnd : block.position() + static_cast<int>(next);
        } else if (block.next().isValid()) {
            to = position + 1;
        } else {
            return;  // the end of the text
        }
    }

    const AnnotationList staying = annotationsLeavingWith(document, from, to);
    if (staying.isEmpty()) {
        if (backward) {
            cursor.deletePreviousChar();
        } else {
            cursor.deleteChar();
        }
        return;
    }
    cursor.beginEditBlock();
    cursor.setPosition(from);
    cursor.setPosition(to, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    anchorToPlace(document, cursor.position(), staying);
    cursor.endEditBlock();
}

QTextCharFormat withRenamedAnnotations(QTextCharFormat format, const QHash<QString, QString>& newIds) {
    AnnotationList annotations = annotationsOf(format);
    bool renamed = false;
    for (Annotation& annotation : annotations) {
        if (const auto it = newIds.constFind(annotation.id); it != newIds.cend()) {
            annotation.id = *it;
            renamed = true;
        }
    }
    if (renamed) {
        setAnnotations(format, annotations);
    }
    return format;
}

void prepareForInsertion(QTextDocument& content, const QTextDocument& document,
                         const AnnotationList& fragments) {
    // Annotations the document already has come in as copies, unless the text goes into
    // them: then it simply joins them
    const QSet<QString> taken = annotationIdsIn(document);
    QHash<QString, QString> newIds;
    for (const QString& id : annotationIdsIn(content)) {
        if (taken.contains(id) && !hasFragment(fragments, id)) {
            newIds.insert(id, newAnnotationId());
        }
    }

    std::vector<FormatPiece> pieces;
    for (QTextBlock block = content.begin(); block.isValid(); block = block.next()) {
        QTextCharFormat paragraph = withRenamedAnnotations(block.charFormat(), newIds);
        AnnotationList points = annotationsOf(paragraph);
        points.removeIf([](const Annotation& a) { return !a.point; });
        setAnnotations(paragraph, points);
        if (paragraph != block.charFormat()) {
            pieces.push_back({block.position(), -1, paragraph});
        }

        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            QTextCharFormat format = withRenamedAnnotations(fragment.charFormat(), newIds);
            AnnotationList annotations = annotationsOf(format);
            for (const Annotation& annotation : fragments) {
                annotations = withAnnotation(annotations, annotation);
            }
            setAnnotations(format, annotations);
            if (format != fragment.charFormat()) {
                pieces.push_back(
                    {fragment.position(), fragment.position() + fragment.length(), format});
            }
        }
    }
    applyPieces(content, pieces);
}

}  // namespace kalahari::editor
