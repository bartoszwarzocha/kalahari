// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file pdf_document.h
/// @brief A PDF file read by PDFium: its pages drawn, their text, its data and its contents

#ifndef KALAHARI_GUI_PDF_DOCUMENT_H
#define KALAHARI_GUI_PDF_DOCUMENT_H

#include <QImage>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>

#include <memory>
#include <vector>

namespace kalahari::gui {

/// @brief Text of a page of a PDF file and where it is
///
/// The places are in points of the page as it is shown: from its top left corner, with the
/// page turned as the file says (the coordinates of PdfDocument).
class PdfSelection {
public:
    /// @brief No text
    PdfSelection() = default;

    /// @brief Text @p text at @p bounds, the characters @p startIndex to @p endIndex
    /// (excluded) of the text of its page
    PdfSelection(QString text, QList<QPolygonF> bounds, int startIndex, int endIndex);

    /// @brief Whether it has a place on the page
    [[nodiscard]] bool isValid() const { return !m_bounds.isEmpty(); }

    /// @brief The text, as the file has it
    [[nodiscard]] const QString& text() const { return m_text; }

    /// @brief Where its lines are, a polygon each
    [[nodiscard]] const QList<QPolygonF>& bounds() const { return m_bounds; }

    /// @brief The smallest rectangle around all of it
    [[nodiscard]] QRectF boundingRectangle() const { return m_boundingRectangle; }

    /// @brief Its first character in the text of the page, from 0
    [[nodiscard]] int startIndex() const { return m_startIndex; }

    /// @brief The character after its last one in the text of the page
    [[nodiscard]] int endIndex() const { return m_endIndex; }

private:
    QString m_text;
    QList<QPolygonF> m_bounds;
    QRectF m_boundingRectangle;
    int m_startIndex = 0;
    int m_endIndex = 0;
};

/// @brief A text found on a page of a PDF file
struct PdfHit {
    int page = -1;              ///< The page, from 0
    int startIndex = 0;         ///< Its first character in the text of the page
    int length = 0;             ///< How many characters it has
    QList<QRectF> rectangles;   ///< Where its letters are, a rectangle a line
    QString contextBefore;      ///< The words before it on the page, on one line
    QString contextAfter;       ///< The words after it on the page, on one line
};

/// @brief An entry of the table of contents of a PDF file (its outline)
///
/// PdfDocument::contents() lists the entries in the order of the table: each one is followed
/// by the entries under it, a level deeper.
struct PdfContentsEntry {
    QString title;  ///< As the file has it
    int page = -1;  ///< The page it goes to, from 0; -1: none
    int level = 0;  ///< How deep it is: 0 at the top
};

/// @brief A PDF file read by PDFium, for the preview of PDF files and the cards of materials
///
/// The pages are drawn on white paper with the comments of the file (its annotations). The
/// places on a page are in points (1/72 inch) from its top left corner as it is shown, turned
/// as the file says. PDFium is built without JavaScript and XFA, so a file runs no code.
///
/// PDFium serves one thread at a time: every call takes a lock shared by all documents.
class PdfDocument : public QObject {
    Q_OBJECT

public:
    /// @brief Why a file did not load
    enum class Error {
        None,                      ///< It loaded
        Unknown,                   ///< Another reason
        FileNotFound,              ///< No file, or it cannot be opened
        InvalidFileFormat,         ///< Not a PDF file, or damaged
        IncorrectPassword,         ///< It needs a password
        UnsupportedSecurityScheme  ///< It is protected in a way PDFium does not read
    };

    /// @brief The data of the file (its document information)
    enum class MetaDataField {
        Title,        ///< Its title
        Author,       ///< Its author or authors
        Subject,      ///< Its subject
        CreationDate  ///< When it was made, as the file has it ("D:20190301120000Z")
    };

    /// @brief How many characters of a hit's words around it (PdfHit::contextBefore)
    static constexpr int CONTEXT_LENGTH = 64;

    /// @brief No file
    explicit PdfDocument(QObject* parent = nullptr);

    /// @brief Closes the file
    ~PdfDocument() override;

    PdfDocument(const PdfDocument&) = delete;
    PdfDocument& operator=(const PdfDocument&) = delete;
    PdfDocument(PdfDocument&&) = delete;
    PdfDocument& operator=(PdfDocument&&) = delete;

    /// @brief Read PDF file @p path, closing the one read before
    /// @return Error::None, or why it did not load (then no file is open)
    Error load(const QString& path);

    /// @brief Close the file
    void close();

    /// @brief Whether a file is open
    [[nodiscard]] bool isOpen() const;

    /// @brief How many pages the file has; 0 without one
    [[nodiscard]] int pageCount() const;

    /// @brief The size of page @p page (from 0) in points, as it is shown; empty: no such page
    [[nodiscard]] QSizeF pagePointSize(int page) const;

    /// @brief The label of page @p page as the file names it ("xii" in a preface); empty when
    /// the file does not name its pages
    [[nodiscard]] QString pageLabel(int page) const;

    /// @brief @p field of the data of the file, as it has it; empty: none
    [[nodiscard]] QString metaData(MetaDataField field) const;

    /// @brief How many entries of the table of contents contents() gives at most
    static constexpr int MAX_CONTENTS = 10000;

    /// @brief How deep the table of contents goes in contents(): its deeper entries are left
    /// out
    static constexpr int MAX_CONTENTS_LEVELS = 32;

    /// @brief The table of contents of the file, in its order (PdfContentsEntry); empty: none
    [[nodiscard]] std::vector<PdfContentsEntry> contents() const;

    /// @brief Page @p page drawn at @p size pixels; a null image when it cannot be drawn
    [[nodiscard]] QImage render(int page, const QSize& size) const;

    /// @brief The part @p part (in pixels) of page @p page drawn at @p size pixels; a null image
    /// when it cannot be drawn
    [[nodiscard]] QImage render(int page, const QSize& size, const QRect& part) const;

    /// @brief The text of page @p page from the letter at @p start to the one at @p end (in
    /// points); empty when there is no letter at both places
    [[nodiscard]] PdfSelection getSelection(int page, const QPointF& start,
                                            const QPointF& end) const;

    /// @brief The text of page @p page from its character @p startIndex, at most @p length
    /// characters
    [[nodiscard]] PdfSelection getSelectionAtIndex(int page, int startIndex, int length) const;

    /// @brief All the text of page @p page
    [[nodiscard]] PdfSelection getAllText(int page) const;

    /// @brief Where page @p page has @p text, in any case
    [[nodiscard]] QList<PdfHit> find(int page, const QString& text) const;

private:
    struct Page;
    struct Private;

    /// @brief Page @p page loaded (the last one loaded is kept); nullptr: no such page
    [[nodiscard]] Page* loadPage(int page) const;

    std::unique_ptr<Private> d;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PDF_DOCUMENT_H
