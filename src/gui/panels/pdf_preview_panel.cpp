// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_preview_panel.cpp
/// @brief The preview of the PDF file of a material, on a tab in the center of the window

#include "kalahari/gui/panels/pdf_preview_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/panels/pdf_pages_view.h"
#include "kalahari/gui/pdf_files.h"
#include "kalahari/gui/source_names.h"
#include "kalahari/gui/widgets/color_menu.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPdfBookmarkModel>
#include <QPdfDocument>
#include <QPdfLink>
#include <QPdfSearchModel>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>

namespace kalahari::gui {

namespace {

/// Milliseconds after the typing stops until the search begins
constexpr int SEARCH_DELAY = 400;

/// Milliseconds after the page changes until it is kept with the material
constexpr int SAVE_DELAY = 1500;

/// The width of the side panel at first
constexpr int SIDE_WIDTH = 220;

/// The width of the pages beside it at first
constexpr int PAGES_WIDTH = 800;

/// A window lighter than this is of a light theme (QColor::lightness())
constexpr int DARK_LIGHTNESS = 128;

/// Two zooms closer than this are the same
constexpr double SAME_ZOOM = 0.001;

}  // namespace

PdfPreviewPanel::PdfPreviewPanel(core::ProjectManager& book, const QString& elementId,
                                 QWidget* parent)
    : PreviewPanel(elementId, parent), m_book(book) {
    setProperty("tabIconId", QStringLiteral("material.pdf"));
    if (const core::ProjectElement* element = m_book.findElement(elementId)) {
        m_path = m_book.filePathOf(*element);
    }
    m_document = new QPdfDocument(this);
    m_search = new QPdfSearchModel(this);
    m_contents = new QPdfBookmarkModel(this);
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(SEARCH_DELAY);
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(SAVE_DELAY);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    buildToolBar();
    layout->addWidget(m_toolBar);

    // The side panel and the pages, or why there are none
    m_pages = new QStackedWidget(this);
    m_splitter = new QSplitter(Qt::Horizontal, m_pages);
    m_splitter->setChildrenCollapsible(false);
    buildSidePanel();
    m_splitter->addWidget(m_side);
    m_view = new PdfPagesView(m_splitter);
    m_view->setSearchModel(m_search);
    //: The pages on the tab of the preview of a PDF file. In Polish: Strony pliku PDF: PgUp i
    //: PgDn zmieniają stronę, Enter otwiera kartę materiału
    m_view->setAccessibleName(
        tr("Pages of the PDF file: PgUp and PgDn change the page, Enter opens the card of the "
           "material"));
    m_splitter->addWidget(m_view);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({SIDE_WIDTH, PAGES_WIDTH});
    m_pages->addWidget(m_splitter);

    m_pages->addWidget(buildProblemPage());
    layout->addWidget(m_pages, 1);
    buildSelectionCommands();

    // The pages
    connect(m_view, &PdfPagesView::currentPageChanged, this, &PdfPreviewPanel::onPageChanged);
    connect(m_view, &PdfPagesView::zoomChanged, this, [this](double zoom) {
        showZoom();
        m_saveTimer->start();
        emit zoomChanged(zoom);
    });
    connect(m_view, &PdfPagesView::selectionChanged, this, [this]() {
        placeSelectionBar(false);
        updateCommands();
    });
    connect(m_view, &PdfPagesView::selectionMade, this, [this]() { placeSelectionBar(true); });
    connect(m_view, &PdfPagesView::selectionMenuRequested, this,
            &PdfPreviewPanel::showSelectionMenu);
    connect(m_view, &PdfPagesView::activated, this,
            [this]() { emit openCardRequested(this->elementId()); });
    const auto followBar = [this]() { placeSelectionBar(m_selectionBar->isVisible()); };
    connect(m_view->verticalScrollBar(), &QScrollBar::valueChanged, this, followBar);
    connect(m_view->horizontalScrollBar(), &QScrollBar::valueChanged, this, followBar);

    // The search: the first hit found is gone to
    connect(m_search, &QAbstractItemModel::rowsInserted, this, [this]() {
        if (m_goToFirstHit && m_search->rowCount(QModelIndex()) > 0) {
            m_goToFirstHit = false;
            goToHit(0);
        }
        updateHits();
    });
    connect(m_search, &QAbstractItemModel::modelReset, this, [this]() {
        m_hit = -1;
        updateHits();
    });
    connect(m_searchTimer, &QTimer::timeout, this,
            [this]() { search(m_searchEdit->text()); });
    connect(m_saveTimer, &QTimer::timeout, this, &PdfPreviewPanel::saveState);

    // The theme, its colors and the icons
    auto& themes = core::ThemeManager::getInstance();
    connect(&themes, &core::ThemeManager::themeStyleChanged, this,
            &PdfPreviewPanel::applyAppearance);
    connect(&themes, &core::ThemeManager::editorColorsChanged, this,
            &PdfPreviewPanel::applyAppearance);
    auto& art = core::ArtProvider::getInstance();
    const auto refreshIcons = [this]() {
        auto& icons = core::ArtProvider::getInstance();
        const int size = icons.getIconSize(core::IconContext::Toolbar);
        m_toolBar->setIconSize(QSize(size, size));
        m_problemButton->setIcon(
            icons.getIcon(QStringLiteral("common.openExternal"), core::IconContext::Button));
        m_addBookmarkButton->setIcon(
            icons.getIcon(QStringLiteral("pdf.bookmarks"), core::IconContext::Button));
        m_highlightColorButton->setIcon(
            icons.getIcon(QStringLiteral("format.highlightColor"), core::IconContext::Button));
        const std::initializer_list<std::pair<int, const char*>> tabs{
            {BookmarksTab, "pdf.bookmarks"},
            {HighlightsTab, "pdf.highlight"},
            {ContentsTab, "pdf.contents"},
            {ThumbnailsTab, "pdf.thumbnails"},
            {ResultsTab, "pdf.results"}};
        for (const auto& [tab, icon] : tabs) {
            m_side->setTabIcon(tab, icons.getIcon(QString::fromLatin1(icon),
                                                  core::IconContext::Button));
        }
    };
    refreshIcons();
    connect(&art, &core::ArtProvider::resourcesChanged, this, refreshIcons);

    // The settings of the preview, changed in Settings or by another preview
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [panel = QPointer<PdfPreviewPanel>(this)](const std::string& key) {
            if (panel == nullptr || !key.starts_with("materials.pdf.")) {
                return;
            }
            QMetaObject::invokeMethod(
                panel,
                [panel]() {
                    if (panel != nullptr) {
                        panel->applySettings(false);
                        panel->applyAppearance();
                    }
                },
                Qt::QueuedConnection);
        });

    // F6, Ctrl+G and the keys of the fields and lists
    for (QWidget* child : findChildren<QWidget*>()) {
        if (child->focusPolicy() != Qt::NoFocus) {
            child->installEventFilter(this);
        }
    }
    m_thumbnails->viewport()->installEventFilter(this);
    setFocusProxy(m_view);

    reload();
    applySettings(true);
    applyAppearance();
}

PdfPreviewPanel::~PdfPreviewPanel() {
    try {
        core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
        if (m_saveTimer->isActive()) {
            saveState();
        }
    } catch (...) {
        // A destructor may not throw: the page shown last is then not kept
        static_cast<void>(
            std::fputs("PdfPreviewPanel: the state of the preview was not saved\n", stderr));
    }
}

bool PdfPreviewPanel::hasDocument() const {
    return m_problem.isEmpty() && m_document->status() == QPdfDocument::Status::Ready &&
           m_document->pageCount() > 0;
}

QString PdfPreviewPanel::pageLabel(int page) const {
    const QString label = m_document->pageLabel(page).trimmed();
    return label.isEmpty() ? QString::number(page + 1) : label;
}

double PdfPreviewPanel::zoom() const {
    return m_view->zoom();
}

QString PdfPreviewPanel::pageStatus() const {
    if (!hasDocument()) {
        return {};
    }
    //: The page shown in the status bar: %1 its number, %2 the number of pages. In Polish:
    //: Strona %1 z %2
    return tr("Page %1 of %2").arg(pageLabel(m_view->currentPage())).arg(m_view->pageCount());
}

bool PdfPreviewPanel::canFind() const {
    return hasDocument();
}

bool PdfPreviewPanel::showPage(const QString& page) {
    int found = pageNamed(page);
    if (found < 0) {
        // A range or a page with more after it: "41–42", "45 n."
        static const QRegularExpression first(QStringLiteral("^\\s*([0-9]+|[ivxlcdm]+)\\b"),
                                              QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = first.match(page);
        if (match.hasMatch()) {
            found = pageNamed(match.captured(1));
        }
    }
    if (found < 0) {
        return false;
    }
    m_view->setCurrentPage(found);
    return true;
}

void PdfPreviewPanel::refresh() {
    fillBookmarks();
    fillHighlights();
    updateCommands();
}

QString PdfPreviewPanel::textWithReference() const {
    if (!m_view->hasSelection()) {
        return {};
    }
    return SourceNames::quotedWithReference(m_book, elementId(), m_view->selectedText(),
                                            pageLabel(m_view->selectionPage()));
}

QAction* PdfPreviewPanel::layoutAction(PdfPagesView::Layout layout) const {
    switch (layout) {
    case PdfPagesView::Layout::Single:
        return m_singleAction;
    case PdfPagesView::Layout::Two:
        return m_twoAction;
    case PdfPagesView::Layout::Continuous:
        break;
    }
    return m_continuousAction;
}

QList<QAction*> PdfPreviewPanel::selectionActions() const {
    return {m_copyAction, m_copyReferenceAction, m_quoteAction, m_bookmarkHereAction,
            m_highlightAction, m_selectPageAction};
}

void PdfPreviewPanel::zoomIn() {
    m_view->zoomIn();
    showZoom();
}

void PdfPreviewPanel::zoomOut() {
    m_view->zoomOut();
    showZoom();
}

void PdfPreviewPanel::zoomReset() {
    m_view->setZoom(1.0);
    showZoom();
}

void PdfPreviewPanel::zoomToPageWidth() {
    m_view->setFit(PdfPagesView::Fit::PageWidth);
    showZoom();
    m_saveTimer->start();
}

void PdfPreviewPanel::zoomToWholePage() {
    m_view->setFit(PdfPagesView::Fit::WholePage);
    showZoom();
    m_saveTimer->start();
}

void PdfPreviewPanel::find() {
    if (!hasDocument()) {
        return;
    }
    m_searchEdit->setFocus(Qt::ShortcutFocusReason);
    m_searchEdit->selectAll();
}

void PdfPreviewPanel::goToPage() {
    if (!hasDocument()) {
        return;
    }
    m_pageEdit->setFocus(Qt::ShortcutFocusReason);
    m_pageEdit->selectAll();
}

void PdfPreviewPanel::search(const QString& text) {
    m_searchTimer->stop();
    const QString wanted = text.trimmed();
    m_hit = -1;
    m_view->setCurrentHit(-1, {});
    m_goToFirstHit = !wanted.isEmpty();
    m_search->setSearchString(wanted);
    if (!wanted.isEmpty() && m_side->isVisible()) {
        m_side->setCurrentIndex(ResultsTab);
    }
    updateHits();
}

void PdfPreviewPanel::nextHit() {
    const QString text = m_searchEdit->text().trimmed();
    if (m_search->searchString() != text) {
        search(text);
        return;
    }
    const int count = m_search->rowCount(QModelIndex());
    if (count > 0) {
        goToHit(m_hit < 0 ? 0 : (m_hit + 1) % count);
    }
}

void PdfPreviewPanel::previousHit() {
    const QString text = m_searchEdit->text().trimmed();
    if (m_search->searchString() != text) {
        search(text);
        return;
    }
    const int count = m_search->rowCount(QModelIndex());
    if (count > 0) {
        goToHit(m_hit <= 0 ? count - 1 : m_hit - 1);
    }
}

void PdfPreviewPanel::goToHit(int index) {
    if (index < 0 || index >= m_search->rowCount(QModelIndex())) {
        return;
    }
    const QPdfLink hit = m_search->resultAtIndex(index);
    m_hit = index;
    m_goToFirstHit = false;
    const QList<QRectF> rectangles = hit.rectangles();
    QRectF place;
    for (const QRectF& rectangle : rectangles) {
        place = place.united(rectangle);
    }
    m_view->showPlace(hit.page(), place);
    m_view->setCurrentHit(hit.page(), rectangles);
    // The hit is the selection: Copy and the menu of the selection work on it
    int occurrence = 0;
    for (int before = index - 1;
         before >= 0 && m_search->resultAtIndex(before).page() == hit.page(); --before) {
        ++occurrence;
    }
    m_view->selectHit(hit.page(), m_search->searchString(), occurrence, rectangles);
    m_resultsList->setCurrentIndex(m_search->index(index, 0));
    updateHits();
}

void PdfPreviewPanel::copyWithReference() const {
    const QString text = textWithReference();
    if (!text.isEmpty()) {
        QGuiApplication::clipboard()->setText(text);
    }
}

void PdfPreviewPanel::createQuote() {
    if (m_view->hasSelection()) {
        emit newQuoteRequested(elementId(), m_view->selectedText().simplified(),
                               pageLabel(m_view->selectionPage()));
    }
}

void PdfPreviewPanel::reload() {
    const int page = hasDocument() ? m_view->currentPage() : -1;
    m_view->setDocument(nullptr);
    m_search->setDocument(nullptr);
    m_contents->setDocument(nullptr);
    m_document->close();
    m_problem.clear();
    const MaterialFile file = MaterialFile::of(m_path);
    if (!file.exists) {
        m_problem = file.problem();
    } else if (const QPdfDocument::Error error = m_document->load(m_path);
               error != QPdfDocument::Error::None || m_document->pageCount() < 1) {
        m_problem = PdfFiles::loadProblem(static_cast<int>(error));
    }
    if (!m_problem.isEmpty()) {
        core::Logger::getInstance().info("PdfPreviewPanel: No pages of {}: {}",
                                         m_path.toStdString(), m_problem.toStdString());
        m_document->close();
        m_problemLabel->setText(m_problem);
        m_pages->setCurrentIndex(1);
    } else {
        m_pages->setCurrentIndex(0);
        m_search->setDocument(m_document);
        m_contents->setDocument(m_document);
        m_view->setDocument(m_document);
    }
    const int count = hasDocument() ? m_document->pageCount() : 0;
    //: The number of pages after the field of the page: %1 how many. In Polish: z %1
    m_pageCountLabel->setText(count > 0 ? tr("of %1").arg(count) : QString());

    fillThumbnails(count);
    const bool contents = m_contents->rowCount(QModelIndex()) > 0;
    m_contentsTree->setVisible(contents);
    m_contentsEmpty->setVisible(!contents);
    fillBookmarks();
    fillHighlights();
    if (page >= 0) {
        m_view->setCurrentPage(page);
    }
    onPageChanged(m_view->currentPage());
    updateHits();
    showZoom();
    QTimer::singleShot(0, this, &PdfPreviewPanel::drawThumbnails);
}

bool PdfPreviewPanel::eventFilter(QObject* watched, QEvent* event) {
    const QEvent::Type type = event->type();
    if (watched == m_thumbnails->viewport() && (type == QEvent::Resize || type == QEvent::Show)) {
        QTimer::singleShot(0, this, &PdfPreviewPanel::drawThumbnails);
    } else if ((type == QEvent::ShortcutOverride || type == QEvent::KeyPress) &&
               handleKey(watched, *static_cast<QKeyEvent*>(event))) {
        return true;
    }
    return PreviewPanel::eventFilter(watched, event);
}

bool PdfPreviewPanel::handleKey(QObject* watched, QKeyEvent& key) {
    Qt::KeyboardModifiers modifiers = key.modifiers();
    modifiers.setFlag(Qt::KeypadModifier, false);
    if (partKey(key, modifiers)) {
        return true;
    }
    if (key.type() != QEvent::KeyPress) {
        return false;
    }
    if (watched == m_bookmarksList) {
        return modifiers == Qt::NoModifier && bookmarkKey(key.key());
    }
    if (watched == m_highlightsList) {
        return modifiers == Qt::NoModifier && highlightKey(key.key());
    }
    if (watched == m_searchEdit) {
        return searchKey(key.key(), modifiers);
    }
    if (watched == m_pageEdit && key.key() == Qt::Key_Escape) {
        m_pageEdit->setText(pageLabel(m_view->currentPage()));
        m_view->setFocus(Qt::OtherFocusReason);
        return true;
    }
    return false;
}

bool PdfPreviewPanel::partKey(QKeyEvent& key, Qt::KeyboardModifiers modifiers) {
    // F6 and Shift+F6 go between the parts; Ctrl+G to the number of the page: over the keys
    // of the window
    const bool f6 = key.key() == Qt::Key_F6 &&
                    (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier);
    const bool ctrlG = key.key() == Qt::Key_G && modifiers == Qt::ControlModifier;
    if (!f6 && !ctrlG) {
        return false;
    }
    if (key.type() == QEvent::ShortcutOverride) {
        key.accept();
    } else if (f6) {
        focusNextPart(modifiers == Qt::NoModifier);
    } else {
        goToPage();
    }
    return true;
}

bool PdfPreviewPanel::searchKey(int key, Qt::KeyboardModifiers modifiers) {
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (modifiers == Qt::ShiftModifier) {
            previousHit();
        } else {
            nextHit();
        }
        return true;
    }
    if (key == Qt::Key_Escape) {
        m_view->setFocus(Qt::OtherFocusReason);
        return true;
    }
    return false;
}

void PdfPreviewPanel::applySettings(bool opening) {
    using Layout = PdfPagesView::Layout;
    const QString layout = PdfFiles::choice(PdfFiles::LAYOUT_SETTING, PdfFiles::CONTINUOUS,
                                            {PdfFiles::CONTINUOUS, PdfFiles::SINGLE,
                                             PdfFiles::TWO});
    Layout shown = Layout::Continuous;
    if (layout == QLatin1String(PdfFiles::SINGLE)) {
        shown = Layout::Single;
    } else if (layout == QLatin1String(PdfFiles::TWO)) {
        shown = Layout::Two;
    }
    m_view->setPageLayout(shown);
    layoutAction(shown)->setChecked(true);
    applyHighlightsSetting();
    if (!opening) {
        return;
    }
    applyZoomSetting();
    applySideSetting();

    // The page shown last
    const core::ProjectElement* element = m_book.findElement(elementId());
    if (element != nullptr && element->lastPage > 0 &&
        core::SettingsManager::getInstance().get<bool>(PdfFiles::LAST_PAGE_SETTING, true)) {
        m_view->setCurrentPage(element->lastPage - 1);
    }
    m_saveTimer->stop();
}

void PdfPreviewPanel::applyZoomSetting() {
    QString zoom = PdfFiles::choice(PdfFiles::ZOOM_SETTING, PdfFiles::PAGE_WIDTH,
                                    {PdfFiles::PAGE_WIDTH, PdfFiles::WHOLE_PAGE,
                                     PdfFiles::ACTUAL, PdfFiles::LAST});
    if (zoom == QLatin1String(PdfFiles::LAST)) {
        zoom = QString::fromStdString(core::SettingsManager::getInstance().get<std::string>(
            PdfFiles::LAST_ZOOM_SETTING, PdfFiles::PAGE_WIDTH));
    }
    bool number = false;
    const double factor = zoom.toDouble(&number);
    if (zoom == QLatin1String(PdfFiles::WHOLE_PAGE)) {
        m_view->setFit(PdfPagesView::Fit::WholePage);
    } else if (zoom == QLatin1String(PdfFiles::ACTUAL)) {
        m_view->setZoom(1.0);
    } else if (number && factor > 0.0) {
        // The nearest of the zooms of the box
        const auto nearest = std::ranges::min_element(
            PdfPagesView::zoomSteps(), std::less<>(),
            [factor](double step) { return std::abs(step - factor); });
        m_view->setZoom(*nearest);
    } else {
        m_view->setFit(PdfPagesView::Fit::PageWidth);
    }
    showZoom();
}

void PdfPreviewPanel::applySideSetting() {
    const QString side = PdfFiles::choice(PdfFiles::SIDE_SETTING, PdfFiles::BOOKMARKS,
                                          {PdfFiles::BOOKMARKS, PdfFiles::CONTENTS,
                                           PdfFiles::THUMBNAILS, PdfFiles::HIDDEN});
    const core::ProjectElement* element = m_book.findElement(elementId());
    const bool bookmarks = element != nullptr && !element->bookmarks.isEmpty();
    const bool contents = m_contents->rowCount(QModelIndex()) > 0;
    if (side == QLatin1String(PdfFiles::HIDDEN)) {
        m_sideAction->setChecked(false);
    } else if (side == QLatin1String(PdfFiles::CONTENTS)) {
        m_side->setCurrentIndex(ContentsTab);
    } else if (side == QLatin1String(PdfFiles::THUMBNAILS)) {
        m_side->setCurrentIndex(ThumbnailsTab);
    } else if (bookmarks) {
        m_side->setCurrentIndex(BookmarksTab);
    } else {
        m_side->setCurrentIndex(contents ? ContentsTab : ThumbnailsTab);
    }
}

void PdfPreviewPanel::applyAppearance() {
    // The look of the pages in a dark theme; in a light one they are as in the file
    const bool dark = palette().color(QPalette::Window).lightness() < DARK_LIGHTNESS;
    const QString look = PdfFiles::choice(PdfFiles::DARK_SETTING, PdfFiles::AS_FILE,
                                          {PdfFiles::AS_FILE, PdfFiles::DIMMED,
                                           PdfFiles::INVERTED});
    PdfPagesView::Look shown = PdfPagesView::Look::AsFile;
    if (dark && look == QLatin1String(PdfFiles::DIMMED)) {
        shown = PdfPagesView::Look::Dimmed;
    } else if (dark && look == QLatin1String(PdfFiles::INVERTED)) {
        shown = PdfPagesView::Look::Inverted;
    }
    m_view->setLook(shown);

    // The editor's colors of the theme: around the paper, the selection and the hits
    const auto& themes = core::ThemeManager::getInstance();
    const editor::EditorColors defaults;
    PdfPagesView::Colors colors;
    colors.desk = dark ? themes.editorColor(editor::DARK_PAPER_COLOR_KEYS.desk,
                                            defaults.continuous.deskDark)
                       : themes.editorColor(editor::LIGHT_PAPER_COLOR_KEYS.desk,
                                            defaults.continuous.deskLight);
    colors.selection = editor::underTextColor(
        themes.editorColor(editor::SELECTION_COLOR_KEY, defaults.selection),
        editor::SELECTION_OPACITY);
    colors.searchMatch = editor::underTextColor(
        themes.editorColor(editor::SEARCH_MATCH_COLOR_KEY, defaults.searchMatch),
        editor::SEARCH_MATCH_OPACITY);
    colors.currentSearchMatch = editor::underTextColor(
        themes.editorColor(editor::CURRENT_SEARCH_MATCH_COLOR_KEY, defaults.currentSearchMatch),
        editor::CURRENT_SEARCH_MATCH_OPACITY);
    m_view->setColors(colors);
}

void PdfPreviewPanel::showZoom() {
    const QSignalBlocker blocker(m_zoomBox);
    int index = -1;
    if (m_view->fit() == PdfPagesView::Fit::PageWidth) {
        index = m_zoomBox->findData(QString::fromLatin1(PdfFiles::PAGE_WIDTH));
    } else if (m_view->fit() == PdfPagesView::Fit::WholePage) {
        index = m_zoomBox->findData(QString::fromLatin1(PdfFiles::WHOLE_PAGE));
    } else {
        index = m_zoomBox->findData(QString::number(m_view->zoom()));
    }
    m_zoomBox->setCurrentIndex(index);
    const QList<double>& steps = PdfPagesView::zoomSteps();
    const bool shown = hasDocument();
    m_zoomInAction->setEnabled(shown && m_view->zoom() < steps.last() - SAME_ZOOM);
    m_zoomOutAction->setEnabled(shown && m_view->zoom() > steps.first() + SAME_ZOOM);
}

void PdfPreviewPanel::updateCommands() {
    const bool shown = hasDocument();
    const bool selected = shown && m_view->hasSelection();
    const bool writable = m_book.isProjectOpen() && !m_book.isReadOnly();
    const int page = m_view->currentPage();
    m_previousAction->setEnabled(shown && page > 0);
    m_nextAction->setEnabled(shown && page < m_view->pageCount() - 1);
    for (QAction* action : {m_continuousAction, m_singleAction, m_twoAction}) {
        action->setEnabled(shown);
    }
    m_pageEdit->setEnabled(shown);
    m_zoomBox->setEnabled(shown);
    m_searchEdit->setEnabled(shown);
    m_copyAction->setEnabled(selected);
    m_copyReferenceAction->setEnabled(selected);
    m_quoteAction->setEnabled(selected && writable);
    m_bookmarkHereAction->setEnabled(shown && writable);
    m_selectPageAction->setEnabled(shown);
    m_addBookmarkButton->setEnabled(shown && writable);
    m_highlightAction->setEnabled(selected && writable);
    m_newHighlightColors->menuAction()->setEnabled(selected && writable);
    m_highlightButton->setEnabled(selected && writable);
    const bool highlighted = highlightTarget() >= 0;
    m_highlightColorButton->setEnabled(highlighted && writable);
    m_highlightNoteAction->setEnabled(highlighted && writable);
    m_removeHighlightAction->setEnabled(highlighted && writable);
}

void PdfPreviewPanel::showSelectionMenu(const QPoint& globalPosition) {
    if (!hasDocument()) {
        return;
    }
    const QPoint position = m_view->viewport()->mapFromGlobal(globalPosition);
    const bool selected = m_view->hasSelection();
    m_menuPage = selected ? -1 : m_view->pageAt(position);
    m_menuHighlight = selected || !PdfFiles::highlightsOn() ? -1 : m_view->highlightAt(position);
    updateCommands();
    QMenu menu(this);
    menu.addAction(m_copyAction);
    menu.addAction(m_copyReferenceAction);
    menu.addAction(m_quoteAction);
    menu.addSeparator();
    menu.addAction(m_bookmarkHereAction);
    addHighlightCommands(menu);
    menu.addSeparator();
    menu.addAction(m_selectPageAction);
    menu.exec(globalPosition);
    m_menuPage = -1;
    m_menuHighlight = -1;
    updateCommands();
}

void PdfPreviewPanel::goToTypedPage() {
    const int page = pageNamed(m_pageEdit->text());
    if (page < 0) {
        m_pageEdit->setText(pageLabel(m_view->currentPage()));
        m_pageEdit->selectAll();
        return;
    }
    m_view->setCurrentPage(page);
    m_pageEdit->setText(pageLabel(page));
    m_view->setFocus(Qt::OtherFocusReason);
}

int PdfPreviewPanel::pageNamed(const QString& text) const {
    const QString wanted = text.trimmed();
    if (wanted.isEmpty() || !hasDocument()) {
        return -1;
    }
    const int count = m_document->pageCount();
    for (int page = 0; page < count; ++page) {
        if (pageLabel(page).compare(wanted, Qt::CaseInsensitive) == 0) {
            return page;
        }
    }
    bool number = false;
    const int page = wanted.toInt(&number);
    return number && page >= 1 && page <= count ? page - 1 : -1;
}

void PdfPreviewPanel::focusNextPart(bool forward) {
    QList<QWidget*> parts{m_pageEdit};
    if (m_side->isVisible() && m_side->currentWidget() != nullptr) {
        parts << m_side;
    }
    parts << m_view;
    const qsizetype at = partWithFocus(parts);
    const qsizetype count = parts.size();
    const qsizetype next = at < 0 ? 0 : (at + (forward ? 1 : count - 1)) % count;
    QWidget* part = parts.at(next);
    if (part == m_side) {
        // The list of the tab shown, else its tabs
        const QList<QWidget*> lists{m_bookmarksList, m_highlightsList, m_contentsTree,
                                    m_thumbnails, m_resultsList};
        QWidget* list = lists.value(m_side->currentIndex());
        part = list != nullptr && list->isVisible() ? list : m_side->tabBar();
    }
    part->setFocus(forward ? Qt::TabFocusReason : Qt::BacktabFocusReason);
}

qsizetype PdfPreviewPanel::partWithFocus(const QList<QWidget*>& parts) const {
    const QWidget* focus = QApplication::focusWidget();
    qsizetype at = -1;
    for (qsizetype index = 0; index < parts.size(); ++index) {
        const QWidget* part = parts.at(index);
        if (part == focus || part->isAncestorOf(focus) ||
            (part == m_view && focus == m_view->viewport())) {
            at = index;
        }
    }
    return at;
}

void PdfPreviewPanel::saveState() {
    m_saveTimer->stop();
    if (hasDocument()) {
        const core::ProjectElement* element = m_book.findElement(elementId());
        const int page = m_view->currentPage() + 1;
        if (element != nullptr && element->lastPage != page && !m_book.isReadOnly()) {
            m_book.setLastPage(elementId(), page);
        }
    }
    if (PdfFiles::choice(PdfFiles::ZOOM_SETTING, PdfFiles::PAGE_WIDTH,
                         {PdfFiles::PAGE_WIDTH, PdfFiles::WHOLE_PAGE, PdfFiles::ACTUAL,
                          PdfFiles::LAST}) != QLatin1String(PdfFiles::LAST)) {
        return;
    }
    std::string zoom = QString::number(m_view->zoom()).toStdString();
    if (m_view->fit() == PdfPagesView::Fit::PageWidth) {
        zoom = PdfFiles::PAGE_WIDTH;
    } else if (m_view->fit() == PdfPagesView::Fit::WholePage) {
        zoom = PdfFiles::WHOLE_PAGE;
    }
    auto& settings = core::SettingsManager::getInstance();
    if (settings.get<std::string>(PdfFiles::LAST_ZOOM_SETTING, std::string()) != zoom) {
        settings.set<std::string>(PdfFiles::LAST_ZOOM_SETTING, zoom);
    }
}

int PdfPreviewPanel::commandPage() const {
    if (m_view->hasSelection()) {
        return m_view->selectionPage();
    }
    return m_menuPage >= 0 ? m_menuPage : m_view->currentPage();
}

}  // namespace kalahari::gui
