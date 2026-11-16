// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file print_book_dialog.cpp
/// @brief File > Print: the printer, the texts of the book, their layout and the sheets, with
///        a preview of the sheets, and the progress and the result of the print in one window

#include "kalahari/gui/dialogs/print_book_dialog.h"
#include "book_output_parts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_pages.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/dialogs/message_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
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
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

using book_output::nameOfValue;
using book_output::newRemark;
using book_output::valueOfName;
using book_output::widthOfLetters;

/// The width of the preview, in letters of the window's text: a sheet standing upright as
/// high as the tabs
constexpr int PREVIEW_LETTERS = 30;
/// The width of the window: as wide as the screen of 1366x768 at 150% allows (911 pixels
/// with the frame), for the preview beside the tabs
constexpr int WINDOW_WIDTH = 880;
/// The lowest the sheet of the preview gets, in lines of the window's text
constexpr int PREVIEW_LINES = 8;
/// The room around the sheet in the preview, in pixels
constexpr int PREVIEW_MARGIN = 10;
/// The most copies the window prints
constexpr int MAX_COPIES = 99;
/// Below this lightness the window's theme is dark: the preview's desk is the dark one
constexpr int DARK_LIGHTNESS = 128;

/// The pages on a sheet as the settings keep them
const std::array<std::pair<editor::SheetLayout, const char*>, 3>& sheetLayoutNames() {
    static const std::array<std::pair<editor::SheetLayout, const char*>, 3> names = {{
        {editor::SheetLayout::OnePage, "onePage"},
        {editor::SheetLayout::TwoPages, "twoPages"},
        {editor::SheetLayout::Booklet, "booklet"},
    }};
    return names;
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

}  // namespace

// =============================================================================
// SheetPreview
// =============================================================================

/// @brief A side of a sheet as it comes out of the printer, on the desk of the PDF preview
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

    /// @brief Show @p sheet
    void setSheet(Sheet sheet) {
        m_sheet = std::move(sheet);
        m_message.clear();
        redraw();
    }

    /// @brief Show @p text in place of a sheet
    void setMessage(const QString& text) {
        m_sheet = Sheet();
        m_message = text;
        redraw();
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
        if (m_sheet.pages != nullptr && !m_sheet.paper.isEmpty()) {
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
            default:
                QWidget::keyPressEvent(event);
        }
    }

    void resizeEvent(QResizeEvent* event) override {
        m_image = QImage();
        QWidget::resizeEvent(event);
    }

    void changeEvent(QEvent* event) override {
        if (event->type() == QEvent::PaletteChange) {
            m_image = QImage();
        }
        QWidget::changeEvent(event);
    }

private:
    /// @brief The sheet drawn again
    void redraw() {
        m_image = QImage();
        update();
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

    /// @brief Where the sheet shows: as large as the preview allows, in its middle
    QRect sheetRect() const {
        const QRectF room = QRectF(rect()).adjusted(PREVIEW_MARGIN, PREVIEW_MARGIN,
                                                    -PREVIEW_MARGIN, -PREVIEW_MARGIN);
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
    mutable QImage m_image;  ///< The sheet drawn at its size
};

// =============================================================================
// PrintBookDialog
// =============================================================================

PrintBookDialog::PrintBookDialog(const Book& book, const QList<Printer>& printers,
                                 PageMaker pageMaker, Printing printing,
                                 TurnQuestion turnQuestion, QWidget* parent)
    : BookOutputDialog(book, parent)
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

    // The tabs at the left, the preview of the sheets at the right; the progress and the
    // result under them
    auto* body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->addWidget(tabs(), 1);
    fieldsLayout()->addLayout(body);

    // The printer: the printer, the copies, the pages, both sides, the color and the paper
    //: A tab of the window of the print. In Polish: &Drukarka
    QGridLayout* printer = newTab(tr("Pri&nter"));
    m_printerTab = printer->parentWidget();
    m_printerBox = new QComboBox(m_printerTab);
    for (const Printer& each : std::as_const(m_printers)) {
        //: The default printer of the system in the list of the printers; %1: its name. In
        //: Polish: %1 (domyślna)
        m_printerBox->addItem(each.isDefault ? tr("%1 (default)").arg(each.name) : each.name);
    }
    m_printerBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_printerBox->setEnabled(!m_printers.isEmpty());
    //: In Polish: D&rukarka:
    addRow(printer, tr("Prin&ter:"), newLine(m_printerTab, {m_printerBox}))
        ->setBuddy(m_printerBox);
    m_printerNote = addNote(printer);

    m_copiesBox = new QSpinBox(m_printerTab);
    m_copiesBox->setRange(1, MAX_COPIES);
    //: Each copy whole before the next. In Polish: &Sortuj kopie
    m_collateBox = new QCheckBox(tr("C&ollate"), m_printerTab);
    m_collateBox->setChecked(settings.get<bool>("print.collate"));
    //: In Polish: K&opie:
    addRow(printer, tr("&Copies:"), newLine(m_printerTab, {m_copiesBox, m_collateBox}))
        ->setBuddy(m_copiesBox);

    //: Which pages print: all of them. In Polish: &Wszystkie
    m_allButton = new QRadioButton(tr("&All"), m_printerTab);
    m_currentButton = new QRadioButton(m_printerTab);
    //: Which pages print: those typed in the field beside. In Polish: &Zakres:
    m_rangeButton = new QRadioButton(tr("&Range:"), m_printerTab);
    m_rangeField = new QLineEdit(m_printerTab);
    //: The field of the range of the pages, while it is empty. In Polish: np. 3–5, 9
    m_rangeField->setPlaceholderText(tr("e.g. 3-5, 9"));
    m_rangeField->setFixedWidth(widthOfLetters(m_rangeField->fontMetrics(), 16));
    //: The name of the field of the range of the pages for screen readers. In Polish:
    //: Zakres stron
    m_rangeField->setAccessibleName(tr("Range of the pages"));
    m_allButton->setChecked(true);
    //: In Polish: Strony:
    addRow(printer, tr("Pages:"),
           newLine(m_printerTab, {m_allButton, m_currentButton, m_rangeButton, m_rangeField}));
    m_rangeNote = addNote(printer);

    m_parityBox = new QComboBox(m_printerTab);
    //: Which of the pages print. In Polish: Wszystkie strony
    m_parityBox->addItem(tr("All pages"), static_cast<int>(editor::PageParity::All));
    //: Which of the pages print. In Polish: Tylko nieparzyste
    m_parityBox->addItem(tr("Only odd pages"), static_cast<int>(editor::PageParity::Odd));
    //: Which of the pages print. In Polish: Tylko parzyste
    m_parityBox->addItem(tr("Only even pages"), static_cast<int>(editor::PageParity::Even));
    m_parityBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: In Polish: Dr&ukuj:
    addRow(printer, tr("Pr&int:"), newLine(m_printerTab, {m_parityBox}))->setBuddy(m_parityBox);

    m_duplexBox = new QComboBox(m_printerTab);
    m_duplexBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: Printing on both sides of the sheets. In Polish: Dwustronn&ie:
    addRow(printer, tr("T&wo-sided:"), newLine(m_printerTab, {m_duplexBox}))
        ->setBuddy(m_duplexBox);
    m_duplexNote = addNote(printer);

    // The color and the paper in one row: the tab as high as the others
    m_colorBox = new QComboBox(m_printerTab);
    m_colorBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_paperBox = new QComboBox(m_printerTab);
    m_paperBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: In Polish: P&apier:
    auto* paperLabel = new QLabel(tr("&Paper:"), m_printerTab);
    paperLabel->setBuddy(m_paperBox);
    //: In Polish: Ko&lor:
    addRow(printer, tr("Co&lor:"), newLine(m_printerTab, {m_colorBox, paperLabel, m_paperBox}))
        ->setBuddy(m_colorBox);
    endTab(printer);

    // The texts and the layout, as in the window of the export
    addTextsTab(tr("Text&s"),
                book.title.isEmpty()
                    //: Under the text in front of the window of the print, without a book. In
                    //: Polish: Tekst drukuje się także z niezapisanymi zmianami.
                    ? tr("The text prints also with its unsaved changes.")
                    //: Under the texts of the window of the print. In Polish: Za pierwszym
                    //: razem okno wybiera tekst na wierzchu, a potem pamięta ostatni wybór.
                    : tr("The first time the window chooses the text in front, then it keeps "
                         "the last choice."));
    QGridLayout* look = addLayoutTab(tr("La&yout"), true);
    endTab(look);
    setPagesVisible(true);

    // The sheets: the pages on a sheet, and their size
    //: A tab of the window of the print: how the pages go on the sheets. In Polish: &Na kartce
    QGridLayout* sheets = newTab(tr("On the Sh&eet"));
    m_sheetTab = sheets->parentWidget();
    //: A group of the tab of the sheets. In Polish: Strony książki na kartce
    auto* layoutGroup = new QGroupBox(tr("Pages of the book on a sheet"), m_sheetTab);
    auto* layoutBox = new QVBoxLayout(layoutGroup);
    //: In Polish: &Jedna strona
    m_onePageButton = new QRadioButton(tr("&One page"), layoutGroup);
    //: In Polish: Dwie strony o&bok siebie
    m_twoPagesButton = new QRadioButton(tr("T&wo pages side by side"), layoutGroup);
    //: In Polish: Broszur&a do złożenia na pół
    m_bookletButton = new QRadioButton(tr("&Booklet to fold in half"), layoutGroup);
    layoutBox->addWidget(m_onePageButton);
    layoutBox->addWidget(m_twoPagesButton);
    layoutBox->addWidget(m_bookletButton);
    layoutBox->addWidget(indented(
        newRemark(layoutGroup,
                  //: Under the booklet. In Polish: Po złożeniu na pół stos kartek czyta się po
                  //: kolei.
                  tr("Folded in half, the stack of the sheets reads in order.")),
        m_bookletButton));
    layoutBox->addStretch(1);
    //: A group of the tab of the sheets: the size of the pages on them. In Polish: Rozmiar
    auto* sizeGroup = new QGroupBox(tr("Size"), m_sheetTab);
    auto* sizeBox = new QVBoxLayout(sizeGroup);
    //: In Polish: Do&pasuj do kartki
    m_fitButton = new QRadioButton(tr("&Fit to the sheet"), sizeGroup);
    //: The page of its own size in the middle of the sheet; %1: its size (A5). In Polish:
    //: R&zeczywisty rozmiar (%1)
    m_actualSizeButton = new QRadioButton(
        tr("&Actual size (%1)").arg(book_output::literal(pageSizeName())), sizeGroup);
    //: The marks of the corners of a page, where the sheet is cut. In Polish: Znaczniki
    //: &cięcia
    m_cropMarksBox = new QCheckBox(tr("&Crop marks"), sizeGroup);
    const QStyle* style = m_actualSizeButton->style();
    auto* cropMarksLine = new QHBoxLayout();
    cropMarksLine->setContentsMargins(
        style->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, nullptr, m_actualSizeButton) +
            style->pixelMetric(QStyle::PM_RadioButtonLabelSpacing, nullptr, m_actualSizeButton),
        0, 0, 0);
    cropMarksLine->addWidget(m_cropMarksBox);
    sizeBox->addWidget(m_fitButton);
    sizeBox->addWidget(m_actualSizeButton);
    sizeBox->addLayout(cropMarksLine);
    sizeBox->addStretch(1);
    // The two groups side by side: the tab as high as the others
    sheets->setColumnStretch(0, 1);
    sheets->addWidget(layoutGroup, 0, 0);
    sheets->addWidget(sizeGroup, 0, 1);
    endTab(sheets);
    const editor::SheetLayout sheetLayout =
        valueOfName(sheetLayoutNames(), settings.get<std::string>("print.sheet.layout"),
                    editor::SheetLayout::OnePage);
    sheetButton(sheetLayout)->setChecked(true);
    (settings.get<bool>("print.sheet.fit") ? m_fitButton : m_actualSizeButton)->setChecked(true);
    m_cropMarksBox->setChecked(settings.get<bool>("print.sheet.cropMarks"));

    // The preview of the sheets, with the sheet shown under it and its buttons
    auto* previewBox = new QWidget(this);
    previewBox->setFixedWidth(widthOfLetters(fontMetrics(), PREVIEW_LETTERS));
    auto* previewLayout = new QVBoxLayout(previewBox);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    //: The caption of the preview of the sheets. In Polish: Podgląd
    auto* caption = new QLabel(tr("Preview"), previewBox);
    QFont bold = caption->font();
    bold.setBold(true);
    caption->setFont(bold);
    previewLayout->addWidget(caption);
    m_preview = new SheetPreview(previewBox);
    //: The name of the preview of the sheets for screen readers. In Polish: Podgląd kartki
    m_preview->setAccessibleName(tr("Preview of the sheet"));
    //: What the keys of the preview do, for screen readers. In Polish: Page Up i Page Down
    //: zmieniają kartkę
    m_preview->setAccessibleDescription(tr("Page Up and Page Down change the sheet"));
    caption->setBuddy(m_preview);
    previewLayout->addWidget(m_preview, 1);
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
    navigation->addWidget(m_previousButton);
    navigation->addWidget(m_previewLabel, 1);
    navigation->addWidget(m_nextButton);
    previewLayout->addLayout(navigation);
    body->addWidget(previewBox);

    //: The button of the window of the print. In Polish: Drukuj
    setAcceptText(tr("Print"));

    m_layoutTimer = new QTimer(this);
    m_layoutTimer->setSingleShot(true);
    m_layoutTimer->setInterval(0);
    connect(m_layoutTimer, &QTimer::timeout, this, &PrintBookDialog::layOutNow);

    readLayout("print.content");
    showPrinter();
    showSheetOptions();

    const auto changed = [this]() {
        dropFailure();
        showPages();
    };
    connect(m_printerBox, &QComboBox::currentIndexChanged, this, [this]() {
        dropFailure();
        showPrinter();
        contentChanged();
    });
    connect(m_copiesBox, &QSpinBox::valueChanged, this, changed);
    connect(m_collateBox, &QCheckBox::toggled, this, &PrintBookDialog::dropFailure);
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
    for (QComboBox* box : {m_parityBox, m_duplexBox, m_colorBox, m_paperBox}) {
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
    connect(m_previousButton, &QToolButton::clicked, this,
            [this]() { showSide(m_shownSide - 1); });
    connect(m_nextButton, &QToolButton::clicked, this, [this]() { showSide(m_shownSide + 1); });
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

    PrintBookDialog::contentChanged();
    layOutNow();
    tabs()->setCurrentIndex(0);
    m_printerBox->setFocus();
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
    return m_previewLabel->text();
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
    SheetPreview::Sheet sheet;
    sheet.pages = m_laidOut.pages;
    sheet.side = shown;
    sheet.paper = paperSize();
    sheet.printable = printableRect();
    sheet.options = now.sheet;
    sheet.gray = !now.color;
    m_preview->setSheet(std::move(sheet));
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
    const bool front = frontButton()->isChecked();
    const bool chosen = chosenButton()->isChecked();
    const QString title = front ? book().frontTitle : book().title;
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
    // The size of the page in the option of its actual size, after the gear
    if (m_actualSizeButton != nullptr) {
        m_actualSizeButton->setText(
            tr("&Actual size (%1)").arg(book_output::literal(pageSizeName())));
    }
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
        //: Under the tabs, when no text is chosen. In Polish: Nie ma czego drukować: wybierz
        //: teksty na zakładce „Teksty”.
        m_status = tr("Nothing to print: choose the texts on the tab \"Texts\".");
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
    showSide(m_shownSide < 0 ? 0 : m_shownSide);
    acceptButton()->setEnabled(printer != nullptr && !m_sides.empty() && m_printing != nullptr);
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
    settings.set<std::string>("print.sheet.layout",
                              nameOfValue(sheetLayoutNames(), now.sheet.layout));
    settings.set<bool>("print.sheet.fit", m_fitButton->isChecked());
    settings.set<bool>("print.sheet.cropMarks", m_cropMarksBox->isChecked());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
