// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_preview_panel.h
/// @brief The preview of the PDF file of a material, on a tab in the center of the window

#ifndef KALAHARI_GUI_PANELS_PDF_PREVIEW_PANEL_H
#define KALAHARI_GUI_PANELS_PDF_PREVIEW_PANEL_H

#include "kalahari/gui/panels/pdf_pages_view.h"
#include "kalahari/gui/panels/preview_panel.h"

#include <QString>

class QAction;
class QComboBox;
class QFrame;
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
class QToolBar;
class QTreeView;
class QTreeWidget;

namespace kalahari::core {
class ProjectManager;
struct PageBookmark;
}  // namespace kalahari::core

namespace kalahari::gui {

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
    enum SideTab { BookmarksTab, ContentsTab, ThumbnailsTab, ResultsTab };

    /// @brief Create the preview of material @p elementId of the open book
    PdfPreviewPanel(core::ProjectManager& book, const QString& elementId,
                    QWidget* parent = nullptr);

    /// @brief Keeps the page shown with the material
    ~PdfPreviewPanel() override;

    PdfPreviewPanel(const PdfPreviewPanel&) = delete;
    PdfPreviewPanel& operator=(const PdfPreviewPanel&) = delete;

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
    [[nodiscard]] PdfPagesView* view() const { return m_view; }
    [[nodiscard]] QToolBar* toolBar() const { return m_toolBar; }
    [[nodiscard]] QLineEdit* pageEdit() const { return m_pageEdit; }
    [[nodiscard]] QLabel* pageCountLabel() const { return m_pageCountLabel; }
    [[nodiscard]] QComboBox* zoomBox() const { return m_zoomBox; }
    [[nodiscard]] QLineEdit* searchEdit() const { return m_searchEdit; }
    [[nodiscard]] QLabel* hitsLabel() const { return m_hitsLabel; }
    [[nodiscard]] QTabWidget* sidePanel() const { return m_side; }
    [[nodiscard]] QTreeWidget* bookmarksList() const { return m_bookmarksList; }
    [[nodiscard]] QPushButton* addBookmarkButton() const { return m_addBookmarkButton; }
    [[nodiscard]] QTreeView* contentsTree() const { return m_contentsTree; }
    [[nodiscard]] QListWidget* thumbnails() const { return m_thumbnails; }
    [[nodiscard]] QListView* resultsList() const { return m_resultsList; }
    [[nodiscard]] QFrame* selectionBar() const { return m_selectionBar; }
    [[nodiscard]] QPdfSearchModel* searchModel() const { return m_search; }
    [[nodiscard]] QAction* sideAction() const { return m_sideAction; }
    [[nodiscard]] QAction* layoutAction(PdfPagesView::Layout layout) const;
    [[nodiscard]] QList<QAction*> selectionActions() const;
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

    /// @brief Copy the selection with the reference to its page (copyWithReference())
    void copyWithReference();

    /// @brief Ask for a quote of the selection from its page (newQuoteRequested())
    void createQuote();

    /// @brief Add a bookmark at the page of the selection, with the selection as its note,
    /// or at the page shown without one
    void addBookmark();

    /// @brief Take the current bookmark of the side panel away
    void removeBookmark();

    /// @brief Read the file again and show it, at the page shown
    void reload();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The bar over the pages
    void buildToolBar();

    /// @brief The side panel and its tabs
    void buildSidePanel();

    /// @brief The bar over a selection and the commands of the selection
    void buildSelectionCommands();

    /// @brief The settings of the preview, at its opening
    void applySettings(bool opening);

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
    QAction* m_selectPageAction = nullptr;

    QTimer* m_searchTimer = nullptr;  ///< Searches a while after the typing stops
    QTimer* m_saveTimer = nullptr;    ///< Keeps the page shown a while after it changes
    bool m_filling = false;           ///< The bookmarks are being shown: no change to save
    core::ProjectManager& m_book;     ///< The book of its window
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PDF_PREVIEW_PANEL_H
