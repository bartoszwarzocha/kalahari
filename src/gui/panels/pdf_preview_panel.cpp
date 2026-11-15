// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_preview_panel.cpp
/// @brief The preview of the PDF file of a material, on a tab in the center of the window

#include "kalahari/gui/panels/pdf_preview_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/source_description.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/pdf_files.h"
#include "kalahari/gui/source_names.h"

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
#include <cstdio>

namespace kalahari::gui {

namespace {

/// Milliseconds after the typing stops until the search begins
constexpr int SEARCH_DELAY = 400;

/// Letters a search begins with by itself; Enter searches for fewer
constexpr int SHORTEST_SEARCH = 3;

/// Milliseconds after the page changes until it is kept with the material
constexpr int SAVE_DELAY = 1500;

/// The largest small page of the side panel
constexpr int THUMBNAIL_WIDTH = 96;
constexpr int THUMBNAIL_HEIGHT = 128;

/// Characters of a note made of a selection
constexpr int NOTE_LENGTH = 80;

/// The width of the side panel at first
constexpr int SIDE_WIDTH = 220;

/// The width of the pages beside it at first
constexpr int PAGES_WIDTH = 800;

/// Room between a selection and its bar, and the bar and the edge of the window
constexpr int BAR_GAP = 6;

/// A window lighter than this is of a light theme (QColor::lightness())
constexpr int DARK_LIGHTNESS = 128;

/// Room of the text of a hit in its list
constexpr int HIT_MARGIN = 2;

/// Width of the text of a hit before the list has its own
constexpr int HIT_WIDTH = 200;

/// Item data of a small page: whether it was drawn
constexpr int DRAWN_ROLE = Qt::UserRole + 1;

/// The columns of the bookmarks
constexpr int PAGE_COLUMN = 0;
constexpr int NOTE_COLUMN = 1;

/// @p text with its key in brackets, as the tooltips of the program have it: "Zoom In (Ctrl++)"
QString withKey(const QString& text, const QString& key) {
    return QStringLiteral("%1 (%2)").arg(text, key);
}

/// @p key as the system writes it
QString keyText(const QKeySequence& key) {
    return key.toString(QKeySequence::NativeText);
}

/// The hit of a search in its list: its page, and its text with the words around it
class HitDelegate : public QStyledItemDelegate {
public:
    HitDelegate(const PdfPreviewPanel& panel, QObject* parent)
        : QStyledItemDelegate(parent), m_panel(panel) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem item = option;
        initStyleOption(&item, index);
        item.text.clear();
        const QWidget* widget = option.widget;
        const QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

        QTextDocument text;
        prepare(text, index, option);
        const QRect area = style->subElementRect(QStyle::SE_ItemViewItemText, &item, widget);
        painter->save();
        painter->translate(area.topLeft());
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette = option.palette;
        const bool selected = (option.state & QStyle::State_Selected) != 0;
        context.palette.setColor(QPalette::Text,
                                 option.palette.color(selected ? QPalette::HighlightedText
                                                               : QPalette::Text));
        context.clip = QRectF(0, 0, area.width(), area.height());
        text.documentLayout()->draw(painter, context);
        painter->restore();
    }

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const override {
        QTextDocument text;
        prepare(text, index, option);
        return {static_cast<int>(text.idealWidth()), static_cast<int>(text.size().height())};
    }

private:
    void prepare(QTextDocument& text, const QModelIndex& index,
                 const QStyleOptionViewItem& option) const {
        text.setDefaultFont(option.font);
        text.setDocumentMargin(HIT_MARGIN);
        int width = option.rect.width();
        if (const auto* view = qobject_cast<const QAbstractItemView*>(option.widget)) {
            width = view->viewport()->width();
        }
        text.setTextWidth(width > 0 ? width : HIT_WIDTH);
        const int page = index.data(static_cast<int>(QPdfSearchModel::Role::Page)).toInt();
        const QString before =
            index.data(static_cast<int>(QPdfSearchModel::Role::ContextBefore)).toString();
        const QString after =
            index.data(static_cast<int>(QPdfSearchModel::Role::ContextAfter)).toString();
        const QString hit = m_panel.searchModel()->searchString();
        //: A page in the list of the hits of a search: %1 its number. In Polish: s. %1
        const QString pageText = PdfPreviewPanel::tr("p. %1").arg(m_panel.pageLabel(page));
        text.setHtml(QStringLiteral("<b>%1</b>&nbsp; %2<b><u>%3</u></b>%4")
                         .arg(pageText.toHtmlEscaped(), before.toHtmlEscaped(),
                              hit.toHtmlEscaped(), after.toHtmlEscaped()));
    }

    const PdfPreviewPanel& m_panel;
};

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

    auto* problemPage = new QWidget(m_pages);
    auto* problemLayout = new QVBoxLayout(problemPage);
    problemLayout->addStretch();
    m_problemLabel = new QLabel(problemPage);
    m_problemLabel->setObjectName(QStringLiteral("pdfPreviewProblem"));
    m_problemLabel->setWordWrap(true);
    m_problemLabel->setAlignment(Qt::AlignCenter);
    m_problemLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    problemLayout->addWidget(m_problemLabel);
    //: In Polish: &Otwórz w programie systemowym
    m_problemButton = new QPushButton(tr("&Open in the System Program"), problemPage);
    m_problemButton->setObjectName(QStringLiteral("pdfPreviewOpenSystem"));
    connect(m_problemButton, &QPushButton::clicked, this,
            [this]() { emit openWithSystemRequested(this->elementId()); });
    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    buttonRow->addWidget(m_problemButton);
    buttonRow->addStretch();
    problemLayout->addLayout(buttonRow);
    problemLayout->addStretch();
    m_pages->addWidget(problemPage);
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
        const std::initializer_list<std::pair<int, const char*>> tabs{
            {BookmarksTab, "pdf.bookmarks"},
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
            if (panel == nullptr || key.rfind("materials.pdf.", 0) != 0) {
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
        std::fputs("PdfPreviewPanel: the state of the preview was not saved\n", stderr);
    }
}

void PdfPreviewPanel::buildToolBar() {
    auto& art = core::ArtProvider::getInstance();
    m_toolBar = new QToolBar(this);
    m_toolBar->setObjectName(QStringLiteral("pdfPreviewBar"));
    const auto addAction = [this, &art](const char* iconId, const QString& text,
                                        const QString& tooltip) {
        QAction* action = art.createAction(QString::fromLatin1(iconId), text, m_toolBar);
        action->setToolTip(tooltip);
        m_toolBar->addAction(action);
        return action;
    };

    //: The button that shows and hides the side panel of the preview. In Polish: Panel boczny
    m_sideAction = addAction("pdf.sidePanel", tr("Side Panel"), tr("Side Panel"));
    m_sideAction->setCheckable(true);
    m_sideAction->setChecked(true);
    m_toolBar->addSeparator();

    //: In Polish: Poprzednia strona
    m_previousAction = addAction("common.chevronLeft", tr("Previous Page"),
                                 withKey(tr("Previous Page"), keyText(QKeySequence(Qt::Key_PageUp))));
    //: In Polish: Następna strona
    m_nextAction = addAction("common.chevronRight", tr("Next Page"),
                             withKey(tr("Next Page"), keyText(QKeySequence(Qt::Key_PageDown))));
    //: In Polish: Strona
    auto* pageLabel = new QLabel(tr("Page"), m_toolBar);
    pageLabel->setContentsMargins(BAR_GAP, 0, BAR_GAP, 0);
    m_toolBar->addWidget(pageLabel);
    m_pageEdit = new QLineEdit(m_toolBar);
    m_pageEdit->setObjectName(QStringLiteral("pdfPreviewPage"));
    m_pageEdit->setAlignment(Qt::AlignCenter);
    m_pageEdit->setFixedWidth(m_pageEdit->fontMetrics().horizontalAdvance(QStringLiteral("00000")) +
                              2 * BAR_GAP);
    //: In Polish: Numer strony (Ctrl+G)
    m_pageEdit->setAccessibleName(
        withKey(tr("Page number"), keyText(QKeySequence(Qt::CTRL | Qt::Key_G))));
    m_pageEdit->setToolTip(m_pageEdit->accessibleName());
    pageLabel->setBuddy(m_pageEdit);
    m_toolBar->addWidget(m_pageEdit);
    m_pageCountLabel = new QLabel(m_toolBar);
    m_pageCountLabel->setObjectName(QStringLiteral("pdfPreviewPageCount"));
    m_pageCountLabel->setContentsMargins(BAR_GAP, 0, BAR_GAP, 0);
    m_toolBar->addWidget(m_pageCountLabel);
    m_toolBar->addSeparator();

    m_zoomOutAction = addAction("view.zoomOut", tr("Zoom Out"),
                                withKey(tr("Zoom Out"), keyText(QKeySequence(Qt::CTRL | Qt::Key_Minus))));
    m_zoomBox = new QComboBox(m_toolBar);
    m_zoomBox->setObjectName(QStringLiteral("pdfPreviewZoom"));
    //: In Polish: Powiększenie
    m_zoomBox->setAccessibleName(tr("Zoom"));
    //: The zoom at which the width of the page fills the window. In Polish: Szerokość strony
    m_zoomBox->addItem(tr("Page Width"), QString::fromLatin1(PdfFiles::PAGE_WIDTH));
    //: The zoom at which the whole page is in the window. In Polish: Cała strona
    m_zoomBox->addItem(tr("Whole Page"), QString::fromLatin1(PdfFiles::WHOLE_PAGE));
    for (double step : PdfPagesView::zoomSteps()) {
        //: A zoom of the pages; %1: the percent. In Polish: %1%
        m_zoomBox->addItem(tr("%1%").arg(qRound(step * 100)), QString::number(step));
    }
    m_toolBar->addWidget(m_zoomBox);
    m_zoomInAction = addAction("view.zoomIn", tr("Zoom In"),
                               withKey(tr("Zoom In"), keyText(QKeySequence(Qt::CTRL | Qt::Key_Plus))));
    m_toolBar->addSeparator();

    //: In Polish: Strony jedna pod drugą
    m_continuousAction = addAction("pdf.layoutContinuous", tr("Pages One under Another"),
                                   tr("Pages One under Another"));
    //: In Polish: Po jednej stronie
    m_singleAction = addAction("pdf.layoutSingle", tr("One Page at a Time"),
                               tr("One Page at a Time"));
    //: In Polish: Dwie strony obok siebie
    m_twoAction = addAction("pdf.layoutTwo", tr("Two Pages Side by Side"),
                            tr("Two Pages Side by Side"));
    auto* layouts = new QActionGroup(this);
    layouts->setExclusive(true);
    for (QAction* action : {m_continuousAction, m_singleAction, m_twoAction}) {
        action->setCheckable(true);
        layouts->addAction(action);
    }
    m_toolBar->addSeparator();

    m_searchEdit = new QLineEdit(m_toolBar);
    m_searchEdit->setObjectName(QStringLiteral("pdfPreviewSearch"));
    m_searchEdit->setClearButtonEnabled(true);
    //: In Polish: Szukaj w pliku (Ctrl+F)
    m_searchEdit->setPlaceholderText(
        withKey(tr("Find in the File"), keyText(QKeySequence(Qt::CTRL | Qt::Key_F))));
    m_searchEdit->setAccessibleName(m_searchEdit->placeholderText());
    m_searchEdit->addAction(art.getIcon(QStringLiteral("edit.find")),
                            QLineEdit::LeadingPosition);
    m_searchEdit->setMinimumWidth(m_searchEdit->fontMetrics().horizontalAdvance(
                                      m_searchEdit->placeholderText()) +
                                  4 * BAR_GAP);
    m_toolBar->addWidget(m_searchEdit);
    m_hitsLabel = new QLabel(m_toolBar);
    m_hitsLabel->setObjectName(QStringLiteral("pdfPreviewHits"));
    m_hitsLabel->setContentsMargins(BAR_GAP, 0, BAR_GAP, 0);
    m_toolBar->addWidget(m_hitsLabel);
    //: In Polish: Poprzedni wynik
    m_previousHitAction = addAction("edit.findPrevious", tr("Previous Hit"),
                                    withKey(tr("Previous Hit"),
                                            keyText(QKeySequence(Qt::SHIFT | Qt::Key_Return))));
    //: In Polish: Następny wynik
    m_nextHitAction = addAction("edit.findNext", tr("Next Hit"),
                                withKey(tr("Next Hit"), keyText(QKeySequence(Qt::Key_Return))));

    auto* room = new QWidget(m_toolBar);
    room->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(room);
    //: The button that opens the card of the material of the PDF file. In Polish: Karta
    //: materiału
    m_cardAction = addAction("pdf.card", tr("Material Card"),
                             withKey(tr("Material Card"), keyText(QKeySequence(Qt::Key_Return))));
    if (auto* button = qobject_cast<QToolButton*>(m_toolBar->widgetForAction(m_cardAction))) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    //: In Polish: Otwórz w programie systemowym
    m_systemAction = addAction("common.openExternal", tr("Open in the System Program"),
                               tr("Open in the System Program"));
    //: The gear of the preview of a PDF file. In Polish: Ustawienia podglądu PDF
    m_settingsAction = addAction("edit.settings", tr("PDF Preview Settings"),
                                 tr("PDF Preview Settings"));

    connect(m_sideAction, &QAction::toggled, this, [this](bool shown) {
        m_side->setVisible(shown);
        if (shown) {
            QTimer::singleShot(0, this, &PdfPreviewPanel::drawThumbnails);
        }
    });
    connect(m_previousAction, &QAction::triggered, this,
            [this]() { m_view->setCurrentPage(m_view->currentPage() - 1); });
    connect(m_nextAction, &QAction::triggered, this,
            [this]() { m_view->setCurrentPage(m_view->currentPage() + 1); });
    connect(m_pageEdit, &QLineEdit::returnPressed, this, &PdfPreviewPanel::goToTypedPage);
    connect(m_zoomOutAction, &QAction::triggered, this, &PdfPreviewPanel::zoomOut);
    connect(m_zoomInAction, &QAction::triggered, this, &PdfPreviewPanel::zoomIn);
    connect(m_zoomBox, &QComboBox::activated, this, [this](int index) {
        const QString zoom = m_zoomBox->itemData(index).toString();
        if (zoom == QLatin1String(PdfFiles::PAGE_WIDTH)) {
            zoomToPageWidth();
        } else if (zoom == QLatin1String(PdfFiles::WHOLE_PAGE)) {
            zoomToWholePage();
        } else {
            m_view->setZoom(zoom.toDouble());
            showZoom();
        }
    });
    // The layout is the setting: every preview follows it
    const std::initializer_list<std::pair<QAction*, const char*>> layoutSettings{
        {m_continuousAction, PdfFiles::CONTINUOUS},
        {m_singleAction, PdfFiles::SINGLE},
        {m_twoAction, PdfFiles::TWO}};
    for (const auto& [action, value] : layoutSettings) {
        connect(action, &QAction::triggered, this, [this, value = value]() {
            core::SettingsManager::getInstance().set<std::string>(PdfFiles::LAYOUT_SETTING,
                                                                  value);
            applySettings(false);
        });
    }
    connect(m_searchEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        if (text.trimmed().length() >= SHORTEST_SEARCH) {
            m_searchTimer->start();
        } else {
            m_searchTimer->stop();
            if (text.trimmed().isEmpty()) {
                search(QString());
            }
        }
    });
    connect(m_previousHitAction, &QAction::triggered, this, &PdfPreviewPanel::previousHit);
    connect(m_nextHitAction, &QAction::triggered, this, &PdfPreviewPanel::nextHit);
    connect(m_cardAction, &QAction::triggered, this,
            [this]() { emit openCardRequested(elementId()); });
    connect(m_systemAction, &QAction::triggered, this,
            [this]() { emit openWithSystemRequested(elementId()); });
    connect(m_settingsAction, &QAction::triggered, this, [this]() { emit settingsRequested(); });
}

void PdfPreviewPanel::buildSidePanel() {
    m_side = new QTabWidget(m_splitter);
    m_side->setObjectName(QStringLiteral("pdfPreviewSide"));
    m_side->setDocumentMode(true);
    //: In Polish: Panel boczny
    m_side->setAccessibleName(tr("Side Panel"));
    const auto page = [this](QWidget* list, QLabel* hint, QWidget* below = nullptr) {
        auto* widget = new QWidget(m_side);
        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(list, 1);
        hint->setWordWrap(true);
        hint->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        hint->setForegroundRole(QPalette::PlaceholderText);
        hint->setContentsMargins(BAR_GAP, BAR_GAP, BAR_GAP, BAR_GAP);
        layout->addWidget(hint, 1);
        if (below != nullptr) {
            layout->addWidget(below);
        }
        return widget;
    };

    // The bookmarks of the material
    m_bookmarksList = new QTreeWidget();
    m_bookmarksList->setObjectName(QStringLiteral("pdfPreviewBookmarks"));
    m_bookmarksList->setColumnCount(2);
    m_bookmarksList->setHeaderLabels({
        //: The columns of the bookmarks of the preview. In Polish: Strona
        tr("Page"),
        //: In Polish: Notatka
        tr("Note")});
    m_bookmarksList->setRootIsDecorated(false);
    m_bookmarksList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_bookmarksList->header()->setSectionResizeMode(PAGE_COLUMN, QHeaderView::ResizeToContents);
    m_bookmarksList->header()->setStretchLastSection(true);
    //: In Polish: Zakładki: Enter przechodzi do strony, Insert dodaje zakładkę, F2 zmienia
    //: notatkę, Delete usuwa
    m_bookmarksList->setAccessibleName(tr("Bookmarks: Enter goes to the page, Insert adds a "
                                          "bookmark, F2 changes the note, Delete takes it away"));
    //: In Polish: Bez zakładek. Przycisk niżej albo Insert dodaje zakładkę na pokazanej
    //: stronie.
    m_bookmarksEmpty = new QLabel(tr("No bookmarks. The button below or Insert adds a "
                                     "bookmark at the page shown."));
    m_addBookmarkButton = new QPushButton();
    m_addBookmarkButton->setObjectName(QStringLiteral("pdfPreviewAddBookmark"));
    m_side->addTab(page(m_bookmarksList, m_bookmarksEmpty, m_addBookmarkButton), QString());
    //: In Polish: Zakładki
    m_side->setTabToolTip(BookmarksTab, tr("Bookmarks"));

    // The contents of the file
    m_contentsTree = new QTreeView();
    m_contentsTree->setObjectName(QStringLiteral("pdfPreviewContents"));
    m_contentsTree->setModel(m_contents);
    m_contentsTree->setHeaderHidden(true);
    m_contentsTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //: In Polish: Spis treści pliku
    m_contentsTree->setAccessibleName(tr("Contents of the file"));
    //: In Polish: Plik nie ma spisu treści.
    m_contentsEmpty = new QLabel(tr("The file has no table of contents."));
    m_side->addTab(page(m_contentsTree, m_contentsEmpty), QString());
    //: In Polish: Spis treści
    m_side->setTabToolTip(ContentsTab, tr("Contents"));

    // The pages small
    m_thumbnails = new QListWidget();
    m_thumbnails->setObjectName(QStringLiteral("pdfPreviewThumbnails"));
    m_thumbnails->setViewMode(QListView::IconMode);
    m_thumbnails->setFlow(QListView::LeftToRight);
    m_thumbnails->setWrapping(true);
    m_thumbnails->setResizeMode(QListView::Adjust);
    m_thumbnails->setMovement(QListView::Static);
    m_thumbnails->setUniformItemSizes(true);
    m_thumbnails->setIconSize(QSize(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT));
    //: In Polish: Strony
    m_thumbnails->setAccessibleName(tr("Pages"));
    m_side->addTab(page(m_thumbnails, new QLabel()), QString());
    //: In Polish: Miniatury
    m_side->setTabToolTip(ThumbnailsTab, tr("Thumbnails"));

    // The hits of the search
    m_resultsList = new QListView();
    m_resultsList->setObjectName(QStringLiteral("pdfPreviewResults"));
    m_resultsList->setModel(m_search);
    m_resultsList->setItemDelegate(new HitDelegate(*this, m_resultsList));
    m_resultsList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsList->setWordWrap(true);
    //: In Polish: Wyniki szukania
    m_resultsList->setAccessibleName(tr("Search results"));
    m_resultsEmpty = new QLabel();
    m_side->addTab(page(m_resultsList, m_resultsEmpty), QString());
    m_side->setTabToolTip(ResultsTab, tr("Search results"));
    for (int tab = 0; tab < m_side->count(); ++tab) {
        m_side->setTabWhatsThis(tab, m_side->tabToolTip(tab));
    }

    connect(m_addBookmarkButton, &QPushButton::clicked, this, &PdfPreviewPanel::addBookmark);
    const auto goToBookmark = [this](QTreeWidgetItem* item) {
        const core::ProjectElement* element = m_book.findElement(elementId());
        const int index = m_bookmarksList->indexOfTopLevelItem(item);
        if (element != nullptr && index >= 0 && index < element->bookmarks.size()) {
            m_view->setCurrentPage(element->bookmarks.at(index).page - 1);
        }
    };
    connect(m_bookmarksList, &QTreeWidget::itemActivated, this, goToBookmark);
    connect(m_bookmarksList, &QTreeWidget::itemClicked, this, goToBookmark);
    connect(m_bookmarksList, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem* item, int column) {
                const core::ProjectElement* element = m_book.findElement(elementId());
                const int index = m_bookmarksList->indexOfTopLevelItem(item);
                if (m_filling || column != NOTE_COLUMN || element == nullptr || index < 0 ||
                    index >= element->bookmarks.size()) {
                    return;
                }
                QList<core::PageBookmark> bookmarks = element->bookmarks;
                bookmarks[index].note = item->text(NOTE_COLUMN).trimmed();
                if (bookmarks != element->bookmarks) {
                    saveBookmarks(bookmarks);
                }
            });
    const auto goToContents = [this](const QModelIndex& index) {
        m_view->setCurrentPage(index.data(static_cast<int>(QPdfBookmarkModel::Role::Page)).toInt());
    };
    connect(m_contentsTree, &QTreeView::activated, this, goToContents);
    connect(m_contentsTree, &QTreeView::clicked, this, goToContents);
    const auto goToThumbnail = [this](const QListWidgetItem* item) {
        m_view->setCurrentPage(m_thumbnails->row(item));
    };
    connect(m_thumbnails, &QListWidget::itemActivated, this, goToThumbnail);
    connect(m_thumbnails, &QListWidget::itemClicked, this, goToThumbnail);
    connect(m_thumbnails->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &PdfPreviewPanel::drawThumbnails);
    const auto goToResult = [this](const QModelIndex& index) { goToHit(index.row()); };
    connect(m_resultsList, &QListView::activated, this, goToResult);
    connect(m_resultsList, &QListView::clicked, this, goToResult);
    connect(m_side, &QTabWidget::currentChanged, this, [this](int tab) {
        if (tab == ThumbnailsTab) {
            QTimer::singleShot(0, this, &PdfPreviewPanel::drawThumbnails);
        }
    });
}

void PdfPreviewPanel::buildSelectionCommands() {
    auto& art = core::ArtProvider::getInstance();
    //: In Polish: Kopiuj
    m_copyAction = art.createAction(QStringLiteral("edit.copy"), tr("Copy"), this);
    m_copyAction->setToolTip(withKey(tr("Copy"), keyText(QKeySequence(Qt::CTRL | Qt::Key_C))));
    //: In Polish: Kopiuj z odnośnikiem
    m_copyReferenceAction =
        art.createAction(QStringLiteral("edit.copy"), tr("Copy with Reference"), this);
    //: In Polish: Kopiuj razem z odnośnikiem do źródła i strony
    m_copyReferenceAction->setToolTip(
        tr("Copy together with the reference to the source and its page"));
    //: In Polish: Utwórz cytat…
    m_quoteAction = art.createAction(QStringLiteral("pdf.quote"), tr("Create Quote..."), this);
    //: In Polish: Zakładka tutaj
    m_bookmarkHereAction =
        art.createAction(QStringLiteral("pdf.bookmarks"), tr("Bookmark Here"), this);
    //: In Polish: Zaznacz cały tekst strony
    m_selectPageAction =
        art.createAction(QStringLiteral("edit.selectAll"), tr("Select All Text of the Page"), this);
    connect(m_copyAction, &QAction::triggered, m_view, &PdfPagesView::copyHere);
    connect(m_copyReferenceAction, &QAction::triggered, this,
            &PdfPreviewPanel::copyWithReference);
    connect(m_quoteAction, &QAction::triggered, this, &PdfPreviewPanel::createQuote);
    connect(m_bookmarkHereAction, &QAction::triggered, this, &PdfPreviewPanel::addBookmark);
    connect(m_selectPageAction, &QAction::triggered, this,
            [this]() { m_view->selectPage(commandPage()); });

    // The bar over a selection made with the mouse; the keys have the menu
    m_selectionBar = new QFrame(m_view->viewport());
    m_selectionBar->setObjectName(QStringLiteral("pdfPreviewSelectionBar"));
    m_selectionBar->setFrameShape(QFrame::StyledPanel);
    m_selectionBar->setAutoFillBackground(true);
    m_selectionBar->setBackgroundRole(QPalette::Window);
    //: In Polish: Zaznaczenie
    m_selectionBar->setAccessibleName(tr("Selection"));
    auto* barLayout = new QHBoxLayout(m_selectionBar);
    barLayout->setContentsMargins(2, 2, 2, 2);
    barLayout->setSpacing(2);
    for (QAction* action : {m_copyAction, m_copyReferenceAction, m_quoteAction,
                            m_bookmarkHereAction}) {
        auto* button = new QToolButton(m_selectionBar);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setFocusPolicy(Qt::NoFocus);
        button->setAutoRaise(true);
        barLayout->addWidget(button);
    }
    m_selectionBar->hide();
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
            m_selectPageAction};
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
    m_view->selectHit(hit.page(), rectangles);
    m_resultsList->setCurrentIndex(m_search->index(index, 0));
    updateHits();
}

void PdfPreviewPanel::copyWithReference() {
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

void PdfPreviewPanel::addBookmark() {
    const core::ProjectElement* element = m_book.findElement(elementId());
    if (element == nullptr || m_book.isReadOnly() || !hasDocument()) {
        return;
    }
    const int page = commandPage();
    QString note;
    if (m_view->hasSelection() && m_view->selectionPage() == page) {
        note = m_view->selectedText().simplified();
        if (note.size() > NOTE_LENGTH) {
            note = note.left(NOTE_LENGTH - 1).trimmed() + QChar(0x2026);
        }
    }
    QList<core::PageBookmark> bookmarks = element->bookmarks;
    core::PageBookmark bookmark;
    bookmark.page = page + 1;
    bookmark.note = note;
    bookmarks.append(bookmark);
    saveBookmarks(bookmarks);
    // The new one, with its note to write
    if (m_side->isHidden()) {
        m_sideAction->setChecked(true);
    }
    m_side->setCurrentIndex(BookmarksTab);
    if (QTreeWidgetItem* item =
            m_bookmarksList->topLevelItem(m_bookmarksList->topLevelItemCount() - 1)) {
        m_bookmarksList->setCurrentItem(item);
        if (note.isEmpty()) {
            m_bookmarksList->setFocus(Qt::OtherFocusReason);
            m_bookmarksList->editItem(item, NOTE_COLUMN);
        }
    }
}

void PdfPreviewPanel::removeBookmark() {
    const core::ProjectElement* element = m_book.findElement(elementId());
    const int index = m_bookmarksList->indexOfTopLevelItem(m_bookmarksList->currentItem());
    if (element == nullptr || m_book.isReadOnly() || index < 0 ||
        index >= element->bookmarks.size()) {
        return;
    }
    // A note is the writer's words: asked first
    const QTreeWidgetItem* item = m_bookmarksList->topLevelItem(index);
    if (!element->bookmarks.at(index).note.trimmed().isEmpty() &&
        //: In Polish: Usuń zakładkę
        !dialogs::MessageDialog::confirm(
            this, tr("Delete Bookmark"),
            //: %1: the page. In Polish: Usunąć zakładkę strony %1 razem z notatką?
            tr("Delete the bookmark of page %1 with its note?").arg(item->text(PAGE_COLUMN)),
            //: In Polish: &Usuń
            tr("&Delete"))) {
        return;
    }
    QList<core::PageBookmark> bookmarks = element->bookmarks;
    bookmarks.removeAt(index);
    saveBookmarks(bookmarks);
    if (m_bookmarksList->topLevelItemCount() > 0) {
        m_bookmarksList->setCurrentItem(m_bookmarksList->topLevelItem(
            std::min(index, m_bookmarksList->topLevelItemCount() - 1)));
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

    // The pages small: drawn when they are in the window of the list
    m_thumbnails->clear();
    for (int each = 0; each < count; ++each) {
        auto* item = new QListWidgetItem(pageLabel(each), m_thumbnails);
        QPixmap blank(QSize(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT));
        blank.fill(palette().color(QPalette::Base));
        item->setIcon(QIcon(blank));
        item->setData(DRAWN_ROLE, false);
        item->setTextAlignment(Qt::AlignHCenter);
    }
    const bool contents = m_contents->rowCount(QModelIndex()) > 0;
    m_contentsTree->setVisible(contents);
    m_contentsEmpty->setVisible(!contents);
    fillBookmarks();
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
        return PreviewPanel::eventFilter(watched, event);
    }
    if (type != QEvent::ShortcutOverride && type != QEvent::KeyPress) {
        return PreviewPanel::eventFilter(watched, event);
    }
    auto* key = static_cast<QKeyEvent*>(event);
    const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
    // F6 and Shift+F6 go between the parts; Ctrl+G to the number of the page: over the keys
    // of the window
    const bool f6 = key->key() == Qt::Key_F6 &&
                    (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier);
    const bool ctrlG = key->key() == Qt::Key_G && modifiers == Qt::ControlModifier;
    if (f6 || ctrlG) {
        if (type == QEvent::ShortcutOverride) {
            key->accept();
        } else if (f6) {
            focusNextPart(modifiers == Qt::NoModifier);
        } else {
            goToPage();
        }
        return true;
    }
    if (type != QEvent::KeyPress) {
        return PreviewPanel::eventFilter(watched, event);
    }
    const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
    if (watched == m_searchEdit) {
        if (enter) {
            if (modifiers == Qt::ShiftModifier) {
                previousHit();
            } else {
                nextHit();
            }
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            m_view->setFocus(Qt::OtherFocusReason);
            return true;
        }
    }
    if (watched == m_pageEdit && key->key() == Qt::Key_Escape) {
        m_pageEdit->setText(pageLabel(m_view->currentPage()));
        m_view->setFocus(Qt::OtherFocusReason);
        return true;
    }
    if (watched == m_bookmarksList && modifiers == Qt::NoModifier) {
        switch (key->key()) {
        case Qt::Key_Insert:
            addBookmark();
            return true;
        case Qt::Key_Delete:
            removeBookmark();
            return true;
        case Qt::Key_F2:
            if (QTreeWidgetItem* item = m_bookmarksList->currentItem();
                item != nullptr && !m_book.isReadOnly()) {
                m_bookmarksList->editItem(item, NOTE_COLUMN);
            }
            return true;
        default:
            break;
        }
    }
    return PreviewPanel::eventFilter(watched, event);
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
    if (!opening) {
        return;
    }

    // The zoom it opens with
    auto& settings = core::SettingsManager::getInstance();
    QString zoom = PdfFiles::choice(PdfFiles::ZOOM_SETTING, PdfFiles::PAGE_WIDTH,
                                    {PdfFiles::PAGE_WIDTH, PdfFiles::WHOLE_PAGE,
                                     PdfFiles::ACTUAL, PdfFiles::LAST});
    if (zoom == QLatin1String(PdfFiles::LAST)) {
        zoom = QString::fromStdString(settings.get<std::string>(PdfFiles::LAST_ZOOM_SETTING,
                                                                PdfFiles::PAGE_WIDTH));
    }
    bool number = false;
    const double factor = zoom.toDouble(&number);
    if (zoom == QLatin1String(PdfFiles::WHOLE_PAGE)) {
        m_view->setFit(PdfPagesView::Fit::WholePage);
    } else if (zoom == QLatin1String(PdfFiles::ACTUAL)) {
        m_view->setZoom(1.0);
    } else if (number && factor > 0.0) {
        // The nearest of the zooms of the box
        const QList<double>& steps = PdfPagesView::zoomSteps();
        const auto nearest = std::min_element(steps.cbegin(), steps.cend(),
                                              [factor](double one, double other) {
                                                  return std::abs(one - factor) <
                                                         std::abs(other - factor);
                                              });
        m_view->setZoom(*nearest);
    } else {
        m_view->setFit(PdfPagesView::Fit::PageWidth);
    }
    showZoom();

    // What the side panel shows first
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

    // The page shown last
    if (element != nullptr && element->lastPage > 0 &&
        settings.get<bool>(PdfFiles::LAST_PAGE_SETTING, true)) {
        m_view->setCurrentPage(element->lastPage - 1);
    }
    m_saveTimer->stop();
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

void PdfPreviewPanel::onPageChanged(int page) {
    if (hasDocument()) {
        if (!m_pageEdit->hasFocus()) {
            m_pageEdit->setText(pageLabel(page));
        }
        //: The tooltip of the field of the page: %1 the page counted from the first one of
        //: the file. In Polish: Strona %1 pliku (Ctrl+G)
        m_pageEdit->setToolTip(withKey(tr("Page %1 of the file").arg(page + 1),
                                       keyText(QKeySequence(Qt::CTRL | Qt::Key_G))));
        //: The button under the bookmarks: %1 the page shown. In Polish: Zakładka na s. %1
        m_addBookmarkButton->setText(tr("Bookmark at Page %1").arg(pageLabel(page)));
        const QSignalBlocker blocker(m_thumbnails);
        m_thumbnails->setCurrentRow(page);
    } else {
        m_pageEdit->clear();
        //: In Polish: Dodaj zakładkę
        m_addBookmarkButton->setText(tr("Add Bookmark"));
    }
    updateCommands();
    m_saveTimer->start();
    emit pageChanged();
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
    m_zoomInAction->setEnabled(shown && m_view->zoom() < steps.last() - 0.001);
    m_zoomOutAction->setEnabled(shown && m_view->zoom() > steps.first() + 0.001);
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
}

void PdfPreviewPanel::fillBookmarks() {
    m_filling = true;
    // The current row stays, so that the list shown again after a change keeps its place
    const int current = m_bookmarksList->currentIndex().row();
    m_bookmarksList->clear();
    const core::ProjectElement* element = m_book.findElement(elementId());
    const int count = hasDocument() ? m_document->pageCount() : 0;
    if (element != nullptr) {
        for (const core::PageBookmark& bookmark : element->bookmarks) {
            const QString page = bookmark.page >= 1 && bookmark.page <= count
                                     ? pageLabel(bookmark.page - 1)
                                     : QString::number(bookmark.page);
            auto* item = new QTreeWidgetItem(m_bookmarksList, {page, bookmark.note});
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        }
    }
    const int rows = m_bookmarksList->topLevelItemCount();
    if (current >= 0 && rows > 0) {
        m_bookmarksList->setCurrentItem(m_bookmarksList->topLevelItem(std::min(current, rows - 1)));
    }
    const bool none = rows == 0;
    m_bookmarksList->setVisible(!none);
    m_bookmarksEmpty->setVisible(none);
    m_filling = false;
}

void PdfPreviewPanel::saveBookmarks(const QList<core::PageBookmark>& bookmarks) {
    if (m_book.setBookmarks(elementId(), bookmarks)) {
        fillBookmarks();
        emit materialChanged(elementId());
    }
}

void PdfPreviewPanel::drawThumbnails() {
    if (!hasDocument() || !m_thumbnails->isVisible()) {
        return;
    }
    const QRect window = m_thumbnails->viewport()->rect();
    const qreal ratio = devicePixelRatioF();
    for (int row = 0; row < m_thumbnails->count(); ++row) {
        QListWidgetItem* item = m_thumbnails->item(row);
        if (item->data(DRAWN_ROLE).toBool() ||
            !m_thumbnails->visualItemRect(item).intersects(window)) {
            continue;
        }
        const QSize size = m_document->pagePointSize(row)
                               .scaled(QSizeF(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT),
                                       Qt::KeepAspectRatio)
                               .toSize()
                               .expandedTo(QSize(1, 1));
        QImage page = m_document->render(row, (QSizeF(size) * ratio).toSize());
        if (page.isNull()) {
            continue;
        }
        page.setDevicePixelRatio(ratio);
        item->setIcon(QIcon(QPixmap::fromImage(page)));
        item->setData(DRAWN_ROLE, true);
    }
}

void PdfPreviewPanel::updateHits() {
    const int count = m_search->rowCount(QModelIndex());
    const bool searching = !m_search->searchString().isEmpty();
    if (!searching) {
        m_hitsLabel->clear();
    } else if (count == 0) {
        //: In Polish: Brak wyników
        m_hitsLabel->setText(tr("No hits"));
    } else if (m_hit < 0) {
        //: How many hits the search found. In Polish: %n wynik / %n wyniki / %n wyników
        m_hitsLabel->setText(tr("%n hit(s)", nullptr, count));
    } else {
        //: The hit gone to: %1 which, %2 how many. In Polish: %1 z %2
        m_hitsLabel->setText(tr("%1 of %2").arg(m_hit + 1).arg(count));
    }
    m_previousHitAction->setEnabled(count > 0);
    m_nextHitAction->setEnabled(count > 0);
    m_resultsList->setVisible(count > 0);
    m_resultsEmpty->setVisible(count == 0);
    if (searching) {
        //: In Polish: Nie ma w pliku tego tekstu.
        m_resultsEmpty->setText(tr("The file does not have this text."));
    } else {
        //: In Polish: Wpisz w pole „Szukaj w pliku” co najmniej trzy litery albo naciśnij
        //: Enter.
        m_resultsEmpty->setText(tr("Type at least three letters in the field \"Find in the "
                                   "File\", or press Enter."));
    }
}

void PdfPreviewPanel::placeSelectionBar(bool show) {
    const QRect selection = m_view->selectionRect();
    if (!show || selection.isEmpty()) {
        m_selectionBar->hide();
        return;
    }
    m_selectionBar->adjustSize();
    const QSize size = m_selectionBar->size();
    const QRect window = m_view->viewport()->rect();
    const int x = std::clamp(selection.center().x() - size.width() / 2, BAR_GAP,
                             std::max(BAR_GAP, window.width() - size.width() - BAR_GAP));
    int y = selection.top() - size.height() - BAR_GAP;
    if (y < BAR_GAP) {
        y = std::min(selection.bottom() + BAR_GAP, window.height() - size.height() - BAR_GAP);
    }
    m_selectionBar->move(x, y);
    m_selectionBar->show();
    m_selectionBar->raise();
}

void PdfPreviewPanel::showSelectionMenu(const QPoint& globalPosition) {
    if (!hasDocument()) {
        return;
    }
    m_menuPage = m_view->hasSelection()
                     ? -1
                     : m_view->pageAt(m_view->viewport()->mapFromGlobal(globalPosition));
    updateCommands();
    QMenu menu(this);
    menu.addAction(m_copyAction);
    menu.addAction(m_copyReferenceAction);
    menu.addAction(m_quoteAction);
    menu.addSeparator();
    menu.addAction(m_bookmarkHereAction);
    menu.addSeparator();
    menu.addAction(m_selectPageAction);
    menu.exec(globalPosition);
    m_menuPage = -1;
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
    const QWidget* focus = QApplication::focusWidget();
    qsizetype at = -1;
    for (qsizetype index = 0; index < parts.size(); ++index) {
        if (parts.at(index) == focus || parts.at(index)->isAncestorOf(focus) ||
            (parts.at(index) == m_view && focus == m_view->viewport())) {
            at = index;
        }
    }
    const qsizetype count = parts.size();
    const qsizetype next = at < 0 ? 0 : (at + (forward ? 1 : count - 1)) % count;
    QWidget* part = parts.at(next);
    if (part == m_side) {
        // The list of the tab shown, else its tabs
        const QList<QWidget*> lists{m_bookmarksList, m_contentsTree, m_thumbnails,
                                    m_resultsList};
        QWidget* list = lists.value(m_side->currentIndex());
        part = list != nullptr && list->isVisible() ? list : m_side->tabBar();
    }
    part->setFocus(forward ? Qt::TabFocusReason : Qt::BacktabFocusReason);
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
