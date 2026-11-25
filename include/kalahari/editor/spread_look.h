// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file spread_look.h
/// @brief How a spread lies on its two pages: what the pages of a PDF, the print and the card
///        of a spread draw (core::SpreadData with the file of its picture and the words of its
///        caption)
///
/// The geometry is in millimetres from the top left corner of the left page: the left page
/// takes 0 to the width of a page, the right one the next width. The picture lies within the
/// top, bottom and outer margins of the pages, across the spine, or over both pages to the
/// edges of paper. With room at the spine (gutter) each half of the picture moves away from
/// the fold by half of it, so that the strip at the spine shows on both pages.

#pragma once

#include <kalahari/editor/text_captions.h>

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>

class QPainter;

namespace kalahari::editor {

/// @brief A spread as its pages show it
struct SpreadLook {
    QString picture;        ///< The source of its picture among the images of the book
                            ///< (ImageStore, BookExport::pictures); empty: none
    int rotation = 0;       ///< The turn of its picture, clockwise: 0, 90, 180 or 270 degrees
    bool toEdges = false;   ///< Over both pages to the edges of paper; false: in the margins
    bool fills = false;     ///< It fills its area, its edges cut off; false: the whole picture
    double cropX = 0.0;     ///< Of a picture that fills its area: where it lies, -1 to 1
    double cropY = 0.0;
    int zoom = 100;         ///< Of a picture that fills its area, in percent (100-300)
    double gutter = 0.0;    ///< The room at the spine, in millimetres
    QString caption;        ///< The whole caption ("Map 1. The route..."); empty: none
    bool pageNumbers = false;  ///< The numbers of its pages and the running head show

    bool operator==(const SpreadLook&) const = default;
};

/// @brief The page of the book, in millimetres
struct SpreadPage {
    QSizeF size{148.0, 210.0};  ///< Of one page
    double top = 20.0;          ///< The margins the picture keeps within (SpreadLook::toEdges
    double bottom = 22.0;       ///< false); the inner margin plays no part: the picture goes
    double outer = 18.0;        ///< across the spine

    bool operator==(const SpreadPage&) const = default;
};

/// @brief Where the picture of a spread lies, in millimetres from the top left of the left page
struct SpreadGeometry {
    QRectF area;     ///< The area of the picture
    QRectF picture;  ///< The whole picture as drawn, before the gutter moves its halves; empty:
                     ///< no picture (none, or its file cannot be read)
};

/// @brief The size of a picture turned by @p rotation degrees (90 and 270 swap its sides)
QSize turnedSize(const QSize& size, int rotation);

/// @brief Where the picture of @p look lies on @p page
/// @param pictureSize Of its file as turned (turnedSize()); empty: no picture
SpreadGeometry spreadGeometry(const SpreadLook& look, const SpreadPage& page,
                              const QSize& pictureSize);

/// @brief The dots per inch of the picture across as @p geometry draws it; 0 without a picture
int spreadDpi(const SpreadGeometry& geometry, const QSize& pictureSize);

/// @brief Where a picture that fills its area lies after it is dragged by @p deltaMm
///        (millimetres), as SpreadLook::cropX and cropY
/// @return The new cropX and cropY, each -1 to 1; as they were when the picture cannot move
///         that way (it is as wide or as high as its area)
QPointF spreadCropMovedBy(const SpreadGeometry& geometry, double cropX, double cropY,
                          const QPointF& deltaMm);

/// @brief The caption of a spread: its text alone, or its label and number before it
///        ("Map 3. The route of the caravan")
/// @param label The label it counts; empty: the text alone
/// @param context The numbering of the book around the spread (its captions before it)
QString spreadCaptionText(const QString& label, const QString& text,
                          const CaptionContext& context);

/// @brief The KML of a spread for a file that shows a page at a time (Word, an e-book,
///        Markdown): the whole picture in a paragraph of its own and its caption under it
/// @param captionId The id of the caption's number (the book counts it with the captions of
///        its texts)
QString spreadKml(const SpreadLook& look, const QString& alt, const QString& label,
                  const QString& text, const QString& captionId);

/// @brief How a page of a spread is drawn
struct SpreadPaint {
    double dpi = 96.0;     ///< The painter's units: pixels at so many dots per inch
    QColor paper = Qt::white;
    QColor text = Qt::black;
    QFont font;            ///< Of the caption (in italics) and of the page numbers
    QColor fold;           ///< The band of the spine (the card only); invalid: none
    QColor guide;          ///< The frame of the area and the gutter (the card only); invalid:
                           ///< none
    bool line = false;     ///< A faint line at the fold, where the pages meet (the card)
    int number = 0;        ///< The number of the page to draw under it; 0: none
};

/// @brief Draw a page of a spread
/// @param painter Its origin at the top left corner of the page, its units pixels at
///        SpreadPaint::dpi
/// @param picture The picture as turned; null: none (the page stays empty)
/// @param right The right page; false: the left one
void paintSpreadPage(QPainter* painter, const SpreadLook& look, const SpreadPage& page,
                     const QImage& picture, bool right, const SpreadPaint& paint);

/// @brief The width of the band of the spine on each side of the fold, in millimetres: the
///        part of the pages that hides in the binding (a hint of the card)
inline constexpr double SPREAD_FOLD_MM = 8.0;

}  // namespace kalahari::editor
