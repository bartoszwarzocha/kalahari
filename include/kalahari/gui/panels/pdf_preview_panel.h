// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_preview_panel.h
/// @brief The preview of the PDF file of a material, on a tab in the center of the window

#ifndef KALAHARI_GUI_PANELS_PDF_PREVIEW_PANEL_H
#define KALAHARI_GUI_PANELS_PDF_PREVIEW_PANEL_H

#include "kalahari/gui/panels/pdf_pages_view.h"
#include "kalahari/gui/panels/preview_panel.h"

#include <QColor>
#include <QString>

#include <functional>

class QAction;
class QComboBox;
class QFrame;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QListView;
class QListWidget;
class QPdfBookmarkModel;
class QPdfDocument;
class QPdfSearchModel;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QMenu;
class QToolBar;
class QToolButton;
class QTreeView;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari::core {
class ProjectManager;
struct PageBookmark;
struct PdfHighlight;
}  // namespace kalahari::core

namespace kalahari::gui {

class ColorMenu;

/// @brief The preview of the PDF file of a material, on a tab beside the chapters
///
/// The bar: the side panel, the previous and the next page, the number of the page (Ctrl+G;
/// as the file numbers its pages, "xii" in a preface), the zoom (also View > Zoom and Ctrl
/// with the wheel), the layout of the pages, the search in the file (Ctrl+F, Enter and
/// Shift+Enter the next and the previous hit, Esc back to the pages), the card of the
/// material, the system program and the settings (Settings > Materials).
///
/// The side panel shows the bookmarks of the material (Insert adds one at the page shown,
/// F2 changes its note, Delete takes it away, asking first when it has a note), the contents
/// of the file, the pages small and the hits of the search with the text around them; Enter
/// goes to the page. F6 and Shift+F6 go between the bar, the side panel and the pages.
///
/// When the Settings allow it (PdfFiles::HIGHLIGHTS_SETTING), text selected is highlighted in
/// a color, which the book keeps with a note: the side panel lists the highlights next to the
/// bookmarks (Enter goes to the text, F2 changes the note, Delete takes it away), and the menu
/// of a highlight changes its color or its note. The PDF file stays as it is.
///
/// Text selected on a page is copied (Edit > Copy), copied with the reference to the source
/// ("(Wiśniewska 2011, s. 45)"), made a quote of its page and a bookmark; the bar over the
/// selection and its menu (the menu key, Shift+F10) have these commands. A hit gone to is
/// selected, so all of them work from the keyboard.
///
/// It opens at the page shown last (Settings > Materials > PDF Preview), which it keeps with
/// the material. The look of the pages in a dark theme, the layout, the zoom and the side
/// panel shown first are settings; the colors are the editor's (Settings > Editor > Colors).
/// A file it cannot read gets a sentence saying why and the button of the system program.
class PdfPreviewPanel : public PreviewPanel {
    Q_OBJECT

public:
    /// @brief The tabs of the side panel
    enum SideTab {
        BookmarksTab,   ///< The bookmarks of the material
        HighlightsTab,  ///< The highlights of the material, when the Settings allow them
        ContentsTab,    ///< The table of contents of the file
        ThumbnailsTab,  ///< The pages small
        ResultsTab      ///< The hits of the search
    };

    /// @brief Create the preview of material @p elementId of the open book
    PdfPreviewPanel(core::ProjectManager& book, const QString& elementId,
                    QWidget* parent = nullptr);

    /// @brief Keeps the page shown with the material
    ~PdfPreviewPanel() override;

    PdfPreviewPanel(const PdfPreviewPanel&) = delete;
    PdfPreviewPanel& operator=(const PdfPreviewPanel&) = delete;
    PdfPreviewPanel(PdfPreviewPanel&&) = delete;
    PdfPreviewPanel& operator=(PdfPreviewPanel&&) = delete;

    /// @brief Whether it shows the file; false with the sentence of why not (problem())
    [[nodiscard]] bool hasDocument() const;

    /// @brief Why it does not show the file; empty when it shows it
    [[nodiscard]] QString problem() const { return m_problem; }

    /// @brief The label of page @p page (from 0) as the file has it, else its number
    [[nodiscard]] QString pageLabel(int page) const;

    // PreviewPanel
    [[nodiscard]] double zoom() const override;
    [[nodiscard]] QString pageStatus() const override;
    [[nodiscard]] bool canFind() const override;
    bool showPage(const QString& page) override;
    void refresh() override;

    /// @brief The text that Copy with Reference copies: the selection with the reference to
    /// its page; empty without a selection
    [[nodiscard]] QString textWithReference() const;

    // The parts, for the tests
    /// @brief The pages
    [[nodiscard]] PdfPagesView* view() const { return m_view; }
    /// @brief The bar over the pages
    [[nodiscard]] QToolBar* toolBar() const { return m_toolBar; }
    /// @brief The field of the page shown
    [[nodiscard]] QLineEdit* pageEdit() const { return m_pageEdit; }
    /// @brief The number of pages after it: "of 120"
    [[nodiscard]] QLabel* pageCountLabel() const { return m_pageCountLabel; }
    /// @brief The box of the zoom
    [[nodiscard]] QComboBox* zoomBox() const { return m_zoomBox; }
    /// @brief The field of the search
    [[nodiscard]] QLineEdit* searchEdit() const { return m_searchEdit; }
    /// @brief The hits found: "3 of 12"
    [[nodiscard]] QLabel* hitsLabel() const { return m_hitsLabel; }
    /// @brief The side panel and its tabs (SideTab)
    [[nodiscard]] QTabWidget* sidePanel() const { return m_side; }
    /// @brief The bookmarks of the material: a page and a note each
    [[nodiscard]] QTreeWidget* bookmarksList() const { return m_bookmarksList; }
    /// @brief The button under the bookmarks: a bookmark at the page shown
    [[nodiscard]] QPushButton* addBookmarkButton() const { return m_addBookmarkButton; }
    /// @brief The highlights of the material: a page, the text and a note each
    [[nodiscard]] QTreeWidget* highlightsList() const { return m_highlightsList; }
    /// @brief The button under the highlights: the color of the current one
    [[nodiscard]] QPushButton* highlightColorButton() const { return m_highlightColorButton; }
    /// @brief Highlight: the selection in the color chosen last
    [[nodiscard]] QAction* highlightAction() const { return m_highlightAction; }
    /// @brief The colors to highlight the selection in (Highlight in Color)
    [[nodiscard]] ColorMenu* newHighlightColors() const { return m_newHighlightColors; }
    /// @brief The colors of a highlight there is (Highlight Color)
    [[nodiscard]] ColorMenu* highlightColors() const { return m_highlightColors; }
    /// @brief The table of contents of the file
    [[nodiscard]] QTreeView* contentsTree() const { return m_contentsTree; }
    /// @brief The pages small
    [[nodiscard]] QListWidget* thumbnails() const { return m_thumbnails; }
    /// @brief The hits of the search
    [[nodiscard]] QListView* resultsList() const { return m_resultsList; }
    /// @brief The bar over a selection made with the mouse
    [[nodiscard]] QFrame* selectionBar() const { return m_selectionBar; }
    /// @brief The search in the file
    [[nodiscard]] QPdfSearchModel* searchModel() const { return m_search; }
    /// @brief The button that shows and hides the side panel
    [[nodiscard]] QAction* sideAction() const { return m_sideAction; }
    /// @brief The button of the layout @p layout of the pages
    [[nodiscard]] QAction* layoutAction(PdfPagesView::Layout layout) const;
    /// @brief The commands of a selection: Copy, Copy with Reference, Create Quote, Bookmark
    /// Here, Highlight and Select All Text of the Page
    [[nodiscard]] QList<QAction*> selectionActions() const;
    /// @brief The hit gone to, from 0; -1: none
    [[nodiscard]] int currentHit() const { return m_hit; }

public slots:
    void zoomIn() override;
    void zoomOut() override;
    void zoomReset() override;
    void zoomToPageWidth() override;
    void zoomToWholePage() override;

    /// @brief Edit > Find: the field of the search gets the keys, its text selected
    void find() override;

    /// @brief Ctrl+G: the field of the page number gets the keys, its text selected
    void goToPage();

    /// @brief Search the file for @p text at once; the first hit is gone to when it is found
    void search(const QString& text);

    /// @brief Go to the next hit of the search (Enter in its field)
    void nextHit();

    /// @brief Go to the previous hit of the search (Shift+Enter in its field)
    void previousHit();

    /// @brief Go to hit @p index of the search, from 0, and select it
    void goToHit(int index);

    /// @brief Copy the selection with the reference to its page (textWithReference())
    void copyWithReference() const;

    /// @brief Ask for a quote of the selection from its page (newQuoteRequested())
    void createQuote();

    /// @brief Add a bookmark at the page of the selection, with the selection as its note,
    /// or at the page shown without one
    void addBookmark();

    /// @brief Take the current bookmark of the side panel away
    void removeBookmark();

    /// @brief Highlight the selection in the color chosen last
    void highlight();

    /// @brief Highlight the selection in @p color; the text highlighted already gets the color
    void highlightIn(const QColor& color);

    /// @brief Give the highlight of the menu, else the current one of the side panel, color
    /// @p color
    void setHighlightColor(const QColor& color);

    /// @brief Take the highlight of the menu, else the current one of the side panel, away,
    /// asking first when it has a note
    void removeHighlight();

    /// @brief Write the note of the highlight of the menu, else of the current one of the side
    /// panel, in its row of the side panel
    void editHighlightNote();

    /// @brief Read the file again and show it, at the page shown
    void reload();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The bar over the pages
    void buildToolBar();

    /// @brief What the buttons and the fields of the bar over the pages do
    void connectToolBar();

    /// @brief What shows instead of the pages when they cannot be shown: why, and the button
    /// that opens the file in the system program
    [[nodiscard]] QWidget* buildProblemPage();

    /// @brief The side panel and its tabs
    void buildSidePanel();

    /// @brief The lists of the side panel go to their pages, and the notes of the bookmarks
    /// are kept as they are typed
    void connectSidePanel();

    /// @brief Go to the page of the bookmark of @p item
    void goToBookmark(QTreeWidgetItem* item);

    /// @brief Keep the note typed in @p item of the bookmarks; @p column: the column changed
    void saveBookmarkNote(QTreeWidgetItem* item, int column);

    /// @brief The tab of the highlights in the side panel and the colors of the highlights
    void buildHighlightsTab(const std::function<QWidget*(QWidget*, QLabel*, QWidget*)>& page);

    /// @brief The list of the highlights goes to their text, and their notes are kept as they
    /// are typed
    void connectHighlights();

    /// @brief Go to the text of the highlight of @p item
    void goToHighlight(QTreeWidgetItem* item);

    /// @brief Keep the note typed in @p item of the highlights; @p column: the column changed
    void saveHighlightNote(QTreeWidgetItem* item, int column);

    /// @brief The bar over a selection and the commands of the selection
    void buildSelectionCommands();

    /// @brief The commands of the highlights in the menu of the selection: on a highlight its
    /// color, its note and Delete, else Highlight and Highlight in Color
    void addHighlightCommands(QMenu& menu);

    /// @brief The settings of the preview: the layout of the pages; at its opening also the
    /// zoom, the tab of the side panel and the page shown last
    void applySettings(bool opening);

    /// @brief The zoom the preview opens with (Settings > Materials > PDF Preview)
    void applyZoomSetting();

    /// @brief The tab the side panel opens with, or the side panel hidden
    void applySideSetting();

    /// @brief The keys of the preview: @p key on its part @p watched
    /// @return true when the preview used it
    bool handleKey(QObject* watched, QKeyEvent& key);

    /// @brief F6, Shift+F6 and Ctrl+G, over the keys of the window: @p key with @p modifiers
    /// (without the one of the keypad)
    /// @return false for another key
    bool partKey(QKeyEvent& key, Qt::KeyboardModifiers modifiers);

    /// @brief Enter and Shift+Enter in the field of the search go to the next and the previous
    /// hit; Esc to the pages
    /// @return false for another key
    bool searchKey(int key, Qt::KeyboardModifiers modifiers);

    /// @brief Insert, Delete and F2 in the bookmarks: add one, take one away, change its note
    /// @return false for another key
    bool bookmarkKey(int key);

    /// @brief Insert, Delete and F2 in the highlights: highlight the selection, take one away,
    /// change its note
    /// @return false for another key
    bool highlightKey(int key);

    /// @brief The note of a new bookmark at page @p page: the text selected on it, shortened;
    /// empty without one
    [[nodiscard]] QString noteOfSelection(int page) const;

    /// @brief The look of the pages and their colors, for the theme in use
    void applyAppearance();

    /// @brief The page shown changed
    void onPageChanged(int page);

    /// @brief The zoom changed: the box of the zoom shows it
    void showZoom();

    /// @brief The commands follow the selection, the document and a book open read-only
    void updateCommands();

    /// @brief Show the bookmarks of the material
    void fillBookmarks();

    /// @brief Give the material the bookmarks @p bookmarks
    void saveBookmarks(const QList<core::PageBookmark>& bookmarks);

    /// @brief Show the highlights of the material, in the side panel and on the pages, when the
    /// Settings allow them
    void fillHighlights();

    /// @brief Give the material the highlights @p highlights
    void saveHighlights(const QList<core::PdfHighlight>& highlights);

    /// @brief The highlights shown when the Settings allow them, else hidden with their
    /// commands; the book keeps them
    void applyHighlightsSetting();

    /// @brief The highlight the commands of a highlight work on: of the menu, else the current
    /// one of the side panel; -1: none
    [[nodiscard]] int highlightTarget() const;

    /// @brief The color of @p highlight: its own, else the first highlight color
    [[nodiscard]] QColor colorOf(const core::PdfHighlight& highlight) const;

    /// @brief The small pages of the @p count pages, blank until they are drawn
    void fillThumbnails(int count);

    /// @brief Show the pages small, for the pages in the window of the list
    void drawThumbnails();

    /// @brief The hits found: "3 of 12"
    void updateHits();

    /// @brief Show the bar over the selection, or hide it
    void placeSelectionBar(bool show);

    /// @brief Show the menu of the selection at @p globalPosition
    void showSelectionMenu(const QPoint& globalPosition);

    /// @brief Go to the page typed in its field
    void goToTypedPage();

    /// @brief The page (from 0) named @p text: as the file labels its pages, or a number from
    /// 1; -1: none
    [[nodiscard]] int pageNamed(const QString& text) const;

    /// @brief F6 and Shift+F6: the keys go to the next part (or the previous one) of the bar,
    /// the side panel and the pages
    void focusNextPart(bool forward);

    /// @brief Which of @p parts has the keys; -1: none
    [[nodiscard]] qsizetype partWithFocus(const QList<QWidget*>& parts) const;

    /// @brief Keep the page shown and the zoom (Settings > Materials > PDF Preview > As
    /// Last Time)
    void saveState();

    /// @brief The page a bookmark or a command of the menu works on: of the selection, else
    /// the one the menu was opened on, else the one shown
    [[nodiscard]] int commandPage() const;

    QString m_path;
    QString m_problem;
    QPdfDocument* m_document = nullptr;
    QPdfSearchModel* m_search = nullptr;
    QPdfBookmarkModel* m_contents = nullptr;
    int m_hit = -1;              ///< The hit gone to; -1: none
    bool m_goToFirstHit = false; ///< The first hit found is gone to
    int m_menuPage = -1;         ///< The page the menu was opened on; -1: none
    int m_menuHighlight = -1;    ///< The highlight the menu was opened on; -1: none
    QColor m_firstHighlightColor;  ///< Of a highlight without its own
    int m_settingsListener = 0;  ///< core::SettingsManager::subscribe() id

    QToolBar* m_toolBar = nullptr;
    QAction* m_sideAction = nullptr;
    QAction* m_previousAction = nullptr;
    QAction* m_nextAction = nullptr;
    QLineEdit* m_pageEdit = nullptr;
    QLabel* m_pageCountLabel = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QComboBox* m_zoomBox = nullptr;
    QAction* m_zoomInAction = nullptr;
    QAction* m_continuousAction = nullptr;
    QAction* m_singleAction = nullptr;
    QAction* m_twoAction = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QLabel* m_hitsLabel = nullptr;
    QAction* m_previousHitAction = nullptr;
    QAction* m_nextHitAction = nullptr;
    QAction* m_cardAction = nullptr;
    QAction* m_systemAction = nullptr;
    QAction* m_settingsAction = nullptr;

    QStackedWidget* m_pages = nullptr;
    QSplitter* m_splitter = nullptr;
    QTabWidget* m_side = nullptr;
    QTreeWidget* m_bookmarksList = nullptr;
    QLabel* m_bookmarksEmpty = nullptr;
    QPushButton* m_addBookmarkButton = nullptr;
    QTreeWidget* m_highlightsList = nullptr;
    QLabel* m_highlightsEmpty = nullptr;
    QPushButton* m_highlightColorButton = nullptr;
    ColorMenu* m_newHighlightColors = nullptr;
    ColorMenu* m_highlightColors = nullptr;
    QTreeView* m_contentsTree = nullptr;
    QLabel* m_contentsEmpty = nullptr;
    QListWidget* m_thumbnails = nullptr;
    QListView* m_resultsList = nullptr;
    QLabel* m_resultsEmpty = nullptr;
    PdfPagesView* m_view = nullptr;
    QLabel* m_problemLabel = nullptr;
    QPushButton* m_problemButton = nullptr;

    QFrame* m_selectionBar = nullptr;
    QAction* m_copyAction = nullptr;
    QAction* m_copyReferenceAction = nullptr;
    QAction* m_quoteAction = nullptr;
    QAction* m_bookmarkHereAction = nullptr;
    QAction* m_highlightAction = nullptr;
    QAction* m_highlightNoteAction = nullptr;
    QAction* m_removeHighlightAction = nullptr;
    QToolButton* m_highlightButton = nullptr;  ///< Highlight on the bar of the selection
    QAction* m_selectPageAction = nullptr;

    QTimer* m_searchTimer = nullptr;  ///< Searches a while after the typing stops
    QTimer* m_saveTimer = nullptr;    ///< Keeps the page shown a while after it changes
    bool m_filling = false;           ///< The lists are being shown: no change to save
    core::ProjectManager& m_book;     ///< The book of its window
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PDF_PREVIEW_PANEL_H
