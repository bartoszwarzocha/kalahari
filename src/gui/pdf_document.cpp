// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_document.cpp
/// @brief A PDF file read by PDFium: its pages drawn, their text, its data and its contents

#include "kalahari/gui/pdf_document.h"

#include <QFile>
#include <QRegularExpression>
#include <QTransform>

#include <cpp/fpdf_scopers.h>
#include <fpdf_doc.h>
#include <fpdf_text.h>
#include <fpdfview.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace kalahari::gui {

namespace {

/// How far from a letter, in points, a place still finds it (as Qt PDF)
constexpr double LETTER_TOLERANCE = 16.0;

/// The longest text of the file read (a title, a label), in bytes
constexpr unsigned long MAX_TEXT_BYTES = 1UL << 20U;

/// Pixels of a point when the places of a page are measured: PDFium gives whole pixels
constexpr int MEASURE_SCALE = 64;

/// Points between the places measured
constexpr double MEASURE_SPAN = 1000.0;

/// The version of FPDF_LIBRARY_CONFIG filled in
constexpr int LIBRARY_CONFIG_VERSION = 2;

/// PDFium serves one thread at a time
std::recursive_mutex& pdfiumMutex() {
    static std::recursive_mutex mutex;
    return mutex;
}

/// The lock of PDFium, the library started
[[nodiscard]] std::unique_lock<std::recursive_mutex> lockPdfium() {
    static std::once_flag started;
    std::call_once(started, []() {
        FPDF_LIBRARY_CONFIG config{};
        config.version = LIBRARY_CONFIG_VERSION;
        FPDF_InitLibraryWithConfig(&config);
    });
    return std::unique_lock<std::recursive_mutex>(pdfiumMutex());
}

/// FPDF_FILEACCESS::m_GetBlock: @p size bytes of the file (a QFile) from @p position
int readBlock(void* param, unsigned long position, unsigned char* buffer, unsigned long size) {
    auto* file = static_cast<QFile*>(param);
    const auto wanted = static_cast<qint64>(size);
    if (!file->seek(static_cast<qint64>(position))) {
        return 0;
    }
    return file->read(reinterpret_cast<char*>(buffer), wanted) == wanted ? 1 : 0;
}

/// Why the file did not load, by FPDF_GetLastError()
PdfDocument::Error lastError() {
    switch (FPDF_GetLastError()) {
    case FPDF_ERR_FILE:
        return PdfDocument::Error::FileNotFound;
    case FPDF_ERR_FORMAT:
        return PdfDocument::Error::InvalidFileFormat;
    case FPDF_ERR_PASSWORD:
        return PdfDocument::Error::IncorrectPassword;
    case FPDF_ERR_SECURITY:
        return PdfDocument::Error::UnsupportedSecurityScheme;
    default:
        return PdfDocument::Error::Unknown;
    }
}

/// A text PDFium writes in UTF-16LE with its terminator: @p read(buffer, bytes) writes it
/// and gives its bytes
template <typename Read>
QString utf16Text(const Read& read) {
    const unsigned long bytes = read(nullptr, 0);
    if (bytes <= sizeof(char16_t) || bytes > MAX_TEXT_BYTES) {
        return {};
    }
    std::vector<char16_t> buffer((bytes / sizeof(char16_t)) + 1, u'\0');
    if (read(buffer.data(), static_cast<unsigned long>(buffer.size() * sizeof(char16_t))) !=
        bytes) {
        return {};
    }
    const auto end = std::find(buffer.cbegin(), buffer.cend(), u'\0');
    return QString::fromUtf16(buffer.data(), std::distance(buffer.cbegin(), end));
}

/// The tag of @p field in the data of a PDF file
const char* metaDataTag(PdfDocument::MetaDataField field) {
    switch (field) {
    case PdfDocument::MetaDataField::Title:
        return "Title";
    case PdfDocument::MetaDataField::Author:
        return "Author";
    case PdfDocument::MetaDataField::Subject:
        return "Subject";
    case PdfDocument::MetaDataField::CreationDate:
        return "CreationDate";
    }
    return "Title";
}

/// The page an entry of the contents goes to; -1: none
int destinationPage(FPDF_DOCUMENT document, FPDF_BOOKMARK bookmark) {
    FPDF_DEST destination = FPDFBookmark_GetDest(document, bookmark);
    if (destination == nullptr) {
        if (FPDF_ACTION action = FPDFBookmark_GetAction(bookmark);
            action != nullptr && FPDFAction_GetType(action) == PDFACTION_GOTO) {
            destination = FPDFAction_GetDest(document, action);
        }
    }
    if (destination == nullptr) {
        return -1;
    }
    const int page = FPDFDest_GetDestPageIndex(document, destination);
    return page >= 0 && page < FPDF_GetPageCount(document) ? page : -1;
}

/// A space, a tab or a line break
const QRegularExpression& space() {
    static const QRegularExpression expression(QStringLiteral("\\s"));
    return expression;
}

/// @p text on one line: its line breaks and runs of spaces as one space
QString oneLine(QString text) {
    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    return text.replace(spaces, QStringLiteral(" "));
}

}  // namespace

// --- PdfSelection ------------------------------------------------------------------------

PdfSelection::PdfSelection(QString text, QList<QPolygonF> bounds, int startIndex, int endIndex)
    : m_text(std::move(text)),
      m_bounds(std::move(bounds)),
      m_startIndex(startIndex),
      m_endIndex(endIndex) {
    for (const QPolygonF& polygon : std::as_const(m_bounds)) {
        m_boundingRectangle = m_boundingRectangle.isNull()
                                  ? polygon.boundingRect()
                                  : m_boundingRectangle.united(polygon.boundingRect());
    }
}

// --- PdfDocument -------------------------------------------------------------------------

/// A page loaded, with what it takes to tell its places as they are shown
struct PdfDocument::Page {
    int index = -1;
    ScopedFPDFPage page;
    ScopedFPDFTextPage text;  ///< Loaded when first asked for (textPage())
    QTransform toView;        ///< From the space of the page to points as it is shown
    QTransform toPage;        ///< Back
    bool measured = false;    ///< The page has a size: its places can be told

    /// The text of the page; nullptr: it cannot be read
    FPDF_TEXTPAGE textPage() {
        if (!text && measured) {
            text.reset(FPDFText_LoadPage(page.get()));
        }
        return text.get();
    }

    /// How many characters the text of the page has
    int characterCount() {
        FPDF_TEXTPAGE loaded = textPage();
        return loaded != nullptr ? std::max(0, FPDFText_CountChars(loaded)) : 0;
    }

    /// The character at @p place (in points as shown); negative: none
    int characterAt(const QPointF& place) {
        const QPointF at = toPage.map(place);
        return FPDFText_GetCharIndexAtPos(textPage(), at.x(), at.y(), LETTER_TOLERANCE,
                                          LETTER_TOLERANCE);
    }

    /// Characters @p start to @p start + @p count (excluded)
    QString textOf(int start, int count) {
        FPDF_TEXTPAGE loaded = textPage();
        if (loaded == nullptr || count <= 0 || start < 0) {
            return {};
        }
        std::vector<unsigned short> buffer(static_cast<std::size_t>(count) + 1, 0);
        // The characters written, with the terminator
        const int written = FPDFText_GetText(loaded, start, count, buffer.data());
        QString result;
        result.reserve(std::max(0, written - 1));
        for (int i = 0; i + 1 < written; ++i) {
            result.append(QChar(buffer.at(static_cast<std::size_t>(i))));
        }
        return result;
    }

    /// Where characters @p start to @p start + @p count (excluded) are, a rectangle a line, in
    /// points as shown
    QList<QRectF> rectanglesOf(int start, int count) {
        FPDF_TEXTPAGE loaded = textPage();
        QList<QRectF> result;
        if (loaded == nullptr || count <= 0 || start < 0) {
            return result;
        }
        const int rectangles = FPDFText_CountRects(loaded, start, count);
        for (int i = 0; i < rectangles; ++i) {
            double left = 0.0;
            double top = 0.0;
            double right = 0.0;
            double bottom = 0.0;
            if (FPDFText_GetRect(loaded, i, &left, &top, &right, &bottom) != 0) {
                const QRectF onPage(QPointF(left, bottom), QPointF(right, top));
                result.append(toView.mapRect(onPage.normalized()));
            }
        }
        return result;
    }

    /// Characters @p start to @p start + @p count (excluded) with their places
    PdfSelection selectionOf(int start, int count) {
        QString words = textOf(start, count);
        if (words.isEmpty()) {
            return {};
        }
        const auto length = static_cast<int>(words.size());
        QList<QPolygonF> bounds;
        for (const QRectF& rectangle : rectanglesOf(start, length)) {
            bounds.append(QPolygonF(rectangle));
        }
        return {std::move(words), std::move(bounds), start, start + length};
    }

    /// The words of the page around characters @p start to @p end (excluded), at most
    /// CONTEXT_LENGTH characters on each side, without a word cut at the far ends
    std::pair<QString, QString> contextOf(int start, int end) {
        FPDF_TEXTPAGE loaded = textPage();
        const int count = characterCount();
        const auto isSpace = [loaded](int index) {
            return QChar::isSpace(static_cast<char32_t>(FPDFText_GetUnicode(loaded, index)));
        };
        const int from = std::max(0, start - CONTEXT_LENGTH);
        QString before = textOf(from, start - from);
        if (from > 0 && !isSpace(from - 1)) {
            const qsizetype cut = before.indexOf(space());
            before = cut >= 0 ? before.mid(cut) : QString();
        }
        const int to = std::min(count, end + CONTEXT_LENGTH);
        QString after = textOf(end, to - end);
        if (to < count && !isSpace(to)) {
            const qsizetype cut = after.lastIndexOf(space());
            after = cut >= 0 ? after.left(cut + 1) : QString();
        }
        before = oneLine(std::move(before));
        after = oneLine(std::move(after));
        if (before.startsWith(QLatin1Char(' '))) {
            before.remove(0, 1);
        }
        if (after.endsWith(QLatin1Char(' '))) {
            after.chop(1);
        }
        return {std::move(before), std::move(after)};
    }
};

struct PdfDocument::Private {
    QFile file;
    FPDF_FILEACCESS access{};
    ScopedFPDFDocument document;  ///< Closed after its page
    std::unique_ptr<Page> page;   ///< The page loaded last
};

PdfDocument::PdfDocument(QObject* parent) : QObject(parent), d(std::make_unique<Private>()) {}

PdfDocument::~PdfDocument() {
    close();
}

PdfDocument::Error PdfDocument::load(const QString& path) {
    const auto lock = lockPdfium();
    close();
    if (path.isEmpty()) {
        return Error::FileNotFound;
    }
    d->file.setFileName(path);
    if (!d->file.open(QIODevice::ReadOnly)) {
        return Error::FileNotFound;
    }
    const qint64 size = d->file.size();
    // PDFium reads files of at most ULONG_MAX bytes (4 GB on Windows)
    if (size <= 0 ||
        static_cast<quint64>(size) > std::numeric_limits<unsigned long>::max()) {
        d->file.close();
        return size <= 0 ? Error::InvalidFileFormat : Error::Unknown;
    }
    d->access.m_FileLen = static_cast<unsigned long>(size);
    d->access.m_GetBlock = &readBlock;
    d->access.m_Param = &d->file;
    d->document.reset(FPDF_LoadCustomDocument(&d->access, nullptr));
    if (!d->document) {
        const Error error = lastError();
        d->file.close();
        return error;
    }
    return Error::None;
}

void PdfDocument::close() {
    const auto lock = lockPdfium();
    d->page.reset();
    d->document.reset();
    d->access = FPDF_FILEACCESS{};
    d->file.close();
}

bool PdfDocument::isOpen() const {
    const auto lock = lockPdfium();
    return static_cast<bool>(d->document);
}

int PdfDocument::pageCount() const {
    const auto lock = lockPdfium();
    return d->document ? std::max(0, FPDF_GetPageCount(d->document.get())) : 0;
}

QSizeF PdfDocument::pagePointSize(int page) const {
    const auto lock = lockPdfium();
    FS_SIZEF size{};
    if (!d->document || page < 0 ||
        FPDF_GetPageSizeByIndexF(d->document.get(), page, &size) == 0) {
        return {};
    }
    return {size.width, size.height};
}

QString PdfDocument::pageLabel(int page) const {
    const auto lock = lockPdfium();
    if (!d->document || page < 0 || page >= FPDF_GetPageCount(d->document.get())) {
        return {};
    }
    FPDF_DOCUMENT document = d->document.get();
    return utf16Text([document, page](void* buffer, unsigned long length) {
        return FPDF_GetPageLabel(document, page, buffer, length);
    });
}

QString PdfDocument::metaData(MetaDataField field) const {
    const auto lock = lockPdfium();
    if (!d->document) {
        return {};
    }
    FPDF_DOCUMENT document = d->document.get();
    const char* tag = metaDataTag(field);
    return utf16Text([document, tag](void* buffer, unsigned long length) {
        return FPDF_GetMetaText(document, tag, buffer, length);
    });
}

std::vector<PdfContentsEntry> PdfDocument::contents() const {
    const auto lock = lockPdfium();
    std::vector<PdfContentsEntry> entries;
    if (!d->document) {
        return entries;
    }
    FPDF_DOCUMENT document = d->document.get();
    // In the order of the table: an entry, the entries under it, then the next one. A file
    // may lead an entry back to one before it: each is taken once.
    struct Next {
        FPDF_BOOKMARK bookmark;
        int level;
    };
    std::vector<Next> next{{FPDFBookmark_GetFirstChild(document, nullptr), 0}};
    std::set<FPDF_BOOKMARK> taken;
    while (!next.empty() && entries.size() < static_cast<std::size_t>(MAX_CONTENTS)) {
        const Next each = next.back();
        next.pop_back();
        if (each.bookmark == nullptr || !taken.insert(each.bookmark).second) {
            continue;
        }
        PdfContentsEntry entry;
        FPDF_BOOKMARK bookmark = each.bookmark;
        entry.title = utf16Text([bookmark](void* buffer, unsigned long length) {
            return FPDFBookmark_GetTitle(bookmark, buffer, length);
        });
        entry.page = destinationPage(document, bookmark);
        entry.level = each.level;
        entries.push_back(std::move(entry));
        next.push_back({FPDFBookmark_GetNextSibling(document, bookmark), each.level});
        if (each.level + 1 < MAX_CONTENTS_LEVELS) {
            next.push_back({FPDFBookmark_GetFirstChild(document, bookmark), each.level + 1});
        }
    }
    return entries;
}

QImage PdfDocument::render(int page, const QSize& size) const {
    return render(page, size, QRect(QPoint(0, 0), size));
}

QImage PdfDocument::render(int page, const QSize& size, const QRect& part) const {
    if (size.isEmpty() || part.isEmpty()) {
        return {};
    }
    const auto lock = lockPdfium();
    Page* loaded = loadPage(page);
    if (loaded == nullptr) {
        return {};
    }
    QImage image(part.size(), QImage::Format_ARGB32);
    if (image.isNull()) {
        return image;
    }
    // A PDF page is drawn on white paper
    image.fill(Qt::white);
    const ScopedFPDFBitmap bitmap(FPDFBitmap_CreateEx(image.width(), image.height(),
                                                      FPDFBitmap_BGRA, image.bits(),
                                                      static_cast<int>(image.bytesPerLine())));
    if (!bitmap) {
        return {};
    }
    // The page at its full size, moved so that the part is in the image
    FPDF_RenderPageBitmap(bitmap.get(), loaded->page.get(), -part.x(), -part.y(), size.width(),
                          size.height(), 0, FPDF_ANNOT);
    return image;
}

PdfSelection PdfDocument::getSelection(int page, const QPointF& start, const QPointF& end) const {
    const auto lock = lockPdfium();
    Page* loaded = loadPage(page);
    if (loaded == nullptr || loaded->textPage() == nullptr) {
        return {};
    }
    int first = loaded->characterAt(start);
    int last = loaded->characterAt(end);
    if (first < 0 || last < 0 || first == last) {
        return {};
    }
    QPointF lastPlace = end;
    if (first > last) {
        std::swap(first, last);
        lastPlace = start;
    }
    // The letter at the far end is in when the place is past its middle
    double left = 0.0;
    double right = 0.0;
    double bottom = 0.0;
    double top = 0.0;
    const double x = loaded->toPage.map(lastPlace).x();
    if (FPDFText_GetCharBox(loaded->textPage(), last, &left, &right, &bottom, &top) != 0 &&
        std::abs(right - x) < std::abs(left - x)) {
        ++last;
    }
    return loaded->selectionOf(first, last - first);
}

PdfSelection PdfDocument::getSelectionAtIndex(int page, int startIndex, int length) const {
    if (startIndex < 0 || length <= 0) {
        return {};
    }
    const auto lock = lockPdfium();
    Page* loaded = loadPage(page);
    if (loaded == nullptr) {
        return {};
    }
    const int count = loaded->characterCount();
    if (startIndex >= count) {
        return {};
    }
    return loaded->selectionOf(startIndex, std::min(length, count - startIndex));
}

PdfSelection PdfDocument::getAllText(int page) const {
    const auto lock = lockPdfium();
    Page* loaded = loadPage(page);
    if (loaded == nullptr) {
        return {};
    }
    return loaded->selectionOf(0, loaded->characterCount());
}

QList<PdfHit> PdfDocument::find(int page, const QString& text) const {
    QList<PdfHit> hits;
    if (text.isEmpty()) {
        return hits;
    }
    const auto lock = lockPdfium();
    Page* loaded = loadPage(page);
    if (loaded == nullptr || loaded->textPage() == nullptr) {
        return hits;
    }
    // Flags 0: in any case, also within words
    const ScopedFPDFTextFind search(FPDFText_FindStart(loaded->textPage(), text.utf16(), 0, 0));
    if (!search) {
        return hits;
    }
    while (FPDFText_FindNext(search.get()) != 0) {
        PdfHit hit;
        hit.page = page;
        hit.startIndex = FPDFText_GetSchResultIndex(search.get());
        hit.length = FPDFText_GetSchCount(search.get());
        hit.rectangles = loaded->rectanglesOf(hit.startIndex, hit.length);
        if (hit.rectangles.isEmpty()) {
            continue;
        }
        std::tie(hit.contextBefore, hit.contextAfter) =
            loaded->contextOf(hit.startIndex, hit.startIndex + hit.length);
        hits.append(std::move(hit));
    }
    return hits;
}

PdfDocument::Page* PdfDocument::loadPage(int page) const {
    if (!d->document || page < 0 || page >= FPDF_GetPageCount(d->document.get())) {
        return nullptr;
    }
    if (d->page && d->page->index == page) {
        return d->page.get();
    }
    d->page.reset();
    auto loaded = std::make_unique<Page>();
    loaded->index = page;
    loaded->page.reset(FPDF_LoadPage(d->document.get(), page));
    if (!loaded->page) {
        return nullptr;
    }
    // The space of the page as it is shown: FPDF_PageToDevice() tells whole pixels, so the
    // page is measured large
    const float width = FPDF_GetPageWidthF(loaded->page.get());
    const float height = FPDF_GetPageHeightF(loaded->page.get());
    if (width > 0.0F && height > 0.0F) {
        const int pixelsWide = std::max(1, static_cast<int>(std::lround(width * MEASURE_SCALE)));
        const int pixelsHigh = std::max(1, static_cast<int>(std::lround(height * MEASURE_SCALE)));
        const auto measure = [&loaded, pixelsWide, pixelsHigh](double x, double y) {
            int deviceX = 0;
            int deviceY = 0;
            FPDF_PageToDevice(loaded->page.get(), 0, 0, pixelsWide, pixelsHigh, 0, x, y,
                              &deviceX, &deviceY);
            return QPointF(static_cast<double>(deviceX) / MEASURE_SCALE,
                           static_cast<double>(deviceY) / MEASURE_SCALE);
        };
        const QPointF origin = measure(0.0, 0.0);
        const QPointF alongX = (measure(MEASURE_SPAN, 0.0) - origin) / MEASURE_SPAN;
        const QPointF alongY = (measure(0.0, MEASURE_SPAN) - origin) / MEASURE_SPAN;
        loaded->toView = QTransform(alongX.x(), alongX.y(), alongY.x(), alongY.y(), origin.x(),
                                    origin.y());
        loaded->toPage = loaded->toView.inverted(&loaded->measured);
    }
    d->page = std::move(loaded);
    return d->page.get();
}

}  // namespace kalahari::gui
