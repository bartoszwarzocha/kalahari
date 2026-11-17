// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_writer.cpp
/// @brief Export of a book to a PDF document: the pages of the page view (BookPages) drawn by
///        Qt's PDF writer, then the bookmarks, the links inside the book, the comments and the
///        way the reader shows the pages added to the file as an incremental update (the file
///        Qt writes stays as it is, the new objects follow it)

#include "kalahari/editor/detail/book_export_internal.h"

#include <kalahari/editor/book_pages.h>

#include <QBuffer>
#include <QByteArray>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QTimeZone>
#include <QtGlobal>

#include <algorithm>
#include <map>
#include <optional>
#include <vector>

namespace kalahari::editor {

namespace {

/// The writer's dots per inch: of the pictures of the images (the text and the lines do not
/// depend on it), as print shops want them
constexpr int PDF_RESOLUTION = 300;

/// The icon of a comment, in points
constexpr double COMMENT_ICON = 16.0;

/// The headings deeper than this have their bookmarks closed
constexpr int OPEN_BOOKMARK_DEPTH = 2;

/// The progress of the parts of the work: the layout, the pages, the update of the file
constexpr int LAYOUT_SHARE = 50;
constexpr int PAGES_SHARE = 45;

/// @brief A text string of PDF: UTF-16BE with its byte order mark, in hex
QByteArray pdfText(const QString& text) {
    QByteArray hex("<FEFF");
    for (const QChar ch : text) {
        hex += QByteArray::number(ch.unicode(), 16).rightJustified(4, '0').toUpper();
    }
    return hex + '>';
}

/// @brief A number of PDF: at most three decimals, without an exponent
QByteArray pdfNumber(double value) {
    QByteArray text = QByteArray::number(value, 'f', 3);
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.'))) {
        text.chop(1);
    }
    return text == "-0" ? QByteArray("0") : text;
}

/// @brief A date of PDF, in UTC
QByteArray pdfDate(const QDateTime& date) {
    return "(D:" + date.toUTC().toString(QStringLiteral("yyyyMMddHHmmss")).toLatin1() + "Z)";
}

/// @brief A color of PDF: its red, green and blue from 0 to 1
QByteArray pdfColor(const QColor& color) {
    return '[' + pdfNumber(color.redF()) + ' ' + pdfNumber(color.greenF()) + ' ' +
           pdfNumber(color.blueF()) + ']';
}

/// @brief The objects added to a PDF file that Qt wrote, and the file with them
///
/// Reads what the update needs of the file: the trailer, the catalog, the pages in order and
/// the array of the annotations each page of Qt has. Objects replaced (the catalog, those
/// arrays) and added follow the file with their own cross-reference section, which points
/// to the file's (/Prev): the file's bytes stay as they are.
class PdfUpdate {
public:
    explicit PdfUpdate(QByteArray& file) : m_file(file) {}

    /// @brief Read the file; false when it is not one this class understands
    bool read() {
        const qsizetype start = m_file.lastIndexOf("startxref");
        if (start < 0) {
            return false;
        }
        static const QRegularExpression number(QStringLiteral("startxref\\s+(\\d+)"));
        const QRegularExpressionMatch found =
            number.match(QString::fromLatin1(m_file.mid(start, 64)));
        if (!found.hasMatch()) {
            return false;
        }
        m_previous = found.captured(1).toLongLong();
        if (!readCrossReferences()) {
            return false;
        }
        const qsizetype trailer = m_file.indexOf("trailer", m_previous);
        if (trailer < 0) {
            return false;
        }
        const QString dictionary = QString::fromLatin1(m_file.mid(trailer, start - trailer));
        static const QRegularExpression size(QStringLiteral("/Size\\s+(\\d+)"));
        static const QRegularExpression root(QStringLiteral("/Root\\s+(\\d+)\\s+\\d+\\s+R"));
        static const QRegularExpression info(QStringLiteral("/Info\\s+(\\d+)\\s+\\d+\\s+R"));
        static const QRegularExpression id(QStringLiteral("/ID\\s*\\[[^\\]]*\\]"));
        const QRegularExpressionMatch sizeMatch = size.match(dictionary);
        const QRegularExpressionMatch rootMatch = root.match(dictionary);
        if (!sizeMatch.hasMatch() || !rootMatch.hasMatch()) {
            return false;
        }
        m_size = sizeMatch.captured(1).toInt();
        m_root = rootMatch.captured(1).toInt();
        if (const QRegularExpressionMatch infoMatch = info.match(dictionary);
            infoMatch.hasMatch()) {
            m_info = infoMatch.captured(1).toInt();
        }
        if (const QRegularExpressionMatch idMatch = id.match(dictionary); idMatch.hasMatch()) {
            m_id = idMatch.captured(0).toLatin1();
        }

        // The pages in order, each with the array of its annotations
        const std::optional<QByteArray> catalog = object(m_root);
        if (!catalog) {
            return false;
        }
        static const QRegularExpression pages(QStringLiteral("/Pages\\s+(\\d+)\\s+\\d+\\s+R"));
        const QRegularExpressionMatch pagesMatch = pages.match(QString::fromLatin1(*catalog));
        if (!pagesMatch.hasMatch()) {
            return false;
        }
        m_catalog = *catalog;
        const std::optional<QByteArray> tree = object(pagesMatch.captured(1).toInt());
        if (!tree) {
            return false;
        }
        const qsizetype kidsAt = tree->indexOf("/Kids");
        const qsizetype open = tree->indexOf('[', kidsAt);
        const qsizetype close = tree->indexOf(']', open);
        if (kidsAt < 0 || open < 0 || close < 0) {
            return false;
        }
        static const QRegularExpression reference(QStringLiteral("(\\d+)\\s+\\d+\\s+R"));
        static const QRegularExpression annots(QStringLiteral("/Annots\\s+(\\d+)\\s+\\d+\\s+R"));
        const QString kids = QString::fromLatin1(tree->mid(open + 1, close - open - 1));
        for (auto it = reference.globalMatch(kids); it.hasNext();) {
            const int page = it.next().captured(1).toInt();
            const std::optional<QByteArray> body = object(page);
            const QRegularExpressionMatch annotsMatch =
                body ? annots.match(QString::fromLatin1(*body)) : QRegularExpressionMatch();
            if (!annotsMatch.hasMatch()) {
                return false;  // Qt gives each page its array of annotations
            }
            m_pages.push_back(page);
            m_annotationArrays.push_back(annotsMatch.captured(1).toInt());
        }
        return !m_pages.empty();
    }

    /// @brief The pages of the file
    int pageCount() const { return static_cast<int>(m_pages.size()); }

    /// @brief A reference to the object of a page
    QByteArray pageReference(int page) const {
        return QByteArray::number(m_pages[static_cast<size_t>(page)]) + " 0 R";
    }

    /// @brief The number the next object added gets
    int nextObject() const { return m_size + static_cast<int>(m_added.size()); }

    /// @brief Add an object (its body without "obj" and "endobj"); its number
    int add(const QByteArray& body) {
        const int number = nextObject();
        m_added.push_back(body);
        return number;
    }

    /// @brief Add an annotation to a page
    void annotate(int page, int annotation) {
        m_annotations[page].push_back(annotation);
    }

    /// @brief Entries added to the catalog (bookmarks, the layout of the pages, the language)
    void addToCatalog(const QByteArray& entries) { m_catalogEntries += entries; }

    /// @brief Write the update after the file
    bool finish() {
        std::map<int, QByteArray> replaced;
        if (!m_catalogEntries.isEmpty()) {
            const qsizetype end = m_catalog.lastIndexOf(">>");
            if (end < 0) {
                return false;
            }
            replaced[m_root] = m_catalog.left(end) + m_catalogEntries + ">>";
        }
        for (const auto& [page, annotations] : m_annotations) {
            const int array = m_annotationArrays[static_cast<size_t>(page)];
            const std::optional<QByteArray> old = object(array);
            QByteArray content = old ? *old : QByteArray("[]");
            const qsizetype close = content.lastIndexOf(']');
            if (close < 0) {
                return false;
            }
            content.truncate(close);
            for (const int annotation : annotations) {
                content += ' ' + QByteArray::number(annotation) + " 0 R";
            }
            replaced[array] = content + " ]";
        }
        if (replaced.empty() && m_added.empty()) {
            return true;  // nothing to add
        }

        std::map<int, qint64> offsets;
        if (!m_file.endsWith('\n')) {
            m_file += '\n';
        }
        const auto write = [&](int number, const QByteArray& body) {
            offsets[number] = m_file.size();
            m_file += QByteArray::number(number) + " 0 obj\n" + body + "\nendobj\n";
        };
        for (const auto& [number, body] : replaced) {
            write(number, body);
        }
        for (size_t i = 0; i < m_added.size(); ++i) {
            write(m_size + static_cast<int>(i), m_added[i]);
        }

        // The cross-references of the objects written, in runs of consecutive numbers
        const qint64 xref = m_file.size();
        m_file += "xref\n";
        for (auto it = offsets.cbegin(); it != offsets.cend();) {
            auto end = it;
            int last = it->first;
            while (std::next(end) != offsets.cend() && std::next(end)->first == last + 1) {
                ++end;
                ++last;
            }
            m_file += QByteArray::number(it->first) + ' ' +
                      QByteArray::number(last - it->first + 1) + '\n';
            for (auto entry = it; entry != std::next(end); ++entry) {
                m_file += QByteArray::number(entry->second).rightJustified(10, '0') + " 00000 n \n";
            }
            it = std::next(end);
        }
        m_file += "trailer\n<<\n/Size " + QByteArray::number(nextObject()) + "\n/Root " +
                  QByteArray::number(m_root) + " 0 R\n";
        if (m_info > 0) {
            m_file += "/Info " + QByteArray::number(m_info) + " 0 R\n";
        }
        if (!m_id.isEmpty()) {
            m_file += m_id + '\n';
        }
        m_file += "/Prev " + QByteArray::number(m_previous) + "\n>>\nstartxref\n" +
                  QByteArray::number(xref) + "\n%%EOF\n";
        return true;
    }

private:
    /// @brief Read the cross-reference section of the file (one, as Qt writes it)
    bool readCrossReferences() {
        qsizetype at = m_previous;
        if (!m_file.mid(at, 4).startsWith("xref")) {
            return false;
        }
        at = m_file.indexOf('\n', at) + 1;
        static const QRegularExpression head(QStringLiteral("^(\\d+)\\s+(\\d+)\\s*$"));
        while (at > 0 && at < m_file.size() && !m_file.mid(at, 7).startsWith("trailer")) {
            const qsizetype lineEnd = m_file.indexOf('\n', at);
            if (lineEnd < 0) {
                return false;
            }
            const QRegularExpressionMatch subsection =
                head.match(QString::fromLatin1(m_file.mid(at, lineEnd - at)).trimmed());
            if (!subsection.hasMatch()) {
                return false;
            }
            const int first = subsection.captured(1).toInt();
            const int count = subsection.captured(2).toInt();
            at = lineEnd + 1;
            for (int i = 0; i < count; ++i) {
                const QByteArray entry = m_file.mid(at, 20);
                if (entry.size() < 18) {
                    return false;
                }
                if (entry.at(17) == 'n') {
                    m_offsets[first + i] = entry.left(10).toLongLong();
                }
                at += 20;
            }
        }
        return true;
    }

    /// @brief The body of an object of the file, between "obj" and "endobj" (not a stream)
    std::optional<QByteArray> object(int number) const {
        const auto found = m_offsets.find(number);
        if (found == m_offsets.end()) {
            return std::nullopt;
        }
        const qsizetype start = m_file.indexOf("obj", found->second);
        const qsizetype end = start < 0 ? -1 : m_file.indexOf("endobj", start);
        if (end < 0) {
            return std::nullopt;
        }
        return m_file.mid(start + 3, end - start - 3).trimmed();
    }

    QByteArray& m_file;
    qint64 m_previous = 0;       ///< The file's cross-reference section
    std::map<int, qint64> m_offsets;
    int m_size = 0;              ///< The objects of the file
    int m_root = 0;
    int m_info = 0;
    QByteArray m_id;
    QByteArray m_catalog;
    QByteArray m_catalogEntries;
    std::vector<int> m_pages;
    std::vector<int> m_annotationArrays;
    std::map<int, std::vector<int>> m_annotations;  ///< Page, the annotations added to it
    std::vector<QByteArray> m_added;
};

/// @brief A place of a page as a destination of PDF: its top left, in the PDF's points (from
///        the bottom of the page)
QByteArray destination(const PdfUpdate& update, const PagePlace& place, double pageHeight) {
    return '[' + update.pageReference(place.page) + " /XYZ " + pdfNumber(place.point.x()) + ' ' +
           pdfNumber(pageHeight - place.point.y()) + " null]";
}

/// @brief A rectangle of a page in the PDF's points (from the bottom of the page)
QByteArray pdfRect(const QRectF& rect, double pageHeight) {
    return '[' + pdfNumber(rect.left()) + ' ' + pdfNumber(pageHeight - rect.bottom()) + ' ' +
           pdfNumber(rect.right()) + ' ' + pdfNumber(pageHeight - rect.top()) + ']';
}

/// @brief The bookmarks of the headings, a tree by their levels
void addBookmarks(PdfUpdate& update, const std::vector<PageHeading>& headings, double pageHeight) {
    struct Node {
        int object = 0;
        int parent = -1;  ///< Index of the parent node; -1: the root
        int depth = 0;
        std::vector<int> children;
    };
    std::vector<Node> nodes;
    std::vector<int> topLevel;
    std::vector<std::pair<int, int>> open;  // level, node of the headings above
    int next = update.nextObject() + 1;     // the root of the outlines first
    for (const PageHeading& heading : headings) {
        if (heading.place.page < 0 || heading.place.page >= update.pageCount() ||
            heading.text.isEmpty()) {
            continue;
        }
        while (!open.empty() && open.back().first >= heading.level) {
            open.pop_back();
        }
        Node node;
        node.object = next++;
        node.parent = open.empty() ? -1 : open.back().second;
        node.depth = static_cast<int>(open.size());
        const int index = static_cast<int>(nodes.size());
        if (node.parent < 0) {
            topLevel.push_back(index);
        } else {
            nodes[static_cast<size_t>(node.parent)].children.push_back(index);
        }
        nodes.push_back(node);
        open.emplace_back(heading.level, index);
    }
    if (nodes.empty()) {
        return;
    }

    // The visible descendants of each node: the children of an open one count
    std::vector<int> visible(nodes.size(), 0);
    for (size_t i = nodes.size(); i-- > 0;) {
        for (const int child : nodes[i].children) {
            const bool childOpen = nodes[static_cast<size_t>(child)].depth < OPEN_BOOKMARK_DEPTH;
            visible[i] += 1 + (childOpen ? visible[static_cast<size_t>(child)] : 0);
        }
    }
    int total = 0;
    for (const int index : topLevel) {
        total += 1 + (nodes[static_cast<size_t>(index)].depth < OPEN_BOOKMARK_DEPTH
                          ? visible[static_cast<size_t>(index)]
                          : 0);
    }
    const int root = update.add(
        "<<\n/Type /Outlines\n/First " +
        QByteArray::number(nodes[static_cast<size_t>(topLevel.front())].object) + " 0 R\n/Last " +
        QByteArray::number(nodes[static_cast<size_t>(topLevel.back())].object) + " 0 R\n/Count " +
        QByteArray::number(total) + "\n>>");
    size_t heading = 0;
    for (size_t i = 0; i < nodes.size(); ++i, ++heading) {
        while (headings[heading].place.page < 0 ||
               headings[heading].place.page >= update.pageCount() ||
               headings[heading].text.isEmpty()) {
            ++heading;
        }
        const Node& node = nodes[i];
        const std::vector<int>& siblings =
            node.parent < 0 ? topLevel : nodes[static_cast<size_t>(node.parent)].children;
        const auto at = std::find(siblings.cbegin(), siblings.cend(), static_cast<int>(i));
        QByteArray body =
            "<<\n/Title " + pdfText(headings[heading].text) + "\n/Parent " +
            QByteArray::number(node.parent < 0 ? root
                                               : nodes[static_cast<size_t>(node.parent)].object) +
            " 0 R\n";
        if (at != siblings.cbegin()) {
            body += "/Prev " +
                    QByteArray::number(nodes[static_cast<size_t>(*std::prev(at))].object) +
                    " 0 R\n";
        }
        if (std::next(at) != siblings.cend()) {
            body += "/Next " +
                    QByteArray::number(nodes[static_cast<size_t>(*std::next(at))].object) +
                    " 0 R\n";
        }
        if (!node.children.empty()) {
            body += "/First " +
                    QByteArray::number(nodes[static_cast<size_t>(node.children.front())].object) +
                    " 0 R\n/Last " +
                    QByteArray::number(nodes[static_cast<size_t>(node.children.back())].object) +
                    " 0 R\n/Count " +
                    QByteArray::number(node.depth < OPEN_BOOKMARK_DEPTH
                                           ? visible[i]
                                           : -static_cast<int>(node.children.size())) +
                    '\n';
        }
        body += "/Dest " + destination(update, headings[heading].place, pageHeight) + "\n>>";
        update.add(body);
    }
    update.addToCatalog("/Outlines " + QByteArray::number(root) + " 0 R\n/PageMode /UseOutlines\n");
}

/// @brief The comment of PDF of an annotation: a note beside its fragment, with its author,
///        its date and its kind's color
QByteArray commentObject(const BookExport& book, const PageComment& comment, double pageHeight,
                         bool archive) {
    const Annotation& annotation = comment.annotation;
    const EditorColors::AnnotationColors& colors = book.colors.annotations(EditorColorMode::Light);
    const QColor color = annotation.kind == AnnotationKind::Todo   ? colors.todo
                         : annotation.kind == AnnotationKind::Note ? colors.note
                                                                   : colors.comment;
    QString text = annotation.text;
    const QString label = book.annotationLabels.value(annotation.kind);
    if (!label.isEmpty()) {
        text = label + QStringLiteral(": ") + text;
    }
    const QRectF icon(comment.point.x(), comment.point.y() - COMMENT_ICON, COMMENT_ICON,
                      COMMENT_ICON);
    QByteArray body =
        "<<\n/Type /Annot\n/Subtype /Text\n/Rect " + pdfRect(icon, pageHeight) + "\n/Contents " +
        pdfText(text) + "\n/Name /" +
        (annotation.kind == AnnotationKind::Note ? QByteArray("Note") : QByteArray("Comment")) +
        "\n/Open false\n";
    if (!annotation.author.isEmpty()) {
        body += "/T " + pdfText(annotation.author) + '\n';
    }
    if (annotation.created.isValid()) {
        body += "/M " + pdfDate(annotation.created) + '\n';
    }
    if (color.isValid()) {
        body += "/C " + pdfColor(color) + '\n';
    }
    if (archive) {
        body += "/F 4\n";  // PDF/A: printed with the page, never hidden
    }
    return body + ">>";
}

}  // namespace

bool pdfPurposeSupported(PdfPurpose purpose) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    Q_UNUSED(purpose)
    return true;
#else
    return purpose != PdfPurpose::PrintShop;
#endif
}

bool pdfGrayscaleSupported() {
    return QT_VERSION >= QT_VERSION_CHECK(6, 8, 0);
}

bool exportPdf(const BookExport& book, const QString& path, const ExportProgress& progress,
               ExportResult* result, ExportProblem* problem) {
    const PdfOptions& options = book.pdf;
    const bool printShop = options.purpose == PdfPurpose::PrintShop;
    const bool archive = options.purpose == PdfPurpose::Archive;
    if (!pdfPurposeSupported(options.purpose)) {
        problem->kind = ExportProblem::Kind::Other;
        problem->lines << QStringLiteral(
                              "PDF/X-4 needs Qt 6.8 or later (the program runs with Qt %1)")
                              .arg(QString::fromLatin1(qVersion()));
        return false;
    }

    // The pages, laid out as the page view does: a file for a print shop without links
    BookPages pages(book, BookPages::Options{!printShop});
    pages.layOut([&progress](int percent) {
        if (progress) {
            progress(percent * LAYOUT_SHARE / 100);
        }
    });

    QByteArray data;
    {
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        QPdfWriter writer(&buffer);
        writer.setResolution(PDF_RESOLUTION);
        writer.setCreator(QStringLiteral("Kalahari"));
        writer.setTitle(book.title);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        writer.setAuthor(book.author);
        if (options.grayscale) {
            writer.setColorModel(QPdfWriter::ColorModel::Grayscale);
        }
#endif
        if (archive) {
            writer.setPdfVersion(QPagedPaintDevice::PdfVersion_A1b);
        } else if (printShop) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            writer.setPdfVersion(QPagedPaintDevice::PdfVersion_X4);
#endif
        } else {
            writer.setPdfVersion(QPagedPaintDevice::PdfVersion_1_6);
        }
        const QSizeF size = pages.pageSize();
        QPageLayout layout(QPageSize(size, QPageSize::Point, QString(), QPageSize::ExactMatch),
                           QPageLayout::Portrait, QMarginsF(), QPageLayout::Point);
        layout.setMode(QPageLayout::FullPageMode);
        writer.setPageLayout(layout);

        QPainter painter;
        if (!painter.begin(&writer)) {
            problem->kind = ExportProblem::Kind::Other;
            problem->lines << QStringLiteral("%1: the PDF writer cannot start").arg(path);
            return false;
        }
        const double scale = static_cast<double>(PDF_RESOLUTION) / POINTS_PER_INCH;
        const int count = pages.pageCount();
        for (int page = 0; page < count; ++page) {
            if (page > 0) {
                writer.newPage();
            }
            painter.save();
            painter.scale(scale, scale);
            pages.paint(&painter, page);
            painter.restore();
            if (progress) {
                progress(LAYOUT_SHARE + PAGES_SHARE * (page + 1) / std::max(1, count));
            }
        }
        painter.end();
    }

    // The bookmarks, the links inside the book, the comments, the pages in pairs, the language
    PdfUpdate update(data);
    if (!update.read() || update.pageCount() != pages.pageCount()) {
        problem->kind = ExportProblem::Kind::Other;
        problem->lines << QStringLiteral("%1: the PDF Qt wrote cannot be read").arg(path);
        return false;
    }
    const double pageHeight = pages.pageSize().height();
    if (options.bookmarks) {
        addBookmarks(update, pages.headings(), pageHeight);
    }
    if (!printShop) {
        for (const PageLink& link : pages.links()) {
            if (link.target.page < 0 || link.page >= update.pageCount()) {
                continue;
            }
            QByteArray body = "<<\n/Type /Annot\n/Subtype /Link\n/Rect " +
                              pdfRect(link.rect, pageHeight) + "\n/Border [0 0 0]\n/Dest " +
                              destination(update, link.target, pageHeight) + '\n';
            if (archive) {
                body += "/F 4\n";
            }
            update.annotate(link.page, update.add(body + ">>"));
        }
        if (book.annotationsAsComments) {
            for (const PageComment& comment : pages.comments()) {
                if (comment.page >= 0 && comment.page < update.pageCount()) {
                    update.annotate(comment.page,
                                    update.add(commentObject(book, comment, pageHeight, archive)));
                }
            }
        }
    }
    if (options.facingPages) {
        // The first page alone on the right, then the pages in pairs (PDF 1.4 of PDF/A-1 has
        // the columns, PDF 1.5 the pages)
        update.addToCatalog(archive ? QByteArray("/PageLayout /TwoColumnRight\n")
                                    : QByteArray("/PageLayout /TwoPageRight\n"));
    }
    static const QRegularExpression languageTag(
        QStringLiteral("^[A-Za-z]{2,8}(-[A-Za-z0-9]{1,8})*$"));
    if (languageTag.match(book.language).hasMatch()) {
        update.addToCatalog("/Lang (" + book.language.toLatin1() + ")\n");
    }
    if (!update.finish()) {
        problem->kind = ExportProblem::Kind::Other;
        problem->lines << QStringLiteral("%1: the PDF Qt wrote cannot be updated").arg(path);
        return false;
    }
    if (!exporting::writeExportFile(path, data, problem)) {
        return false;
    }
    if (progress) {
        progress(100);
    }
    result->file = path;
    result->images = pages.images() - pages.missingImages();
    result->missingImages = pages.missingImages();
    return true;
}

}  // namespace kalahari::editor
