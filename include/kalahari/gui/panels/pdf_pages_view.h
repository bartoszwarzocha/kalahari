// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_pages_view.h
/// @brief The pages of a PDF file as the preview shows them: one under another, one at a time
/// or two side by side, with the selected text and the search hits

#ifndef KALAHARI_GUI_PANELS_PDF_PAGES_VIEW_H
#define KALAHARI_GUI_PANELS_PDF_PAGES_VIEW_H

#include "kalahari/gui/copy_target.h"

#include <QAbstractScrollArea>
#include <QCache>
#include <QColor>
#include <QHash>
#include <QImage>
#include <QList>
#include <QPdfDocument>
#include <QPdfSearchModel>
#include <QPdfSelection>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QSizeF>
#include <QVector>

#include <functional>
#include <optional>

class QPainter;

namespace kalahari::gui {

/// @brief The pages of a PDF file, drawn by the program (QPdfDocument::render())
///
/// QPdfView of Qt 6.4 selects no text, so the view draws the pages itself: it selects the text
/// of a page with the mouse (QPdfDocument::getSelection()), draws the hits of a search
/// (QPdfSearchModel), shows the pages darker or with their colors turned in a dark theme, and
/// two pages side by side as in a book: the first alone, then the even pages on the left.
///
/// Keys: PgUp and PgDn the previous and the next page, Home and End the first and the last
/// one, the arrows move the pages, Ctrl with the wheel zooms, Enter asks for the card of the
/// material (activated()), Esc takes the selection away, the menu key and Shift+F10 ask for
/// the menu of the selection. Edit > Copy and Select All work on it (CopyTarget).
class PdfPagesView : public QAbstractScrollArea, public CopyTarget {
    Q_OBJECT

public:
    /// @brief How the pages are laid out
    enum class Layout {
        Continuous,  ///< One under another
        Single,      ///< One page at a time
        Two          ///< Two side by side, one spread at a time
    };

    /// @brief How the pages look
    enum class Look {
        AsFile,   ///< As in the file
        Dimmed,   ///< Darker
        Inverted  ///< Their colors turned: light text on a dark page
    };

    /// @brief What the zoom follows
    enum class Fit {
        None,       ///< The zoom set
        PageWidth,  ///< The width of the page in the window
        WholePage   ///< The whole page in the window
    };

    /// @brief The colors it draws besides the pages; the selection and the hits are drawn
    /// over the text, so they need to let it through
    struct Colors {
        QColor desk;                ///< Around the pages
        QColor selection;           ///< The selected text
        QColor searchMatch;         ///< The hits of the search
        QColor currentSearchMatch;  ///< The hit gone to
    };

    /// @brief The zooms of Zoom In and Zoom Out, from the smallest
    [[nodiscard]] static const QList<double>& zoomSteps();

    /// @brief The view without a document: no pages
    explicit PdfPagesView(QWidget* parent = nullptr);

    /// @brief Show the pages of @p document (not owned), from the first; nullptr: none
    void setDocument(QPdfDocument* document);

    /// @brief The file it shows
    [[nodiscard]] QPdfDocument* document() const { return m_document; }

    /// @brief Draw the hits of @p model (not owned); nullptr: none
    void setSearchModel(QPdfSearchModel* model);

    /// @brief How many pages the file has
    [[nodiscard]] int pageCount() const;

    /// @brief The page shown, from 0: the one in the middle of the window
    [[nodiscard]] int currentPage() const { return m_page; }

    /// @brief Show page @p page (from 0) from its top
    void setCurrentPage(int page);

    /// @brief Show page @p page (from 0) with the place @p points (in points of the page)
    /// in the window
    void showPlace(int page, const QRectF& points);

    /// @brief How the pages are laid out
    [[nodiscard]] Layout pageLayout() const { return m_layout; }

    /// @brief Lay out the pages as @p layout says, at the page shown
    void setPageLayout(Layout layout);

    /// @brief Zoom: 1 is 100%, the page as large as printed
    [[nodiscard]] double zoom() const { return m_zoom; }

    /// @brief What the zoom follows
    [[nodiscard]] Fit fit() const { return m_fit; }

    /// @brief Zoom @p zoom (within zoomSteps()), which follows no longer the window
    void setZoom(double zoom);

    /// @brief Follow the window as @p fit says
    void setFit(Fit fit);

    /// @brief The next larger zoom of zoomSteps()
    void zoomIn();

    /// @brief The next smaller zoom of zoomSteps()
    void zoomOut();

    /// @brief How the pages look
    [[nodiscard]] Look look() const { return m_look; }

    /// @brief Show the pages as @p look says
    void setLook(Look look);

    /// @brief Draw with @p colors
    void setColors(const Colors& colors);

    /// @brief The colors it draws with
    [[nodiscard]] const Colors& colors() const { return m_colors; }

    /// @brief Whether some text is selected
    [[nodiscard]] bool hasSelection() const;

    /// @brief The selected text; empty: none
    [[nodiscard]] QString selectedText() const;

    /// @brief The page of the selection, from 0; -1: none
    [[nodiscard]] int selectionPage() const { return m_selection ? m_selectionPage : -1; }

    /// @brief Select the text of page @p page between @p start and @p end (in points)
    void select(int page, const QPointF& start, const QPointF& end);

    /// @brief Select the text of a hit of the search: its @p rectangles on page @p page
    void selectHit(int page, const QList<QRectF>& rectangles);

    /// @brief Select all the text of page @p page
    void selectPage(int page);

    /// @brief Take the selection away
    void clearSelection();

    /// @brief The selection in the coordinates of the viewport; empty: none, or not shown
    [[nodiscard]] QRect selectionRect() const;

    /// @brief Draw the hit gone to: its @p rectangles on page @p page; -1: none
    void setCurrentHit(int page, const QList<QRectF>& rectangles);

    /// @brief Page @p page (from 0) in the coordinates of the viewport; empty: not laid out
    [[nodiscard]] QRect pageRect(int page) const;

    /// @brief The page at @p position of the viewport, from 0; -1: none
    [[nodiscard]] int pageAt(const QPoint& position) const;

    // CopyTarget
    [[nodiscard]] bool canCopyHere() const override;
    void copyHere() override;
    [[nodiscard]] bool canSelectAllHere() const override;
    void selectAllHere() override;
    QMetaObject::Connection whenSelectionChanges(QObject* context,
                                                 std::function<void()> update) override;

signals:
    /// @brief Another page is shown: @p page, from 0
    void currentPageChanged(int page);

    /// @brief The zoom changed, also by itself with the window
    void zoomChanged(double zoom);

    /// @brief The selection changed
    void selectionChanged();

    /// @brief The mouse let go of a selection: the bar of the selection can show
    void selectionMade();

    /// @brief The menu of the selection was asked for at @p globalPosition
    void selectionMenuRequested(const QPoint& globalPosition);

    /// @brief Enter: the card of the material
    void activated();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    /// @brief Lay out the pages anew, keeping the place shown
    void relayout();

    /// @brief The place shown: how far down the page shown the window begins, in parts of its
    /// height; 0 when the pages show one row at a time
    [[nodiscard]] double shownDown() const;

    /// @brief The rows laid out: all of them, or the row of the page shown
    [[nodiscard]] QList<QList<int>> shownRows() const;

    /// @brief Lay out the pages of @p shownRows at m_zoom: m_rects and m_size
    void placePages(const QList<QList<int>>& shownRows);

    /// @brief Draw over page @p page, at @p rect of the viewport, the hits of the search, the
    /// hit gone to and the selection
    void paintMarks(QPainter& painter, int page, const QRect& rect) const;

    /// @brief One page or one spread at a time: past the end of the row, the wheel turned by
    /// @p delta goes to the next row; false when it scrolls the row
    bool wheelToNextRow(int delta);

    /// @brief The keys of the pages: @p key with @p modifiers (without the one of the keypad)
    /// @return false when the pages do not use it
    bool handleKey(int key, Qt::KeyboardModifiers modifiers);

    /// @brief Page Down, Page Up, Home and End go to another row, the first or the last page
    /// @return false for another key
    bool moveKey(int key);

    /// @brief Where the menu of the selection opens from the keys, on the screen: under the
    /// selection, or in the middle of the pages without one
    [[nodiscard]] QPoint selectionMenuPlace() const;

    /// @brief The pages in each row, from the top: one, or two side by side
    [[nodiscard]] QList<QList<int>> rows() const;

    /// @brief The row of page @p page in rows()
    [[nodiscard]] int rowOf(int page) const;

    /// @brief Pixels of the window for a point of the page at zoom 1
    [[nodiscard]] double pixelsAtFullSize() const;

    /// @brief The zoom at which the pages of @p shownRows fit the window as m_fit says
    [[nodiscard]] double fittedZoom(const QList<QList<int>>& shownRows) const;

    /// @brief Where the pages begin in the viewport: the scroll, or the room to center them
    [[nodiscard]] QPoint origin() const;

    /// @brief @p points on page @p page in the coordinates of the viewport
    [[nodiscard]] QRectF toViewport(int page, const QRectF& points) const;

    /// @brief Position @p position of the viewport on page @p page, in points, within the page
    [[nodiscard]] QPointF toPoints(int page, const QPoint& position) const;

    /// @brief Page @p page drawn at @p size pixels (the part @p visible of it for a page too
    /// large to keep), as m_look says
    QImage pageImage(int page, const QSize& size, const QRect& visible);

    /// @brief Draw the page as m_look says
    void applyLook(QImage& image) const;

    /// @brief The text of page @p page from @p start to @p end (in points), also from a place
    /// beside the text, as in the margin: the places go to the text, and toward each other
    /// until each is at a letter; none: no text between them
    [[nodiscard]] std::optional<QPdfSelection> textBetween(int page, const QPointF& start,
                                                           const QPointF& end);

    /// @brief Where the text of page @p page is, in points; empty: it has none
    [[nodiscard]] QRectF textBounds(int page);

    /// @brief The page in the middle of the window becomes the page shown
    void followScroll();

    /// @brief Set the page shown and say so
    void setPage(int page);

    /// @brief Go @p count rows down (up for a negative one), at the top of the page
    void moveRows(int count);

    QPointer<QPdfDocument> m_document;
    QPointer<QPdfSearchModel> m_search;
    QVector<QSizeF> m_pointSizes;  ///< The size of each page in points
    QVector<QRect> m_rects;        ///< Each page laid out, in pixels of the pages; empty: not
    QSize m_size;                  ///< All the pages laid out with the room around them
    int m_page = 0;
    Layout m_layout = Layout::Continuous;
    Fit m_fit = Fit::PageWidth;
    double m_zoom = 1.0;
    Look m_look = Look::AsFile;
    Colors m_colors;
    QCache<int, QImage> m_images;  ///< The pages drawn, by page
    QHash<int, QRectF> m_textBounds;  ///< textBounds() of the pages asked for
    std::optional<QPdfSelection> m_selection;
    int m_selectionPage = -1;
    int m_hitPage = -1;
    QList<QRectF> m_hitRectangles;
    int m_dragPage = -1;  ///< The page the mouse selects on; -1: none
    QPointF m_dragStart;  ///< Where it was pressed, in points
    bool m_scrolling = false;  ///< The view itself scrolls to a page: the page stays
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PDF_PAGES_VIEW_H
