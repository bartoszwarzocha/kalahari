/// @file version_comparison.cpp
/// @brief A version of a text compared with the text as it is now, as a text to read

#include <kalahari/editor/version_comparison.h>
#include <kalahari/core/text_diff.h>
#include <kalahari/editor/annotation.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/kml_serializer.h>
#include "book_editor_internal.h"

#include <QStringList>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>

#include <algorithm>

namespace kalahari::editor {

namespace {

using core::DiffPiece;
using core::DiffParagraph;

/// @brief Fill @p document with text @p kml as an editor reads it, so that its positions are
/// those of the editor's document (BookEditor::fromKml())
void readKml(QTextDocument& document, const QString& kml) {
    document.setUndoRedoEnabled(false);
    KmlDocumentModel model;
    model.loadKml(kml);  // unreadable KML: the paragraphs read before the error
    QTextCursor cursor(&document);
    cursor.beginEditBlock();
    appendParagraphs(cursor, model);
    cursor.endEditBlock();
}

/// @brief The texts of the paragraphs of @p document
QStringList paragraphsOf(const QTextDocument& document) {
    QStringList paragraphs;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        paragraphs << block.text();
    }
    return paragraphs;
}

/// @brief @p format without annotations, with @p mark over it
QTextCharFormat shownFormat(QTextCharFormat format, const QTextCharFormat& mark) {
    setAnnotations(format, {});
    format.merge(mark);
    return format;
}

/// @brief Insert @p text at the cursor: the characters of @p block from @p offset with their
/// formats, @p mark over them; the text without formats when the block does not have it there
void appendPiece(QTextCursor& cursor, const QTextBlock& block, int offset, const QString& text,
                 const QTextCharFormat& mark) {
    if (!block.isValid() || QStringView(block.text()).mid(offset, text.size()) != text) {
        cursor.insertText(text, mark);
        return;
    }
    const int from = block.position() + offset;
    const int to = from + static_cast<int>(text.size());
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        const int start = std::max(fragment.position(), from);
        const int end = std::min(fragment.position() + fragment.length(), to);
        if (start < end) {
            cursor.insertText(fragment.text().mid(start - fragment.position(), end - start),
                              shownFormat(fragment.charFormat(), mark));
        }
    }
}

}  // namespace

ComparisonRange VersionComparison::olderRange(int from, int to) const {
    if (to < from) {
        std::swap(from, to);
    }
    // The first character of the version at or after from, and the one after the last before to
    int start = -1;
    int end = -1;
    for (const OlderPiece& piece : olderPieces) {
        if (start < 0 && piece.end > from) {
            start = piece.older + std::max(0, from - piece.start);
        }
        if (piece.start < to) {
            end = piece.older + std::min(to, piece.end) - piece.start;
        }
    }
    if (start < 0 || end < 0) {
        return {};
    }
    // The end of the last paragraph is the end of the document
    end = std::min(end, olderLength);
    return end > start ? ComparisonRange{start, end} : ComparisonRange{};
}

VersionComparison compareVersions(const QString& olderKml, const QString& newerKml,
                                  const ComparisonColors& colors) {
    QTextDocument older;
    readKml(older, olderKml);
    QTextDocument newer;
    readKml(newer, newerKml);
    const core::TextDiff diff = core::diffTexts(paragraphsOf(older), paragraphsOf(newer));

    QTextCharFormat deletedMark;
    deletedMark.setFontStrikeOut(true);
    deletedMark.setForeground(colors.deleted);
    QTextCharFormat addedMark;
    addedMark.setFontUnderline(true);
    addedMark.setForeground(colors.added);

    VersionComparison comparison;
    comparison.deletedWords = diff.deletedWords;
    comparison.addedWords = diff.addedWords;
    comparison.olderLength = older.characterCount() - 1;

    QTextDocument shown;
    shown.setUndoRedoEnabled(false);
    QTextCursor cursor(&shown);
    cursor.beginEditBlock();

    // The changes as core::diffTexts() counts them: deleted and added pieces next to each
    // other, going on in the next paragraph when both are deleted or added whole
    bool inChange = false;
    bool lastWhole = false;
    bool first = true;
    for (const DiffParagraph& paragraph : diff.paragraphs) {
        const QTextBlock olderBlock =
            paragraph.older >= 0 ? older.findBlockByNumber(paragraph.older) : QTextBlock();
        const QTextBlock newerBlock =
            paragraph.newer >= 0 ? newer.findBlockByNumber(paragraph.newer) : QTextBlock();
        const QTextBlock& look = newerBlock.isValid() ? newerBlock : olderBlock;
        const QTextBlockFormat blockFormat = look.blockFormat();
        const QTextCharFormat blockCharFormat = shownFormat(look.charFormat(), {});
        if (first) {
            cursor.setBlockFormat(blockFormat);
            cursor.setBlockCharFormat(blockCharFormat);
            first = false;
        } else {
            cursor.insertBlock(blockFormat, blockCharFormat);
        }

        const bool whole = paragraph.older < 0 || paragraph.newer < 0;
        if (!(whole && lastWhole)) {
            inChange = false;
        }
        lastWhole = whole;
        if (paragraph.pieces.isEmpty()) {  // an empty paragraph
            if (whole && !inChange) {
                comparison.changes.append({cursor.position(), cursor.position()});
            }
            inChange = whole;
        }

        int olderOffset = 0;
        int newerOffset = 0;
        for (const DiffPiece& piece : paragraph.pieces) {
            const int start = cursor.position();
            const int length = static_cast<int>(piece.text.size());
            switch (piece.kind) {
            case DiffPiece::Kind::Same:
                appendPiece(cursor, newerBlock, newerOffset, piece.text, {});
                comparison.olderPieces.append(
                    {start, cursor.position(), olderBlock.position() + olderOffset});
                olderOffset += length;
                newerOffset += length;
                inChange = false;
                continue;
            case DiffPiece::Kind::Deleted:
                appendPiece(cursor, olderBlock, olderOffset, piece.text, deletedMark);
                comparison.olderPieces.append(
                    {start, cursor.position(), olderBlock.position() + olderOffset});
                olderOffset += length;
                break;
            case DiffPiece::Kind::Added:
                appendPiece(cursor, newerBlock, newerOffset, piece.text, addedMark);
                newerOffset += length;
                break;
            }
            if (inChange && !comparison.changes.isEmpty()) {
                comparison.changes.last().end = cursor.position();
            } else {
                comparison.changes.append({start, cursor.position()});
                inChange = true;
            }
        }
        // A whole paragraph in a change: the change goes on in the next one when it is whole
        if (whole && inChange && !comparison.changes.isEmpty()) {
            comparison.changes.last().end = cursor.position();
        }
        // The end of a paragraph of the version is its end in the comparison too
        if (olderBlock.isValid()) {
            comparison.olderPieces.append({cursor.position(), cursor.position() + 1,
                                           olderBlock.position() + olderBlock.length() - 1});
        }
    }
    cursor.endEditBlock();

    comparison.kml = KmlSerializer().toKml(&shown);
    return comparison;
}

QString kmlFragment(const QString& kml, int from, int to) {
    QTextDocument document;
    readKml(document, kml);
    const int last = document.characterCount() - 1;
    from = std::clamp(from, 0, last);
    to = std::clamp(to, 0, last);
    if (to <= from) {
        return QString();
    }
    return KmlSerializer().toKml(&document, from, to);
}

}  // namespace kalahari::editor
