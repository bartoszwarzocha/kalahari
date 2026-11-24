// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_pages.cpp
/// @brief The pages of a book as the page view of the editor shows them, for a PDF file and
///        for a printer

#include <kalahari/editor/book_pages.h>

#include "kalahari/editor/detail/book_editor_internal.h"
#include "kalahari/editor/detail/book_export_internal.h"

#include <kalahari/editor/editor_render_pipeline.h>
#include <kalahari/editor/image_store.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/text_captions.h>
#include <kalahari/editor/text_fields.h>
#include <kalahari/editor/text_images.h>
#include <kalahari/editor/text_link.h>
#include <kalahari/editor/text_lists.h>
#include <kalahari/editor/text_source_adapter.h>
#include <kalahari/editor/text_tables.h>

#include <QFontMetricsF>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextLayout>
#include <QTextTable>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>

namespace kalahari::editor {

namespace {

/// The running head and the page number are smaller than the text, as in the page view
constexpr double MARGIN_TEXT_SCALE = 0.8;

/// The letters of the running head (capitals) a little apart, in percent of their width
constexpr double RUNNING_HEAD_SPACING = 108.0;

/// A text goes on on the last page of the one before it when so many lines of Normal fit
/// under it
constexpr double LINES_TO_GO_ON = 3.0;

/// The table of contents: the share of the column of the page numbers, in percent, and the
/// indent of a heading of each level below the first, in points
constexpr int CONTENTS_NUMBER_PERCENT = 12;
constexpr double CONTENTS_LEVEL_INDENT = 14.0;

/// What a page number of the table of contents shows before the pages are known: as wide as
/// most numbers, so the table's lines break as they will
constexpr const char* CONTENTS_NUMBER_PLACEHOLDER = "000";

/// How often the pages are laid out again for the page numbers of the tables of contents:
/// the numbers move only when the table takes another number of pages
constexpr int CONTENTS_PASSES = 3;

/// The tab stops every so many centimetres, as Word has them
constexpr double TAB_CENTIMETRES = 1.25;

constexpr double CENTIMETRES_PER_INCH = 2.54;

/// Positions closer than this are the same (document units)
constexpr double SAME_PLACE = 0.5;

/// @brief A number of a style's properties; @p fallback when it has none
double numberOf(const QVariantMap& properties, const char* name, double fallback) {
    bool ok = false;
    const double value = properties.value(QLatin1String(name)).toDouble(&ok);
    return ok ? value : fallback;
}

/// @brief Whether the first paragraph with text of a model is a heading: the text keeps its
///        own, without the title of the Navigator (exporting::startsWithHeading())
bool modelStartsWithHeading(const KmlDocumentModel& model, const ParagraphStyleSet& styles) {
    for (size_t i = 0; i < model.paragraphCount(); ++i) {
        if (model.paragraphCell(i) || model.paragraphObject(i) != TextObject::None) {
            return false;
        }
        if (model.paragraphText(i).trimmed().isEmpty()) {
            continue;
        }
        const QString style = model.paragraphStyle(i);
        const QString shown = styles.contains(style) ? style : QString::fromLatin1(NORMAL_STYLE_ID);
        return exporting::headingLevelOfStyle(styles, shown) > 0;
    }
    return false;
}

/// @brief The colors a printed page is drawn in: the light paper's, without what only the
///        screen shows (the faint lines of a table without borders, the background of a field)
RenderColors printColors(const EditorColors& colors) {
    RenderColors result;
    const EditorColorMode light = EditorColorMode::Light;
    result.text = colors.textColor(light);
    result.background = colors.background(light);
    result.inactiveText = result.text;
    result.link = colors.linkColor(light);
    result.tableLines = colors.tables.linesLight;
    result.tableHeader = colors.tables.headerLight;
    result.tableGridlines = QColor(Qt::transparent);
    result.field = QColor(Qt::transparent);
    result.brokenField = QColor(Qt::transparent);
    return result;
}

/// @brief The text of a heading as it reads: its fields as they show, its notes and images
///        left out (exporting::plainTextOf())
QString headingText(const QTextBlock& block, const KalahariTextDocumentLayout& layout) {
    QString text;
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        const QString characters = fragment.text();
        for (qsizetype i = 0; i < characters.size(); ++i) {
            const QChar ch = characters.at(i);
            if (ch == QChar::ObjectReplacementCharacter) {
                const QTextCharFormat format = fragment.charFormat();
                if (captionNumberOf(format) || crossReferenceOf(format)) {
                    text += layout.captionNumbers()
                                .fieldText(fragment.position() + static_cast<int>(i), format)
                                .text;
                }
                continue;
            }
            text += ch == QChar::LineSeparator ? QChar(QLatin1Char(' ')) : ch;
        }
    }
    return text.simplified();
}

/// @brief A line of text in the middle of an area of a margin (document coordinates), laid
///        out as the text of the page is: its size does not depend on the painter's device
void drawMarginLine(QPainter* painter, const QString& text, const QFont& font, const QRectF& area) {
    QTextLayout layout(text, font);
    QTextOption option(Qt::AlignHCenter);
    option.setWrapMode(QTextOption::NoWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (!line.isValid()) {
        layout.endLayout();
        return;
    }
    line.setLineWidth(area.width());
    layout.endLayout();
    layout.draw(painter, QPointF(area.left(), area.top() + (area.height() - line.height()) / 2.0));
}

}  // namespace

// =============================================================================
// An item of the book
// =============================================================================

struct BookPages::Item {
    const ExportItem* source = nullptr;
    std::unique_ptr<QTextDocument> document;
    KalahariTextDocumentLayout* layout = nullptr;  ///< The document's (it owns it)
    std::unique_ptr<EditorRenderPipeline> pipeline;
    bool titled = false;  ///< Its title is its first paragraph
    int textStart = 0;    ///< Where its text starts after the title
    int firstPage = 0;    ///< Its first page in the book
    int pages = 1;        ///< Its pages
    bool goesOn = false;  ///< It starts on the last page of the item before it

    /// @brief The page of the item a point of its document is on (document coordinates)
    int pageOf(double y, double pitch) const {
        return std::clamp(static_cast<int>(std::floor((y + SAME_PLACE) / pitch)), 0,
                          std::max(0, pages - 1));
    }
};

// =============================================================================
// The pages
// =============================================================================

BookPages::BookPages(const BookExport& book, const Options& options)
    : m_book(book), m_options(options) {}

BookPages::~BookPages() = default;

void BookPages::layOut(const ExportProgress& progress) {
    m_items.clear();
    m_pages.clear();
    m_headings.clear();
    m_links.clear();
    m_comments.clear();

    // The document units are the layout's pixels: those of the font's points
    QFont font(m_book.fontFamily.isEmpty() ? QFont().family() : m_book.fontFamily);
    const double normalPoints = numberOf(m_book.styles.normal().properties, "fontSize",
                                         DEFAULT_NORMAL_POINT_SIZE);
    font.setPointSizeF(normalPoints > 0.0 ? normalPoints : DEFAULT_NORMAL_POINT_SIZE);
    m_dpi = QFontMetricsF(font).fontDpi();
    const double pixelsPerPoint = m_dpi / POINTS_PER_INCH;

    // The look of the styles: Normal's line height, space after and first-line indent are the
    // typography of every paragraph (the editor's settings play no part)
    const QVariantMap& normal = m_book.styles.normal().properties;
    LayoutTypography typography;
    const double lineHeight = numberOf(normal, "lineHeight", 1.0);
    typography.lineSpacing = lineHeight > 0.0 ? lineHeight : 1.0;
    typography.paragraphSpacing =
        std::max(0.0, numberOf(normal, "spaceAfter", 0.0)) * pixelsPerPoint;
    typography.firstLineIndent = numberOf(normal, "firstLineIndent", 0.0) * pixelsPerPoint;
    typography.tabStops = TabStops::Indent;
    typography.tabIndent = TAB_CENTIMETRES / CENTIMETRES_PER_INCH * m_dpi;

    // The page in every item's pipeline: the sheet at 100%, no gap, no shadow, no numbers
    RenderContext context;
    context.screenDpi = m_dpi;
    context.paperScale = 1.0;
    context.zoomFactor = 1.0;
    context.font = font;
    context.typography = typography;
    context.colors = printColors(m_book.colors);
    context.viewMode = ViewMode::Page;
    const ExportPage& page = m_book.page;
    context.pageMode.pageSize = QSizeF(page.width * POINTS_PER_INCH / MM_PER_INCH,
                                       page.height * POINTS_PER_INCH / MM_PER_INCH);
    context.pageMode.marginsMm = page.mirrored
                                     ? QMarginsF(page.inner, page.top, page.outer, page.bottom)
                                     : QMarginsF(page.left, page.top, page.right, page.bottom);
    context.pageMode.pageSpacing = 0.0;
    context.pageMode.showPageBreaks = false;
    context.pageMode.showPageNumbers = false;
    context.pageMode.mirrorMargins = page.mirrored;
    context.cursor.visible = false;
    context.viewportSize = QSizeF(context.pageMode.pageSize.width() * m_dpi / POINTS_PER_INCH,
                                  context.pageMode.pageSize.height() * m_dpi / POINTS_PER_INCH);
    EditorRenderPipeline probe;  // the page's sizes, as every item's pipeline has them
    probe.configure(context);
    m_context = probe.context();
    const RenderContext::Computed& computed = m_context.computed;
    m_pitch = computed.pagePitch;
    m_textHeight = computed.textAreaHeight;
    m_goOnRoom = LINES_TO_GO_ON * QFontMetricsF(font).lineSpacing() * typography.lineSpacing;

    m_store = std::make_shared<ImageStore>();
    m_store->setFolder(m_book.imagesFolder);

    // The texts first, then the tables of contents with their headings
    const size_t count = m_book.items.size();
    for (const ExportItem& source : m_book.items) {
        auto item = std::make_unique<Item>();
        item->source = &source;
        m_items.push_back(std::move(item));
    }
    bool contents = false;
    for (size_t index = 0; index < count; ++index) {
        if (m_items[index]->source->kind == ExportItemKind::Contents) {
            contents = true;
            continue;
        }
        build(index);
        if (progress) {
            progress(static_cast<int>(60 * (index + 1) / std::max<size_t>(1, count)));
        }
    }
    findHeadings();
    for (size_t index = 0; index < count; ++index) {
        if (m_items[index]->source->kind == ExportItemKind::Contents) {
            build(index);
        }
    }
    place();
    collect();
    // The tables of contents with the pages of the headings, until the pages stay
    for (int pass = 1; contents && pass < CONTENTS_PASSES; ++pass) {
        std::vector<int> pages;
        pages.reserve(m_headings.size());
        for (const PageHeading& heading : m_headings) {
            pages.push_back(heading.place.page);
        }
        if (pages == m_contentsPages) {
            break;
        }
        m_contentsPages = pages;
        for (size_t index = 0; index < count; ++index) {
            if (m_items[index]->source->kind == ExportItemKind::Contents) {
                build(index);
            }
        }
        place();
        collect();
    }
    if (progress) {
        progress(90);
    }
}

void BookPages::build(size_t index) {
    Item& item = *m_items[index];
    const ExportItem& source = *item.source;
    const ParagraphStyleSet& styles = m_book.styles;

    // The document laid out as the editor lays out a text: the layout and the pipeline get the
    // font, the width and the page before the content, which is laid out once
    item.pipeline.reset();
    item.document = std::make_unique<QTextDocument>();
    QTextDocument* document = item.document.get();
    document->setUndoRedoEnabled(false);
    document->setDocumentMargin(0);
    auto* layout = new KalahariTextDocumentLayout(document);
    document->setDocumentLayout(layout);
    item.layout = layout;
    layout->setPrinted(true);
    layout->setParagraphStyles(styles);
    layout->setImageStore(m_store);
    layout->setSceneBreakLook(m_book.sceneBreak);
    layout->setCaptionContext(source.captions);
    layout->setFootnoteContext(source.footnotes);
    layout->setFieldContext(exportFieldContext(m_book, source));
    item.pipeline = std::make_unique<EditorRenderPipeline>();
    item.pipeline->setTextSource(std::make_unique<QTextDocumentSource>(document));
    item.pipeline->configure(m_context);
    KalahariTextDocumentLayout::NoteColors notes;
    notes.text = m_context.colors.text;
    notes.line = m_book.colors.noteLineColor(EditorColorMode::Light);
    notes.link = m_context.colors.link;
    item.pipeline->setPageDecorator([layout, notes](QPainter* painter, int page) {
        layout->drawPageNotes(painter, page, notes);
    });

    // Its title, as a paragraph of its title's style, unless its text starts with a heading
    KmlDocumentModel model;
    if (source.kind == ExportItemKind::Text) {
        model.loadKml(source.kml);  // unreadable KML: the paragraphs read before the error
    } else if (source.kind == ExportItemKind::Contents) {
        model.loadKml(contentsKml());
    }
    item.titled = m_book.titleHeadings && !source.title.trimmed().isEmpty() &&
                  (source.kind != ExportItemKind::Text || !modelStartsWithHeading(model, styles));
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    QTextBlockFormat plain;
    plain.setTopMargin(0);
    plain.setBottomMargin(0);
    if (item.titled) {
        QTextBlockFormat title = plain;
        setParagraphStyle(title, exporting::titleStyleOf(source, styles));
        cursor.setBlockFormat(title);
        cursor.insertText(source.title.simplified(), QTextCharFormat());
        if (model.paragraphCount() > 0) {
            cursor.insertBlock(plain, QTextCharFormat());
        }
    }
    item.textStart = cursor.position();
    appendParagraphs(cursor, model);
    cursor.endEditBlock();

    // The links: those the program opens stay links (a PDF makes them clickable), underlined
    // and in the link color unless they have their own, as the page view shows them; the
    // others are text, as on paper (the formats change after the fragments are read)
    struct Link {
        int start = 0;
        int end = 0;
        QTextCharFormat format;
    };
    std::vector<Link> links;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const QString address =
                fragment.isValid() ? linkAddressOf(fragment.charFormat()) : QString();
            if (address.isEmpty()) {
                continue;
            }
            QTextCharFormat format = fragment.charFormat();
            if (m_options.links && isOpenableLinkAddress(address)) {
                if (!format.hasProperty(QTextFormat::ForegroundBrush)) {
                    format.setForeground(m_context.colors.link);
                }
                format.setFontUnderline(true);
            } else {
                setLinkAddress(format, QString());
            }
            links.push_back(
                Link{fragment.position(), fragment.position() + fragment.length(), format});
        }
    }
    for (const Link& link : links) {
        QTextCursor range(document);
        range.setPosition(link.start);
        range.setPosition(link.end, QTextCursor::KeepAnchor);
        range.setCharFormat(link.format);
    }
}

void BookPages::findHeadings() {
    // The titles of the items and the headings of their texts, in reading order (the
    // writers of DOCX and Markdown take the same)
    m_headings.clear();
    m_headingBlocks.clear();
    for (size_t index = 0; index < m_items.size(); ++index) {
        const Item& item = *m_items[index];
        if (item.source->kind == ExportItemKind::Contents || !item.document) {
            continue;
        }
        const ParagraphStyleSet& styles = m_book.styles;
        if (item.titled) {
            PageHeading heading;
            heading.level = std::max(1, exporting::headingLevelOfStyle(
                                            styles, exporting::titleStyleOf(*item.source, styles)));
            heading.text = item.source->title.simplified();
            m_headings.push_back(heading);
            m_headingBlocks.emplace_back(index, 0);
        }
        for (QTextBlock block = item.document->findBlock(item.textStart); block.isValid();
             block = block.next()) {
            if ((item.titled && block.blockNumber() == 0) || tableOf(block) != nullptr ||
                blockObject(block) != TextObject::None || listItemOf(block.blockFormat())) {
                continue;
            }
            const int level =
                exporting::headingLevelOfStyle(styles, exporting::shownStyleOf(styles, block));
            const QString text = level > 0 ? headingText(block, *item.layout) : QString();
            if (!text.isEmpty()) {
                m_headings.push_back(PageHeading{level, text, PagePlace{}});
                m_headingBlocks.emplace_back(index, block.blockNumber());
            }
        }
    }
}

QString BookPages::contentsKml() const {
    // A table without borders: the headings, indented by their levels, their pages on the
    // right (as wide as most numbers until the pages are known)
    if (m_headings.empty()) {
        return QString();
    }
    QString kml =
        QStringLiteral("<kml><table borders=\"none\"><col width=\"%1%\"/><col width=\"%2%\"/>")
            .arg(100 - CONTENTS_NUMBER_PERCENT)
            .arg(CONTENTS_NUMBER_PERCENT);
    for (size_t i = 0; i < m_headings.size(); ++i) {
        const PageHeading& heading = m_headings[i];
        const int page = i < m_contentsPages.size() ? m_contentsPages[i] : -1;
        const QString number = page >= 0 ? QString::number(page + 1)
                                         : QString::fromLatin1(CONTENTS_NUMBER_PLACEHOLDER);
        const double indent = (std::clamp(heading.level, 1, 3) - 1) * CONTENTS_LEVEL_INDENT;
        kml += QStringLiteral("<tr><td><p indent-first=\"0\" indent-left=\"%1\">").arg(indent) +
               exporting::xmlEscaped(heading.text) +
               QStringLiteral("</p></td><td><p align=\"right\" indent-first=\"0\">") + number +
               QStringLiteral("</p></td></tr>");
    }
    return kml + QStringLiteral("</table></kml>");
}

void BookPages::place() {
    m_pages.clear();
    for (size_t index = 0; index < m_items.size(); ++index) {
        Item& item = *m_items[index];

        // A text goes on after the one before it on its last page (without new pages for the
        // texts), when the page has room for a few lines and no footnotes at its bottom
        double top = 0.0;
        item.goesOn = false;
        if (index > 0 && !m_book.textsOnNewPage && !m_pages.empty()) {
            const Item& before = *m_items[index - 1];
            const int last = before.pages - 1;
            const double bottom = before.layout->contentBottom() - last * m_pitch;
            if (before.layout->pageNotesHeight(last) <= 0.0 &&
                bottom + m_goOnRoom <= m_textHeight) {
                item.goesOn = true;
                top = std::ceil(std::max(0.0, bottom));
            }
        }
        int start =
            item.goesOn ? static_cast<int>(m_pages.size()) - 1 : static_cast<int>(m_pages.size());

        // With mirror margins a text starts on a right page (an even index), unless it starts
        // on a left one: the page before it is left empty
        if (!item.goesOn && index > 0 && m_book.textsOnNewPage && m_book.page.mirrored &&
            (start % 2 == 1) != item.source->firstPageLeft) {
            m_pages.push_back(Page{{}, true});
            ++start;
        }

        // The margins of its first page: the inner one on the left of a right page
        RenderContext context = item.pipeline->context();
        const ExportPage& page = m_book.page;
        if (page.mirrored) {
            const bool right = start % 2 == 0;
            context.pageMode.marginsMm = QMarginsF(right ? page.inner : page.outer, page.top,
                                                   right ? page.outer : page.inner, page.bottom);
        }
        context.pageMode.firstPageTop = top;
        item.pipeline->configure(context);
        item.layout->layoutPendingBlocks();
        item.firstPage = start;
        item.pages = std::max(1, item.pipeline->pageCount());
        for (int itemPage = 0; itemPage < item.pages; ++itemPage) {
            if (itemPage == 0 && item.goesOn) {
                m_pages.back().parts.emplace_back(index, 0);
            } else {
                m_pages.push_back(Page{{{index, itemPage}}, false});
            }
        }
    }
}

double BookPages::leftMarginOf(int page) const {
    // The pages of the probe: its first one is a right page
    const RenderContext::Computed& computed = m_context.computed;
    return m_book.page.mirrored && page % 2 == 1 ? computed.marginRight : computed.marginLeft;
}

PagePlace BookPages::placeOf(const Item& item, const QPointF& point) const {
    const int itemPage = item.pageOf(point.y(), m_pitch);
    PagePlace place;
    place.page = item.firstPage + itemPage;
    const double toPoints = POINTS_PER_INCH / m_dpi;
    place.point =
        QPointF((leftMarginOf(place.page) + point.x()) * toPoints,
                (m_context.computed.marginTop + point.y() - itemPage * m_pitch) * toPoints);
    return place;
}

namespace {

/// @brief The box of the character at a position, on its line (document coordinates)
std::optional<QRectF> characterBox(const QTextDocument& document,
                                   const KalahariTextDocumentLayout& layout, int position) {
    const QTextBlock block = document.findBlock(position);
    QTextLayout* lines = KalahariTextDocumentLayout::blockLayout(block);
    if (lines == nullptr || lines->lineCount() == 0) {
        return std::nullopt;
    }
    const int offset = std::clamp(position - block.position(), 0, std::max(0, block.length() - 1));
    const QTextLine line = lines->lineForTextPosition(offset);
    if (!line.isValid()) {
        return std::nullopt;
    }
    const double left = line.cursorToX(offset);
    const double right = line.cursorToX(std::min(offset + 1, line.textStart() + line.textLength()));
    return QRectF(std::min(left, right), layout.blockY(block.blockNumber()) + line.y(),
                  std::max(1.0, std::abs(right - left)), line.height());
}

}  // namespace

void BookPages::collect() {
    m_links.clear();
    m_comments.clear();
    m_images = 0;
    m_missingImages = 0;
    const double toPoints = POINTS_PER_INCH / m_dpi;

    // The headings where their first lines are
    for (size_t i = 0; i < m_headings.size(); ++i) {
        const auto [index, number] = m_headingBlocks[i];
        const Item& item = *m_items[index];
        const QTextBlock block = item.document->findBlockByNumber(number);
        QTextLayout* lines = KalahariTextDocumentLayout::blockLayout(block);
        const double top =
            item.layout->blockY(number) +
            (lines != nullptr && lines->lineCount() > 0 ? lines->lineAt(0).y() : 0.0);
        m_headings[i].place = placeOf(item, QPointF(0.0, top));
    }

    // The images, the captions' numbers and the cross-references to them, the annotations
    std::map<QString, PagePlace> captions;
    struct Reference {
        PagePlace from;
        QRectF rect;
        QString target;
    };
    std::vector<Reference> references;
    for (const std::unique_ptr<Item>& owned : m_items) {
        const Item& item = *owned;
        const QTextDocument& document = *item.document;
        for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
            for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
                const QTextFragment fragment = it.fragment();
                if (!fragment.isValid() ||
                    !fragment.text().contains(QChar::ObjectReplacementCharacter)) {
                    continue;
                }
                const QTextCharFormat format = fragment.charFormat();
                if (const std::optional<ImageProperties> image = imageOf(format)) {
                    ++m_images;
                    if (item.layout->isImageMissing(*image)) {
                        ++m_missingImages;
                    }
                    continue;
                }
                const std::optional<QRectF> box =
                    characterBox(document, *item.layout, fragment.position());
                if (!box) {
                    continue;
                }
                if (const std::optional<CaptionNumberField> number = captionNumberOf(format)) {
                    captions.emplace(number->id, placeOf(item, box->topLeft()));
                } else if (const std::optional<CrossReference> reference = crossReferenceOf(format);
                           reference && reference->link) {
                    const PagePlace from = placeOf(item, box->topLeft());
                    references.push_back(
                        {from, QRectF(from.point, box->size() * toPoints), reference->target});
                }
            }
        }
        if (m_book.annotationsAsComments) {
            for (const AnnotationPlace& place : annotationsIn(document)) {
                const std::optional<QRectF> box = characterBox(document, *item.layout, place.start);
                if (box) {
                    const PagePlace at = placeOf(item, box->topLeft());
                    m_comments.push_back(PageComment{at.page, at.point, place.annotation});
                }
            }
        }
    }
    if (!m_options.links) {
        return;
    }
    for (const Reference& reference : references) {
        const auto target = captions.find(reference.target);
        if (target != captions.end()) {
            m_links.push_back(PageLink{reference.from.page, reference.rect, target->second});
        }
    }

    // The lines of the tables of contents go to their headings
    for (const std::unique_ptr<Item>& owned : m_items) {
        const Item& item = *owned;
        if (item.source->kind != ExportItemKind::Contents) {
            continue;
        }
        QTextTable* table = nullptr;
        for (QTextBlock block = item.document->findBlock(item.textStart); block.isValid() && !table;
             block = block.next()) {
            table = tableOf(block);
        }
        if (table == nullptr) {
            continue;
        }
        for (int row = 0; row < table->rows() && static_cast<size_t>(row) < m_headings.size();
             ++row) {
            const QTextBlock cell = item.document->findBlock(table->cellAt(row, 0).firstPosition());
            const QRectF rect = item.layout->blockBoundingRect(cell);
            const PagePlace from = placeOf(item, rect.topLeft());
            m_links.push_back(PageLink{
                from.page,
                QRectF(from.point,
                       QSizeF(m_context.computed.textWidth * toPoints, rect.height() * toPoints)),
                m_headings[static_cast<size_t>(row)].place});
        }
    }
}

// =============================================================================
// What the pages show
// =============================================================================

int BookPages::pageCount() const {
    return static_cast<int>(m_pages.size());
}

QSizeF BookPages::pageSize() const {
    return m_context.pageMode.pageSize;
}

bool BookPages::isBlank(int page) const {
    return page >= 0 && page < pageCount() && m_pages[static_cast<size_t>(page)].blank;
}

int BookPages::pageAt(const QString& elementId, int position) const {
    for (const std::unique_ptr<Item>& owned : m_items) {
        const Item& item = *owned;
        const bool same =
            elementId.isEmpty() ? m_items.size() == 1 : item.source->elementId == elementId;
        if (!same || item.source->kind != ExportItemKind::Text || !item.document) {
            continue;
        }
        const int last = std::max(0, item.document->characterCount() - 1);
        const std::optional<QRectF> box = characterBox(
            *item.document, *item.layout, std::clamp(item.textStart + position, 0, last));
        return box ? placeOf(item, box->topLeft()).page : item.firstPage;
    }
    return -1;
}

void BookPages::paint(QPainter* painter, int page) const {
    if (painter == nullptr || page < 0 || page >= pageCount()) {
        return;
    }
    painter->save();
    const double scale = POINTS_PER_INCH / m_dpi;
    painter->scale(scale, scale);
    for (const auto& [index, itemPage] : m_pages[static_cast<size_t>(page)].parts) {
        m_items[index]->pipeline->renderPrintedPage(painter, itemPage);
    }
    paintMargins(painter, page);
    painter->restore();
}

void BookPages::paintMargins(QPainter* painter, int page) const {
    const Page& shown = m_pages[static_cast<size_t>(page)];
    if (shown.blank || shown.parts.empty()) {
        return;
    }
    // The text the page shows last; its first page opens it (as when another text starts on it)
    const Item& item = *m_items[shown.parts.back().first];
    const bool opening = std::any_of(shown.parts.cbegin(), shown.parts.cend(),
                                     [](const auto& part) { return part.second == 0; });
    // Neither on the pages of a part's title nor on those of a text without a title (the
    // title page, the copyright page, the dedication, the motto)
    const bool text =
        item.source->kind != ExportItemKind::Group && !item.source->title.trimmed().isEmpty();

    const RenderContext::Computed& computed = m_context.computed;
    const double left = leftMarginOf(page);
    QFont font = m_context.font;
    font.setPointSizeF(font.pointSizeF() * MARGIN_TEXT_SCALE);
    painter->save();
    painter->setPen(m_context.colors.text);

    // The running head above the text, not on the first page of a text
    const bool leftPage = page % 2 == 1;
    QString head;
    switch (m_book.runningHead) {
        case RunningHead::None:
            break;
        case RunningHead::AuthorAndTitle:
            head = leftPage ? m_book.author : m_book.title;
            break;
        case RunningHead::TitleAndChapter:
            head = leftPage ? m_book.title : item.source->title;
            break;
        case RunningHead::Chapter:
            head = item.source->title;
            break;
    }
    head = head.simplified();
    if (text && !opening && !head.isEmpty()) {
        QFont headFont = font;
        headFont.setCapitalization(QFont::AllUppercase);
        headFont.setLetterSpacing(QFont::PercentageSpacing, RUNNING_HEAD_SPACING);
        const QRectF area(left, 0.0, computed.textWidth, computed.marginTop);
        drawMarginLine(painter,
                       QFontMetricsF(headFont).elidedText(head, Qt::ElideRight, area.width()),
                       headFont, area);
    }

    // The number of the page below the text
    if (text && m_book.page.pageNumbers) {
        const QRectF area(left, computed.marginTop + m_textHeight, computed.textWidth,
                          computed.pageHeightPixels - computed.marginTop - m_textHeight);
        drawMarginLine(painter, QString::number(page + 1), font, area);
    }
    painter->restore();
}

}  // namespace kalahari::editor
