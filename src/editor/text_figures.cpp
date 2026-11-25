// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file text_figures.cpp
/// @brief The images and the tables of a text in text order, each with its caption

#include <kalahari/editor/text_figures.h>

#include "kalahari/editor/detail/book_editor_internal.h"

#include <QFileInfo>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextTable>

#include <algorithm>
#include <optional>

namespace kalahari::editor {

namespace {

/// The characters between a caption's number and its text: ". ", ": ", " – "
bool isSeparatorAfterNumber(QChar character)
{
    return character.isSpace() || character == u'.' || character == u':' ||
           character == u'-' || character == QChar(0x2013) || character == QChar(0x2014);
}

/// The text after a caption's number, without the separator that starts it
QString withoutSeparator(const QString& text)
{
    qsizetype start = 0;
    while (start < text.size() && isSeparatorAfterNumber(text.at(start))) {
        ++start;
    }
    return text.mid(start);
}

/// The caption of a figure from the paragraph of its caption (an invalid block: none)
void readCaption(FigureInText& figure, const QTextBlock& block, const CaptionNumbers& numbers)
{
    if (!block.isValid()) {
        return;
    }
    figure.captioned = true;
    const CaptionParts parts = captionParts(block);
    BookCaption caption;
    if (parts.numberAt >= 0) {
        figure.captionId = parts.number.id;
        if (const BookCaption* counted = numbers.find(parts.number.id)) {
            caption = *counted;
        } else {
            // Not counted yet: the label without a number
            caption.id = parts.number.id;
            caption.label = parts.number.label;
            caption.pieces = captionPieces(block);
        }
    } else {
        caption.pieces = captionPieces(block);
    }
    int numberStart = -1;
    int numberLength = 0;
    const QString whole = numbers.captionText(caption, &numberStart, &numberLength);
    figure.whole = whole.simplified();
    if (numberStart >= 0) {
        figure.title = whole.left(numberStart + numberLength).simplified();
        figure.caption = withoutSeparator(whole.mid(numberStart + numberLength)).simplified();
    } else {
        figure.caption = figure.whole;
    }
}

/// The tables of a frame and of the frames in it, ordered by their first characters (as Edit >
/// Go To counts them)
std::vector<const QTextTable*> tablesIn(const QTextFrame& root)
{
    std::vector<const QTextTable*> tables;
    std::vector<const QTextFrame*> frames{&root};
    while (!frames.empty()) {
        const QTextFrame* const frame = frames.back();
        frames.pop_back();
        for (const QTextFrame* child : frame->childFrames()) {
            if (const auto* table = qobject_cast<const QTextTable*>(child)) {
                tables.push_back(table);
            }
            frames.push_back(child);
        }
    }
    std::ranges::sort(tables, {}, [](const QTextTable* table) {
        return table->cellAt(0, 0).firstPosition();
    });
    return tables;
}

}  // namespace

std::vector<FigureInText> figuresIn(const QTextDocument& document, const CaptionNumbers& numbers)
{
    std::vector<FigureInText> figures;
    // The tables first: a table goes before the images of its cells
    int ordinal = 0;
    for (const QTextTable* table : tablesIn(*document.rootFrame())) {
        FigureInText figure;
        figure.kind = FigureKind::Table;
        figure.ordinal = ++ordinal;
        figure.position = table->cellAt(0, 0).firstPosition();
        figure.rows = table->rows();
        figure.columns = table->columns();
        readCaption(figure, captionBlockOfTable(*table), numbers);
        figures.push_back(std::move(figure));
    }
    ordinal = 0;
    for (const int position : imagePositions(document, 0, document.characterCount())) {
        FigureInText figure;
        figure.kind = FigureKind::Image;
        figure.ordinal = ++ordinal;
        figure.position = position;
        if (const std::optional<ImageProperties> image = imageAt(document, position)) {
            figure.image = *image;
        }
        readCaption(figure, captionBlockOf(document.findBlock(position)), numbers);
        figures.push_back(std::move(figure));
    }
    std::ranges::stable_sort(figures, {}, &FigureInText::position);
    return figures;
}

std::vector<FigureInText> figuresInKml(const QString& kml, const CaptionContext& context)
{
    if (!kml.contains(QLatin1String("<img")) && !kml.contains(QLatin1String("<table"))) {
        return {};
    }
    QTextDocument document;
    readKmlInto(document, kml);
    const CaptionNumbers numbers(document, context);
    return figuresIn(document, numbers);
}

QString imageNameOf(const ImageProperties& image)
{
    if (!image.name.trimmed().isEmpty()) {
        return image.name.simplified();
    }
    if (!image.alt.trimmed().isEmpty()) {
        return image.alt.simplified();
    }
    return QFileInfo(image.source).fileName();
}

}  // namespace kalahari::editor
