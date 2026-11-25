// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file text_figures.h
/// @brief The images and the tables of a text in text order, each with its caption: what the
///        panel Images and Tables lists

#ifndef KALAHARI_EDITOR_TEXT_FIGURES_H
#define KALAHARI_EDITOR_TEXT_FIGURES_H

#include <kalahari/editor/text_captions.h>
#include <kalahari/editor/text_images.h>

#include <QString>

#include <vector>

class QTextDocument;

namespace kalahari::editor {

/// @brief What a figure of a text is
enum class FigureKind {
    Image,  ///< An image (text_images.h)
    Table,  ///< A table (text_tables.h)
};

/// @brief An image or a table of a text, with its caption
struct FigureInText {
    FigureKind kind = FigureKind::Image;
    /// Its place among the figures of its kind in its text, from 1: the number Edit > Go To
    /// goes to it by (BookEditor::goToPlace())
    int ordinal = 0;
    int position = -1;  ///< Its image's character, or the first character of its table
    bool captioned = false;  ///< It has a caption (a paragraph in the style Caption next to it)
    QString title;    ///< The label and the number of a numbered caption ("Figure 3"); empty:
                      ///< no caption, or one without a number
    QString caption;  ///< The text of its caption after the number and its separator ("The
                      ///< bridge in 1931"), or all of a caption without a number
    QString whole;    ///< The whole caption as the text shows it: "Figure 3. The bridge in 1931"
    QString captionId;      ///< The id of the number of its caption; empty: none
    ImageProperties image;  ///< An image: its properties (its file, name, alternative text)
    int rows = 0;           ///< A table: its rows
    int columns = 0;        ///< and its columns

    bool operator==(const FigureInText&) const = default;
};

/// @brief The images and the tables of a document in text order (a table before the images in
///        its cells), their captions with the numbers @p numbers gives them
std::vector<FigureInText> figuresIn(const QTextDocument& document, const CaptionNumbers& numbers);

/// @brief The images and the tables of a text's KML, as figuresIn() lists them in the document
///        the editor makes of it, numbered in the book around it as @p context says
std::vector<FigureInText> figuresInKml(const QString& kml, const CaptionContext& context);

/// @brief What an image is called where it has no caption: its name, else its alternative
///        text, else the name of its file
QString imageNameOf(const ImageProperties& image);

}  // namespace kalahari::editor

#endif  // KALAHARI_EDITOR_TEXT_FIGURES_H
