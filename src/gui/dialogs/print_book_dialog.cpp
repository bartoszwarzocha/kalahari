// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file print_book_dialog.cpp
/// @brief File > Print: a large preview of the sheets and the sections of the printer, the
///        texts, the pages, the sheet, the page of the book and the look of the text, and the
///        progress and the result of the print in one window

#include "kalahari/gui/dialogs/print_book_dialog.h"
#include "book_output_parts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_pages.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/widgets/collapsible_section.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QApplication>
#include <QButtonGroup>
#include <QChar>
#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

using book_output::nameOfValue;
using book_output::valueOfName;

/// The width of the window at least: as wide as the screen of 1366x768 at 150% allows (911
/// pixels with the frame)
constexpr int WINDOW_WIDTH = 880;
/// The width the window starts with, in widths of the column of the sections: the preview
/// gets more than half of the window
constexpr double START_WIDTH_IN_COLUMNS = 2.4;
/// The height the window starts with, in lines of the window's text (within the screen)
constexpr int START_LINES = 40;
/// The narrowest column of the sections, in letters of the window's text
constexpr int COLUMN_LETTERS = 52;
/// The height the column of the sections asks for, in lines: the window fits a small screen;
/// a higher window gives it more, and it scrolls
constexpr int COLUMN_HINT_LINES = 12;
/// The lowest the column of the sections gets, in lines
constexpr int COLUMN_MINIMUM_LINES = 6;
/// The lowest the sheet of the preview gets, in lines of the window's text
constexpr int PREVIEW_LINES = 8;
/// The room around the sheet in the preview, in pixels
constexpr int PREVIEW_MARGIN = 10;
/// How far a turn of the wheel or an arrow scrolls a sheet as wide as the preview, in lines
constexpr int SCROLL_LINES = 3;
/// The most copies the window prints
constexpr int MAX_COPIES = 99;
/// Below this lightness the window's theme is dark: the preview's desk is the dark one
constexpr int DARK_LIGHTNESS = 128;
/// A space no line breaks at
constexpr QChar NO_BREAK_SPACE(0x00A0);
/// The narrowest list of the printers, in letters of their names (longer ones are cut in it,
/// whole in its list)
constexpr int PRINTER_NAME_LETTERS = 18;
/// The field of the range of the pages at least, in average letters
constexpr int RANGE_LETTERS = 12;
/// The room of the frame of a field around its text, in average letters
constexpr int RANGE_FRAME_LETTERS = 3;
/// The size of a page of a book its own way, in millimeters: the smallest and the largest
constexpr double MIN_PAGE_SIDE = 50.0;
constexpr double MAX_PAGE_SIDE = 500.0;
/// The largest margin of a page, in millimeters
constexpr double MAX_MARGIN = 100.0;
/// Lengths closer than this are the same, in millimeters
constexpr double SAME_LENGTH = 1e-6;
/// The steps of a turn of the wheel of a mouse
constexpr int WHEEL_STEP = 120;

/// The pages on a sheet as the settings keep them
const std::array<std::pair<editor::SheetLayout, const char*>, 3>& sheetLayoutNames() {
    static const std::array<std::pair<editor::SheetLayout, const char*>, 3> names = {{
        {editor::SheetLayout::OnePage, "onePage"},
        {editor::SheetLayout::TwoPages, "twoPages"},
        {editor::SheetLayout::Booklet, "booklet"},
    }};
    return names;
}

/// The sections as the settings keep which are open (print.openSections)
const std::array<std::pair<PrintBookDialog::Section, const char*>, 6>& sectionNames() {
    using Section = PrintBookDialog::Section;
    static const std::array<std::pair<Section, const char*>, 6> names = {{
        {Section::Printer, "printer"},
        {Section::Content, "content"},
        {Section::Pages, "pages"},
        {Section::Sheet, "sheet"},
        {Section::BookPage, "bookPage"},
        {Section::Look, "look"},
    }};
    return names;
}

/// The zooms of the preview as the settings keep them (print.previewZoom)
const std::array<std::pair<PrintBookDialog::Zoom, const char*>, 2>& zoomNames() {
    static const std::array<std::pair<PrintBookDialog::Zoom, const char*>, 2> names = {{
        {PrintBookDialog::Zoom::WholeSheet, "wholeSheet"},
        {PrintBookDialog::Zoom::Width, "width"},
    }};
    return names;
}

/// The margins of the page and their settings, in the order of PrintBookDialog::Margin
const std::array<const char*, 6>& marginKeys() {
    static const std::array<const char*, 6> keys = {
        "editor.margins.pageTop",  "editor.margins.pageBottom", "editor.margins.pageLeft",
        "editor.margins.pageRight", "editor.margins.pageInner", "editor.margins.pageOuter"};
    return keys;
}

/// @p remark under radio button @p button, in line with its text
QLabel* indented(QLabel* remark, const QRadioButton* button) {
    const QStyle* style = button->style();
    remark->setContentsMargins(
        style->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, nullptr, button) +
            style->pixelMetric(QStyle::PM_RadioButtonLabelSpacing, nullptr, button),
        0, 0, 0);
    return remark;
}

/// A row of widgets side by side, the rest of the row empty
QWidget* newLine(QWidget* parent, std::initializer_list<QWidget*> widgets) {
    auto* line = new QWidget(parent);
    auto* layout = new QHBoxLayout(line);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(line->fontMetrics().averageCharWidth() * 2);
    for (QWidget* widget : widgets) {
        widget->setParent(line);
        layout->addWidget(widget);
    }
    layout->addStretch(1);
    return line;
}

/// The numbers the pages show: a page from 0 shows one higher
QString pageNumber(int page) {
    return QString::number(page + 1);
}

/// The pages of the range chosen, from 0
struct RangePages {
    std::vector<int> pages;
    bool typed = true;  ///< false: the field of the range is empty
    bool valid = true;  ///< false: its text is not a range
};

/// The pages of @p range of the @p count pages: all of them, the page of the cursor
/// (@p current), or those of the typed @p text
RangePages pagesOfRange(PrintBookDialog::PageRange range, const QString& text, int count,
                        int current) {
    RangePages result;
    if (range == PrintBookDialog::PageRange::All) {
        result.pages.reserve(static_cast<size_t>(std::max(0, count)));
        for (int page = 0; page < count; ++page) {
            result.pages.push_back(page);
        }
    } else if (range == PrintBookDialog::PageRange::Current) {
        result.pages = {current};
    } else if (text.trimmed().isEmpty()) {
        result.typed = false;
    } else if (const auto typed = editor::pagesOfRange(text, count)) {
        result.pages = *typed;
    } else {
        result.valid = false;
    }
    return result;
}

/// A quality of a printer as the list shows it: "600 dpi", "600 x 1200 dpi"
QString qualityName(const QSize& quality) {
    return quality.width() == quality.height()
               //: A quality of a printer; %1: its dots per inch. In Polish: %1 dpi
               ? PrintBookDialog::tr("%1 dpi").arg(quality.width())
               //: A quality of a printer; %1 and %2: its dots per inch across and down. In
               //: Polish: %1 × %2 dpi
               : PrintBookDialog::tr("%1 x %2 dpi").arg(quality.width()).arg(quality.height());
}

/// The column of the sections at the right of the window: as wide as its widest section,
/// and it scrolls when the window is lower than its open sections
class SectionsArea : public QScrollArea {
public:
    SectionsArea(QWidget* content, QWidget* parent) : QScrollArea(parent) {
        setWidget(content);
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        // A field that Tab or an access key reaches scrolls into view
        connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
            if (now != nullptr && widget() != nullptr && widget()->isAncestorOf(now)) {
                ensureWidgetVisible(now);
            }
        });
    }

    /// @brief As wide as the widest of @p sections needs, open or closed, beside the scroll
    ///        bar: the wrapped sentences take more lines, the fields keep their widths
    void fitWidth(const QList<CollapsibleSection*>& sections) {
        int widest = widthOfLetters(fontMetrics(), COLUMN_LETTERS);
        for (const CollapsibleSection* each : sections) {
            widest = std::max({widest, each->header()->minimumSizeHint().width(),
                               each->body()->minimumSizeHint().width()});
        }
        const int bar = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
        setFixedWidth(widest + bar);
    }

    [[nodiscard]] QSize sizeHint() const override {
        return {width(), fontMetrics().height() * COLUMN_HINT_LINES};
    }

    [[nodiscard]] QSize minimumSizeHint() const override {
        return {width(), fontMetrics().height() * COLUMN_MINIMUM_LINES};
    }
};

}  // namespace

// =============================================================================
// SheetPreview
// =============================================================================

/// @brief A side of a sheet as it comes out of the printer, on the desk of the PDF preview:
///        the whole sheet, or as wide as the preview with a scroll bar beside it
class SheetPreview : public QWidget {
public:
    /// @brief What the preview shows
    struct Sheet {
        std::shared_ptr<const editor::BookPages> pages;  ///< Null: no sheet
        editor::SheetSide side;
        QSizeF paper;       ///< As it lies, in points
        QRectF printable;   ///< In points
        editor::SheetOptions options;
        bool gray = false;  ///< In shades of gray
    };

    /// @brief Told the key the writer pressed: Page Up, Page Down, Home, End
    std::function<void(int key)> keyPressed;

    explicit SheetPreview(QWidget* parent) : QWidget(parent) {
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumHeight(fontMetrics().height() * PREVIEW_LINES);
        // The desk and the paper follow the theme and the colors of the editor
        auto& themes = core::ThemeManager::getInstance();
        connect(&themes, &core::ThemeManager::themeStyleChanged, this, [this]() { redraw(); });
        connect(&themes, &core::ThemeManager::editorColorsChanged, this, [this]() { redraw(); });
    }

    /// @brief The scroll bar beside the preview, for a sheet as wide as it
    void setScrollBar(QScrollBar* bar) {
        m_bar = bar;
        connect(bar, &QScrollBar::valueChanged, this, [this]() { update(); });
        updateScrollBar();
    }

    /// @brief Show the whole sheet, or the sheet as wide as the preview
    void setZoom(PrintBookDialog::Zoom zoom) {
        m_zoom = zoom;
        if (m_bar != nullptr) {
            m_bar->setValue(0);
        }
        redraw();
        updateScrollBar();
    }

    /// @brief Show @p sheet; another side of a sheet shows from its top
    void setSheet(Sheet sheet) {
        const bool otherSide = sheet.side != m_sheet.side || m_sheet.pages == nullptr;
        m_sheet = std::move(sheet);
        m_message.clear();
        if (otherSide && m_bar != nullptr) {
            m_bar->setValue(0);
        }
        redraw();
        updateScrollBar();
    }

    /// @brief Show @p text in place of a sheet
    void setMessage(const QString& text) {
        m_sheet = Sheet();
        m_message = text;
        redraw();
        updateScrollBar();
    }

    /// @brief What the preview says in place of a sheet; empty while it shows one
    QString message() const { return m_message; }

    [[nodiscard]] QSize sizeHint() const override { return minimumSizeHint(); }

    [[nodiscard]] QSize minimumSizeHint() const override {
        return {fontMetrics().averageCharWidth() * 10, minimumHeight()};
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.fillRect(rect(), deskColor());
        if (hasSheet()) {
            const QRect area = sheetRect();
            if (m_image.isNull() || m_image.size() != area.size() * devicePixelRatioF()) {
                m_image = sheetImage(area.size());
            }
            painter.drawImage(area, m_image);
            painter.setPen(palette().color(QPalette::Mid));
            painter.drawRect(area.adjusted(0, 0, -1, -1));
        } else if (!m_message.isEmpty()) {
            painter.setPen(palette().color(QPalette::WindowText));
            painter.drawText(rect().adjusted(PREVIEW_MARGIN, PREVIEW_MARGIN, -PREVIEW_MARGIN,
                                             -PREVIEW_MARGIN),
                             Qt::AlignCenter | Qt::TextWordWrap, m_message);
        }
        if (hasFocus()) {
            QStyleOptionFocusRect option;
            option.initFrom(this);
            option.rect = rect().adjusted(1, 1, -1, -1);
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
        }
    }

    void keyPressEvent(QKeyEvent* event) override {
        switch (event->key()) {
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
            case Qt::Key_Home:
            case Qt::Key_End:
                if (keyPressed) {
                    keyPressed(event->key());
                }
                event->accept();
                return;
            case Qt::Key_Up:
            case Qt::Key_Down:
                // A sheet as wide as the preview scrolls
                if (scrolls()) {
                    m_bar->setValue(m_bar->value() + (event->key() == Qt::Key_Down ? 1 : -1) *
                                                         m_bar->singleStep());
                    event->accept();
                    return;
                }
                QWidget::keyPressEvent(event);
                return;
            default:
                QWidget::keyPressEvent(event);
        }
    }

    void wheelEvent(QWheelEvent* event) override {
        if (!scrolls()) {
            QWidget::wheelEvent(event);
            return;
        }
        m_bar->setValue(m_bar->value() -
                        event->angleDelta().y() * m_bar->singleStep() / WHEEL_STEP);
        event->accept();
    }

    void resizeEvent(QResizeEvent* event) override {
        m_image = QImage();
        updateScrollBar();
        QWidget::resizeEvent(event);
    }

    void changeEvent(QEvent* event) override {
        if (event->type() == QEvent::PaletteChange) {
            m_image = QImage();
        }
        QWidget::changeEvent(event);
    }

private:
    /// @brief Whether a sheet shows
    [[nodiscard]] bool hasSheet() const {
        return m_sheet.pages != nullptr && !m_sheet.paper.isEmpty();
    }

    /// @brief Whether the sheet is as wide as the preview and higher than it
    [[nodiscard]] bool scrolls() const {
        return m_zoom == PrintBookDialog::Zoom::Width && m_bar != nullptr &&
               m_bar->maximum() > 0;
    }

    /// @brief The sheet drawn again
    void redraw() {
        m_image = QImage();
        update();
    }

    /// @brief The scroll bar follows the sheet: shown with a sheet as wide as the preview,
    ///        which it scrolls when the sheet is higher
    void updateScrollBar() {
        if (m_bar == nullptr) {
            return;
        }
        const bool width = m_zoom == PrintBookDialog::Zoom::Width;
        int range = 0;
        if (width && hasSheet()) {
            const double room = std::max(0, this->width() - 2 * PREVIEW_MARGIN);
            const double high = room * m_sheet.paper.height() / m_sheet.paper.width();
            range = std::max(0, static_cast<int>(std::ceil(high)) + 2 * PREVIEW_MARGIN -
                                    height());
        }
        m_bar->setRange(0, range);
        m_bar->setPageStep(std::max(1, height()));
        m_bar->setSingleStep(fontMetrics().height() * SCROLL_LINES);
        // Shown all the while for a sheet as wide as the preview: the preview keeps its width
        m_bar->setVisible(width);
    }

    /// @brief Around the sheet: the desk of the PDF preview (Settings > Editor > Colors)
    QColor deskColor() const {
        const auto& themes = core::ThemeManager::getInstance();
        const editor::EditorColors defaults;
        const bool dark = palette().color(QPalette::Window).lightness() < DARK_LIGHTNESS;
        return dark ? themes.editorColor(editor::DARK_PAPER_COLOR_KEYS.desk,
                                         defaults.continuous.deskDark)
                    : themes.editorColor(editor::LIGHT_PAPER_COLOR_KEYS.desk,
                                         defaults.continuous.deskLight);
    }

    /// @brief Where the sheet shows: as large as the preview allows, in its middle; or as wide
    ///        as the preview, from the place of the scroll bar
    QRect sheetRect() const {
        const QRectF room = QRectF(rect()).adjusted(PREVIEW_MARGIN, PREVIEW_MARGIN,
                                                    -PREVIEW_MARGIN, -PREVIEW_MARGIN);
        if (m_zoom == PrintBookDialog::Zoom::Width) {
            const double scale = std::max(0.0, room.width() / m_sheet.paper.width());
            const int scrolled = m_bar != nullptr ? m_bar->value() : 0;
            return QRectF(QPointF(room.left(), room.top() - scrolled), m_sheet.paper * scale)
                .toAlignedRect();
        }
        const double scale = std::max(0.0, std::min(room.width() / m_sheet.paper.width(),
                                                    room.height() / m_sheet.paper.height()));
        QRectF sheet(QPointF(0, 0), m_sheet.paper * scale);
        sheet.moveCenter(room.center());
        return sheet.toAlignedRect();
    }

    /// @brief The sheet drawn at @p size: the paper of the pages, the pages on it
    QImage sheetImage(const QSize& size) const {
        const qreal ratio = devicePixelRatioF();
        QImage image(size * ratio, QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(ratio);
        // The paper: the light paper of the editor, whose colors the pages have
        const auto& themes = core::ThemeManager::getInstance();
        const editor::EditorColors defaults;
        image.fill(themes.editorColor(editor::LIGHT_PAPER_COLOR_KEYS.paper,
                                      defaults.continuous.backgroundLight));
        {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.scale(size.width() / m_sheet.paper.width(),
                          size.height() / m_sheet.paper.height());
            editor::paintSheetSide(&painter, *m_sheet.pages, m_sheet.side, m_sheet.paper,
                                   m_sheet.printable, m_sheet.options);
        }
        if (m_sheet.gray) {
            image = image.convertToFormat(QImage::Format_Grayscale8);
            image.setDevicePixelRatio(ratio);
        }
        return image;
    }

    Sheet m_sheet;
    QString m_message;
    PrintBookDialog::Zoom m_zoom = PrintBookDialog::Zoom::WholeSheet;
    QScrollBar* m_bar = nullptr;
    mutable QImage m_image;  ///< The sheet drawn at its size
};

// =============================================================================
// PrintBookDialog
// =============================================================================

PrintBookDialog::PrintBookDialog(const Book& book, const QList<Printer>& printers,
                                 PageMaker pageMaker, Printing printing,
                                 TurnQuestion turnQuestion, QWidget* parent)
    : BookOutputDialog(book, parent, false)
    , m_printers(printers)
    , m_pageMaker(std::move(pageMaker))
    , m_printing(std::move(printing))
    , m_turnQuestion(std::move(turnQuestion))
{
    const auto& settings = core::SettingsManager::getInstance();
    //: The title of the window of File > Print. In Polish: Drukuj
    setHeading(tr("Print"));
    setMinimumWidth(WINDOW_WIDTH);
    // The default printer first, the others as the system lists them
    std::stable_partition(m_printers.begin(), m_printers.end(),
                          [](const Printer& printer) { return printer.isDefault; });

    // The preview at the left, the sections at the right; the progress and the result under
    // them
    auto* body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->addWidget(newPreview(), 1);
    m_sectionsColumn = new QWidget(this);
    auto* column = new QVBoxLayout(m_sectionsColumn);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    auto* area = new SectionsArea(m_sectionsColumn, this);
    m_sectionsArea = area;
    body->addWidget(area);
    fieldsLayout()->addLayout(body, 1);

    // The sections in the order of the choices; those of the texts and of their look too
    setPartMaker([this](const QString& title) { return newSection(title); });
    addPrinterSection();
    //: A section of the window of the print: the texts that print. In Polish: C&o drukować
    addTextsTab(tr("&What to Print"),
                book.title.isEmpty()
                    //: Under the text in front of the window of the print, without a book. In
                    //: Polish: Tekst drukuje się także z niezapisanymi zmianami.
                    ? tr("The text prints also with its unsaved changes.")
                    //: Under the texts of the window of the print. In Polish: Za pierwszym
                    //: razem okno wybiera tekst na wierzchu, a potem pamięta ostatni wybór.
                    : tr("The first time the window chooses the text in front, then it keeps "
                         "the last choice."),
                true);
    addPagesSection();
    addSheetSection();
    addBookPageSection();
    //: A section of the window of the print: the layout of the texts, as in the export. In
    //: Polish: Wyg&ląd tekstu
    QGridLayout* look = addLayoutTab(tr("&Look of the Text"), true);
    endTab(look);
    setPagesVisible(true);
    column->addStretch(1);

    //: The button of the window of the print. In Polish: Drukuj
    setAcceptText(tr("Print"));

    m_layoutTimer = new QTimer(this);
    m_layoutTimer->setSingleShot(true);
    m_layoutTimer->setInterval(0);
    connect(m_layoutTimer, &QTimer::timeout, this, &PrintBookDialog::layOutNow);

    readLayout("print.content");
    showPrinter();
    showSheetOptions();
    showBookPage();
    // As wide as the widest section with the fields of the printer
    area->fitWidth(m_sections);

    const auto changed = [this]() {
        dropFailure();
        showPages();
    };
    connect(m_printerBox, &QComboBox::currentIndexChanged, this, [this]() {
        // The settings of the window of the driver are of the printer they were chosen for
        m_driverSettings.clear();
        dropFailure();
        showPrinter();
        contentChanged();
    });
    connect(m_copiesBox, &QSpinBox::valueChanged, this, changed);
    connect(m_collateBox, &QCheckBox::toggled, this, changed);
    connect(m_reverseBox, &QCheckBox::toggled, this, changed);
    for (QRadioButton* button : {m_allButton, m_currentButton, m_rangeButton}) {
        connect(button, &QRadioButton::toggled, this, [changed](bool on) {
            if (on) {
                changed();
            }
        });
    }
    connect(m_rangeButton, &QRadioButton::clicked, this, [this]() {
        m_rangeField->setFocus();
        m_rangeField->selectAll();
    });
    connect(m_rangeField, &QLineEdit::textChanged, this, changed);
    for (QComboBox* box :
         {m_parityBox, m_duplexBox, m_colorBox, m_paperBox, m_sourceBox, m_qualityBox}) {
        connect(box, &QComboBox::currentIndexChanged, this, changed);
    }
    // The writer's own way, which a booklet changes only while it is chosen
    connect(m_duplexBox, &QComboBox::currentIndexChanged, this, [this]() {
        m_ownDuplex = static_cast<Duplex>(m_duplexBox->currentData().toInt());
        showDuplex();
    });
    for (QRadioButton* button : {m_onePageButton, m_twoPagesButton, m_bookletButton,
                                 m_fitButton, m_actualSizeButton}) {
        connect(button, &QRadioButton::toggled, this, [this](bool on) {
            if (on) {
                dropFailure();
                showSheetOptions();
            }
        });
    }
    connect(m_cropMarksBox, &QCheckBox::toggled, this, changed);
    connect(m_driverButton, &QPushButton::clicked, this, &PrintBookDialog::showDriverWindow);
    connect(m_previousButton, &QToolButton::clicked, this,
            [this]() { showSide(m_shownSide - 1); });
    connect(m_nextButton, &QToolButton::clicked, this, [this]() { showSide(m_shownSide + 1); });
    connect(m_spreadButton, &QToolButton::clicked, this, &PrintBookDialog::showSpread);
    m_preview->keyPressed = [this](int key) {
        const int last = static_cast<int>(m_sides.size()) - 1;
        switch (key) {
            case Qt::Key_PageUp:
                showSide(m_shownSide - 1);
                break;
            case Qt::Key_PageDown:
                showSide(m_shownSide + 1);
                break;
            case Qt::Key_Home:
                showSide(0);
                break;
            default:
                showSide(last);
                break;
        }
    };

    // The sections open the last time; the first time the printer, the texts and the pages
    const QStringList open =
        QString::fromStdString(settings.get<std::string>("print.openSections"))
            .split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const auto& [which, name] : sectionNames()) {
        CollapsibleSection* each = section(which);
        each->setExpanded(open.contains(QLatin1String(name)));
        connect(each, &CollapsibleSection::expandedChanged, this,
                [this]() { rememberSections(); });
    }
    setZoom(valueOfName(zoomNames(), settings.get<std::string>("print.previewZoom"),
                        Zoom::WholeSheet));

    PrintBookDialog::contentChanged();
    layOutNow();
    // Larger than the window must be: the preview gets more than half of it (within the
    // screen, when the window shows)
    resize(std::max(WINDOW_WIDTH,
                    static_cast<int>(area->width() * START_WIDTH_IN_COLUMNS)),
           fontMetrics().height() * START_LINES);
    m_printerBox->setFocus();
}

QGridLayout* PrintBookDialog::newSection(const QString& title)
{
    auto* each = new CollapsibleSection(title, m_sectionsColumn);
    static_cast<QVBoxLayout*>(m_sectionsColumn->layout())->addWidget(each);
    m_sections.append(each);
    return each->grid();
}

void PrintBookDialog::addPrinterSection()
{
    const auto& settings = core::SettingsManager::getInstance();
    // The printer, its state and its window; the copies, the paper source and the quality
    //: A section of the window of the print. In Polish: &Drukarka
    QGridLayout* printer = newSection(tr("Pri&nter"));
    QWidget* page = printer->parentWidget();
    m_printerBox = new QComboBox(page);
    for (const Printer& each : std::as_const(m_printers)) {
        //: The default printer of the system in the list of the printers; %1: its name. In
        //: Polish: %1 (domyślna)
        m_printerBox->addItem(each.isDefault ? tr("%1 (default)").arg(each.name) : each.name);
        m_printerBox->setItemData(m_printerBox->count() - 1, m_printerBox->itemText(
                                      m_printerBox->count() - 1), Qt::ToolTipRole);
    }
    // A long name is cut in the field, whole in the list: the column keeps its width
    m_printerBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_printerBox->setMinimumContentsLength(PRINTER_NAME_LETTERS);
    m_printerBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_printerBox->setEnabled(!m_printers.isEmpty());
    //: Opens the window of the driver of the printer. In Polish: Właściwości dr&ukarki...
    m_driverButton = new QPushButton(tr("Printer P&roperties..."), page);
    m_driverButton->setAutoDefault(false);
    auto* printerLine = new QWidget(page);
    auto* printerLayout = new QHBoxLayout(printerLine);
    printerLayout->setContentsMargins(0, 0, 0, 0);
    printerLayout->setSpacing(fontMetrics().averageCharWidth() * 2);
    m_printerBox->setParent(printerLine);
    m_driverButton->setParent(printerLine);
    printerLayout->addWidget(m_printerBox, 1);
    printerLayout->addWidget(m_driverButton);
    // The section's header has the access key of the list
    //: The label of the list of the printers, without an access key: the section has it. In
    //: Polish: Drukarka:
    addRow(printer, tr("Printer:"), printerLine)->setBuddy(m_printerBox);
    m_printerNote = addNote(printer);

    m_copiesBox = new QSpinBox(page);
    m_copiesBox->setRange(1, MAX_COPIES);
    //: Each copy whole before the next. In Polish: &Sortuj kopie
    m_collateBox = new QCheckBox(tr("C&ollate"), page);
    m_collateBox->setChecked(settings.get<bool>("print.collate"));
    //: In Polish: Kopi&e:
    addRow(printer, tr("&Copies:"), newLine(page, {m_copiesBox, m_collateBox}))
        ->setBuddy(m_copiesBox);

    // The paper source and the quality, when the system lists them
    // Their rows hide with their lines: the label of each row has its line, which gives the
    // focus to the list
    m_sourceBox = new QComboBox(page);
    m_sourceBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    QWidget* sourceLine = newLine(page, {m_sourceBox});
    sourceLine->setFocusProxy(m_sourceBox);
    //: The tray the paper comes from. In Polish: &Podajnik:
    addRow(printer, tr("Paper so&urce:"), sourceLine);
    m_qualityBox = new QComboBox(page);
    m_qualityBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    QWidget* qualityLine = newLine(page, {m_qualityBox});
    qualityLine->setFocusProxy(m_qualityBox);
    //: The resolution of the print. In Polish: J&akość:
    addRow(printer, tr("&Quality:"), qualityLine);
    endTab(printer);
}

void PrintBookDialog::addPagesSection()
{
    const auto& settings = core::SettingsManager::getInstance();
    // All the pages, the current one or a range; the odd or the even ones; in the reverse
    // order or not
    //: A section of the window of the print: which pages print. In Polish: Stro&ny
    QGridLayout* pages = newSection(tr("Pa&ges"));
    QWidget* page = pages->parentWidget();
    //: Which pages print: all of them. In Polish: &Wszystkie
    m_allButton = new QRadioButton(tr("&All"), page);
    m_currentButton = new QRadioButton(page);
    //: Which pages print: those typed in the field beside. In Polish: &Zakres:
    m_rangeButton = new QRadioButton(tr("&Range:"), page);
    m_rangeField = new QLineEdit(page);
    //: The field of the range of the pages, while it is empty. In Polish: np. 3–5, 9
    m_rangeField->setPlaceholderText(tr("e.g. 3-5, 9"));
    // Wide enough for its example, with the room of its frame
    const QFontMetrics rangeMetrics = m_rangeField->fontMetrics();
    m_rangeField->setFixedWidth(
        std::max(widthOfLetters(rangeMetrics, RANGE_LETTERS),
                 rangeMetrics.horizontalAdvance(m_rangeField->placeholderText()) +
                     widthOfLetters(rangeMetrics, RANGE_FRAME_LETTERS)));
    //: The name of the field of the range of the pages for screen readers. In Polish:
    //: Zakres stron
    m_rangeField->setAccessibleName(tr("Range of the pages"));
    m_allButton->setChecked(true);
    int row = 0;
    pages->addWidget(newLine(page, {m_allButton, m_currentButton, m_rangeButton, m_rangeField}),
                     row++, 0, 1, 2);
    m_rangeNote.widget = newNote(page, &m_rangeNote.icon, &m_rangeNote.text);
    m_rangeNote.widget->hide();
    pages->addWidget(m_rangeNote.widget, row++, 0, 1, 2);

    // The odd or the even pages beside the reverse order, without a label of their own
    m_parityBox = new QComboBox(page);
    //: Which of the pages print. In Polish: Wszystkie strony
    m_parityBox->addItem(tr("All pages"), static_cast<int>(editor::PageParity::All));
    //: Which of the pages print. In Polish: Tylko nieparzyste
    m_parityBox->addItem(tr("Only odd pages"), static_cast<int>(editor::PageParity::Odd));
    //: Which of the pages print. In Polish: Tylko parzyste
    m_parityBox->addItem(tr("Only even pages"), static_cast<int>(editor::PageParity::Even));
    m_parityBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: The name of the list of the odd and the even pages for screen readers. In Polish:
    //: Strony nieparzyste i parzyste
    m_parityBox->setAccessibleName(tr("Odd and even pages"));
    //: Beside the odd and the even pages: the last sheet first, for a printer that lays the
    //: sheets face up. In Polish: W odwrotne&j kolejności
    m_reverseBox = new QCheckBox(tr("Re&verse order"), page);
    m_reverseBox->setChecked(settings.get<bool>("print.reverse"));
    pages->addWidget(newLine(page, {m_parityBox, m_reverseBox}), row++, 0, 1, 2);
    endTab(pages);
}

void PrintBookDialog::addSheetSection()
{
    const auto& settings = core::SettingsManager::getInstance();
    // The paper, the color and both sides; the pages on a sheet, and their size
    //: A section of the window of the print: the sheet of the printer. In Polish: &Kartka
    QGridLayout* sheet = newSection(tr("&Sheet"));
    QWidget* page = sheet->parentWidget();
    m_paperBox = new QComboBox(page);
    m_paperBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: In Polish: P&apier:
    addRow(sheet, tr("&Paper:"), newLine(page, {m_paperBox}))->setBuddy(m_paperBox);
    m_colorBox = new QComboBox(page);
    m_colorBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: In Polish: Kolo&r:
    addRow(sheet, tr("&Color:"), newLine(page, {m_colorBox}))->setBuddy(m_colorBox);
    m_duplexBox = new QComboBox(page);
    m_duplexBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: Printing on both sides of the sheets. In Polish: D&wustronnie:
    addRow(sheet, tr("Two-si&ded:"), newLine(page, {m_duplexBox}))->setBuddy(m_duplexBox);
    m_duplexNote = addNote(sheet);

    int row = sheet->rowCount();
    //: Over the pages on a sheet. In Polish: Strony książki na kartce
    sheet->addWidget(newRemark(page, tr("Pages of the book on a sheet")), row++, 0, 1, 2);
    //: In Polish: &Jedna strona
    m_onePageButton = new QRadioButton(tr("&One page"), page);
    //: In Polish: Dwie strony o&bok siebie
    m_twoPagesButton = new QRadioButton(tr("Two pages side &by side"), page);
    //: In Polish: Bro&szura do złożenia na pół
    m_bookletButton = new QRadioButton(tr("Booklet to fold &in half"), page);
    sheet->addWidget(m_onePageButton, row++, 0, 1, 2);
    sheet->addWidget(m_twoPagesButton, row++, 0, 1, 2);
    sheet->addWidget(indented(newRemark(page,
                                        //: Under two pages side by side. In Polish: Jak otwarta
                                        //: książka: strona 1 sama, po prawej; dalej 2–3, 4–5...
                                        tr("As an open book: page 1 alone on the right, then "
                                           "2–3, 4–5...")),
                              m_twoPagesButton),
                     row++, 0, 1, 2);
    sheet->addWidget(m_bookletButton, row++, 0, 1, 2);
    sheet->addWidget(indented(newRemark(page,
                                        //: Under the booklet. In Polish: Po złożeniu na pół
                                        //: stos kartek czyta się po kolei.
                                        tr("Folded in half, the stack of the sheets reads in "
                                           "order.")),
                              m_bookletButton),
                     row++, 0, 1, 2);
    m_spreadNote = addNote(sheet);  // where the halves of a spread lie (showSide())
    row = sheet->rowCount();

    //: Over the size of the pages on a sheet. In Polish: Rozmiar
    sheet->addWidget(newRemark(page, tr("Size")), row++, 0, 1, 2);
    // A widget of their own: the radio buttons of the size go apart from those of the pages
    auto* sizeBox = new QWidget(page);
    auto* sizeLayout = new QGridLayout(sizeBox);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    //: In Polish: Do&pasuj do kartki
    m_fitButton = new QRadioButton(tr("&Fit to the sheet"), sizeBox);
    //: The page of its own size in the middle of the sheet; %1: its size (A5). In Polish:
    //: R&zeczywisty rozmiar (%1)
    m_actualSizeButton =
        new QRadioButton(tr("&Actual size (%1)").arg(literal(pageSizeName())), sizeBox);
    //: The marks of the corners of a page, where the sheet is cut. In Polish: Znaczniki
    //: &cięcia
    m_cropMarksBox = new QCheckBox(tr("C&rop marks"), sizeBox);
    sizeLayout->addWidget(m_fitButton, 0, 0, 1, 2);
    sizeLayout->addWidget(m_actualSizeButton, 1, 0);
    sizeLayout->addWidget(m_cropMarksBox, 1, 1);
    sizeLayout->setColumnStretch(2, 1);
    sheet->addWidget(sizeBox, row++, 0, 1, 2);
    endTab(sheet);

    const editor::SheetLayout sheetLayout =
        valueOfName(sheetLayoutNames(), settings.get<std::string>("print.sheet.layout"),
                    editor::SheetLayout::OnePage);
    sheetButton(sheetLayout)->setChecked(true);
    (settings.get<bool>("print.sheet.fit") ? m_fitButton : m_actualSizeButton)->setChecked(true);
    m_cropMarksBox->setChecked(settings.get<bool>("print.sheet.cropMarks"));
}

void PrintBookDialog::addBookPageSection()
{
    // The size and the margins of the page of the book: the settings of the editor
    //: A section of the window of the print: the size and the margins of the pages. In
    //: Polish: Strona ks&iążki
    QGridLayout* grid = newSection(tr("Boo&k Page"));
    QWidget* page = grid->parentWidget();
    m_pageSizeBox = new QComboBox(page);
    //: A size of the page of the book. In Polish: A4 (210 × 297 mm)
    m_pageSizeBox->addItem(tr("A4 (210 x 297 mm)"), QStringLiteral("A4"));
    //: A size of the page of the book. In Polish: A5 (148 × 210 mm)
    m_pageSizeBox->addItem(tr("A5 (148 x 210 mm)"), QStringLiteral("A5"));
    //: A size of the page of the book. In Polish: B5 (176 × 250 mm)
    m_pageSizeBox->addItem(tr("B5 (176 x 250 mm)"), QStringLiteral("B5"));
    //: A size of the page of the book. In Polish: 6 × 9 cali (152 × 229 mm)
    m_pageSizeBox->addItem(tr("6 x 9 in (152 x 229 mm)"), QStringLiteral("6x9"));
    //: A size of the page of the book. In Polish: Letter (8,5 × 11 cali)
    m_pageSizeBox->addItem(tr("Letter (8.5 x 11 in)"), QStringLiteral("Letter"));
    //: A size of the page of the book. In Polish: Legal (8,5 × 14 cali)
    m_pageSizeBox->addItem(tr("Legal (8.5 x 14 in)"), QStringLiteral("Legal"));
    //: A size of the page of the book of its own. In Polish: Własny
    m_pageSizeBox->addItem(tr("Custom"), QStringLiteral("Custom"));
    m_pageSizeBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: In Polish: Ro&zmiar:
    addRow(grid, tr("Si&ze:"), newLine(page, {m_pageSizeBox}))->setBuddy(m_pageSizeBox);
    m_pageWidthBox = new LengthSpinBox(LengthUnit::Millimeters, MIN_PAGE_SIDE, MAX_PAGE_SIDE, page);
    m_pageHeightBox =
        new LengthSpinBox(LengthUnit::Millimeters, MIN_PAGE_SIDE, MAX_PAGE_SIDE, page);
    //: The width of a page of its own size. In Polish: Sz&erokość:
    addRow(grid, tr("Wi&dth:"), m_pageWidthBox);
    //: The height of a page of its own size. In Polish: Wy&sokość:
    addRow(grid, tr("&Height:"), m_pageHeightBox);

    // The margins two in a row, in the order of Settings: the top and the bottom one, the
    // left and the right one (or the inner and the outer one)
    auto* margins = new QWidget(page);
    auto* marginsLayout = new QGridLayout(margins);
    marginsLayout->setContentsMargins(0, 0, 0, 0);
    const std::array<std::pair<Margin, QString>, 6> labels = {{
        //: A margin of the page. In Polish: &Górny
        {Margin::Top, tr("&Top")},
        //: A margin of the page. In Polish: Doln&y
        {Margin::Bottom, tr("B&ottom")},
        //: A margin of the page. In Polish: Le&wy
        {Margin::Left, tr("Le&ft")},
        //: A margin of the page. In Polish: &Prawy
        {Margin::Right, tr("&Right")},
        //: A margin of the page at the binding. In Polish: Wewnę&trzny
        {Margin::Inner, tr("&Inner")},
        //: A margin of the page at the edge. In Polish: Zewnęt&rzny
        {Margin::Outer, tr("O&uter")},
    }};
    for (const auto& [margin, text] : labels) {
        auto* field = new LengthSpinBox(LengthUnit::Millimeters, 0.0, MAX_MARGIN, margins);
        auto* label = new QLabel(text, margins);
        label->setBuddy(field);
        // The inner one where the left one is: one of them shows
        int place = 0;
        switch (margin) {
            case Margin::Top:
                place = 0;
                break;
            case Margin::Bottom:
                place = 1;
                break;
            case Margin::Left:
            case Margin::Inner:
                place = 2;
                break;
            case Margin::Right:
            case Margin::Outer:
                place = 3;
                break;
        }
        marginsLayout->addWidget(label, place / 2, (place % 2) * 2);
        marginsLayout->addWidget(field, place / 2, (place % 2) * 2 + 1);
        m_marginBoxes.append(field);
        const std::string key = marginKeys().at(m_marginBoxes.size() - 1);
        connect(field, &QDoubleSpinBox::valueChanged, this, [field, key]() {
            core::SettingsManager::getInstance().set<double>(key, field->storedValue());
        });
    }
    marginsLayout->setColumnStretch(4, 1);
    //: Over the margins of the page. In Polish: Marginesy
    grid->addWidget(newRemark(page, tr("Margins")), grid->rowCount(), 0, 1, 2);
    grid->addWidget(margins, grid->rowCount(), 0, 1, 2);
    //: Mirror margins of the page. In Polish: L&ustrzane marginesy (lewe i prawe strony)
    m_mirrorBox = new QCheckBox(tr("&Mirror margins (left and right pages)"), page);
    grid->addWidget(m_mirrorBox, grid->rowCount(), 0, 1, 2);
    // Under the whole section, as the remarks over its parts
    FieldNote shared;
    shared.widget = newNote(page, &shared.icon, &shared.text);
    grid->addWidget(shared.widget, grid->rowCount(), 0, 1, 2);
    //: Under the page of the book in the window of the print. In Polish: Te same ustawienia
    //: co Ustawienia › Edytor › Strony i marginesy: zmiana dotyczy też widoku stron i eksportu
    //: do PDF.
    showNote(shared,
             tr("The same settings as Settings › Editor › Pages and Margins: a change changes "
                "also the view of the pages and the export to PDF."),
             QStringLiteral("common.warning"));
    endTab(grid);

    connect(m_pageSizeBox, &QComboBox::currentIndexChanged, this, [this]() {
        core::SettingsManager::getInstance().set<std::string>(
            "editor.page.size", m_pageSizeBox->currentData().toString().toStdString());
        showBookPage();
    });
    connect(m_pageWidthBox, &QDoubleSpinBox::valueChanged, this, [this]() {
        core::SettingsManager::getInstance().set<double>("editor.page.customWidth",
                                                         m_pageWidthBox->storedValue());
    });
    connect(m_pageHeightBox, &QDoubleSpinBox::valueChanged, this, [this]() {
        core::SettingsManager::getInstance().set<double>("editor.page.customHeight",
                                                         m_pageHeightBox->storedValue());
    });
    connect(m_mirrorBox, &QCheckBox::toggled, this, [this](bool mirrored) {
        core::SettingsManager::getInstance().set<bool>("editor.margins.mirrorEnabled",
                                                       mirrored);
        showBookPage();
    });
}

QWidget* PrintBookDialog::newPreview()
{
    // The preview of the sheets: its caption and its zoom over it, the sheet shown and its
    // buttons under it
    auto* previewBox = new QWidget(this);
    auto* previewLayout = new QVBoxLayout(previewBox);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    auto* top = new QHBoxLayout();
    top->setContentsMargins(0, 0, 0, 0);
    //: The caption of the preview of the sheets. In Polish: Podgląd
    auto* caption = new QLabel(tr("Preview"), previewBox);
    QFont bold = caption->font();
    bold.setBold(true);
    caption->setFont(bold);
    top->addWidget(caption);
    top->addStretch(1);
    m_wholeSheetButton = new QToolButton(previewBox);
    //: The zoom of the preview: the whole sheet. In Polish: Cała kartka
    m_wholeSheetButton->setText(tr("Whole Sheet"));
    m_widthButton = new QToolButton(previewBox);
    //: The zoom of the preview: the sheet as wide as the preview. In Polish: Szerokość
    m_widthButton->setText(tr("Width"));
    // One of them checked
    auto* zoom = new QButtonGroup(previewBox);
    zoom->addButton(m_wholeSheetButton);
    zoom->addButton(m_widthButton);
    for (QToolButton* button : {m_wholeSheetButton, m_widthButton}) {
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::StrongFocus);
        top->addWidget(button);
    }
    //: The tooltip of the zoom of the preview. In Polish: Cała kartka w podglądzie
    m_wholeSheetButton->setToolTip(tr("The whole sheet in the preview"));
    //: The tooltip of the zoom of the preview. In Polish: Kartka na szerokość podglądu
    m_widthButton->setToolTip(tr("The sheet as wide as the preview"));
    connect(m_wholeSheetButton, &QToolButton::clicked, this,
            [this]() { setZoom(Zoom::WholeSheet); });
    connect(m_widthButton, &QToolButton::clicked, this, [this]() { setZoom(Zoom::Width); });
    previewLayout->addLayout(top);

    auto* sheetRow = new QHBoxLayout();
    sheetRow->setContentsMargins(0, 0, 0, 0);
    sheetRow->setSpacing(0);
    m_preview = new SheetPreview(previewBox);
    //: The name of the preview of the sheets for screen readers. In Polish: Podgląd kartki
    m_preview->setAccessibleName(tr("Preview of the sheet"));
    //: What the keys of the preview do, for screen readers. In Polish: Page Up i Page Down
    //: zmieniają kartkę
    m_preview->setAccessibleDescription(tr("Page Up and Page Down change the sheet"));
    caption->setBuddy(m_preview);
    m_previewScroll = new QScrollBar(Qt::Vertical, previewBox);
    m_previewScroll->setFocusPolicy(Qt::NoFocus);
    m_preview->setScrollBar(m_previewScroll);
    sheetRow->addWidget(m_preview, 1);
    sheetRow->addWidget(m_previewScroll);
    previewLayout->addLayout(sheetRow, 1);

    auto* navigation = new QHBoxLayout();
    navigation->setContentsMargins(0, 0, 0, 0);
    m_previousButton = new QToolButton(previewBox);
    m_previousButton->setArrowType(Qt::LeftArrow);
    //: A button under the preview. In Polish: Poprzednia kartka
    m_previousButton->setToolTip(tr("Previous Sheet"));
    m_previousButton->setAccessibleName(m_previousButton->toolTip());
    m_nextButton = new QToolButton(previewBox);
    m_nextButton->setArrowType(Qt::RightArrow);
    //: A button under the preview. In Polish: Następna kartka
    m_nextButton->setToolTip(tr("Next Sheet"));
    m_nextButton->setAccessibleName(m_nextButton->toolTip());
    m_previewLabel = new QLabel(previewBox);
    m_previewLabel->setTextFormat(Qt::PlainText);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setWordWrap(true);
    // To the sheet of a spread of the book (only when it has one)
    m_spreadButton = new QToolButton(previewBox);
    m_spreadButton->setObjectName(QStringLiteral("printSpreadButton"));
    //: A button under the preview: to the sheet with the spread in front (or the next one).
    //: In Polish: Do rozkładówki
    m_spreadButton->setText(tr("To the Spread"));
    m_spreadButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    //: In Polish: Kartka z bieżącą rozkładówką, a kolejne naciśnięcia – z następnymi
    m_spreadButton->setToolTip(tr("The sheet with the current spread; pressed again, with the "
                                  "next ones"));
    m_spreadButton->setAccessibleName(m_spreadButton->text());
    m_spreadButton->setVisible(false);
    navigation->addWidget(m_previousButton);
    navigation->addWidget(m_previewLabel, 1);
    navigation->addWidget(m_nextButton);
    navigation->addWidget(m_spreadButton);
    previewLayout->addLayout(navigation);
    return previewBox;
}

CollapsibleSection* PrintBookDialog::section(Section which) const
{
    const auto index = static_cast<qsizetype>(which);
    return index < m_sections.size() ? m_sections.at(index) : nullptr;
}

LengthSpinBox* PrintBookDialog::marginBox(Margin margin) const
{
    const auto index = static_cast<qsizetype>(margin);
    return index < m_marginBoxes.size() ? m_marginBoxes.at(index) : nullptr;
}

QAbstractButton* PrintBookDialog::zoomButton(Zoom zoom) const
{
    return zoom == Zoom::Width ? m_widthButton : m_wholeSheetButton;
}

void PrintBookDialog::setZoom(Zoom zoom)
{
    m_zoom = zoom;
    zoomButton(zoom)->setChecked(true);
    m_preview->setZoom(zoom);
    core::SettingsManager::getInstance().set<std::string>("print.previewZoom",
                                                          nameOfValue(zoomNames(), zoom));
}

QString PrintBookDialog::sectionNote(const FieldNote& note) const
{
    for (const QWidget* each = note.widget->parentWidget(); each != nullptr;
         each = each->parentWidget()) {
        if (const auto* section = qobject_cast<const CollapsibleSection*>(each)) {
            return note.widget->isVisibleTo(section->body()) ? note.text->text() : QString();
        }
    }
    return noteText(note);
}

void PrintBookDialog::setDriverWindow(DriverWindow window)
{
    m_driverWindow = std::move(window);
    showPrinter();
}

void PrintBookDialog::showDriverWindow()
{
    if (!m_driverWindow || currentPrinter() == nullptr) {
        return;
    }
    const std::optional<DriverChoice> chosen = m_driverWindow(choice(), this);
    if (chosen) {
        dropFailure();
        showDriverChoice(*chosen);
    }
}

void PrintBookDialog::showDriverChoice(const DriverChoice& chosen)
{
    m_driverSettings = chosen.settings;
    const Printer* printer = currentPrinter();
    if (chosen.paper.isValid() && printer != nullptr) {
        for (int i = 0; i < m_paperBox->count(); ++i) {
            const int paper = m_paperBox->itemData(i).toInt();
            if (paper >= 0 && paper < printer->papers.size() &&
                printer->papers.at(paper).isEquivalentTo(chosen.paper)) {
                m_paperBox->setCurrentIndex(i);
                break;
            }
        }
    }
    if (chosen.copies > 0) {
        m_copiesBox->setValue(std::min(chosen.copies, MAX_COPIES));
    }
    if (chosen.collate) {
        m_collateBox->setChecked(*chosen.collate);
    }
    if (chosen.color) {
        if (const int index = m_colorBox->findData(*chosen.color); index >= 0) {
            m_colorBox->setCurrentIndex(index);
        }
    }
    // A booklet keeps its way of both sides
    if (chosen.duplex && !m_bookletButton->isChecked()) {
        if (const int index = m_duplexBox->findData(static_cast<int>(*chosen.duplex));
            index >= 0) {
            m_duplexBox->setCurrentIndex(index);
        }
    }
    if (chosen.source >= 0) {
        if (const int index = m_sourceBox->findData(chosen.source); index >= 0) {
            m_sourceBox->setCurrentIndex(index);
        }
    }
    if (chosen.quality.isValid()) {
        if (const int index = m_qualityBox->findData(chosen.quality); index >= 0) {
            m_qualityBox->setCurrentIndex(index);
        }
    }
    showPages();
}

void PrintBookDialog::showSourcesAndQualities(const Printer* printer)
{
    // The paper sources and the qualities the system lists, the printer's own at first; none:
    // the rows go
    {
        const QSignalBlocker blocker(m_sourceBox);
        m_sourceBox->clear();
        if (printer != nullptr) {
            for (const PaperSource& source : printer->sources) {
                m_sourceBox->addItem(source.name, source.id);
            }
            m_sourceBox->setCurrentIndex(std::max(0, m_sourceBox->findData(printer->source)));
        }
        setRowVisible(m_sourceBox->parentWidget(), m_sourceBox->count() > 0);
    }
    {
        const QSignalBlocker blocker(m_qualityBox);
        m_qualityBox->clear();
        if (printer != nullptr && !printer->qualities.isEmpty()) {
            //: The quality of the print as the printer sets it. In Polish: Ustawienie drukarki
            m_qualityBox->addItem(tr("As the printer sets it"), QSize());
            for (const QSize& quality : printer->qualities) {
                m_qualityBox->addItem(qualityName(quality), quality);
            }
            m_qualityBox->setCurrentIndex(
                printer->quality.isValid()
                    ? std::max(0, m_qualityBox->findData(printer->quality))
                    : 0);
        }
        setRowVisible(m_qualityBox->parentWidget(), m_qualityBox->count() > 0);
    }
}

void PrintBookDialog::showBookPage()
{
    if (m_pageSizeBox == nullptr) {
        return;
    }
    const auto& settings = core::SettingsManager::getInstance();
    const QString size = QString::fromStdString(settings.get<std::string>("editor.page.size"));
    const bool mirrored = settings.get<bool>("editor.margins.mirrorEnabled");
    const LengthUnit unit = currentLengthUnit();
    {
        const QSignalBlocker blocker(m_pageSizeBox);
        m_pageSizeBox->setCurrentIndex(std::max(0, m_pageSizeBox->findData(size)));
    }
    // A field that already shows its value stays as it is, so that its text does not change
    // while it is typed
    const auto show = [unit](LengthSpinBox* field, double value) {
        if (field->displayUnit() == unit && std::abs(field->storedValue() - value) < SAME_LENGTH) {
            return;
        }
        const QSignalBlocker blocker(field);
        field->setDisplayUnit(unit);
        field->setStoredValue(value);
    };
    show(m_pageWidthBox, settings.get<double>("editor.page.customWidth"));
    show(m_pageHeightBox, settings.get<double>("editor.page.customHeight"));
    for (qsizetype i = 0; i < m_marginBoxes.size(); ++i) {
        show(m_marginBoxes.at(i),
             settings.get<double>(marginKeys().at(static_cast<size_t>(i))));
    }
    {
        const QSignalBlocker blocker(m_mirrorBox);
        m_mirrorBox->setChecked(mirrored);
    }
    // The width and the height for a size of its own; the inner and the outer margin for
    // mirror margins, else the left and the right one
    const bool custom = m_pageSizeBox->currentData().toString() == QLatin1String("Custom");
    setRowVisible(m_pageWidthBox, custom);
    setRowVisible(m_pageHeightBox, custom);
    for (const auto& [margin, shown] :
         {std::pair{Margin::Left, !mirrored}, std::pair{Margin::Right, !mirrored},
          std::pair{Margin::Inner, mirrored}, std::pair{Margin::Outer, mirrored}}) {
        LengthSpinBox* field = marginBox(margin);
        field->setVisible(shown);
        for (QLabel* label : field->parentWidget()->findChildren<QLabel*>()) {
            if (label->buddy() == field) {
                label->setVisible(shown);
            }
        }
    }
}

void PrintBookDialog::showSummaries()
{
    if (m_layoutTimer == nullptr) {
        return;  // the window is not made yet
    }
    const Choice now = choice();
    const QString dot = QStringLiteral(" · ");

    // The printer, and the copies when there are more
    const Printer* printer = currentPrinter();
    QString printerSummary;
    if (printer == nullptr) {
        //: The summary of the section of the printer without one. In Polish: Brak drukarki
        printerSummary = tr("No printer");
    } else if (now.copies > 1) {
        //: The summary of the section of the printer; %1: the printer, %n: the copies, more
        //: than one. In Polish: %1 · %n kopie / kopii
        printerSummary = tr("%1 · %n copies", nullptr, now.copies).arg(printer->name);
    } else {
        printerSummary = printer->name;
    }
    section(Section::Printer)->setSummary(printerSummary);

    // What prints, of which volume
    QString content;
    switch (now.content) {
        case Content::Front:
            //: The summary of what prints. In Polish: Tekst na wierzchu
            content = tr("Text in front");
            break;
        case Content::Book:
            //: The summary of what prints. In Polish: Cała książka
            content = tr("Whole book");
            break;
        case Content::Chosen:
            //: The summary of what prints. In Polish: %n wybrany tekst / wybrane teksty /
            //: wybranych tekstów
            content = tr("%n chosen texts", nullptr, static_cast<int>(now.chosen.size()));
            break;
        case Content::Set:
            //: The summary of what prints. In Polish: Zestaw dla wydawnictwa
            content = tr("Set for the publisher");
            break;
    }
    if (volumeBox() != nullptr && now.content != Content::Front) {
        content += dot + volumeBox()->currentText();
    }
    section(Section::Content)->setSummary(content);

    // The pages
    QString pages;
    switch (now.range) {
        case PageRange::All:
            //: The summary of the pages: all of them. In Polish: Wszystkie
            pages = tr("All");
            break;
        case PageRange::Current:
            pages = m_currentButton->text().remove(QLatin1Char('&'));
            break;
        case PageRange::Typed:
            //: The summary of the pages: a range; %1: as typed. In Polish: Zakres: %1
            pages = tr("Range: %1").arg(now.rangeText.trimmed());
            break;
    }
    if (now.parity == editor::PageParity::Odd) {
        //: The summary of the pages: the odd ones. In Polish: tylko nieparzyste
        pages += dot + tr("odd only");
    } else if (now.parity == editor::PageParity::Even) {
        //: The summary of the pages: the even ones. In Polish: tylko parzyste
        pages += dot + tr("even only");
    }
    if (now.reverse) {
        //: The summary of the pages: in the reverse order. In Polish: od końca
        pages += dot + tr("from the end");
    }
    section(Section::Pages)->setSummary(pages);

    // The sheet: the paper, the pages on it, their size, both sides
    QString sheet = m_paperBox->currentText();
    switch (now.sheet.layout) {
        case editor::SheetLayout::OnePage:
            //: The summary of the sheet. In Polish: jedna strona
            sheet += dot + tr("one page") + dot;
            if (now.sheet.fit) {
                //: The summary of the sheet: the page fitted to it. In Polish: dopasowana
                sheet += tr("fitted");
            } else {
                //: The summary of the sheet: the page of its own size. In Polish: rzeczywisty
                //: rozmiar
                sheet += tr("actual size");
            }
            break;
        case editor::SheetLayout::TwoPages:
            //: The summary of the sheet. In Polish: dwie strony obok siebie
            sheet += dot + tr("two pages side by side");
            break;
        case editor::SheetLayout::Booklet:
            //: The summary of the sheet. In Polish: broszura
            sheet += dot + tr("booklet");
            break;
    }
    if (twoSided()) {
        //: The summary of the sheet: printed on both sides. In Polish: dwustronnie
        sheet += dot + tr("two-sided");
    }
    section(Section::Sheet)->setSummary(sheet);

    // The page of the book: its size and its margins, in the order of the fields; one length
    // when they are all the same
    const auto& settings = core::SettingsManager::getInstance();
    const LengthUnit unit = currentLengthUnit();
    const auto length = [&settings, unit](const char* key) {
        return lengthText(settings.get<double>(key), unit);
    };
    const bool mirrored = settings.get<bool>("editor.margins.mirrorEnabled");
    const QStringList margins = {
        length("editor.margins.pageTop"), length("editor.margins.pageBottom"),
        length(mirrored ? "editor.margins.pageInner" : "editor.margins.pageLeft"),
        length(mirrored ? "editor.margins.pageOuter" : "editor.margins.pageRight")};
    const bool same = std::all_of(margins.cbegin(), margins.cend(), [&margins](const QString& each) {
        return each == margins.constFirst();
    });
    //: The summary of the page of the book: its margins; %1: one length, or the top, the
    //: bottom, the left and the right one. In Polish: marginesy %1
    const QString marginsText =
        tr("margins %1").arg(same ? margins.constFirst() : margins.join(QStringLiteral(" / ")));
    section(Section::BookPage)->setSummary(pageSizeName() + dot + marginsText);

    // The look of the text: the font and the running head
    QString font = fontBox()->currentData().toString();
    if (font.isEmpty()) {
        font = QString::fromStdString(settings.get<std::string>("editor.fontFamily"));
    }
    QString head;
    if (now.runningHead == editor::RunningHead::None) {
        //: The summary of the look of the text. In Polish: bez żywej paginy
        head = tr("no running head");
    } else {
        //: The summary of the look of the text. In Polish: żywa pagina
        head = tr("running head");
    }
    section(Section::Look)->setSummary(font + dot + head);
}

void PrintBookDialog::rememberSections() const
{
    QStringList open;
    for (const auto& [which, name] : sectionNames()) {
        if (section(which)->isExpanded()) {
            open << QLatin1String(name);
        }
    }
    core::SettingsManager::getInstance().set<std::string>(
        "print.openSections", open.join(QLatin1Char(',')).toStdString());
}

PrintBookDialog::~PrintBookDialog() = default;

PrintBookDialog::Choice PrintBookDialog::choice() const
{
    Choice choice;
    static_cast<Layout&>(choice) = layoutChoice();
    const Printer* printer = currentPrinter();
    choice.printer = printer != nullptr ? printer->name : QString();
    choice.copies = m_copiesBox->value();
    choice.collate = m_collateBox->isChecked();
    choice.range = PageRange::All;
    if (m_currentButton->isChecked()) {
        choice.range = PageRange::Current;
    } else if (m_rangeButton->isChecked()) {
        choice.range = PageRange::Typed;
    }
    choice.rangeText = m_rangeField->text();
    choice.parity = static_cast<editor::PageParity>(m_parityBox->currentData().toInt());
    choice.reverse = m_reverseBox->isChecked();
    choice.duplex = static_cast<Duplex>(m_duplexBox->currentData().toInt());
    choice.color = m_colorBox->currentData().toBool();
    const int paper = m_paperBox->currentData().toInt();
    choice.paper = printer != nullptr && paper >= 0 && paper < printer->papers.size()
                       ? printer->papers.at(paper)
                       : QPageSize(QPageSize::A4);
    choice.sheet.layout = editor::SheetLayout::OnePage;
    if (m_twoPagesButton->isChecked()) {
        choice.sheet.layout = editor::SheetLayout::TwoPages;
    } else if (m_bookletButton->isChecked()) {
        choice.sheet.layout = editor::SheetLayout::Booklet;
    }
    const bool onePage = choice.sheet.layout == editor::SheetLayout::OnePage;
    choice.sheet.fit = !onePage || m_fitButton->isChecked();
    choice.sheet.cropMarks = !choice.sheet.fit && m_cropMarksBox->isChecked();
    // The paper source and the quality when the system lists them; the driver's own else
    choice.source = m_sourceBox->count() > 0 ? m_sourceBox->currentData().toInt() : -1;
    choice.quality = m_qualityBox->currentData().toSize();
    choice.driverSettings = m_driverSettings;
    return choice;
}

QRadioButton* PrintBookDialog::sheetButton(editor::SheetLayout layout) const
{
    switch (layout) {
        case editor::SheetLayout::TwoPages:
            return m_twoPagesButton;
        case editor::SheetLayout::Booklet:
            return m_bookletButton;
        case editor::SheetLayout::OnePage:
            break;
    }
    return m_onePageButton;
}

int PrintBookDialog::sheetCount() const
{
    return editor::sheetCount(static_cast<int>(m_sides.size()), twoSided());
}

QWidget* PrintBookDialog::preview() const
{
    return m_preview;
}

QString PrintBookDialog::previewMessage() const
{
    return m_preview->message();
}

QString PrintBookDialog::previewText() const
{
    return m_previewLabel->text().replace(NO_BREAK_SPACE, QLatin1Char(' '));
}

void PrintBookDialog::showSide(int side)
{
    if (m_sides.empty()) {
        m_shownSide = -1;
        m_previewLabel->clear();
        m_previousButton->setEnabled(false);
        m_nextButton->setEnabled(false);
        return;
    }
    const int last = static_cast<int>(m_sides.size()) - 1;
    m_shownSide = std::clamp(side, 0, last);
    const editor::SheetSide& shown = m_sides[static_cast<size_t>(m_shownSide)];

    // The sheet and its side; the pages on it
    QStringList numbers;
    for (const int page : shown) {
        if (page >= 0) {
            numbers << pageNumber(page);
        }
    }
    //: A side of a sheet without pages, under the preview. In Polish: pusta
    QString pages = tr("empty");
    if (numbers.size() == 1) {
        //: The page on a side of a sheet, under the preview. In Polish: strona %1
        pages = tr("page %1").arg(numbers.first());
    } else if (numbers.size() > 1) {
        //: The pages on a side of a sheet, under the preview. In Polish: strony %1 i %2
        pages = tr("pages %1 and %2").arg(numbers.at(0), numbers.at(1));
    }
    // The pages in one line under the preview: "page" not apart from its number
    pages.replace(QLatin1Char(' '), NO_BREAK_SPACE);
    const int sheets = sheetCount();
    if (!twoSided()) {
        //: Under the preview; %1: the sheet, %2: the sheets, %3: its pages. In Polish: Kartka
        //: %1 z %2 · %3
        m_previewLabel->setText(
            tr("Sheet %1 of %2 · %3").arg(QString::number(m_shownSide + 1),
                                           QString::number(sheets), pages));
    } else if (m_shownSide % 2 == 0) {
        //: Under the preview; %1: the sheet, %2: the sheets, %3: the pages on its front. In
        //: Polish: Kartka %1 z %2, przód · %3
        m_previewLabel->setText(
            tr("Sheet %1 of %2, front · %3").arg(QString::number(m_shownSide / 2 + 1),
                                                  QString::number(sheets), pages));
    } else {
        //: Under the preview; %1: the sheet, %2: the sheets, %3: the pages on its back. In
        //: Polish: Kartka %1 z %2, tył · %3
        m_previewLabel->setText(
            tr("Sheet %1 of %2, back · %3").arg(QString::number(m_shownSide / 2 + 1),
                                                 QString::number(sheets), pages));
    }
    m_previousButton->setEnabled(m_shownSide > 0);
    m_nextButton->setEnabled(m_shownSide < last);

    const Choice now = choice();
    showSpreadNote(shown, now.sheet.layout);
    SheetPreview::Sheet sheet;
    sheet.pages = m_laidOut.pages;
    sheet.side = shown;
    sheet.paper = paperSize();
    sheet.printable = printableRect();
    sheet.options = now.sheet;
    sheet.gray = !now.color;
    m_preview->setSheet(std::move(sheet));
}

void PrintBookDialog::showSpreadNote(const editor::SheetSide& shown, editor::SheetLayout layout)
{
    const editor::BookPages* pages = m_laidOut.pages.get();
    const std::vector<editor::PageSpread> none;
    const std::vector<editor::PageSpread>& spreads = pages != nullptr ? pages->spreads() : none;
    const auto has = [&shown](int page) {
        return std::find(shown.cbegin(), shown.cend(), page) != shown.cend();
    };
    // The spread with a page on the side shown
    const auto onSide =
        std::find_if(spreads.cbegin(), spreads.cend(), [&has](const editor::PageSpread& spread) {
            return has(spread.page) || has(spread.page + 1);
        });
    const bool chosen =
        std::any_of(spreads.cbegin(), spreads.cend(), [this](const editor::PageSpread& spread) {
            return std::find(m_chosenPages.cbegin(), m_chosenPages.cend(), spread.page) !=
                   m_chosenPages.cend();
        });
    m_spreadButton->setVisible(chosen);
    QString text;
    QString icon;
    if (chosen && layout == editor::SheetLayout::Booklet) {
        //: Under the pages on a sheet, for a book with a spread. In Polish: W broszurze
        //: połówki rozkładówki leżą na różnych kartkach; po złożeniu stykają się w grzbiecie,
        //: tak jak w drukarni.
        text = tr("In a booklet the halves of a spread lie on different sheets; folded, they "
                  "meet at the spine, as at a print shop.");
    } else if (onSide != spreads.cend() && layout == editor::SheetLayout::TwoPages) {
        const QString title = literal(onSide->title);
        if (has(onSide->page) && has(onSide->page + 1)) {
            //: %1: the title of the spread. In Polish: Obie strony rozkładówki „%1” są na tej
            //: kartce.
            text = tr("Both pages of the spread \"%1\" are on this sheet.").arg(title);
        } else {
            //: %1: the title of the spread, %2 and %3: its pages. In Polish: Rozkładówka „%1”
            //: trafia na dwie kartki: strony %2 i %3 nie leżą obok siebie.
            text = tr("The spread \"%1\" goes onto two sheets: pages %2 and %3 do not lie side "
                      "by side.")
                       .arg(title, pageNumber(onSide->page), pageNumber(onSide->page + 1));
            icon = QStringLiteral("common.warning");
        }
    }
    showNote(m_spreadNote, text, icon);
}

void PrintBookDialog::showSpread()
{
    const editor::BookPages* pages = m_laidOut.pages.get();
    if (pages == nullptr || m_sides.empty()) {
        return;
    }
    const auto sideOf = [this](int page) {
        for (size_t side = 0; side < m_sides.size(); ++side) {
            const editor::SheetSide& each = m_sides[side];
            if (std::find(each.cbegin(), each.cend(), page) != each.cend()) {
                return static_cast<int>(side);
            }
        }
        return -1;
    };
    // The sides of the spreads printed, in the order of the print
    std::vector<std::pair<int, int>> sides;  // side, first page
    for (const editor::PageSpread& spread : pages->spreads()) {
        if (const int side = sideOf(spread.page); side >= 0) {
            sides.emplace_back(side, spread.page);
        }
    }
    if (sides.empty()) {
        return;
    }
    std::sort(sides.begin(), sides.end());
    // The spread in front first, unless the side shown has it: then the next one
    const int shownCurrent = sideOf(m_laidOut.current);
    const bool currentIsSpread = std::any_of(
        sides.cbegin(), sides.cend(),
        [this](const std::pair<int, int>& each) { return each.second == m_laidOut.current; });
    if (currentIsSpread && shownCurrent >= 0 && shownCurrent != m_shownSide) {
        showSide(shownCurrent);
        return;
    }
    const auto next =
        std::find_if(sides.cbegin(), sides.cend(),
                     [this](const std::pair<int, int>& each) { return each.first > m_shownSide; });
    showSide(next != sides.cend() ? next->first : sides.front().first);
}

void PrintBookDialog::layOutNow()
{
    m_layoutTimer->stop();
    if (m_pageMaker) {
        // A book of many pages takes a while
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);
        m_laidOut = m_pageMaker(layoutChoice());
        QGuiApplication::restoreOverrideCursor();
    } else {
        m_laidOut = LaidOut();
    }
    showPages();
}

void PrintBookDialog::scheduleLayout()
{
    m_layoutTimer->start();
}

void PrintBookDialog::contentChanged()
{
    const Printer* printer = currentPrinter();
    // The set for a publisher is printed as one text with its title
    const bool front = oneText();
    const bool chosen = chosenButton()->isChecked();
    const QString title = contentTitle();
    QString description;
    if (printer != nullptr && front) {
        //: %1: the title of the text, %2: the printer. In Polish: Drukuje tekst „%1” na
        //: drukarce „%2”.
        description = tr("Prints the text \"%1\" on the printer \"%2\".").arg(title, printer->name);
    } else if (printer != nullptr && chosen) {
        //: %1: the title of the book, %2: the printer. In Polish: Drukuje wybrane teksty
        //: książki „%1” na drukarce „%2”.
        description = tr("Prints the chosen texts of the book \"%1\" on the printer \"%2\".")
                          .arg(title, printer->name);
    } else if (printer != nullptr) {
        //: %1: the title of the book, %2: the printer. In Polish: Drukuje książkę „%1” na
        //: drukarce „%2”.
        description =
            tr("Prints the book \"%1\" on the printer \"%2\".").arg(title, printer->name);
    } else if (front) {
        //: Without a printer; %1: the title of the text. In Polish: Drukuje tekst „%1”.
        description = tr("Prints the text \"%1\".").arg(title);
    } else if (chosen) {
        //: Without a printer; %1: the title of the book. In Polish: Drukuje wybrane teksty
        //: książki „%1”.
        description = tr("Prints the chosen texts of the book \"%1\".").arg(title);
    } else {
        //: Without a printer; %1: the title of the book. In Polish: Drukuje książkę „%1”.
        description = tr("Prints the book \"%1\".").arg(title);
    }
    setHeading(tr("Print"), description);
    showSummaries();
    if (m_layoutTimer != nullptr) {
        scheduleLayout();
    }
}

void PrintBookDialog::updateFields()
{
    // At once what the window can say before the pages are laid out again: no text chosen
    showPages();
    scheduleLayout();
}

void PrintBookDialog::layoutChanged()
{
    // The size of the page in the option of its actual size and the fields of the page, after
    // a change of the settings here or in Settings
    if (m_actualSizeButton != nullptr) {
        m_actualSizeButton->setText(
            tr("&Actual size (%1)").arg(literal(pageSizeName())));
    }
    showBookPage();
    if (m_layoutTimer != nullptr) {
        scheduleLayout();
    }
}

void PrintBookDialog::showPrinter()
{
    const Printer* printer = currentPrinter();
    if (printer == nullptr) {
        //: Under the list of the printers, when the system has none. In Polish: System nie ma
        //: drukarki: dodaj ją w ustawieniach systemu.
        showNote(m_printerNote,
                 tr("The system has no printer: add one in the settings of the system."),
                 QStringLiteral("common.warning"));
    } else {
        QString state;
        switch (printer->state) {
            case PrinterState::Ready:
                //: The state of a printer. In Polish: Gotowa
                state = tr("Ready");
                break;
            case PrinterState::Printing:
                //: The state of a printer. In Polish: Drukuje
                state = tr("Printing");
                break;
            case PrinterState::Stopped:
                //: The state of a printer. In Polish: Zatrzymana
                state = tr("Stopped");
                break;
            case PrinterState::Error:
                //: The state of a printer. In Polish: Błąd
                state = tr("Error");
                break;
        }
        //: Under the list of the printers; %1: its state, %2: its color, %3: its sides. In
        //: Polish: %1 · %2 · %3
        showNote(m_printerNote,
                 tr("%1 · %2 · %3")
                     .arg(state,
                          printer->color
                              //: The color of a printer. In Polish: kolorowa
                              ? tr("color")
                              //: The color of a printer. In Polish: czarno-biała
                              : tr("black and white"),
                          printer->duplex
                              //: The sides of a printer. In Polish: drukuje dwustronnie
                              ? tr("prints on both sides")
                              //: The sides of a printer. In Polish: drukuje jednostronnie
                              : tr("prints on one side")),
                 printer->state == PrinterState::Error ? QStringLiteral("common.warning")
                                                       : QString());
    }

    showDuplexChoices(printer);
    showSourcesAndQualities(printer);

    // The window of the driver: where the system has one
    m_driverButton->setEnabled(printer != nullptr && m_driverWindow != nullptr);
    if (m_driverWindow != nullptr) {
        //: The tooltip of Printer Properties... In Polish: Okno sterownika drukarki: jego
        //: własne ustawienia, np. rodzaj papieru i oszczędzanie tonera
        m_driverButton->setToolTip(tr("The window of the printer's driver: its own settings, "
                                      "as the type of paper and saving the toner"));
    } else {
        //: The tooltip of Printer Properties... without such a window. In Polish: Okno
        //: sterownika drukarki jest dostępne tylko w Windows.
        m_driverButton->setToolTip(tr("The window of the printer's driver is there only on "
                                      "Windows."));
    }

    // The color: only gray for a black and white printer
    {
        const QSignalBlocker blocker(m_colorBox);
        m_colorBox->clear();
        if (printer == nullptr || printer->color) {
            //: The color of the print. In Polish: Kolorowo
            m_colorBox->addItem(tr("In color"), true);
        }
        //: The color of the print. In Polish: W odcieniach szarości
        m_colorBox->addItem(tr("In shades of gray"), false);
        m_colorBox->setCurrentIndex(0);
    }

    // The papers of the printer, its own at first
    {
        const QSignalBlocker blocker(m_paperBox);
        m_paperBox->clear();
        if (printer != nullptr) {
            int start = 0;
            for (int i = 0; i < printer->papers.size(); ++i) {
                m_paperBox->addItem(printer->papers.at(i).name(), i);
                if (printer->papers.at(i).isEquivalentTo(printer->paper)) {
                    start = i;
                }
            }
            m_paperBox->setCurrentIndex(start);
        }
        m_paperBox->setEnabled(m_paperBox->count() > 0);
    }
    showDuplex();
}

void PrintBookDialog::showDuplexChoices(const Printer* printer)
{
    // Both sides: by the printer, or by hand; the printer's way at first
    const QSignalBlocker blocker(m_duplexBox);
    const auto was = static_cast<Duplex>(m_duplexBox->currentData().toInt());
    const bool printerTurns = printer != nullptr && printer->duplex;
    m_duplexBox->clear();
    //: Two-sided printing. In Polish: Nie
    m_duplexBox->addItem(tr("No"), static_cast<int>(Duplex::None));
    if (printerTurns) {
        //: Two-sided printing. In Polish: Wzdłuż dłuższej krawędzi
        m_duplexBox->addItem(tr("Along the long edge"), static_cast<int>(Duplex::LongEdge));
        //: Two-sided printing. In Polish: Wzdłuż krótszej krawędzi
        m_duplexBox->addItem(tr("Along the short edge"), static_cast<int>(Duplex::ShortEdge));
    } else {
        //: Two-sided printing without a printer that does it. In Polish: Ręcznie – program
        //: poprosi o odwrócenie kartek
        m_duplexBox->addItem(tr("By hand – Kalahari asks to turn the sheets over"),
                             static_cast<int>(Duplex::Manual));
    }
    // The writer's choice when this printer has it, else the printer's own way
    int index = m_duplexBox->findData(static_cast<int>(was));
    if (index < 0 || was == Duplex::None) {
        index = printerTurns ? 1 : 0;
    }
    m_duplexBox->setCurrentIndex(index);
    m_ownDuplex = static_cast<Duplex>(m_duplexBox->currentData().toInt());
}

void PrintBookDialog::showDuplex()
{
    const Printer* printer = currentPrinter();
    const bool printerTurns = printer != nullptr && printer->duplex;
    const bool booklet = m_bookletButton->isChecked();
    // A booklet prints on both sides, along the short edge; the other sheets as the writer
    // chose
    {
        const QSignalBlocker blocker(m_duplexBox);
        Duplex shown = m_ownDuplex;
        if (booklet) {
            shown = printerTurns ? Duplex::ShortEdge : Duplex::Manual;
        }
        m_duplexBox->setCurrentIndex(std::max(0, m_duplexBox->findData(static_cast<int>(shown))));
    }
    m_duplexBox->setEnabled(!booklet);
    const auto duplex = static_cast<Duplex>(m_duplexBox->currentData().toInt());
    if (booklet && printerTurns) {
        //: Under both sides, for a booklet. In Polish: Broszura drukuje się dwustronnie,
        //: wzdłuż krótszej krawędzi.
        showNote(m_duplexNote, tr("A booklet prints on both sides, along the short edge."));
    } else if (booklet) {
        //: Under both sides, for a booklet without a printer that prints on both sides. In
        //: Polish: Broszura: najpierw przody kartek, potem program poprosi o odwrócenie stosu.
        showNote(m_duplexNote, tr("A booklet: the fronts of the sheets first, then Kalahari asks "
                                  "to turn the stack over."));
    } else if (duplex == Duplex::Manual) {
        //: Under both sides by hand. In Polish: Program wydrukuje przody kartek, a potem
        //: w swoim oknie poprosi o odwrócenie stosu i wydrukuje tyły.
        showNote(m_duplexNote, tr("Kalahari prints the fronts of the sheets, then asks in its own "
                                  "window to turn the stack over, and prints the backs."));
    } else {
        showNote(m_duplexNote, QString());
    }
    showPages();
}

void PrintBookDialog::showSheetOptions()
{
    // The size only for one page on a sheet: two pages fit it; the crop marks only for a page
    // of its own size
    const bool onePage = m_onePageButton->isChecked();
    m_fitButton->setEnabled(onePage);
    m_actualSizeButton->setEnabled(onePage);
    m_cropMarksBox->setEnabled(onePage && m_actualSizeButton->isChecked());
    showDuplex();
}

void PrintBookDialog::showPages()
{
    if (m_preview == nullptr) {
        return;  // the window is not made yet
    }
    const editor::BookPages* pages = m_laidOut.pages.get();
    const int count = pages != nullptr ? pages->pageCount() : 0;

    // The page of the cursor
    const int current = m_laidOut.current >= 0 && m_laidOut.current < count ? m_laidOut.current
                                                                             : -1;
    m_currentButton->setText(current >= 0
                                 //: Which pages print: the page of the cursor; %1: its number.
                                 //: In Polish: &Bieżąca (%1)
                                 ? tr("C&urrent (%1)").arg(pageNumber(current))
                                 //: Which pages print: the page of the cursor, when it is not
                                 //: among the pages. In Polish: &Bieżąca
                                 : tr("C&urrent"));
    m_currentButton->setEnabled(current >= 0);
    if (current < 0 && m_currentButton->isChecked()) {
        const QSignalBlocker blocker(m_allButton);
        m_allButton->setChecked(true);
    }
    m_rangeField->setEnabled(m_rangeButton->isChecked());
    m_collateBox->setEnabled(m_copiesBox->value() > 1);

    // The pages chosen, and the sides of their sheets
    const Choice now = choice();
    const bool textsChosen = updateChosen();
    const RangePages range = pagesOfRange(now.range, now.rangeText, count, current);
    const bool rangeTyped = range.typed;
    const bool rangeValid = range.valid;
    m_chosenPages =
        textsChosen ? editor::pagesOfParity(range.pages, now.parity) : std::vector<int>();
    m_sides = editor::sheetSides(m_chosenPages, now.sheet.layout);
    if (now.reverse) {
        m_sides = editor::reversedSheets(m_sides, twoSided());
    }
    showNote(m_rangeNote,
             rangeValid ? QString()
                        //: Under a range of the pages that is not one. In Polish: To nie jest
                        //: zakres stron: wpisz numery i zakresy, np. 3–5, 9.
                        : tr("This is not a range of the pages: type numbers and ranges, e.g. "
                             "3-5, 9."),
             QStringLiteral("common.warning"));

    // The preview: the side shown before, within the sides
    QString message;
    const Printer* printer = currentPrinter();
    if (!textsChosen) {
        //: In the preview, when no text is chosen. In Polish: Nie wybrano żadnego tekstu.
        message = tr("No text is chosen.");
        //: Under the preview, when no text is chosen. In Polish: Nie ma czego drukować:
        //: wybierz teksty w sekcji „Co drukować”.
        m_status = tr("Nothing to print: choose the texts in \"What to Print\".");
    } else if (!rangeTyped) {
        //: In the preview, while the field of the range is empty. In Polish: Wpisz strony
        //: zakresu.
        message = tr("Type the pages of the range.");
        //: Under the tabs, while the field of the range is empty. In Polish: Nie ma czego
        //: drukować: wpisz strony zakresu.
        m_status = tr("Nothing to print: type the pages of the range.");
    } else if (m_sides.empty()) {
        //: In the preview, when no page is chosen. In Polish: Zakres nie obejmuje żadnej
        //: strony.
        message = tr("The range has no page.");
        //: Under the tabs, when no page is chosen. In Polish: Nie ma czego drukować: zmień
        //: zakres stron.
        m_status = tr("Nothing to print: change the range of the pages.");
    } else {
        //: How many pages print. In Polish: %n strona / strony / stron
        const QString pagesText =
            tr("%n pages", nullptr, static_cast<int>(m_chosenPages.size()));
        //: How many sheets the pages print on, after "on". In Polish (miejscownik): %n kartce
        //: / kartkach
        const QString sheetsText = tr("%n sheets", nullptr, sheetCount());
        if (now.copies > 1) {
            //: Under the tabs; %1: the pages, %2: the sheets, %3: the copies. In Polish: %1 na
            //: %2, %3. Postęp i wynik pokażą się tutaj.
            m_status = tr("%1 on %2, %3. The progress and the result will show here.")
                           //: How many copies print. In Polish: %n kopia / kopie / kopii
                           .arg(pagesText, sheetsText, tr("%n copies", nullptr, now.copies));
        } else {
            //: Under the tabs; %1: the pages, %2: the sheets. In Polish: %1 na %2. Postęp i
            //: wynik pokażą się tutaj.
            m_status = tr("%1 on %2. The progress and the result will show here.")
                           .arg(pagesText, sheetsText);
        }
    }
    if (printer == nullptr) {
        //: Under the tabs, when the system has no printer. In Polish: Nie ma drukarki, na
        //: której można drukować.
        m_status = tr("There is no printer to print on.");
    }
    setHint(m_status);
    m_preview->setMessage(message);
    // The first time: the sheet of the spread in front, else the first one
    if (m_shownSide < 0 && m_laidOut.pages != nullptr) {
        const std::vector<editor::PageSpread>& spreads = m_laidOut.pages->spreads();
        if (std::any_of(spreads.cbegin(), spreads.cend(),
                        [this](const editor::PageSpread& spread) {
                            return spread.page == m_laidOut.current;
                        })) {
            showSpread();
        }
    }
    showSide(m_shownSide < 0 ? 0 : m_shownSide);
    acceptButton()->setEnabled(printer != nullptr && !m_sides.empty() && m_printing != nullptr);
    showSummaries();
}

const PrintBookDialog::Printer* PrintBookDialog::currentPrinter() const
{
    const int index = m_printerBox->currentIndex();
    return index >= 0 && index < m_printers.size() ? &m_printers.at(index) : nullptr;
}

bool PrintBookDialog::twoSided() const
{
    return static_cast<Duplex>(m_duplexBox->currentData().toInt()) != Duplex::None;
}

QSizeF PrintBookDialog::paperSize() const
{
    const Choice now = choice();
    const QSizeF paper = now.paper.size(QPageSize::Point);
    const QSizeF page = m_laidOut.pages != nullptr ? m_laidOut.pages->pageSize() : QSizeF();
    return editor::sheetSideways(now.sheet, page) ? paper.transposed() : paper;
}

QRectF PrintBookDialog::printableRect() const
{
    const QRectF paper(QPointF(0, 0), paperSize());
    const Printer* printer = currentPrinter();
    if (printer == nullptr || !printer->margins) {
        return paper;
    }
    const Choice now = choice();
    const bool sideways = paper.width() > paper.height();
    const QString key = printer->name + QLatin1Char('\n') + now.paper.key() +
                        (sideways ? QStringLiteral("\nsideways") : QString());
    auto found = m_margins.constFind(key);
    if (found == m_margins.constEnd()) {
        found = m_margins.insert(key, printer->margins(now.paper, sideways));
    }
    return paper.marginsRemoved(*found);
}

void PrintBookDialog::run()
{
    layOutNow();
    if (!acceptButton()->isEnabled() || m_laidOut.pages == nullptr) {
        return;
    }
    rememberChoices();
    const Choice now = choice();
    const Printer* printer = currentPrinter();
    const QString printerName = printer->name;
    const bool both = twoSided();
    const std::vector<editor::SheetSide> sides =
        editor::copiesOf(m_sides, now.copies, now.collate, both);
    const int sheetsOfOneCopy = sheetCount();
    const int pagesOfOneCopy = static_cast<int>(m_chosenPages.size());

    startWork();
    const Progress percent = progress();
    const int total = static_cast<int>(sides.size());
    // The sides of a run, from @p done of all of them: the bar and the sheet sent
    const auto runProgress = [this, &percent, total, both](int done, int sheets,
                                                           bool oneSidePerSheet) {
        return [this, &percent, total, done, sheets, both, oneSidePerSheet](int printed) {
            const int sheet =
                std::min(sheets, (oneSidePerSheet || !both ? printed : printed / 2) + 1);
            //: While the print lasts; %1: the sheet sent, %2: the sheets. In Polish: Wysyłanie
            //: do drukarki: kartka %1 z %2...
            setProgressText(tr("Sending to the printer: sheet %1 of %2...")
                                .arg(QString::number(sheet), QString::number(sheets)));
            percent(total > 0 ? (done + printed) * 100 / total : 100);
            return true;
        };
    };

    QStringList problem;
    bool printed = false;
    bool backsDeclined = false;
    int frontSheets = 0;
    if (now.duplex == Duplex::Manual) {
        // By hand: the fronts, then the backs on the stack the writer turns
        const std::vector<editor::SheetSide> fronts = editor::sidesOfSheets(sides, false);
        const std::vector<editor::SheetSide> backs = editor::sidesOfSheets(sides, true);
        frontSheets = static_cast<int>(fronts.size());
        const auto frontsProgress = runProgress(0, frontSheets, true);
        frontsProgress(0);
        printed = m_printing(now, *m_laidOut.pages, fronts, frontsProgress, &problem);
        if (printed) {
            const bool turned =
                m_turnQuestion ? m_turnQuestion(frontSheets) : askToTurn(frontSheets);
            if (turned) {
                const auto backsProgress =
                    runProgress(static_cast<int>(fronts.size()), frontSheets, true);
                backsProgress(0);
                printed = m_printing(now, *m_laidOut.pages, backs, backsProgress, &problem);
            } else {
                backsDeclined = true;
            }
        }
    } else {
        const auto allProgress = runProgress(0, editor::sheetCount(total, both), false);
        allProgress(0);
        printed = m_printing(now, *m_laidOut.pages, sides, allProgress, &problem);
    }

    if (!printed) {
        //: In Polish: Nie udało się wysłać wydruku do drukarki.
        failWork(tr("The print could not be sent to the printer."), QString(), problem);
        showPages();
        return;
    }
    QString text;
    if (backsDeclined) {
        //: After the print by hand without its backs; %1: the printer. In Polish: Wysłano do
        //: drukarki „%1” przody %n kartki / kartek. Tyłów nie wydrukowano.
        text = tr("Sent to the printer \"%1\" the fronts of %n sheets. The backs were not "
                  "printed.",
                  nullptr, frontSheets)
                   .arg(printerName);
    } else {
        //: How many pages printed. In Polish: %n strona / strony / stron
        const QString pagesText = tr("%n pages", nullptr, pagesOfOneCopy);
        //: How many sheets the pages printed on, after "on". In Polish (miejscownik): %n
        //: kartce / kartkach
        const QString sheetsText = tr("%n sheets", nullptr, sheetsOfOneCopy);
        text = now.copies > 1
                   //: After the print; %1: the printer, %2: the pages, %3: the sheets, %4: the
                   //: copies. In Polish: Wysłano do drukarki „%1”: %2 na %3, %4.
                   ? tr("Sent to the printer \"%1\": %2 on %3, %4.")
                         .arg(printerName, pagesText, sheetsText,
                              //: How many copies printed. In Polish: %n kopia / kopie / kopii
                              tr("%n copies", nullptr, now.copies))
                   //: After the print; %1: the printer, %2: the pages, %3: the sheets. In
                   //: Polish: Wysłano do drukarki „%1”: %2 na %3.
                   : tr("Sent to the printer \"%1\": %2 on %3.")
                         .arg(printerName, pagesText, sheetsText);
    }
    finishWork(text, QString());
    // The result in the place of the bar, which said only that the work went on: the window
    // with the result keeps the height of the screen of 1366x768 at 150%
    progressBar()->hide();
}

bool PrintBookDialog::askToTurn(int sheets)
{
    //: The question of the print by hand, after the fronts; %n: the sheets. In Polish: Przody
    //: kartek (%n) są wydrukowane. Wyjmij stos z drukarki, odwróć go i włóż z powrotem tak,
    //: by pierwsza kartka weszła pierwsza. Potem wydrukuj tyły.
    return MessageDialog::confirm(
        this, tr("Print"),
        tr("The fronts of the sheets (%n) are printed. Take the stack out of the printer, turn "
           "it over and put it back so that the first sheet goes in first. Then print the "
           "backs.",
           nullptr, sheets),
        //: The button of the question of the print by hand. In Polish: Drukuj &tyły
        tr("Print the &Backs"));
}

void PrintBookDialog::rememberChoices() const
{
    auto& settings = core::SettingsManager::getInstance();
    const Choice now = choice();
    rememberLayout("print.content");
    settings.set<bool>("print.collate", m_collateBox->isChecked());
    settings.set<bool>("print.reverse", m_reverseBox->isChecked());
    settings.set<std::string>("print.sheet.layout",
                              nameOfValue(sheetLayoutNames(), now.sheet.layout));
    settings.set<bool>("print.sheet.fit", m_fitButton->isChecked());
    settings.set<bool>("print.sheet.cropMarks", m_cropMarksBox->isChecked());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
