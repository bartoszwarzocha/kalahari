// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

#include <kalahari/editor/spread_look.h>

#include <kalahari/editor/detail/book_export_internal.h>
#include <kalahari/editor/render_context.h>

#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <QTextLayout>
#include <QTextOption>

#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

/// @brief The caption's room under the area of the picture, from its bottom (millimetres)
constexpr double CAPTION_GAP_MM = 3.0;

/// @brief The caption over a picture to the edges: its distance from the edges of the paper
constexpr double CAPTION_EDGE_MM = 8.0;

/// @brief Around the caption over the picture: the light frame it is written in
constexpr double CAPTION_PADDING_MM = 1.5;
constexpr int CAPTION_FRAME_ALPHA = 220;

/// @brief The caption and the page numbers: a share of the size of the book's font
constexpr double CAPTION_SCALE = 0.85;
constexpr double NUMBER_SCALE = 0.85;

/// @brief The page number: the middle of its line from the bottom of the paper (millimetres)
constexpr double NUMBER_FROM_BOTTOM_MM = 9.0;

/// @brief The band of the spine and the frame of the area
constexpr int FOLD_ALPHA = 46;
constexpr double LINE_WIDTH_MM = 0.25;
constexpr double DASH = 4.0;
constexpr int FOLD_LINE_ALPHA = 56;

/// @brief A line of text laid out as the text of the pages is: its size does not depend on the
///        painter's device
struct Line {
    QTextLayout layout;
    QTextLine line;
};

void layOut(Line& line, const QString& text, const QFont& font, double width,
            Qt::Alignment align) {
    line.layout.setText(text);
    line.layout.setFont(font);
    QTextOption option(align);
    option.setWrapMode(QTextOption::NoWrap);
    line.layout.setTextOption(option);
    line.layout.beginLayout();
    line.line = line.layout.createLine();
    if (line.line.isValid()) {
        line.line.setLineWidth(width);
    }
    line.layout.endLayout();
}

}  // namespace

QSize turnedSize(const QSize& size, int rotation) {
    const int quarter = ((rotation % 360) + 360) % 360;
    return quarter == 90 || quarter == 270 ? size.transposed() : size;
}

SpreadGeometry spreadGeometry(const SpreadLook& look, const SpreadPage& page,
                              const QSize& pictureSize) {
    SpreadGeometry geometry;
    const double width = page.size.width();
    const double height = page.size.height();
    geometry.area =
        look.toEdges
            ? QRectF(0.0, 0.0, 2.0 * width, height)
            : QRectF(page.outer, page.top, 2.0 * width - 2.0 * page.outer,
                     height - page.top - page.bottom);
    if (pictureSize.isEmpty() || geometry.area.isEmpty()) {
        return geometry;
    }
    const QRectF& area = geometry.area;
    const double ratio = static_cast<double>(pictureSize.width()) / pictureSize.height();
    const bool wider = ratio > area.width() / area.height();
    double drawnWidth = 0.0;
    double drawnHeight = 0.0;
    if (!look.fills) {
        // The whole picture, as large as the area allows
        drawnWidth = wider ? area.width() : area.height() * ratio;
        drawnHeight = wider ? area.width() / ratio : area.height();
    } else {
        // It covers the area, larger by its zoom
        const double zoom = std::clamp(look.zoom, 100, 300) / 100.0;
        drawnHeight = wider ? area.height() * zoom : area.width() * zoom / ratio;
        drawnWidth = wider ? drawnHeight * ratio : area.width() * zoom;
    }
    const double placeX = look.fills ? (1.0 + std::clamp(look.cropX, -1.0, 1.0)) / 2.0 : 0.5;
    const double placeY = look.fills ? (1.0 + std::clamp(look.cropY, -1.0, 1.0)) / 2.0 : 0.5;
    geometry.picture = QRectF(area.left() + (area.width() - drawnWidth) * placeX,
                              area.top() + (area.height() - drawnHeight) * placeY, drawnWidth,
                              drawnHeight);
    return geometry;
}

int spreadDpi(const SpreadGeometry& geometry, const QSize& pictureSize) {
    if (geometry.picture.isEmpty() || pictureSize.isEmpty()) {
        return 0;
    }
    return static_cast<int>(
        std::lround(pictureSize.width() / (geometry.picture.width() / MM_PER_INCH)));
}

QPointF spreadCropMovedBy(const SpreadGeometry& geometry, double cropX, double cropY,
                          const QPointF& deltaMm) {
    // The picture moves as far as it is larger than its area: -1 to 1 over that distance
    const double spareX = geometry.picture.width() - geometry.area.width();
    const double spareY = geometry.picture.height() - geometry.area.height();
    // Dragged right, the picture's left edge goes right: towards -1
    const double x = spareX > 0.0 ? cropX - 2.0 * deltaMm.x() / spareX : cropX;
    const double y = spareY > 0.0 ? cropY - 2.0 * deltaMm.y() / spareY : cropY;
    return QPointF(std::clamp(x, -1.0, 1.0), std::clamp(y, -1.0, 1.0));
}

QString spreadCaptionText(const QString& label, const QString& text,
                          const CaptionContext& context) {
    QString words = text.simplified();
    if (label.trimmed().isEmpty()) {
        return words;
    }
    const QString own = label.trimmed();
    const CaptionNumbering numbering = context.numberingOf(own);
    int number = context.counted.value(own, -1);
    if (number < 0) {
        // Through the book: after the captions of the label before it
        number = static_cast<int>(std::count_if(
            context.before.cbegin(), context.before.cend(),
            [&own](const BookCaption& caption) { return caption.label == own; }));
    }
    const QString numbered =
        own + QLatin1Char(' ') + captionNumberText(number + 1, numbering, context.chapter) +
        QLatin1Char('.');
    return words.isEmpty() ? numbered : numbered + QLatin1Char(' ') + words;
}

QString spreadKml(const SpreadLook& look, const QString& alt, const QString& label,
                  const QString& text, const QString& captionId) {
    QString kml = QStringLiteral("<kml>");
    if (!look.picture.isEmpty()) {
        kml += QStringLiteral("<p align=\"center\"><img src=\"%1\" alt=\"%2\" width=\"100%\"/></p>")
                   .arg(exporting::xmlEscaped(look.picture), exporting::xmlEscaped(alt));
    }
    const QString own = label.trimmed();
    const QString words = text.simplified();
    if (!own.isEmpty()) {
        kml += QStringLiteral("<p style=\"caption\">%1 <caption-number label=\"%1\" id=\"%2\"/>.")
                   .arg(exporting::xmlEscaped(own), exporting::xmlEscaped(captionId));
        if (!words.isEmpty()) {
            kml += QLatin1Char(' ') + exporting::xmlEscaped(words);
        }
        kml += QStringLiteral("</p>");
    } else if (!words.isEmpty()) {
        kml += QStringLiteral("<p style=\"caption\">") + exporting::xmlEscaped(words) +
               QStringLiteral("</p>");
    }
    return kml + QStringLiteral("</kml>");
}

void paintSpreadPage(QPainter* painter, const SpreadLook& look, const SpreadPage& page,
                     const QImage& picture, bool right, const SpreadPaint& paint) {
    if (painter == nullptr || page.size.isEmpty()) {
        return;
    }
    const double perMm = paint.dpi / MM_PER_INCH;
    const double width = page.size.width();
    const double height = page.size.height();
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(QRectF(0.0, 0.0, width * perMm, height * perMm), paint.paper);

    // From here on in millimetres of the whole spread, the page's own part of it shown
    painter->save();
    painter->scale(perMm, perMm);
    const double offset = right ? width : 0.0;
    painter->translate(-offset, 0.0);
    const QRectF ownPage(offset, 0.0, width, height);
    const SpreadGeometry geometry =
        spreadGeometry(look, page, picture.isNull() ? QSize() : picture.size());
    if (!picture.isNull() && !geometry.picture.isEmpty()) {
        painter->save();
        painter->setClipRect(geometry.area.intersected(ownPage));
        // Each half moves away from the fold by half of the gutter
        const double moved = (right ? 1.0 : -1.0) * look.gutter / 2.0;
        painter->drawImage(geometry.picture.translated(moved, 0.0), picture);
        painter->restore();
    }

    // The hints of the card: the band of the spine, the frame of the area, the gutter
    if (paint.fold.isValid()) {
        QColor band = paint.fold;
        band.setAlpha(FOLD_ALPHA);
        painter->fillRect(QRectF(width - SPREAD_FOLD_MM, 0.0, 2.0 * SPREAD_FOLD_MM, height)
                              .intersected(ownPage),
                          band);
        QPen pen(paint.fold, LINE_WIDTH_MM, Qt::DashLine);
        pen.setDashPattern({DASH, DASH});
        painter->setPen(pen);
        painter->drawLine(QPointF(width, 0.0), QPointF(width, height));
    } else if (paint.line) {
        QColor line = paint.text;
        line.setAlpha(FOLD_LINE_ALPHA);
        painter->setPen(QPen(line, LINE_WIDTH_MM));
        painter->drawLine(QPointF(width, 0.0), QPointF(width, height));
    }
    if (paint.guide.isValid()) {
        QPen pen(paint.guide, LINE_WIDTH_MM, Qt::DashLine);
        pen.setDashPattern({DASH, DASH * 0.7});
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        if (!look.toEdges) {
            painter->drawRect(geometry.area);
        }
        if (look.gutter > 0.0 && !geometry.picture.isEmpty()) {
            pen.setDashPattern({DASH / 3.0, DASH / 2.0});
            painter->setPen(pen);
            for (const double x : {width - look.gutter / 2.0, width + look.gutter / 2.0}) {
                painter->drawLine(QPointF(x, 0.0), QPointF(x, height));
            }
        }
    }
    painter->restore();

    // The caption under the picture on the right page, and the number of the page; laid out
    // in the painter's own units, as the text of the pages is
    QFont font = paint.font;
    if (font.pointSizeF() <= 0.0) {
        font.setPointSizeF(10.0);
    }
    if (right && !look.caption.trimmed().isEmpty()) {
        QFont italic = font;
        italic.setItalic(true);
        italic.setPointSizeF(font.pointSizeF() * CAPTION_SCALE);
        const double room =
            (look.toEdges ? width - 2.0 * CAPTION_EDGE_MM : width - page.outer -
                                                                 CAPTION_EDGE_MM) * perMm;
        const QString text = QFontMetricsF(italic).elidedText(look.caption.simplified(),
                                                              Qt::ElideRight, room);
        Line line;
        layOut(line, text, italic, room, Qt::AlignRight);
        if (line.line.isValid()) {
            const double lineHeight = line.line.height();
            const double rightEdge =
                (look.toEdges ? width - CAPTION_EDGE_MM : width - page.outer) * perMm;
            double top = 0.0;
            if (look.toEdges) {
                // In a light frame over the picture, near the bottom right corner
                top = (height - CAPTION_EDGE_MM) * perMm - lineHeight;
                const double used = line.line.naturalTextWidth();
                const double padding = CAPTION_PADDING_MM * perMm;
                QColor frame = paint.paper;
                frame.setAlpha(CAPTION_FRAME_ALPHA);
                painter->fillRect(QRectF(rightEdge - used - padding, top - padding / 2.0,
                                         used + 2.0 * padding, lineHeight + padding),
                                  frame);
            } else {
                top = (height - page.bottom + CAPTION_GAP_MM) * perMm;
            }
            painter->setPen(paint.text);
            line.layout.draw(painter, QPointF(rightEdge - room, top));
        }
    }
    if (paint.number > 0) {
        QFont small = font;
        small.setPointSizeF(font.pointSizeF() * NUMBER_SCALE);
        Line line;
        layOut(line, QString::number(paint.number), small, width * perMm, Qt::AlignHCenter);
        if (line.line.isValid()) {
            painter->setPen(paint.text);
            line.layout.draw(painter, QPointF(0.0, (height - NUMBER_FROM_BOTTOM_MM) * perMm -
                                                       line.line.height() / 2.0));
        }
    }
    painter->restore();
}

}  // namespace kalahari::editor
