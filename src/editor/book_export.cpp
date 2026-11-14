// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_export.cpp
/// @brief What the writers of DOCX and Markdown share: the texts read as the editor reads
///        them, the pieces of their paragraphs, the kinds of the styles and the pictures

#include "book_export_internal.h"

#include "book_editor_internal.h"

#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/text_link.h>
#include <kalahari/editor/text_lists.h>
#include <kalahari/editor/text_tables.h>

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QPen>
#include <QSaveFile>
#include <QTextCursor>
#include <QTextFragment>

#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

constexpr double POINTS_PER_MILLIMETRE = 72.0 / 25.4;
constexpr double POINTS_PER_CENTIMETRE = 72.0 / 2.54;

/// The room of an image's shadow, its opacity and the lines of a double border, as the editor
/// draws them (KalahariTextDocumentLayout::drawImageFrame())
constexpr double SHADOW_POINTS = 3.0;
constexpr double SHADOW_OPACITY = 0.35;
constexpr double DOUBLE_BORDER_LINES = 3.0;

/// The pixels a picture is drawn in: a drawing at 300 an inch, a photo at most so (no more
/// than its file has), a framed one at least at 144 so its frame is smooth, and at most so many
/// pixels on its longer side
constexpr double DRAWING_PIXELS_PER_POINT = 300.0 / 72.0;
constexpr double FRAME_PIXELS_PER_POINT = 2.0;
constexpr int MAX_PICTURE_PIXELS = 6000;

/// The JPEG quality of a photo drawn again (cropped, turned, recolored)
constexpr int PHOTO_QUALITY = 92;

}  // namespace

double ExportPage::textWidthPoints() const {
    const double margins = mirrored ? inner + outer : left + right;
    return std::max(36.0, (width - margins) * POINTS_PER_MILLIMETRE);
}

double ExportPage::textHeightPoints() const {
    return std::max(36.0, (height - top - bottom) * POINTS_PER_MILLIMETRE);
}

QString markdownImagesFolderName(const QString& baseName, const QString& imagesWord) {
    return baseName + QStringLiteral(" – ") + imagesWord;
}

namespace exporting {

// =============================================================================
// A text of the export
// =============================================================================

ExportText::ExportText(const ExportItem& item) : m_document(std::make_unique<QTextDocument>()) {
    m_document->setUndoRedoEnabled(false);
    if (item.kind == ExportItemKind::Text && !item.kml.isEmpty()) {
        KmlDocumentModel model;
        model.loadKml(item.kml);  // unreadable KML: the paragraphs read before the error
        QTextCursor cursor(m_document.get());
        cursor.beginEditBlock();
        appendParagraphs(cursor, model);
        cursor.endEditBlock();
    }
    m_captions = CaptionNumbers(*m_document, item.captions);
    m_notes = FootnoteNumbers(*m_document, item.footnotes);
    m_listNumbers = listNumbers(*m_document);
    m_annotations = annotationsIn(*m_document);
}

int ExportText::listNumber(const QTextBlock& block) const {
    const auto index = static_cast<size_t>(block.blockNumber());
    return index < m_listNumbers.size() ? m_listNumbers[index] : -1;
}

// =============================================================================
// The pieces of a paragraph
// =============================================================================

std::vector<Piece> piecesOf(const QTextBlock& block, const ExportText& text) {
    std::vector<Piece> pieces;
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        const QString characters = fragment.text();
        const QTextCharFormat format = fragment.charFormat();
        const QString address = linkAddressOf(format);
        const QString link = isOpenableLinkAddress(address) ? address : QString();
        qsizetype i = 0;
        while (i < characters.size()) {
            const int position = fragment.position() + static_cast<int>(i);
            if (characters.at(i) == QChar::ObjectReplacementCharacter) {
                Piece piece;
                piece.position = position;
                piece.length = 1;
                piece.format = format;
                piece.link = link;
                if (const std::optional<ImageProperties> image = imageOf(format)) {
                    piece.kind = Piece::Kind::Image;
                    piece.image = *image;
                } else if (const std::optional<CaptionNumberField> number = captionNumberOf(format)) {
                    piece.kind = Piece::Kind::Field;
                    piece.captionNumber = number;
                    piece.text = text.captions().fieldText(position, format).text;
                } else if (const std::optional<CrossReference> reference = crossReferenceOf(format)) {
                    piece.kind = Piece::Kind::Field;
                    piece.reference = reference;
                    piece.text = text.captions().fieldText(position, format).text;
                } else if (const std::optional<Footnote> note = footnoteOf(format)) {
                    piece.kind = Piece::Kind::Note;
                    piece.note = *note;
                    piece.text = text.notes().numberAt(position);
                } else {
                    ++i;  // the object of a block (a line, a break), or one the program does not know
                    continue;
                }
                pieces.push_back(std::move(piece));
                ++i;
                continue;
            }
            qsizetype end = i;
            while (end < characters.size() && characters.at(end) != QChar::ObjectReplacementCharacter) {
                ++end;
            }
            const QString part = characters.mid(i, end - i);
            Piece* last = pieces.empty() ? nullptr : &pieces.back();
            if (last && last->kind == Piece::Kind::Text && last->format == format &&
                last->position + last->length == position) {
                last->text += part;
                last->length += static_cast<int>(part.size());
            } else {
                Piece piece;
                piece.text = part;
                piece.format = format;
                piece.position = position;
                piece.length = static_cast<int>(part.size());
                piece.link = link;
                pieces.push_back(std::move(piece));
            }
            i = end;
        }
    }
    return pieces;
}

QString plainTextOf(const QTextBlock& block, const ExportText& text) {
    QString result;
    for (const Piece& piece : piecesOf(block, text)) {
        if (piece.kind == Piece::Kind::Text) {
            result += QString(piece.text).replace(QChar::LineSeparator, QLatin1Char(' '));
        } else if (piece.kind == Piece::Kind::Field) {
            result += piece.text;
        }
    }
    return result.simplified();
}

// =============================================================================
// Styles
// =============================================================================

int headingLevelOfStyle(const ParagraphStyleSet& styles, const QString& id) {
    QString current = id;
    for (int depth = 0; depth < 64 && !current.isEmpty(); ++depth) {
        if (const int level = headingLevelOf(current); level > 0) {
            return level;
        }
        const ParagraphStyle* style = styles.find(current);
        if (!style) {
            return 0;
        }
        current = style->baseStyle;
    }
    return 0;
}

bool isStyleBasedOn(const ParagraphStyleSet& styles, const QString& id, const QString& ancestor) {
    QString current = id;
    for (int depth = 0; depth < 64 && !current.isEmpty(); ++depth) {
        if (current == ancestor) {
            return true;
        }
        const ParagraphStyle* style = styles.find(current);
        if (!style) {
            return false;
        }
        current = style->baseStyle;
    }
    return false;
}

QString shownStyleOf(const ParagraphStyleSet& styles, const QTextBlock& block) {
    const QString id = paragraphStyleOf(block.blockFormat());
    return styles.contains(id) ? id : QString::fromLatin1(NORMAL_STYLE_ID);
}

bool startsWithHeading(const QTextDocument& document, const ParagraphStyleSet& styles) {
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (tableOf(block) != nullptr || blockObject(block) != TextObject::None) {
            return false;
        }
        if (block.text().trimmed().isEmpty()) {
            continue;
        }
        return headingLevelOfStyle(styles, shownStyleOf(styles, block)) > 0;
    }
    return false;
}

QString titleStyleOf(const ExportItem& item, const ParagraphStyleSet& styles) {
    if (!item.titleStyle.isEmpty() && styles.contains(item.titleStyle)) {
        return item.titleStyle;
    }
    return headingStyleId(1);
}

double tableWidthPoints(const TableShape& shape, double lineWidth) {
    if (shape.properties.width && *shape.properties.width > 0.0) {
        return std::min(*shape.properties.width, lineWidth);
    }
    return lineWidth;
}

std::vector<double> columnWidthsPoints(const TableShape& shape, double tableWidth) {
    std::vector<double> widths;
    const int columns = std::max(1, shape.columns);
    double total = 0.0;
    if (static_cast<int>(shape.columnWidths.size()) == columns) {
        for (const double percent : shape.columnWidths) {
            total += std::max(0.0, percent);
        }
    }
    for (int column = 0; column < columns; ++column) {
        const double share = total > 0.0
                                 ? std::max(0.0, shape.columnWidths[static_cast<size_t>(column)]) / total
                                 : 1.0 / columns;
        widths.push_back(tableWidth * share);
    }
    return widths;
}

// =============================================================================
// Pictures
// =============================================================================

ExportPicture exportPicture(const ImageStore& store, const ImageProperties& properties,
                            double lineWidth, double pageHeight) {
    ExportPicture result;
    const ImageProperties image = normalizedImage(properties);
    const QString path = store.filePath(image.source);
    const std::optional<QSize> natural = store.shownPixelSize(image);
    if (path.isEmpty() || !natural || natural->isEmpty()) {
        return result;
    }

    // The size of the picture and of its frame, in points, as the editor gives them
    // (KalahariTextDocumentLayout::imageSize())
    const double line = image.borderLine == ImageBorderLine::None ? 0.0
                        : image.borderLine == ImageBorderLine::Double
                            ? image.borderWidth * DOUBLE_BORDER_LINES
                            : image.borderWidth;
    const double side = image.hasFrame() ? line + image.borderPadding : 0.0;
    const double shadow = image.shadow ? SHADOW_POINTS : 0.0;
    const double frameWidth = 2.0 * side + shadow;
    const double frameHeight = 2.0 * side + shadow;
    double width = 0.0;
    switch (image.unit) {
        case ImageWidthUnit::Natural:
            width = natural->width() * 72.0 / IMAGE_PIXELS_PER_INCH;
            break;
        case ImageWidthUnit::Percent:
            width = lineWidth * image.width / 100.0;
            break;
        case ImageWidthUnit::Centimeters:
            width = image.width * POINTS_PER_CENTIMETRE;
            break;
    }
    width = std::max(1.0, width);
    double height = image.height > 0.0 ? image.height * POINTS_PER_CENTIMETRE
                                       : width * natural->height() / natural->width();
    const double widest = std::max(1.0, lineWidth - frameWidth);
    if (width > widest) {
        height *= widest / width;
        width = widest;
    }
    if (pageHeight > 0.0) {
        const double tallest = pageHeight - frameHeight;
        if (tallest >= 1.0 && height > tallest) {
            width *= tallest / height;
            height = tallest;
        }
    }
    height = std::max(1.0, height);
    result.points = QSizeF(width + frameWidth, height + frameHeight);

    // A file every program reads, shown as it is: its own bytes
    QImageReader reader(path);
    const QByteArray format = reader.format().toLower();
    const bool drawing = QFileInfo(path).suffix().compare(QLatin1String("svg"), Qt::CaseInsensitive) == 0 ||
                         format == "svg" || format == "svgz";
    const bool common = format == "png" || format == "jpeg" || format == "jpg" || format == "gif" ||
                        format == "bmp";
    if (image.hasPlainPicture() && !image.hasFrame() && !drawing && common &&
        reader.transformation() == QImageIOHandler::TransformationNone) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            result.data = file.readAll();
            result.extension = format == "jpg" ? QStringLiteral("jpeg") : QString::fromLatin1(format);
            result.contentType = QStringLiteral("image/") + result.extension;
            result.missing = result.data.isEmpty();
            return result;
        }
        return result;
    }

    // Drawn as the editor shows it: cropped, turned, recolored, in its frame
    double perPoint = natural->width() / width;
    if (drawing) {
        perPoint = DRAWING_PIXELS_PER_POINT;
    } else {
        perPoint = std::min(perPoint, DRAWING_PIXELS_PER_POINT);
        if (image.hasFrame()) {
            perPoint = std::max(perPoint, FRAME_PIXELS_PER_POINT);
        }
    }
    const double longest = std::max(result.points.width(), result.points.height()) * perPoint;
    if (longest > MAX_PICTURE_PIXELS) {
        perPoint *= MAX_PICTURE_PIXELS / longest;
    }
    const QSize picturePixels(std::max(1, static_cast<int>(std::lround(width * perPoint))),
                              std::max(1, static_cast<int>(std::lround(height * perPoint))));
    const QImage picture = store.shownImage(image, picturePixels);
    if (picture.isNull()) {
        return result;
    }
    QImage drawn = picture;
    if (image.hasFrame()) {
        const QSize boxPixels(std::max(1, static_cast<int>(std::lround(result.points.width() * perPoint))),
                              std::max(1, static_cast<int>(std::lround(result.points.height() * perPoint))));
        drawn = QImage(boxPixels, QImage::Format_ARGB32_Premultiplied);
        drawn.fill(Qt::transparent);
        QPainter painter(&drawn);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const double shadowPixels = shadow * perPoint;
        const QRectF outer(0.0, 0.0, boxPixels.width() - shadowPixels, boxPixels.height() - shadowPixels);
        // The color of the text where the image has none of its own: black on paper
        const QColor color = image.borderColor.isValid() ? image.borderColor : QColor(Qt::black);
        if (image.shadow) {
            QColor shade = color;
            shade.setAlphaF(shade.alphaF() * SHADOW_OPACITY);
            painter.fillRect(outer.translated(shadowPixels, shadowPixels), shade);
            painter.fillRect(outer, Qt::white);  // the paper under the picture
        }
        if (image.borderLine != ImageBorderLine::None) {
            const double lineWidthPixels = std::max(1.0, image.borderWidth * perPoint);
            Qt::PenStyle style = Qt::SolidLine;
            if (image.borderLine == ImageBorderLine::Dashed) {
                style = Qt::DashLine;
            } else if (image.borderLine == ImageBorderLine::Dotted) {
                style = Qt::DotLine;
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(color, lineWidthPixels, style, Qt::FlatCap, Qt::MiterJoin));
            const double half = lineWidthPixels / 2.0;
            painter.drawRect(outer.adjusted(half, half, -half, -half));
            if (image.borderLine == ImageBorderLine::Double) {
                const double inner = lineWidthPixels * (DOUBLE_BORDER_LINES - 1.0);
                painter.drawRect(outer.adjusted(inner + half, inner + half, -inner - half, -inner - half));
            }
        }
        const double sidePixels = side * perPoint;
        painter.drawImage(outer.adjusted(sidePixels, sidePixels, -sidePixels, -sidePixels), picture);
        painter.end();
    }

    // A photo without transparency stays a JPEG; anything else is a PNG
    const bool photo = (format == "jpeg" || format == "jpg") && image.transparency == 0 &&
                       !image.hasFrame();
    QBuffer buffer(&result.data);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, photo ? QByteArrayLiteral("jpeg") : QByteArrayLiteral("png"));
    if (photo) {
        writer.setQuality(PHOTO_QUALITY);
    }
    if (!writer.write(photo ? drawn.convertToFormat(QImage::Format_RGB32) : drawn)) {
        result.data.clear();
        return result;
    }
    result.extension = photo ? QStringLiteral("jpeg") : QStringLiteral("png");
    result.contentType = QStringLiteral("image/") + result.extension;
    result.missing = false;
    return result;
}

// =============================================================================
// XML
// =============================================================================

QString xmlEscaped(const QString& text) {
    QString result;
    result.reserve(text.size() + text.size() / 8);
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        const char16_t code = ch.unicode();
        if (ch.isHighSurrogate()) {
            if (i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
                result += ch;
                result += text.at(i + 1);
                ++i;
            }
            continue;
        }
        if (ch.isLowSurrogate()) {
            continue;  // alone: not a character
        }
        switch (code) {
            case u'&':
                result += QLatin1String("&amp;");
                continue;
            case u'<':
                result += QLatin1String("&lt;");
                continue;
            case u'>':
                result += QLatin1String("&gt;");
                continue;
            case u'"':
                result += QLatin1String("&quot;");
                continue;
            default:
                break;
        }
        const bool allowed = code == 0x9 || code == 0xA || code == 0xD ||
                             (code >= 0x20 && code <= 0xD7FF) || (code >= 0xE000 && code <= 0xFFFD);
        if (allowed) {
            result += ch;
        }
    }
    return result;
}

// =============================================================================
// Files
// =============================================================================

bool writeExportFile(const QString& path, const QByteArray& data, ExportProblem* problem) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        problem->kind = ExportProblem::Kind::CannotWrite;
        problem->lines << QStringLiteral("%1: %2").arg(path, file.errorString());
        return false;
    }
    if (file.write(data) != data.size() || !file.commit()) {
        // Qt says that the disk has no room left with ResourceError
        problem->kind = file.error() == QFileDevice::ResourceError ? ExportProblem::Kind::DiskFull
                                                                   : ExportProblem::Kind::CannotWrite;
        problem->lines << QStringLiteral("%1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

}  // namespace exporting

}  // namespace kalahari::editor
