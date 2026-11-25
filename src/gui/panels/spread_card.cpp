// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file spread_card.cpp
/// @brief The card of a spread: its two pages side by side, its settings beside them and its
///        place among the pages of the book

#include "kalahari/gui/panels/spread_card.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/material_kinds.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/editor/text_images.h"
#include "kalahari/gui/book_export_coordinator.h"
#include "kalahari/gui/book_page_numbers.h"
#include "kalahari/gui/book_styles.h"
#include "kalahari/gui/caption_settings.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/utils/program_folders.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace kalahari::gui {

namespace {

constexpr double MM_PER_INCH = 25.4;

/// The margins of the card and the gaps between its parts
constexpr int CARD_MARGIN = 14;
constexpr int CARD_SPACING = 14;
constexpr int BOX_MARGIN = 9;
constexpr int BOX_SPACING = 6;

/// The width of the settings beside the pages, in average characters
constexpr int SETTINGS_CHARACTERS = 40;

/// The pages in the view: the room around them, the narrowest they get to fit, the line of
/// their edge
constexpr int VIEW_MARGIN = 3;
constexpr int VIEW_MIN_WIDTH = 320;
constexpr int VIEW_HINT_WIDTH = 760;
constexpr int EDGE_ALPHA = 90;
constexpr int HINT_ALPHA = 170;

/// The picture as the card reads it: at most so many pixels on a side
constexpr int PICTURE_BOUNDS = 3000;

/// The small picture of the pages beside the spread, in dots per inch
constexpr double STRIP_DPI = 40.0;

/// The pages shown before and after the spread
constexpr int STRIP_AROUND = 4;

/// A small page: its width in lines of the text, the gap between the pairs, the lines of a
/// page with text
constexpr int MINI_WIDTH_LINES = 4;
constexpr int PAIR_GAP = 14;
constexpr int MINI_TEXT_LINES = 7;
constexpr int MINI_FAINT_ALPHA = 60;
constexpr int MINI_SPREAD_ALPHA = 40;
constexpr int MINI_MARK_WIDTH = 2;

/// The card waits for the settings written together
constexpr int SETTINGS_DELAY_MS = 100;

/// The desk under the pages: the color around the paper of the editor
class SpreadDesk : public QWidget {
public:
    using QWidget::QWidget;

    void setColor(const QColor& color) {
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.fillRect(rect(), m_color.isValid() ? m_color : palette().color(QPalette::Mid));
    }

private:
    QColor m_color;
};

/// A hint under a field, in the color of the hints
QLabel* hintOf(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

/// A radio button with its note under it
QRadioButton* addRadio(QVBoxLayout* layout, const QString& text, const QString& note,
                       QWidget* parent) {
    auto* button = new QRadioButton(text, parent);
    layout->addWidget(button);
    QLabel* hint = hintOf(note, parent);
    hint->setContentsMargins(button->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) +
                                 button->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing),
                             0, 0, 0);
    layout->addWidget(hint);
    return button;
}

/// A box of the card on the desk: in the color of the window, with a frame
QFrame* boxOf(QWidget* parent, const QString& name) {
    auto* box = new QFrame(parent);
    box->setObjectName(name);
    box->setFrameShape(QFrame::StyledPanel);
    box->setAutoFillBackground(true);
    box->setBackgroundRole(QPalette::Window);
    return box;
}

/// The page of the book as the Settings give it
editor::SpreadPage pageOfBook() {
    const editor::ExportPage page = BookExportCoordinator::pageOfSettings();
    editor::SpreadPage spread;
    spread.size = QSizeF(page.width, page.height);
    spread.top = page.top;
    spread.bottom = page.bottom;
    spread.outer = page.mirrored ? page.outer : std::max(page.left, page.right);
    return spread;
}

/// The font of the book's text: the editor's, at the size of the book's Normal
QFont fontOfBook(const core::ProjectManager& book) {
    QFont font(QString::fromStdString(
        core::SettingsManager::getInstance().get<std::string>("editor.fontFamily")));
    double points = editor::DEFAULT_NORMAL_POINT_SIZE;
    if (book.project() != nullptr) {
        const QVariantMap& normal = bookParagraphStyles(book).normal().properties;
        const double own = normal.value(QStringLiteral("fontSize")).toDouble();
        points = own > 0.0 ? own : points;
    }
    font.setPointSizeF(points);
    return font;
}

/// The kind of the pictures of the Workshop a picture from the disk goes to
core::KindRef pictureKind(const core::ProjectManager& book) {
    const core::BookProject* project = book.project();
    if (project == nullptr) {
        return {};
    }
    for (const core::KindRef& kind :
         project->kindsIn(book.bookTypes(), core::BookPlace::Workshop)) {
        if (kind.kind->isMaterial() &&
            kind.kind->id == QLatin1String(core::MaterialKinds::PICTURE) &&
            core::ProjectManager::canAdd(kind)) {
            return kind;
        }
    }
    return {};
}

/// Whether a key of the settings changes how the card looks: the page, the paper, the font
bool isLookKey(const std::string& key) {
    return key.rfind("editor.", 0) == 0 || key.rfind("themes.", 0) == 0 ||
           key.rfind("appearance.", 0) == 0;
}

}  // namespace

// =============================================================================
// SpreadPagesView
// =============================================================================

SpreadPagesView::SpreadPagesView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("spreadView"));
    setFocusPolicy(Qt::StrongFocus);
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    updateHints();
}

void SpreadPagesView::setLook(const editor::SpreadLook& look, const editor::SpreadPage& page) {
    if (m_look == look && m_page == page) {
        return;
    }
    const bool resized = !(m_page == page);
    m_look = look;
    m_page = page;
    if (resized) {
        setActualSize(m_actual);
    }
    updateHints();
    update();
}

void SpreadPagesView::setPicture(const QImage& picture) {
    m_picture = picture;
    m_pictureSize = picture.size();
    updateHints();
    update();
}

void SpreadPagesView::setColors(const Colors& colors) {
    m_colors = colors;
    update();
}

void SpreadPagesView::setTextFont(const QFont& font) {
    m_font = font;
    update();
}

void SpreadPagesView::setFoldShown(bool shown) {
    m_fold = shown;
    update();
}

void SpreadPagesView::setGuidesShown(bool shown) {
    m_guides = shown;
    update();
}

void SpreadPagesView::setActualSize(bool actual) {
    m_actual = actual;
    QSizePolicy policy = actual ? QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed)
                                : QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(!actual);
    setSizePolicy(policy);
    if (actual) {
        setFixedSize(sizeHint());
    } else {
        setMinimumSize(minimumSizeHint());
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }
    updateGeometry();
    update();
}

void SpreadPagesView::setFirstPage(int number) {
    if (m_firstPage == number) {
        return;
    }
    m_firstPage = number;
    updateHints();
    update();
}

bool SpreadPagesView::movable() const {
    return m_look.fills && !m_picture.isNull();
}

int SpreadPagesView::labelsHeight() const {
    return fontMetrics().height() + 2 * VIEW_MARGIN;
}

QSizeF SpreadPagesView::actualPagesSize() const {
    // At 100% the pages have their size on paper (as the editor's page view)
    const double scale = editor::BookEditor::paperScaleOf(screen());
    return QSizeF(2.0 * m_page.size.width() / MM_PER_INCH * logicalDpiX() * scale,
                  m_page.size.height() / MM_PER_INCH * logicalDpiY() * scale);
}

QRectF SpreadPagesView::pagesRect() const {
    if (m_page.size.isEmpty()) {
        return {};
    }
    const QRectF room = QRectF(rect()).adjusted(VIEW_MARGIN, VIEW_MARGIN, -VIEW_MARGIN,
                                                -VIEW_MARGIN - labelsHeight());
    const double ratio = m_page.size.height() / (2.0 * m_page.size.width());
    QSizeF size;
    if (m_actual) {
        size = actualPagesSize();
    } else {
        size = QSizeF(room.width(), room.width() * ratio);
        if (size.height() > room.height() && room.height() > 0.0) {
            size = QSizeF(room.height() / ratio, room.height());
        }
    }
    const double left = room.left() + std::max(0.0, (room.width() - size.width()) / 2.0);
    return {left, room.top(), size.width(), size.height()};
}

double SpreadPagesView::drawnDpi() const {
    const QRectF pages = pagesRect();
    if (pages.isEmpty() || m_page.size.isEmpty()) {
        return 0.0;
    }
    return pages.width() / (2.0 * m_page.size.width()) * MM_PER_INCH;
}

QSize SpreadPagesView::sizeHint() const {
    if (m_actual) {
        const QSizeF pages = actualPagesSize();
        return {static_cast<int>(std::ceil(pages.width())) + 2 * VIEW_MARGIN,
                static_cast<int>(std::ceil(pages.height())) + 2 * VIEW_MARGIN + labelsHeight()};
    }
    return {VIEW_HINT_WIDTH, heightForWidth(VIEW_HINT_WIDTH)};
}

QSize SpreadPagesView::minimumSizeHint() const {
    return {VIEW_MIN_WIDTH, heightForWidth(VIEW_MIN_WIDTH)};
}

bool SpreadPagesView::hasHeightForWidth() const {
    return !m_actual;
}

int SpreadPagesView::heightForWidth(int width) const {
    if (m_page.size.isEmpty()) {
        return width / 2;
    }
    const double ratio = m_page.size.height() / (2.0 * m_page.size.width());
    return static_cast<int>(std::ceil((width - 2 * VIEW_MARGIN) * ratio)) + 2 * VIEW_MARGIN +
           labelsHeight();
}

void SpreadPagesView::updateHints() {
    QString name =
        m_firstPage > 0
            //: The two pages of a spread on its card, for screen readers; %1 and %2: their
            //: numbers. In Polish: Strony rozkładówki %1 i %2
            ? tr("Pages of the spread %1 and %2").arg(m_firstPage).arg(m_firstPage + 1)
            //: In Polish: Strony rozkładówki
            : tr("Pages of the spread");
    if (movable()) {
        //: After the name of the pages of a spread, for screen readers. In Polish: ; strzałki
        //: przesuwają obraz w kadrze, Ctrl+] i Ctrl+[ go powiększają i pomniejszają, Home
        //: go wyśrodkowuje
        name += tr("; the arrows move the picture in its area, Ctrl+] and Ctrl+[ make it larger "
                   "and smaller, Home puts it in the middle");
    }
    setAccessibleName(name);
    if (movable()) {
        setCursor(m_dragging ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
    } else {
        unsetCursor();
    }
}

void SpreadPagesView::moveBy(const QPointF& deltaMm, bool fromDragStart) {
    const editor::SpreadGeometry geometry = editor::spreadGeometry(m_look, m_page, m_pictureSize);
    const QPointF from = fromDragStart ? m_dragCrop : QPointF(m_look.cropX, m_look.cropY);
    const QPointF moved = editor::spreadCropMovedBy(geometry, from.x(), from.y(), deltaMm);
    if (moved == QPointF(m_look.cropX, m_look.cropY)) {
        return;
    }
    m_look.cropX = moved.x();
    m_look.cropY = moved.y();
    update();
}

bool SpreadPagesView::event(QEvent* event) {
    // The keys of the picture are its own, before the shortcuts of the window
    if (event->type() == QEvent::ShortcutOverride && movable()) {
        const auto* key = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
        const bool arrow = key->key() == Qt::Key_Left || key->key() == Qt::Key_Right ||
                           key->key() == Qt::Key_Up || key->key() == Qt::Key_Down;
        const bool zoom = (key->key() == Qt::Key_BracketRight ||
                           key->key() == Qt::Key_BracketLeft) &&
                          modifiers == Qt::ControlModifier;
        if ((arrow && (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier)) || zoom ||
            (key->key() == Qt::Key_Home && modifiers == Qt::NoModifier)) {
            event->accept();
            return true;
        }
    }
    return QWidget::event(event);
}

void SpreadPagesView::keyPressEvent(QKeyEvent* event) {
    if (!movable()) {
        QWidget::keyPressEvent(event);
        return;
    }
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if (modifiers == Qt::ControlModifier &&
        (event->key() == Qt::Key_BracketRight || event->key() == Qt::Key_BracketLeft)) {
        emit zoomStepped(event->key() == Qt::Key_BracketRight ? 1 : -1);
        return;
    }
    if (event->key() == Qt::Key_Home && modifiers == Qt::NoModifier) {
        emit centerAsked();
        return;
    }
    if (modifiers != Qt::NoModifier && modifiers != Qt::ShiftModifier) {
        QWidget::keyPressEvent(event);
        return;
    }
    const double step = modifiers == Qt::ShiftModifier ? FAST_STEP_MM : STEP_MM;
    QPointF delta;
    switch (event->key()) {
        case Qt::Key_Left:
            delta = QPointF(-step, 0.0);
            break;
        case Qt::Key_Right:
            delta = QPointF(step, 0.0);
            break;
        case Qt::Key_Up:
            delta = QPointF(0.0, -step);
            break;
        case Qt::Key_Down:
            delta = QPointF(0.0, step);
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }
    const QPointF before(m_look.cropX, m_look.cropY);
    moveBy(delta, false);
    if (before != QPointF(m_look.cropX, m_look.cropY)) {
        emit cropMoved(m_look.cropX, m_look.cropY);
    }
}

void SpreadPagesView::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    if (event->button() == Qt::LeftButton && movable() &&
        pagesRect().contains(event->position())) {
        m_dragging = true;
        m_dragStart = event->position();
        m_dragCrop = QPointF(m_look.cropX, m_look.cropY);
        updateHints();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void SpreadPagesView::mouseMoveEvent(QMouseEvent* event) {
    const double perMm = drawnDpi() / MM_PER_INCH;
    if (!m_dragging || perMm <= 0.0) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    moveBy((event->position() - m_dragStart) / perMm, true);
    event->accept();
}

void SpreadPagesView::mouseReleaseEvent(QMouseEvent* event) {
    if (!m_dragging || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_dragging = false;
    updateHints();
    if (m_dragCrop != QPointF(m_look.cropX, m_look.cropY)) {
        emit cropMoved(m_look.cropX, m_look.cropY);
    }
    event->accept();
}

void SpreadPagesView::paintEvent(QPaintEvent* /*event*/) {
    const QRectF pages = pagesRect();
    const double dpi = drawnDpi();
    if (pages.isEmpty() || dpi <= 0.0) {
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // The pages, each with its half of the picture; the text in the size it has on paper
    editor::SpreadPaint paint;
    paint.dpi = dpi;
    paint.paper = m_colors.paper;
    paint.text = m_colors.text;
    QFont font = m_font;
    if (font.pointSizeF() > 0.0 && logicalDpiY() > 0) {
        font.setPointSizeF(font.pointSizeF() * dpi / logicalDpiY());
    }
    paint.font = font;
    paint.fold = m_fold ? m_colors.fold : QColor();
    paint.guide = m_guides ? m_colors.guide : QColor();
    paint.line = !m_fold;
    for (const bool right : {false, true}) {
        painter.save();
        painter.translate(pages.left() + (right ? pages.width() / 2.0 : 0.0), pages.top());
        paint.number = m_look.pageNumbers && m_firstPage > 0 ? m_firstPage + (right ? 1 : 0)
                                                             : 0;
        editor::paintSpreadPage(&painter, m_look, m_page, m_picture, right, paint);
        painter.restore();
    }
    QColor edge = m_colors.text;
    edge.setAlpha(EDGE_ALPHA);
    painter.setPen(QPen(edge, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(pages.adjusted(-0.5, -0.5, 0.5, 0.5));

    // Without a picture: what to do
    if (m_picture.isNull()) {
        QColor hint = m_colors.text;
        hint.setAlpha(HINT_ALPHA);
        painter.setPen(hint);
        QFont big = this->font();
        big.setPointSizeF(big.pointSizeF() * 1.3);
        const QFontMetricsF metrics(big);
        const QRectF top(pages.left(), pages.center().y() - metrics.height(), pages.width(),
                         metrics.height());
        painter.setFont(big);
        //: Drawn over the empty pages of a spread on its card. In Polish: Rozkładówka bez
        //: obrazu
        painter.drawText(top, Qt::AlignCenter, tr("Spread without a picture"));
        painter.setFont(this->font());
        //: Under it. In Polish: Wybierz obraz na liście „Obraz:” albo „Obraz z dysku...”
        painter.drawText(top.translated(0.0, metrics.height() * 1.2), Qt::AlignCenter,
                         tr("Choose a picture in the list \"Picture:\" or \"Picture from "
                            "disk...\""));
    }

    // The names of the pages under them
    painter.setFont(this->font());
    painter.setPen(m_colors.text);
    const QRectF labels(pages.left(), pages.bottom() + VIEW_MARGIN, pages.width() / 2.0,
                        fontMetrics().height());
    if (m_firstPage > 0) {
        //: Under the left page of a spread; %1: its number. In Polish: strona %1 · lewa
        painter.drawText(labels, Qt::AlignCenter, tr("page %1 · left").arg(m_firstPage));
        //: Under the right page. In Polish: strona %1 · prawa
        painter.drawText(labels.translated(pages.width() / 2.0, 0.0), Qt::AlignCenter,
                         tr("page %1 · right").arg(m_firstPage + 1));
    } else {
        //: Under the left page of a spread while the pages are counted. In Polish: lewa strona
        painter.drawText(labels, Qt::AlignCenter, tr("left page"));
        //: In Polish: prawa strona
        painter.drawText(labels.translated(pages.width() / 2.0, 0.0), Qt::AlignCenter,
                         tr("right page"));
    }

    // The keys are here
    if (hasFocus()) {
        QPen focus(palette().color(QPalette::Highlight), 2.0, Qt::DashLine);
        painter.setPen(focus);
        painter.drawRect(pages.adjusted(-2.0, -2.0, 2.0, 2.0));
    }
}

// =============================================================================
// SpreadStrip
// =============================================================================

SpreadStrip::SpreadStrip(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("spreadStrip"));
    //: The small pages beside a spread on its card, for screen readers. In Polish: Strony obok
    //: rozkładówki
    setAccessibleName(tr("Pages beside the spread"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void SpreadStrip::setPages(const std::vector<Page>& pages) {
    m_pages = pages;
    // For screen readers: the pages in words
    QStringList words;
    for (const Page& page : m_pages) {
        if (page.own) {
            //: A page of the spread itself, for screen readers; %1: its number. In Polish: %1
            //: – ta rozkładówka
            words << tr("%1 – this spread").arg(page.number);
        } else if (page.title.isEmpty()) {
            //: An empty page; %1: its number. In Polish: %1 – pusta
            words << tr("%1 – empty").arg(page.number);
        } else {
            words << QStringLiteral("%1 – %2").arg(page.number).arg(page.title);
        }
    }
    setAccessibleDescription(words.join(QStringLiteral("; ")));
    updateGeometry();
    adjustSize();
    update();
}

void SpreadStrip::setPicture(const QImage& picture) {
    m_picture = picture;
    update();
}

void SpreadStrip::setColors(const QColor& paper, const QColor& text, const QColor& mark) {
    m_paper = paper;
    m_text = text;
    m_mark = mark;
    update();
}

void SpreadStrip::setPageRatio(double ratio) {
    if (ratio > 0.0 && std::abs(ratio - m_ratio) > 1e-6) {
        m_ratio = ratio;
        updateGeometry();
        adjustSize();
        update();
    }
}

QSize SpreadStrip::miniSize() const {
    const int width = fontMetrics().height() * MINI_WIDTH_LINES;
    return {width, static_cast<int>(std::lround(width * m_ratio))};
}

QSize SpreadStrip::sizeHint() const {
    // A pair for each left page, and page 1 alone
    int pairs = 0;
    for (size_t i = 0; i < m_pages.size(); ++i) {
        if (m_pages[i].number % 2 == 0 || i == 0) {
            ++pairs;
        }
    }
    const QSize mini = miniSize();
    return {std::max(1, pairs) * (2 * mini.width() + PAIR_GAP),
            mini.height() + 2 * fontMetrics().height() + 2 * MINI_MARK_WIDTH};
}

QSize SpreadStrip::minimumSizeHint() const {
    return sizeHint();
}

void SpreadStrip::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    const QSize mini = miniSize();
    const QFontMetrics metrics = fontMetrics();
    QFont bold = font();
    bold.setBold(true);
    QColor faint = m_text;
    faint.setAlpha(MINI_FAINT_ALPHA);
    QColor otherSpread = m_text;
    otherSpread.setAlpha(MINI_SPREAD_ALPHA);
    const QColor names = palette().color(QPalette::PlaceholderText);

    const auto drawPage = [&](const Page& page, const QRect& box) {
        painter.fillRect(box, m_paper);
        if (page.own && !m_picture.isNull()) {
            const int half = m_picture.width() / 2;
            painter.drawImage(QRectF(box), m_picture,
                              QRectF(page.right ? half : 0, 0, half, m_picture.height()));
        } else if (page.spread) {
            painter.fillRect(box.adjusted(3, 3, -3, -3), otherSpread);
        } else if (!page.title.isEmpty()) {
            // A page of text: its lines
            painter.setPen(faint);
            const double step = box.height() / (MINI_TEXT_LINES + 2.0);
            for (int line = 1; line <= MINI_TEXT_LINES; ++line) {
                const int y = box.top() + static_cast<int>(step * line);
                painter.drawLine(box.left() + 5, y, box.right() - 5, y);
            }
        }
        painter.setPen(QPen(page.own && m_mark.isValid() ? m_mark : faint,
                            page.own ? MINI_MARK_WIDTH : 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(box.adjusted(0, 0, -1, -1));
        // Its number and what is on it
        const QRect number(box.left() - PAIR_GAP / 2, box.bottom() + MINI_MARK_WIDTH,
                           box.width() + PAIR_GAP, metrics.height());
        painter.setFont(bold);
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(number, Qt::AlignHCenter | Qt::AlignTop, QString::number(page.number));
        painter.setFont(font());
        painter.setPen(names);
        //: An empty page among the small pages beside a spread. In Polish: pusta
        const QString title = page.title.isEmpty() ? tr("empty") : page.title;
        painter.drawText(number.translated(0, metrics.height()), Qt::AlignHCenter | Qt::AlignTop,
                         metrics.elidedText(title, Qt::ElideRight, number.width()));
    };

    int x = MINI_MARK_WIDTH;
    size_t i = 0;
    while (i < m_pages.size()) {
        const Page& first = m_pages[i];
        if (first.number % 2 == 1) {
            // A right page alone: page 1, or the first one shown
            drawPage(first, QRect(QPoint(x + mini.width(), MINI_MARK_WIDTH), mini));
            ++i;
        } else {
            drawPage(first, QRect(QPoint(x, MINI_MARK_WIDTH), mini));
            ++i;
            if (i < m_pages.size() && m_pages[i].number == first.number + 1) {
                drawPage(m_pages[i], QRect(QPoint(x + mini.width(), MINI_MARK_WIDTH), mini));
                ++i;
            }
        }
        x += 2 * mini.width() + PAIR_GAP;
    }
}

// =============================================================================
// SpreadCard
// =============================================================================

SpreadCard::SpreadCard(core::ProjectManager& book, const BookPageNumbers* pages,
                       const QString& elementId, QWidget* parent)
    : QWidget(parent), m_book(book), m_pages(pages), m_elementId(elementId) {
    setObjectName(QStringLiteral("spreadCard"));
    setProperty("elementId", elementId);
    setProperty("tabIconId", QStringLiteral("book.spread"));
    m_page = pageOfBook();

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(SAVE_DELAY_MS);
    connect(m_saveTimer, &QTimer::timeout, this, &SpreadCard::saveNow);
    m_settingsTimer = new QTimer(this);
    m_settingsTimer->setSingleShot(true);
    m_settingsTimer->setInterval(SETTINGS_DELAY_MS);
    connect(m_settingsTimer, &QTimer::timeout, this, [this]() {
        m_page = pageOfBook();
        m_view->setTextFont(fontOfBook(m_book));
        showColors();
        showLook();
        showQuality();
    });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(createBar());

    // The desk: the pages and the strip under them, the settings beside them
    m_desk = new QScrollArea(this);
    m_desk->setObjectName(QStringLiteral("spreadDesk"));
    m_desk->setWidgetResizable(true);
    m_desk->setFrameShape(QFrame::NoFrame);
    m_desk->setFocusPolicy(Qt::NoFocus);
    auto* desk = new SpreadDesk(m_desk);
    desk->setObjectName(QStringLiteral("spreadDeskContents"));
    auto* row = new QHBoxLayout(desk);
    row->setContentsMargins(CARD_MARGIN, CARD_MARGIN, CARD_MARGIN, CARD_MARGIN);
    row->setSpacing(CARD_SPACING);
    auto* column = new QVBoxLayout();
    column->setSpacing(CARD_SPACING);
    // Made in the order of the keys: the pages, the settings, the strip's buttons
    m_view = new SpreadPagesView(desk);
    m_view->setTextFont(fontOfBook(m_book));
    QWidget* settings = createSettings();
    settings->setParent(desk);
    m_stripBox = createStripBox(desk);
    column->addWidget(m_view);
    column->addWidget(m_stripBox);
    column->addStretch(1);
    row->addLayout(column, 1);
    row->addWidget(settings, 0, Qt::AlignTop);
    m_desk->setWidget(desk);
    layout->addWidget(m_desk, 1);
    m_stripBox->setVisible(m_pages != nullptr);

    // The keys go from the bar to the pages, the settings and the buttons of the strip
    QWidget* previous = findChild<QToolButton*>(QStringLiteral("spreadSettings"));
    for (QWidget* next : std::initializer_list<QWidget*>{
             m_view, m_pictureBox, m_diskButton, m_marginsButton, m_edgeButton, m_wholeButton,
             m_fillButton, m_centerButton, m_gutterBox, m_captionBox, m_labelBox, m_captionField,
             m_numbersBox, m_contentsBox, m_earlierButton, m_laterButton}) {
        if (previous != nullptr) {
            setTabOrder(previous, next);
        }
        previous = next;
    }

    connectFields();
    showColors();
    refresh();

    // A change made elsewhere: in the Navigator, in Properties, the pictures of the Materials
    connect(&m_book, &core::ProjectManager::spreadChanged, this, [this](const QString& id) {
        if (id == m_elementId) {
            refresh();
        }
    });
    connect(&m_book, &core::ProjectManager::projectChanged, this, &SpreadCard::refresh);
    // A drag or a caption still waiting is saved before the book closes
    connect(&m_book, &core::ProjectManager::projectAboutToClose, this, &SpreadCard::saveNow);
    if (m_pages != nullptr) {
        connect(m_pages, &BookPageNumbers::counted, this, &SpreadCard::showPlace);
    }
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            [this]() { m_settingsTimer->start(); });
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (isLookKey(key)) {
                QMetaObject::invokeMethod(
                    this, [this]() { m_settingsTimer->start(); }, Qt::QueuedConnection);
            }
        });
}

SpreadCard::~SpreadCard() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    saveNow();
}

// =============================================================================
// The parts of the card
// =============================================================================

QWidget* SpreadCard::createBar() {
    auto& art = core::ArtProvider::getInstance();
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("spreadBar"));
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(CARD_MARGIN / 2, 3, CARD_MARGIN / 2, 3);
    layout->setSpacing(2);
    const auto addButton = [bar, layout, &art](const QString& name, const QString& iconId,
                                               const QString& text, const QString& tip) {
        auto* button = new QToolButton(bar);
        button->setObjectName(name);
        if (!iconId.isEmpty()) {
            button->setIcon(art.getIcon(iconId));
        }
        if (!text.isEmpty()) {
            button->setText(text);
            button->setToolButtonStyle(iconId.isEmpty() ? Qt::ToolButtonTextOnly
                                                        : Qt::ToolButtonTextBesideIcon);
        }
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        button->setAutoRaise(true);
        layout->addWidget(button);
        return button;
    };
    const auto addSeparator = [bar, layout]() {
        auto* line = new QFrame(bar);
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);
        layout->addWidget(line);
    };

    m_headIcon = new QLabel(bar);
    m_headIcon->setPixmap(art.getIcon(QStringLiteral("book.spread"), core::IconContext::Menu)
                              .pixmap(QSize(16, 16)));
    layout->addWidget(m_headIcon);
    m_head = new QLabel(bar);
    m_head->setObjectName(QStringLiteral("spreadHead"));
    QFont headFont = m_head->font();
    headFont.setBold(true);
    m_head->setFont(headFont);
    layout->addWidget(m_head);
    layout->addStretch(1);

    //: The bar of the card of a spread: the keys go to the list of its picture. In Polish:
    //: Obraz...
    QToolButton* picture = addButton(QStringLiteral("spreadPicture"), QStringLiteral("common.image"),
                                     tr("Picture..."),
                                     //: In Polish: Obraz rozkładówki
                                     tr("The Picture of the Spread"));
    connect(picture, &QToolButton::clicked, this, &SpreadCard::focusPicture);
    addSeparator();
    //: The pages fit the card. In Polish: Cała rozkładówka
    m_wholeSpreadButton = addButton(QStringLiteral("spreadWhole"), QString(), tr("Whole Spread"),
                                    //: In Polish: Cała rozkładówka na karcie
                                    tr("The Whole Spread on the Card"));
    //: The pages at their size on paper. In Polish: 100%
    m_actualSizeButton = addButton(QStringLiteral("spreadActual"), QString(), tr("100%"),
                                   //: In Polish: Rzeczywisty rozmiar stron
                                   tr("The Real Size of the Pages"));
    m_wholeSpreadButton->setCheckable(true);
    m_actualSizeButton->setCheckable(true);
    m_wholeSpreadButton->setChecked(true);
    auto* zoom = new QButtonGroup(bar);
    zoom->setExclusive(true);
    zoom->addButton(m_wholeSpreadButton);
    zoom->addButton(m_actualSizeButton);
    addSeparator();
    //: The band of the spine on the pages of a spread, shown or not. In Polish: Pas grzbietu
    m_foldButton = addButton(QStringLiteral("spreadFold"), QStringLiteral("spread.fold"), QString(),
                             tr("Spine Band"));
    //: The frame of the area of the picture, shown or not. In Polish: Pole obrazu
    m_guidesButton = addButton(QStringLiteral("spreadGuides"), QStringLiteral("spread.guides"),
                               QString(), tr("Picture Area"));
    m_foldButton->setCheckable(true);
    m_guidesButton->setCheckable(true);
    m_foldButton->setChecked(true);
    m_guidesButton->setChecked(true);
    addSeparator();
    //: In Polish: Drukuj... (Ctrl+P)
    QToolButton* print = addButton(QStringLiteral("spreadPrint"), QStringLiteral("file.print"),
                                   QString(), tr("Print... (Ctrl+P)"));
    connect(print, &QToolButton::clicked, this, &SpreadCard::printRequested);
    //: The gear of the card of a spread. In Polish: Ustawienia rozkładówek (Ustawienia > Edytor
    //: > Obrazy)
    QToolButton* gear = addButton(QStringLiteral("spreadSettings"), QStringLiteral("dock.settings"),
                                  QString(),
                                  tr("Spreads Settings (Settings > Editor > Images)"));
    connect(gear, &QToolButton::clicked, this, &SpreadCard::settingsRequested);
    return bar;
}

QWidget* SpreadCard::createSettings() {
    QFrame* box = boxOf(this, QStringLiteral("spreadSettingsBox"));
    box->setFixedWidth(box->fontMetrics().averageCharWidth() * SETTINGS_CHARACTERS);
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(BOX_MARGIN, BOX_MARGIN, BOX_MARGIN, BOX_MARGIN);
    layout->setSpacing(BOX_SPACING);
    //: The title of the settings on the card of a spread. In Polish: Rozkładówka
    auto* title = new QLabel(tr("Spread"), box);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setHorizontalSpacing(BOX_SPACING);
    form->setVerticalSpacing(BOX_SPACING + 2);
    addPictureRows(form, box);
    addLookRows(form, box);
    addCaptionRows(form, box);
    layout->addLayout(form);

    // The quality of the picture for print
    auto* quality = new QHBoxLayout();
    quality->setSpacing(BOX_SPACING);
    m_qualityIcon = new QLabel(box);
    m_qualityIcon->setAlignment(Qt::AlignTop);
    m_quality = new QLabel(box);
    m_quality->setObjectName(QStringLiteral("spreadQuality"));
    m_quality->setWordWrap(true);
    m_quality->setTextFormat(Qt::PlainText);
    quality->addWidget(m_qualityIcon, 0, Qt::AlignTop);
    quality->addWidget(m_quality, 1);
    layout->addLayout(quality);
    return box;
}

void SpreadCard::addPictureRows(QFormLayout* form, QWidget* parent) {
    auto* field = new QWidget(parent);
    auto* layout = new QVBoxLayout(field);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(BOX_SPACING);
    m_pictureBox = new QComboBox(field);
    m_pictureBox->setObjectName(QStringLiteral("spreadPictureBox"));
    m_pictureBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_pictureBox->setMinimumContentsLength(12);
    //: For screen readers. In Polish: Obraz rozkładówki
    m_pictureBox->setAccessibleName(tr("Picture of the spread"));
    layout->addWidget(m_pictureBox);
    //: Takes a picture file from the disk to the Materials and the spread. In Polish: Obraz
    //: z dysk&u...
    m_diskButton = new QPushButton(core::ArtProvider::getInstance().getIcon(
                                       QStringLiteral("common.image"), core::IconContext::Menu),
                                   tr("Picture from &disk..."), field);
    m_diskButton->setObjectName(QStringLiteral("spreadDisk"));
    auto* buttons = new QHBoxLayout();
    buttons->addWidget(m_diskButton);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    m_fileInfo = hintOf(QString(), field);
    m_fileInfo->setObjectName(QStringLiteral("spreadFileInfo"));
    layout->addWidget(m_fileInfo);
    //: The picture of a spread. In Polish: Ob&raz:
    form->addRow(tr("Pict&ure:"), field);
    if (auto* label = qobject_cast<QLabel*>(form->labelForField(field))) {
        label->setBuddy(m_pictureBox);
    }
}

void SpreadCard::addLookRows(QFormLayout* form, QWidget* parent) {
    // Where the picture lies
    m_areaRow = new QWidget(parent);
    auto* area = new QVBoxLayout(m_areaRow);
    area->setContentsMargins(0, 0, 0, 0);
    area->setSpacing(1);
    //: In Polish: W marginesach
    m_marginsButton = addRadio(area, tr("Within the margins"),
                               //: In Polish: przez grzbiet bez przerwy
                               tr("across the spine without a break"), m_areaRow);
    m_marginsButton->setObjectName(QStringLiteral("spreadMargins"));
    //: In Polish: Do krawędzi papieru
    m_edgeButton = addRadio(area, tr("To the edges of the paper"),
                            //: In Polish: całe obie strony
                            tr("both pages, whole"), m_areaRow);
    m_edgeButton->setObjectName(QStringLiteral("spreadEdge"));
    m_areaRow->setFocusProxy(m_marginsButton);
    //: Where the picture of a spread lies. In Polish: Obraz &zajmuje:
    form->addRow(tr("The picture ta&kes:"), m_areaRow);

    // How it fits its area
    m_fitRow = new QWidget(parent);
    auto* fit = new QVBoxLayout(m_fitRow);
    fit->setContentsMargins(0, 0, 0, 0);
    fit->setSpacing(1);
    //: In Polish: Cały obraz
    m_wholeButton = addRadio(fit, tr("The whole picture"),
                             //: In Polish: nic nie ginie, wolne miejsce zostaje białe
                             tr("nothing is lost; the room left stays white"), m_fitRow);
    m_wholeButton->setObjectName(QStringLiteral("spreadWholePicture"));
    //: In Polish: Wypełnij
    m_fillButton = addRadio(fit, tr("Fill"),
                            //: In Polish: brzegi poza polem są przycięte
                            tr("the edges beyond the area are cut off"), m_fitRow);
    m_fillButton->setObjectName(QStringLiteral("spreadFill"));
    m_cropHint = hintOf(QString(), m_fitRow);
    m_cropHint->setObjectName(QStringLiteral("spreadCropHint"));
    fit->addSpacing(BOX_SPACING / 2);
    fit->addWidget(m_cropHint);
    //: Puts a picture that fills its area in the middle, at its smallest. In Polish:
    //: Wyśrodkuj obraz
    m_centerButton = new QPushButton(tr("Center the Picture"), m_fitRow);
    m_centerButton->setObjectName(QStringLiteral("spreadCenter"));
    auto* center = new QHBoxLayout();
    center->addWidget(m_centerButton);
    center->addStretch(1);
    fit->addLayout(center);
    m_fitRow->setFocusProxy(m_wholeButton);
    //: How the picture of a spread fits its area. In Polish: &Dopasowanie:
    form->addRow(tr("Fitti&ng:"), m_fitRow);

    // The room at the spine
    auto* gutter = new QWidget(parent);
    auto* gutterLayout = new QVBoxLayout(gutter);
    gutterLayout->setContentsMargins(0, 0, 0, 0);
    gutterLayout->setSpacing(1);
    m_gutterBox = new LengthSpinBox(LengthUnit::Millimeters, 0.0, core::SpreadData::MAX_GUTTER,
                                    gutter);
    m_gutterBox->setObjectName(QStringLiteral("spreadGutterBox"));
    //: For screen readers. In Polish: Zapas na grzbiet
    m_gutterBox->setAccessibleName(tr("Room at the spine"));
    auto* gutterRow = new QHBoxLayout();
    gutterRow->addWidget(m_gutterBox);
    gutterRow->addStretch(1);
    gutterLayout->addLayout(gutterRow);
    //: In Polish: Połówki odsuwają się od zgięcia, a pasek przy grzbiecie jest na obu stronach.
    gutterLayout->addWidget(hintOf(tr("The halves move away from the fold, and the strip at the "
                                      "spine shows on both pages."),
                                   gutter));
    //: The room at the spine of a spread. In Polish: Zapas na &grzbiet:
    form->addRow(tr("Room at the s&pine:"), gutter);
    if (auto* label = qobject_cast<QLabel*>(form->labelForField(gutter))) {
        label->setBuddy(m_gutterBox);
    }
}

void SpreadCard::addCaptionRows(QFormLayout* form, QWidget* parent) {
    auto* caption = new QWidget(parent);
    auto* layout = new QVBoxLayout(caption);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(BOX_SPACING);
    //: In Polish: pod obrazem, na prawej stronie
    m_captionBox = new QCheckBox(tr("under the picture, on the right page"), caption);
    m_captionBox->setObjectName(QStringLiteral("spreadCaptionBox"));
    layout->addWidget(m_captionBox);
    m_labelBox = new QComboBox(caption);
    m_labelBox->setObjectName(QStringLiteral("spreadLabelBox"));
    m_labelBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_labelBox->setMinimumContentsLength(12);
    //: For screen readers. In Polish: Etykieta podpisu
    m_labelBox->setAccessibleName(tr("Label of the caption"));
    layout->addWidget(m_labelBox);
    m_captionField = new QLineEdit(caption);
    m_captionField->setObjectName(QStringLiteral("spreadCaptionField"));
    //: For screen readers and as the placeholder. In Polish: Tekst podpisu
    m_captionField->setAccessibleName(tr("Text of the caption"));
    m_captionField->setPlaceholderText(tr("Text of the caption"));
    layout->addWidget(m_captionField);
    //: In Polish: Przy obrazie do krawędzi papieru podpis jest na obrazie, w jasnej ramce.
    m_edgeCaptionHint = hintOf(tr("With the picture to the edges of the paper the caption lies on "
                                  "the picture, in a light frame."),
                               caption);
    layout->addWidget(m_edgeCaptionHint);
    caption->setFocusProxy(m_captionBox);
    //: The caption of a spread. In Polish: Podp&is:
    form->addRow(tr("&Caption:"), caption);

    auto* pages = new QWidget(parent);
    auto* pagesLayout = new QVBoxLayout(pages);
    pagesLayout->setContentsMargins(0, 0, 0, 0);
    pagesLayout->setSpacing(BOX_SPACING);
    //: In Polish: Nu&mery stron i żywa pagina
    m_numbersBox = new QCheckBox(tr("Page numbe&rs and running head"), pages);
    m_numbersBox->setObjectName(QStringLiteral("spreadNumbersBox"));
    pagesLayout->addWidget(m_numbersBox);
    //: In Polish: T&ytuł w spisie treści
    m_contentsBox = new QCheckBox(tr("Title in the table of content&s"), pages);
    m_contentsBox->setObjectName(QStringLiteral("spreadContentsBox"));
    pagesLayout->addWidget(m_contentsBox);
    //: What the pages of a spread show. In Polish: Na stronach:
    form->addRow(tr("On the pages:"), pages);
}

QWidget* SpreadCard::createStripBox(QWidget* parent) {
    QFrame* box = boxOf(parent, QStringLiteral("spreadStripBox"));
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(BOX_MARGIN, BOX_MARGIN - 1, BOX_MARGIN, BOX_MARGIN - 3);
    layout->setSpacing(BOX_SPACING);

    auto* head = new QHBoxLayout();
    head->setSpacing(BOX_SPACING + 2);
    //: The pages around a spread on its card. In Polish: Położenie w książce
    auto* title = new QLabel(tr("Place in the Book"), box);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    head->addWidget(title);
    m_place = new QLabel(box);
    m_place->setObjectName(QStringLiteral("spreadPlace"));
    m_place->setWordWrap(true);
    head->addWidget(m_place, 1);
    layout->addLayout(head);
    //: In Polish: Rozkładówka zaczyna się zawsze na lewej stronie, a teksty – tam, gdzie mówią
    //: Ustawienia > Edytor > Strony i marginesy. Przyciski działają jak Przenieś w górę
    //: i Przenieś w dół w Nawigatorze.
    layout->addWidget(hintOf(tr("A spread always starts on a left page, and the texts where "
                                "Settings > Editor > Pages and Margins say. The buttons work as "
                                "Move Up and Move Down in the Navigator."),
                             box));

    m_stripArea = new QScrollArea(box);
    m_stripArea->setObjectName(QStringLiteral("spreadStripArea"));
    m_stripArea->setFrameShape(QFrame::NoFrame);
    m_stripArea->setFocusPolicy(Qt::NoFocus);
    m_stripArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_stripArea->setWidgetResizable(false);
    m_stripArea->setBackgroundRole(QPalette::Window);
    m_strip = new SpreadStrip(m_stripArea);
    m_stripArea->setWidget(m_strip);
    m_stripArea->setFixedHeight(m_strip->sizeHint().height() +
                                m_stripArea->horizontalScrollBar()->sizeHint().height());
    layout->addWidget(m_stripArea);

    auto& art = core::ArtProvider::getInstance();
    auto* moves = new QHBoxLayout();
    //: Moves the spread up its list, as Move Up in the Navigator. In Polish: Przenieś wcześniej
    m_earlierButton = new QPushButton(
        art.getIcon(QStringLiteral("common.chevronLeft"), core::IconContext::Menu),
        tr("Move Earlier"), box);
    m_earlierButton->setObjectName(QStringLiteral("spreadEarlier"));
    //: In Polish: Przenieś później
    m_laterButton = new QPushButton(
        art.getIcon(QStringLiteral("common.chevronRight"), core::IconContext::Menu),
        tr("Move Later"), box);
    m_laterButton->setObjectName(QStringLiteral("spreadLater"));
    m_laterButton->setLayoutDirection(Qt::RightToLeft);  // its arrow after its words
    moves->addWidget(m_earlierButton);
    moves->addWidget(m_laterButton);
    moves->addStretch(1);
    layout->addLayout(moves);
    return box;
}

void SpreadCard::connectFields() {
    const auto edited = [this](const auto& change, bool later = false) {
        if (m_loading) {
            return;
        }
        core::SpreadData spread = m_data;
        change(spread);
        store(spread, later);
    };
    connect(m_pictureBox, &QComboBox::currentIndexChanged, this, [this, edited](int index) {
        const QString picture = m_pictureBox->itemData(index).toString();
        edited([&picture](core::SpreadData& spread) { spread.picture = picture; });
    });
    connect(m_diskButton, &QPushButton::clicked, this, &SpreadCard::pictureFromDisk);
    connect(m_marginsButton, &QRadioButton::toggled, this, [edited](bool checked) {
        if (checked) {
            edited([](core::SpreadData& spread) {
                spread.area = QString::fromLatin1(core::SpreadData::MARGINS);
            });
        }
    });
    connect(m_edgeButton, &QRadioButton::toggled, this, [edited](bool checked) {
        if (checked) {
            edited([](core::SpreadData& spread) {
                spread.area = QString::fromLatin1(core::SpreadData::EDGE);
            });
        }
    });
    connect(m_wholeButton, &QRadioButton::toggled, this, [edited](bool checked) {
        if (checked) {
            edited([](core::SpreadData& spread) {
                spread.fit = QString::fromLatin1(core::SpreadData::WHOLE);
            });
        }
    });
    connect(m_fillButton, &QRadioButton::toggled, this, [edited](bool checked) {
        if (checked) {
            edited([](core::SpreadData& spread) {
                spread.fit = QString::fromLatin1(core::SpreadData::FILL);
            });
        }
    });
    connect(m_centerButton, &QPushButton::clicked, this, [edited]() {
        edited([](core::SpreadData& spread) {
            spread.cropX = 0.0;
            spread.cropY = 0.0;
            spread.zoom = core::SpreadData::MIN_ZOOM;
        });
    });
    connect(m_gutterBox, &QDoubleSpinBox::valueChanged, this, [this, edited]() {
        const double gutter = m_gutterBox->storedValue();
        edited([gutter](core::SpreadData& spread) { spread.gutter = gutter; }, true);
    });
    connect(m_captionBox, &QCheckBox::toggled, this, [edited](bool checked) {
        edited([checked](core::SpreadData& spread) { spread.caption = checked; });
    });
    connect(m_labelBox, &QComboBox::currentIndexChanged, this, [this, edited](int index) {
        const QString label = m_labelBox->itemData(index).toString();
        edited([&label](core::SpreadData& spread) { spread.captionLabel = label; });
    });
    connect(m_captionField, &QLineEdit::textEdited, this, [edited](const QString& text) {
        edited([&text](core::SpreadData& spread) { spread.captionText = text; }, true);
    });
    connect(m_captionField, &QLineEdit::editingFinished, this, &SpreadCard::saveNow);
    connect(m_numbersBox, &QCheckBox::toggled, this, [edited](bool checked) {
        edited([checked](core::SpreadData& spread) { spread.pageNumbers = checked; });
    });
    connect(m_contentsBox, &QCheckBox::toggled, this, [edited](bool checked) {
        edited([checked](core::SpreadData& spread) { spread.inContents = checked; });
    });

    // The pages: a drag or the arrows, the zoom of a picture that fills its area, the middle
    connect(m_view, &SpreadPagesView::cropMoved, this, [edited](double cropX, double cropY) {
        edited(
            [cropX, cropY](core::SpreadData& spread) {
                spread.cropX = cropX;
                spread.cropY = cropY;
            },
            true);
    });
    connect(m_view, &SpreadPagesView::zoomStepped, this, [edited](int step) {
        edited(
            [step](core::SpreadData& spread) {
                spread.zoom = std::clamp(spread.zoom + step * ZOOM_STEP, core::SpreadData::MIN_ZOOM,
                                       core::SpreadData::MAX_ZOOM);
            },
            true);
    });
    connect(m_view, &SpreadPagesView::centerAsked, m_centerButton, &QPushButton::click);

    // The bar
    connect(m_actualSizeButton, &QToolButton::toggled, this, [this](bool actual) {
        m_view->setActualSize(actual);
    });
    connect(m_foldButton, &QToolButton::toggled, m_view, &SpreadPagesView::setFoldShown);
    connect(m_guidesButton, &QToolButton::toggled, m_view, &SpreadPagesView::setGuidesShown);

    // The place in the book
    connect(m_earlierButton, &QPushButton::clicked, this,
            [this]() { emit moveRequested(m_elementId, -1); });
    connect(m_laterButton, &QPushButton::clicked, this,
            [this]() { emit moveRequested(m_elementId, 1); });
}

// =============================================================================
// What the card shows
// =============================================================================

void SpreadCard::refresh() {
    if (m_loading) {
        return;  // its own change being saved
    }
    if (m_saveTimer->isActive()) {
        saveNow();
    }
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr) {
        return;  // its tab closes
    }
    m_data = core::ProjectManager::spreadOf(*element);
    fillPictures();
    fillLabels();
    showData();
}

void SpreadCard::focusPicture() {
    m_pictureBox->setFocus(Qt::OtherFocusReason);
}

void SpreadCard::setCaptions(const Captions& captions) {
    m_captions = captions;
    m_labelList.clear();
    fillLabels();
    showData();
}

void SpreadCard::fillPictures() {
    QList<const core::ProjectElement*> pictures;
    QList<const core::ProjectElement*> covers;
    for (const core::ProjectElement* picture : m_book.pictureMaterials()) {
        (CoverNames::isCoverMaterial(m_book, *picture) ? covers : pictures) << picture;
    }
    QString list;
    for (const core::ProjectElement* picture : pictures + covers) {
        list += picture->id + QLatin1Char('\t') + picture->title + QLatin1Char('\t') +
                picture->file + QLatin1Char('\n');
    }
    if (list == m_pictureList && m_pictureBox->count() > 0) {
        return;
    }
    m_pictureList = list;
    const QSignalBlocker blocker(m_pictureBox);
    m_pictureBox->clear();
    //: The list of the picture of a spread: none. In Polish: (bez obrazu)
    m_pictureBox->addItem(tr("(no picture)"), QString());
    const auto add = [this](const QList<const core::ProjectElement*>& group) {
        for (const core::ProjectElement* picture : group) {
            m_pictureBox->addItem(picture->title.isEmpty() ? picture->file : picture->title,
                                  picture->id);
        }
    };
    add(pictures);
    if (!pictures.isEmpty() && !covers.isEmpty()) {
        m_pictureBox->insertSeparator(m_pictureBox->count());
    }
    add(covers);
}

void SpreadCard::fillLabels() {
    QStringList labels = m_captions.labels ? m_captions.labels() : settingsCaptionLabels();
    if (!m_data.captionLabel.isEmpty() && !labels.contains(m_data.captionLabel)) {
        labels << m_data.captionLabel;
    }
    const QString list = labels.join(QLatin1Char('\n'));
    if (list == m_labelList && m_labelBox->count() > 0) {
        return;
    }
    m_labelList = list;
    const QSignalBlocker blocker(m_labelBox);
    m_labelBox->clear();
    //: The caption of a spread: its text alone. In Polish: bez etykiety i numeru
    m_labelBox->addItem(tr("without a label and number"), QString());
    for (const QString& label : labels) {
        //: The caption of a spread: a label with its number; %1: the label ("Map"). In Polish:
        //: %1 z numerem
        m_labelBox->addItem(tr("%1 with a number").arg(label), label);
    }
}

void SpreadCard::showData() {
    m_loading = true;
    const bool editable = !m_book.isReadOnly();
    const int picture = m_pictureBox->findData(m_data.picture);
    m_pictureBox->setCurrentIndex(std::max(0, picture));
    m_marginsButton->setChecked(!m_data.toEdges());
    m_edgeButton->setChecked(m_data.toEdges());
    m_areaRow->setFocusProxy(m_data.toEdges() ? m_edgeButton : m_marginsButton);
    m_wholeButton->setChecked(!m_data.fills());
    m_fillButton->setChecked(m_data.fills());
    m_fitRow->setFocusProxy(m_data.fills() ? m_fillButton : m_wholeButton);
    m_cropHint->setVisible(m_data.fills());
    m_centerButton->setVisible(m_data.fills());
    {
        const QSignalBlocker blocker(m_gutterBox);
        m_gutterBox->setStoredValue(m_data.gutter);
    }
    m_captionBox->setChecked(m_data.caption);
    m_labelBox->setVisible(m_data.caption);
    m_captionField->setVisible(m_data.caption);
    m_labelBox->setCurrentIndex(std::max(0, m_labelBox->findData(m_data.captionLabel)));
    if (m_captionField->text() != m_data.captionText) {
        m_captionField->setText(m_data.captionText);
    }
    m_edgeCaptionHint->setVisible(m_data.caption && m_data.toEdges());
    m_numbersBox->setChecked(m_data.pageNumbers);
    m_contentsBox->setChecked(m_data.inContents);
    for (QWidget* field : std::initializer_list<QWidget*>{
             m_pictureBox, m_diskButton, m_marginsButton, m_edgeButton, m_wholeButton,
             m_fillButton, m_centerButton, m_gutterBox, m_captionBox, m_labelBox,
             m_captionField, m_numbersBox, m_contentsBox}) {
        field->setEnabled(editable);
    }
    m_diskButton->setEnabled(editable && static_cast<bool>(pictureKind(m_book)));
    m_loading = false;
    showPicture();
    showLook();
    showPlace();
    showQuality();
    showMoves();
}

void SpreadCard::showPicture() {
    const core::ProjectElement* material =
        m_data.picture.isEmpty() ? nullptr : m_book.findElement(m_data.picture);
    const QString path = material != nullptr ? m_book.filePathOf(*material) : QString();
    const int rotation = material != nullptr ? material->rotation : 0;
    if (path == m_pictureFile && rotation == m_pictureRotation &&
        (path.isEmpty() || !m_picture.isNull())) {
        return;
    }
    m_pictureFile = path;
    m_pictureRotation = rotation;
    m_picture = QImage();
    m_pictureSize = QSize();
    if (path.isEmpty()) {
        m_fileInfo->clear();
        m_fileInfo->hide();
        m_view->setPicture(QImage());
        return;
    }
    const MaterialFile file = MaterialFile::of(path);
    if (file.readable) {
        m_pictureSize = editor::turnedSize(file.size, rotation);
        m_picture = readPicture(path, rotation, QSize(PICTURE_BOUNDS, PICTURE_BOUNDS));
    }
    if (m_picture.isNull()) {
        m_pictureSize = QSize();
        //: Under the list of the picture of a spread; %1: the name of the file. In Polish:
        //: Nie można odczytać pliku „%1”.
        m_fileInfo->setText(tr("The file \"%1\" cannot be read.").arg(file.name));
    } else {
        const QLocale locale;
        //: Under the list of the picture of a spread: the file, its pixels and its list; %1:
        //: the name of the file, %2 and %3: its width and height, %4: the list ("Pictures").
        //: In Polish: %1 · %2 × %3 px · %4
        m_fileInfo->setText(tr("%1 · %2 × %3 px · %4")
                                .arg(file.name, locale.toString(m_pictureSize.width()),
                                     locale.toString(m_pictureSize.height()),
                                     MaterialList::nameOf(m_book.kindOf(*material).reference())));
    }
    m_fileInfo->show();
    m_view->setPicture(m_picture);
}

editor::SpreadLook SpreadCard::lookOf(const core::SpreadData& spread) const {
    editor::SpreadLook look;
    look.picture = spread.picture;
    look.toEdges = spread.toEdges();
    look.fills = spread.fills();
    look.cropX = spread.cropX;
    look.cropY = spread.cropY;
    look.zoom = spread.zoom;
    look.gutter = spread.gutter;
    if (spread.caption) {
        const editor::CaptionContext context =
            m_captions.context ? m_captions.context(m_elementId) : editor::CaptionContext{};
        look.caption = editor::spreadCaptionText(spread.captionLabel, spread.captionText, context);
    }
    look.pageNumbers = spread.pageNumbers;
    return look;
}

void SpreadCard::showLook() {
    const editor::SpreadLook look = lookOf(m_data);
    m_view->setLook(look, m_page);
    //: Under Fill on the card of a spread; %1: the zoom in percent. In Polish: Kadr: przeciągnij
    //: obraz myszą albo przesuń go strzałkami; Ctrl+] i Ctrl+[ – powiększenie (%1%).
    m_cropHint->setText(tr("Crop: drag the picture with the mouse or move it with the arrows; "
                           "Ctrl+] and Ctrl+[ – zoom (%1%).")
                            .arg(m_data.zoom));

    // The small picture of its two pages for the strip, without the caption and the numbers
    if (m_page.size.isEmpty()) {
        return;
    }
    m_strip->setPageRatio(m_page.size.height() / m_page.size.width());
    m_stripArea->setFixedHeight(m_strip->sizeHint().height() +
                                m_stripArea->horizontalScrollBar()->sizeHint().height());
    if (m_picture.isNull()) {
        m_strip->setPicture(QImage());
        return;
    }
    const double perMm = STRIP_DPI / MM_PER_INCH;
    QImage small(QSize(static_cast<int>(std::ceil(2.0 * m_page.size.width() * perMm)),
                       static_cast<int>(std::ceil(m_page.size.height() * perMm))),
                 QImage::Format_ARGB32_Premultiplied);
    const SpreadPagesView::Colors& colors = m_view->colors();
    small.fill(colors.paper);
    editor::SpreadLook plain = look;
    plain.caption.clear();
    plain.pageNumbers = false;
    editor::SpreadPaint paint;
    paint.dpi = STRIP_DPI;
    paint.paper = colors.paper;
    paint.text = colors.text;
    {
        QPainter painter(&small);
        for (const bool right : {false, true}) {
            painter.save();
            painter.translate(right ? m_page.size.width() * perMm : 0.0, 0.0);
            editor::paintSpreadPage(&painter, plain, m_page, m_picture, right, paint);
            painter.restore();
        }
    }
    m_strip->setPicture(small);
}

void SpreadCard::showPlace() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    const QString title = element != nullptr ? element->title : QString();
    const std::vector<BookPageRun> runs =
        m_pages != nullptr ? m_pages->runsOfBook(m_elementId) : std::vector<BookPageRun>();
    const auto own = std::find_if(runs.cbegin(), runs.cend(), [this](const BookPageRun& run) {
        return run.elementId == m_elementId;
    });
    if (own == runs.cend()) {
        //: The bar of the card of a spread before its pages are counted; %1: its title. In
        //: Polish: %1 · rozkładówka
        m_head->setText(tr("%1 · spread").arg(title));
        m_view->setFirstPage(0);
        //: In Polish: Strony książki są liczone...
        m_place->setText(tr("The pages of the book are being counted..."));
        m_strip->setPages({});
        return;
    }
    const int first = own->index;  // from 0: its number is first + 1
    //: The bar of the card of a spread; %1: its title, %2 and %3: its pages. In Polish: %1 ·
    //: rozkładówka · strony %2–%3
    m_head->setText(tr("%1 · spread · pages %2–%3").arg(title).arg(first + 1).arg(first + 2));
    m_view->setFirstPage(first + 1);

    // The pages around it: each counted place, the pages between them empty
    int total = 0;
    for (const BookPageRun& run : runs) {
        total = std::max(total, run.index + run.pages);
    }
    const auto runAt = [&runs](int index) -> const BookPageRun* {
        const auto found = std::find_if(runs.cbegin(), runs.cend(), [index](const BookPageRun& run) {
            return run.index <= index && index < run.index + run.pages;
        });
        return found != runs.cend() ? &*found : nullptr;
    };
    // From a left page (an odd index), or from page 1, to a right page (an even index)
    int from = std::max(0, first - STRIP_AROUND);
    if (from % 2 == 0 && from > 0) {
        --from;
    }
    int to = std::min(total - 1, first + 1 + STRIP_AROUND);
    if (to % 2 == 1 && to + 1 < total) {
        ++to;
    }
    std::vector<SpreadStrip::Page> pages;
    for (int index = from; index <= to; ++index) {
        SpreadStrip::Page page;
        page.number = index + 1;
        if (const BookPageRun* run = runAt(index)) {
            page.title = run->title;
            page.spread = run->spread;
            page.own = run->elementId == m_elementId;
            page.right = run->spread && index == run->index + 1;
        }
        pages.push_back(page);
    }
    m_strip->setPages(pages);

    // The empty pages beside it
    QStringList blanks;
    if (first > 0 && runAt(first - 1) == nullptr) {
        //: Beside the pages of a spread; %1: the number of the empty page. In Polish: przed nią
        //: pusta strona %1
        blanks << tr("before it the empty page %1").arg(first);
    }
    if (first + 2 < total && runAt(first + 2) == nullptr) {
        //: In Polish: po niej pusta strona %1
        blanks << tr("after it the empty page %1").arg(first + 3);
    }
    //: The place of a spread in the book; %1 and %2: its pages. In Polish: strony %1–%2
    QString place = tr("pages %1–%2").arg(first + 1).arg(first + 2);
    place += QStringLiteral(" · ") +
             (blanks.isEmpty()
                  //: In Polish: bez pustych stron obok
                  ? tr("no empty pages beside it")
                  : blanks.join(QStringLiteral(", ")));
    m_place->setText(place);
}

void SpreadCard::showQuality() {
    if (m_pictureSize.isEmpty()) {
        m_quality->clear();
        m_quality->hide();
        m_qualityIcon->hide();
        return;
    }
    const editor::SpreadGeometry geometry =
        editor::spreadGeometry(lookOf(m_data), m_page, m_pictureSize);
    const int dpi = editor::spreadDpi(geometry, m_pictureSize);
    QString verdict;
    QString iconId;
    if (dpi >= PRINT_DPI) {
        //: In Polish: wystarczy do druku
        verdict = tr("enough for print");
        iconId = QStringLiteral("common.checkCircle");
    } else if (dpi >= HOME_PRINT_DPI) {
        //: In Polish: wystarczy do wydruku w domu; drukarnie chcą zwykle co najmniej 300 dpi
        verdict = tr("enough for printing at home; print shops usually want at least 300 dpi");
        iconId = QStringLiteral("common.warning");
    } else {
        //: In Polish: za mało do druku – obraz wyjdzie nieostry; potrzebny obraz o większej
        //: liczbie pikseli
        verdict = tr("too little for print – the picture will come out blurred; a picture with "
                     "more pixels is needed");
        iconId = QStringLiteral("common.error");
    }
    //: On the card of a spread; %1: the dots per inch of its picture, %2: what they are good
    //: for. In Polish: Jakość do druku: %1 dpi – %2
    m_quality->setText(tr("Quality for print: %1 dpi – %2").arg(dpi).arg(verdict));
    m_qualityIcon->setPixmap(core::ArtProvider::getInstance()
                                 .getIcon(iconId, core::IconContext::Menu)
                                 .pixmap(QSize(16, 16)));
    m_quality->show();
    m_qualityIcon->show();
}

void SpreadCard::showColors() {
    const bool dark = core::SettingsManager::getInstance().get<bool>("editor.darkMode");
    const editor::EditorColorMode mode =
        dark ? editor::EditorColorMode::Dark : editor::EditorColorMode::Light;
    const editor::EditorColors colors = EditorPanel::themeColors();
    SpreadPagesView::Colors own;
    own.paper = colors.background(mode);
    own.text = colors.textColor(mode);
    own.fold = colors.spreadFoldColor(mode);
    own.guide = colors.spreadGuideColor(mode);
    m_view->setColors(own);
    m_strip->setColors(own.paper, own.text, own.guide);
    if (auto* desk = dynamic_cast<SpreadDesk*>(m_desk->widget())) {
        desk->setColor(colors.desk(mode));
    }
}

void SpreadCard::showMoves() {
    qsizetype index = -1;
    const core::BookProject* project = m_book.project();
    const QList<core::ProjectElement>* list =
        project != nullptr ? project->listOf(m_elementId, &index) : nullptr;
    const bool editable = !m_book.isReadOnly() && list != nullptr;
    m_earlierButton->setEnabled(editable && index > 0);
    m_laterButton->setEnabled(editable && index >= 0 && index + 1 < list->size());
}

// =============================================================================
// Changes
// =============================================================================

void SpreadCard::store(const core::SpreadData& spread, bool later) {
    if (m_book.isReadOnly()) {
        showLook();  // the pages as they were
        return;
    }
    m_data = spread.normalized();
    if (later) {
        showLook();
        showQuality();
        m_saveTimer->start();
        return;
    }
    saveNow();
}

void SpreadCard::saveNow() {
    m_saveTimer->stop();
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || m_book.isReadOnly() ||
        core::ProjectManager::spreadOf(*element) == m_data) {
        return;
    }
    m_loading = true;
    const bool saved = m_book.setSpread(m_elementId, m_data);
    m_loading = false;
    if (!saved) {
        //: The status bar. In Polish: Nie udało się zapisać rozkładówki.
        emit statusMessage(tr("The spread could not be saved."));
        if (const core::ProjectElement* again = m_book.findElement(m_elementId)) {
            m_data = core::ProjectManager::spreadOf(*again);
        }
    }
    showData();
}

void SpreadCard::pictureFromDisk() {
    const core::KindRef kind = pictureKind(m_book);
    if (m_book.isReadOnly() || !kind) {
        return;
    }
    using Operation = ProgramFolders::Operation;
    const QString path = QFileDialog::getOpenFileName(
        this,
        //: The system window choosing the picture of a spread. In Polish: Wybierz obraz
        //: rozkładówki
        tr("Choose the Picture of the Spread"), ProgramFolders::windowFolder(Operation::AddMaterial),
        //: The pictures of the window. In Polish: Obrazy
        editor::imageFileFilter(tr("Images")) + QStringLiteral(";;") +
            //: In Polish: Wszystkie pliki (*)
            tr("All Files (*)"));
    if (path.isEmpty()) {
        return;
    }
    ProgramFolders::remember(Operation::AddMaterial, QFileInfo(path).absolutePath());
    //: In Polish: Nie dodano obrazu
    const QString title = tr("Picture Not Added");
    if (!MaterialFile::of(path).readable) {
        dialogs::MessageDialog::warning(
            this, title,
            //: %1: the name of the file. In Polish: Plik „%1” nie jest obrazem, który Kalahari
            //: potrafi pokazać.
            tr("The file \"%1\" is not a picture Kalahari can show.")
                .arg(QFileInfo(path).fileName()));
        return;
    }
    core::NewMaterial material;
    material.title = QFileInfo(path).completeBaseName();
    material.filePath = path;
    const QString materialId = m_book.addMaterial(material, kind);
    if (materialId.isEmpty()) {
        dialogs::MessageDialog::warning(
            this, title,
            //: In Polish: Nie udało się skopiować obrazu do materiałów książki.
            tr("The picture could not be copied into the materials of the book."));
        return;
    }
    emit materialAdded(materialId);
    fillPictures();
    core::SpreadData spread = m_data;
    spread.picture = materialId;
    store(spread);
}

QString SpreadCard::qualityText() const {
    return m_quality->isVisibleTo(this) ? m_quality->text() : QString();
}

QString SpreadCard::placeText() const {
    return m_place->text();
}

QString SpreadCard::headText() const {
    return m_head->text();
}

}  // namespace kalahari::gui
