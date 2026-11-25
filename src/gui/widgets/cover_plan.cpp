// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file cover_plan.cpp
/// @brief The plan of the cover: its parts on sheets with the materials in their places

#include "kalahari/gui/widgets/cover_plan.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/dialogs/color_dialog.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdlib>

namespace kalahari::gui {

namespace {

using core::CoverMaterial;
using core::CoverParts;
using core::Endpaper;

/// Lines of the text of a card at most; the rest is cut
constexpr int CARD_LINES = 6;

/// Size of the picture of a design on its button
constexpr int DESIGN_ICON = 24;

/// Size of the button of a pattern, and of the cells of the pattern on it
constexpr int PATTERN_BUTTON = 38;
constexpr int PATTERN_BUTTON_CELL = 12;

/// Size of the picture of a color on its button
constexpr int SWATCH_WIDTH = 30;
constexpr int SWATCH_HEIGHT = 18;

/// The width a spread asks for, and the most it takes; its height is three fourths of it
constexpr int SPREAD_WIDTH = 330;
constexpr int SPREAD_MAX_WIDTH = 420;

/// The cells of a pattern across a spread: a cell is a part of its width (22 of 400)
constexpr double PATTERN_CELL_PART = 22.0 / 400.0;

/// The width of the fold between the pages of a spread, and how dark its middle is (of 255)
constexpr int FOLD_WIDTH = 18;
constexpr int FOLD_ALPHA = 56;

/// How faint the design behind the front is, so that the cards are read
constexpr double DESIGN_OPACITY = 0.35;

/// The columns of the sheets from the left: the back flap, the back, the spine, the front and
/// the front flap
const std::array<const char*, 5> COVER_COLUMNS{CoverMaterial::BACK_FLAP, CoverMaterial::BACK,
                                               CoverMaterial::SPINE, CoverMaterial::FRONT,
                                               CoverMaterial::FRONT_FLAP};
const std::array<const char*, 5> JACKET_COLUMNS{
    CoverMaterial::JACKET_BACK_FLAP, CoverMaterial::JACKET_BACK, CoverMaterial::JACKET_SPINE,
    CoverMaterial::JACKET_FRONT, CoverMaterial::JACKET_FRONT_FLAP};

/// Whether @p place is a column of @p columns
bool isColumn(const std::array<const char*, 5>& columns, const QString& place) {
    return std::any_of(columns.cbegin(), columns.cend(),
                       [&place](const char* column) { return place == QLatin1String(column); });
}

bool isSpine(const QString& place) {
    return place == QLatin1String(CoverMaterial::SPINE) ||
           place == QLatin1String(CoverMaterial::JACKET_SPINE);
}

bool isFlap(const QString& place) {
    return CoverMaterial::isCoverFlap(place) ||
           place == QLatin1String(CoverMaterial::JACKET_FRONT_FLAP) ||
           place == QLatin1String(CoverMaterial::JACKET_BACK_FLAP);
}

bool isFront(const QString& place) {
    return place == QLatin1String(CoverMaterial::FRONT) ||
           place == QLatin1String(CoverMaterial::JACKET_FRONT);
}

/// The color "#RRGGBB" @p text, else @p fallback
QColor colorOrTheme(const QString& text, const char* themeKey, const QColor& fallback) {
    const QColor own(text);
    if (!text.isEmpty() && own.isValid()) {
        return own;
    }
    return core::ThemeManager::getInstance().editorColor(themeKey, fallback);
}

/// One cell of @p pattern, @p cell pixels large: the ink on the paper
QImage patternCell(const QString& pattern, const QColor& ink, const QColor& paper, int cell) {
    const int size = std::max(4, cell);
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(paper);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal s = size;
    const qreal h = s / 2;
    const auto pen = [&ink, s](qreal part) {
        QPen line(ink);
        line.setWidthF(std::max(1.0, s * part));
        line.setCapStyle(Qt::FlatCap);
        return line;
    };
    if (pattern == QLatin1String(Endpaper::STRIPES)) {
        painter.fillRect(QRectF(0, 0, s * 0.4, s), ink);
    } else if (pattern == QLatin1String(Endpaper::GRID)) {
        painter.setPen(pen(0.08));
        painter.drawLine(QPointF(0, 0), QPointF(s, 0));
        painter.drawLine(QPointF(0, 0), QPointF(0, s));
    } else if (pattern == QLatin1String(Endpaper::DIAGONAL)) {
        painter.setPen(pen(0.12));
        painter.drawLine(QPointF(0, s), QPointF(s, 0));
        painter.drawLine(QPointF(-h, h), QPointF(h, -h));
        painter.drawLine(QPointF(h, s + h), QPointF(s + h, h));
    } else if (pattern == QLatin1String(Endpaper::CHECKER)) {
        painter.fillRect(QRectF(0, 0, h, h), ink);
        painter.fillRect(QRectF(h, h, h, h), ink);
    } else if (pattern == QLatin1String(Endpaper::WAVES)) {
        QPainterPath wave(QPointF(0, h));
        wave.quadTo(QPointF(s / 4, s / 4), QPointF(h, h));
        wave.quadTo(QPointF(s * 3 / 4, s * 3 / 4), QPointF(s, h));
        painter.setPen(pen(0.08));
        painter.drawPath(wave);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(ink);
        painter.drawEllipse(QPointF(h, h), s * 0.18, s * 0.18);
    }
    return image;
}

/// Fill @p area with the cells of @p pattern, from its corner
void fillPattern(QPainter& painter, const QRect& area, const QString& pattern, const QColor& ink,
                 const QColor& paper, int cell) {
    QBrush brush(patternCell(pattern, ink, paper, cell));
    brush.setTransform(QTransform::fromTranslate(area.left(), area.top()));
    painter.fillRect(area, brush);
}

/// The picture of @p color on its button
QIcon swatchIcon(const QColor& color, const QColor& border) {
    QPixmap pixmap(SWATCH_WIDTH, SWATCH_HEIGHT);
    pixmap.fill(color);
    QPainter painter(&pixmap);
    painter.setPen(border);
    painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));
    return QIcon(pixmap);
}

/// The cover unfolded: the columns on a background, the design behind the front, the cloth of
/// a hardback under a jacket
class CoverSheet : public QWidget {
public:
    using QWidget::QWidget;

    void setPicture(const QImage& picture) {
        m_picture = picture;
        update();
    }

    void setFront(QWidget* front) {
        m_front = front;
        update();
    }

    void setCloth(bool cloth) {
        m_cloth = cloth;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        const QPalette& colors = palette();
        painter.fillRect(rect(), colors.color(QPalette::AlternateBase));
        const QList<QWidget*> columns =
            findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        if (m_cloth) {
            // The cover of a hardback under its jacket: cloth, faint lines across its boards
            QColor thread = colors.color(QPalette::Mid);
            thread.setAlphaF(0.25);
            painter.setPen(thread);
            for (const QWidget* column : columns) {
                if (!column->isVisible() || column->property("flap").toBool()) {
                    continue;
                }
                const QRect board = column->geometry();
                for (int y = board.top() + 3; y < board.bottom(); y += 4) {
                    painter.drawLine(board.left(), y, board.right(), y);
                }
            }
        }
        if (m_front != nullptr && !m_picture.isNull()) {
            // The design fills the front, cut to its shape, faint so that the cards are read
            const QRect front = m_front->geometry();
            const QSize scaled =
                m_picture.size().scaled(front.size(), Qt::KeepAspectRatioByExpanding);
            const QRect target(front.center().x() - scaled.width() / 2,
                               front.center().y() - scaled.height() / 2, scaled.width(),
                               scaled.height());
            painter.save();
            painter.setClipRect(front);
            painter.setOpacity(DESIGN_OPACITY);
            painter.drawImage(target, m_picture);
            painter.restore();
        }
        // The folds between the columns
        QPen fold(colors.color(QPalette::Mid));
        fold.setStyle(Qt::DashLine);
        painter.setPen(fold);
        for (const QWidget* column : columns) {
            if (column->isVisible() && column->geometry().left() > 0) {
                painter.drawLine(column->geometry().left() - 1, 0, column->geometry().left() - 1,
                                 height());
            }
        }
    }

private:
    QImage m_picture;
    QWidget* m_front = nullptr;
    bool m_cloth = false;
};

/// The column of @p place on the plan: its name above its cards
QWidget* newColumn(const QString& place, int count, const QString& length, QWidget* parent) {
    auto* column = new QWidget(parent);
    column->setObjectName(QStringLiteral("coverPlanColumn_") + place);
    column->setProperty("flap", isFlap(place));
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    auto* heading = new QLabel(column);
    heading->setObjectName(QStringLiteral("coverPlanHeading"));
    heading->setWordWrap(true);
    QString text = QStringLiteral("<b>%1</b>").arg(
        CoverNames::shortPlaceName(place).toUpper().toHtmlEscaped());
    if (count > 0) {
        text += QStringLiteral("<br><small>%1</small>").arg(
            (length.isEmpty() ? QLocale().toString(count)
                              : QStringLiteral("%1 · %2").arg(QLocale().toString(count), length))
                .toHtmlEscaped());
    } else {
        //: An empty place on the plan of the cover. In Polish: pusto
        text += QStringLiteral("<br><small>%1</small>").arg(CoverPlan::tr("empty").toHtmlEscaped());
    }
    heading->setText(text);
    layout->addWidget(heading);
    return column;
}

} // namespace

// =============================================================================
// A card
// =============================================================================

CoverPlanCard::CoverPlanCard(const CoverPlanItem& item, bool narrow, QWidget* parent,
                             const QString& remark)
    : QFrame(parent)
    , m_id(item.id)
    , m_rejected(item.state == QLatin1String(CoverMaterial::REJECTED)) {
    setObjectName(QStringLiteral("coverPlanCard"));
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);

    const QString shown = item.text.isEmpty() ? item.title : item.text;
    if (!item.picture.isEmpty() && item.text.isEmpty()) {
        auto* picture = new QLabel(this);
        const int side = narrow ? 32 : 96;
        const QImage image = readPicture(item.picture, item.rotation,
                                         QSize(side, side) * devicePixelRatioF());
        QPixmap pixmap = QPixmap::fromImage(image);
        pixmap.setDevicePixelRatio(devicePixelRatioF());
        picture->setPixmap(pixmap);
        layout->addWidget(picture);
    }
    auto* text = new QLabel(this);
    text->setObjectName(QStringLiteral("coverPlanText"));
    text->setWordWrap(true);
    text->setTextFormat(Qt::PlainText);
    text->setText(shown);
    QFont font = text->font();
    font.setStrikeOut(m_rejected);
    if (item.part == QLatin1String(CoverMaterial::TAGLINE) ||
        item.part == QLatin1String(CoverMaterial::OPINION)) {
        font.setItalic(true);
    }
    text->setFont(font);
    text->setMaximumHeight(text->fontMetrics().lineSpacing() * (narrow ? 3 : CARD_LINES) + 2);
    text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(text);

    if (!narrow) {
        QString about = CoverNames::partName(item.part);
        if (!item.author.isEmpty()) {
            //: What a card on the plan of the cover is, with its author: %1 the kind, %2 the
            //: author. In Polish: %1 – %2
            about = CoverPlan::tr("%1 – %2").arg(about, item.author);
        }
        auto* meta = new QLabel(about, this);
        meta->setObjectName(QStringLiteral("coverPlanMeta"));
        meta->setWordWrap(true);
        meta->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(meta);
        auto* state = new QLabel(CoverNames::shortStateName(item.state), this);
        state->setObjectName(QStringLiteral("coverPlanState"));
        QFont stateFont = state->font();
        stateFont.setBold(item.state == QLatin1String(CoverMaterial::CHOSEN));
        stateFont.setStrikeOut(m_rejected);
        state->setFont(stateFont);
        layout->addWidget(state);
    }
    if (!remark.isEmpty()) {
        //: Why a material of the cover waits under the plan: %1 the part turned off. In
        //: Polish: (%1)
        auto* note = new QLabel(CoverPlan::tr("(%1)").arg(remark), this);
        note->setObjectName(QStringLiteral("coverPlanRemark"));
        note->setWordWrap(true);
        QFont noteFont = note->font();
        noteFont.setItalic(true);
        note->setFont(noteFont);
        note->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(note);
    }
    setToolTip(shown);
    QString name = QStringLiteral("%1: %2 (%3)").arg(CoverNames::partName(item.part), shown,
                                                     CoverNames::stateName(item.state));
    if (!remark.isEmpty()) {
        name += QStringLiteral(", ") + remark;
    }
    setAccessibleName(name);
}

void CoverPlanCard::setCurrent(bool current) {
    if (m_current != current) {
        m_current = current;
        update();
    }
}

void CoverPlanCard::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPalette& colors = palette();
    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 4, 4);
    painter.fillPath(shape, colors.color(QPalette::Base));
    QPen border(m_current || hasFocus() ? colors.color(QPalette::Highlight)
                                        : colors.color(QPalette::Mid));
    border.setWidthF(m_current || hasFocus() ? 2.0 : 1.0);
    if (m_rejected) {
        border.setStyle(Qt::DashLine);
    }
    painter.setPen(border);
    painter.drawPath(shape);
}

void CoverPlanCard::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    emit clicked(m_id);
    QFrame::mousePressEvent(event);
}

void CoverPlanCard::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(m_id);
    }
}

void CoverPlanCard::contextMenuEvent(QContextMenuEvent* event) {
    setFocus(Qt::MouseFocusReason);
    emit clicked(m_id);
    emit menuRequested(m_id, event->globalPos());
}

void CoverPlanCard::focusInEvent(QFocusEvent* event) {
    QFrame::focusInEvent(event);
    update();
}

// =============================================================================
// A spread of an endpaper
// =============================================================================

EndpaperSpread::EndpaperSpread(QWidget* parent)
    : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setMinimumWidth(SPREAD_WIDTH / 2);
    setMaximumWidth(SPREAD_MAX_WIDTH);
}

void EndpaperSpread::setEndpaper(const core::Endpaper& endpaper, const QImage& picture) {
    m_endpaper = endpaper;
    m_picture = picture;
    update();
}

void EndpaperSpread::setCurrent(bool current) {
    if (m_current != current) {
        m_current = current;
        update();
    }
}

int EndpaperSpread::heightForWidth(int width) const {
    return width * 3 / 4;
}

QSize EndpaperSpread::sizeHint() const {
    return {SPREAD_WIDTH, heightForWidth(SPREAD_WIDTH)};
}

void EndpaperSpread::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QPalette& colors = palette();
    const QRect area = rect().adjusted(1, 1, -1, -1);
    const QString& fill = m_endpaper.fill;
    const bool picture =
        fill == QLatin1String(Endpaper::PICTURE) || fill == QLatin1String(Endpaper::TILE);
    if (picture && m_picture.isNull()) {
        // No picture yet, or one that cannot be read
        painter.fillRect(area, CoverPlan::blankColor());
        painter.setPen(colors.color(QPalette::PlaceholderText));
        //: On the spread of an endpaper filled with a picture that is not chosen or cannot be
        //: read. In Polish: Brak obrazu
        painter.drawText(area, Qt::AlignCenter | Qt::TextWordWrap, CoverPlan::tr("No picture"));
    } else if (fill == QLatin1String(Endpaper::PICTURE)) {
        // One picture over both pages, cut to the shape of the spread
        const QSize scaled = m_picture.size().scaled(area.size(), Qt::KeepAspectRatioByExpanding);
        const QRect target(area.center().x() - scaled.width() / 2,
                           area.center().y() - scaled.height() / 2, scaled.width(),
                           scaled.height());
        painter.save();
        painter.setClipRect(area);
        painter.drawImage(target, m_picture);
        painter.restore();
    } else if (fill == QLatin1String(Endpaper::TILE)) {
        // The picture as wide as its part of a page, repeated like a wallpaper
        const int width =
            std::max(4, area.width() / 2 * std::clamp(m_endpaper.scale, Endpaper::MIN_SCALE,
                                                      Endpaper::MAX_SCALE) / 100);
        const int height = std::max(
            1, static_cast<int>(static_cast<qint64>(width) * m_picture.height() /
                                std::max(1, m_picture.width())));
        QBrush brush(m_picture.scaled(width, height, Qt::IgnoreAspectRatio,
                                      Qt::SmoothTransformation));
        brush.setTransform(QTransform::fromTranslate(area.left(), area.top()));
        painter.fillRect(area, brush);
    } else if (fill == QLatin1String(Endpaper::PATTERN)) {
        fillPattern(painter, area, m_endpaper.shownPattern(),
                    CoverPlan::patternColorOf(m_endpaper), CoverPlan::paperColorOf(m_endpaper),
                    static_cast<int>(area.width() * PATTERN_CELL_PART));
    } else if (fill == QLatin1String(Endpaper::COLOR)) {
        painter.fillRect(area, CoverPlan::colorOf(m_endpaper));
    } else {
        painter.fillRect(area, CoverPlan::blankColor());
    }

    // The fold between the pasted page and the free leaf
    QColor shadow = colors.color(QPalette::Shadow);
    QColor clear = shadow;
    clear.setAlpha(0);
    shadow.setAlpha(FOLD_ALPHA);
    const qreal middle = area.center().x() + 0.5;
    QLinearGradient fold(middle - FOLD_WIDTH / 2.0, 0, middle + FOLD_WIDTH / 2.0, 0);
    fold.setColorAt(0, clear);
    fold.setColorAt(0.5, shadow);
    fold.setColorAt(1, clear);
    painter.fillRect(QRectF(middle - FOLD_WIDTH / 2.0, area.top(), FOLD_WIDTH, area.height()),
                     fold);

    const bool marked = m_current || hasFocus();
    QPen border(marked ? colors.color(QPalette::Highlight) : colors.color(QPalette::Mid));
    border.setWidth(marked ? 2 : 1);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(marked ? rect().adjusted(1, 1, -1, -1) : rect().adjusted(0, 0, -1, -1));
}

void EndpaperSpread::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}

void EndpaperSpread::focusInEvent(QFocusEvent* event) {
    QWidget::focusInEvent(event);
    update();
}

void EndpaperSpread::focusOutEvent(QFocusEvent* event) {
    QWidget::focusOutEvent(event);
    update();
}

// =============================================================================
// The plan
// =============================================================================

CoverPlan::CoverPlan(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("coverPlan"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // The parts of the cover of the book, and what the plan shows
    auto* options = new QWidget(this);
    auto* optionsLayout = new FlowLayout(options, 12, 4);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    auto* partsFrame = new QFrame(options);
    partsFrame->setObjectName(QStringLiteral("coverPlanParts"));
    partsFrame->setFrameShape(QFrame::StyledPanel);
    auto* partsLayout = new QHBoxLayout(partsFrame);
    partsLayout->setContentsMargins(8, 3, 8, 3);
    partsLayout->setSpacing(14);
    //: Before the boxes of the parts of the cover above its plan. In Polish: Części:
    auto* partsLabel = new QLabel(tr("Parts:"), partsFrame);
    partsLabel->setForegroundRole(QPalette::PlaceholderText);
    partsLayout->addWidget(partsLabel);
    //: A part of the cover of the book: the flaps of a paperback. Its access key differs from
    //: those of the menus. In Polish: Sk&rzydełka okładki
    m_flaps = new QCheckBox(tr("Cover flap&s"), partsFrame);
    m_flaps->setObjectName(QStringLiteral("coverPlanFlaps"));
    m_flaps->setChecked(true);
    //: A part of the cover of the book: the jacket of a hardback, always with its flaps. Its
    //: access key differs from those of the menus. In Polish: Obwol&uta
    m_jacket = new QCheckBox(tr("&Jacket"), partsFrame);
    m_jacket->setObjectName(QStringLiteral("coverPlanJacket"));
    //: A part of the cover of the book: the endpapers, at the front and at the back. Its
    //: access key differs from those of the menus. In Polish: Wykle&jki
    m_endpapers = new QCheckBox(tr("End&papers"), partsFrame);
    m_endpapers->setObjectName(QStringLiteral("coverPlanEndpapers"));
    partsLayout->addWidget(m_flaps);
    partsLayout->addWidget(m_jacket);
    partsLayout->addWidget(m_endpapers);
    //: Shows the materials rejected on the plan of the cover. In Polish: Pokaż odrzucone
    m_rejected = new QCheckBox(tr("Show rejected"), options);
    m_rejected->setObjectName(QStringLiteral("coverPlanRejected"));
    optionsLayout->addWidget(partsFrame);
    optionsLayout->addWidget(m_rejected);
    layout->addWidget(options);

    // The sheets of the parts on tabs
    m_tabs = new QTabBar(this);
    m_tabs->setObjectName(QStringLiteral("coverPlanSheets"));
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(true);
    m_tabs->setUsesScrollButtons(true);
    //: The tabs of the sheets of the plan of the cover. In Polish: Arkusze planu
    m_tabs->setAccessibleName(tr("Sheets of the plan"));
    layout->addWidget(m_tabs);

    // The designs of the sheet, one of them behind it
    m_designs = new QWidget(this);
    m_designs->setObjectName(QStringLiteral("coverPlanDesigns"));
    auto* designsRow = new FlowLayout(m_designs, 6, 4);
    designsRow->setContentsMargins(0, 0, 0, 0);
    m_designsLabel = new QLabel(m_designs);
    m_designsLabel->setForegroundRole(QPalette::PlaceholderText);
    //: Above the plan of the cover or of the jacket, without a design. In Polish: brak projektu
    m_noDesign = new QLabel(tr("no design"), m_designs);
    m_noDesign->setForegroundRole(QPalette::PlaceholderText);
    auto* designs = new QWidget(m_designs);
    m_designsLayout = new QHBoxLayout(designs);
    m_designsLayout->setContentsMargins(0, 0, 0, 0);
    m_designsLayout->setSpacing(4);
    m_designButtons = new QButtonGroup(this);
    m_designButtons->setExclusive(true);
    designsRow->addWidget(m_designsLabel);
    designsRow->addWidget(m_noDesign);
    designsRow->addWidget(designs);
    layout->addWidget(m_designs);

    auto* sheet = new CoverSheet(this);
    m_sheet = sheet;
    m_sheet->setObjectName(QStringLiteral("coverPlanSheet"));
    m_sheet->setAutoFillBackground(false);
    m_sheetLayout = new QHBoxLayout(m_sheet);
    m_sheetLayout->setContentsMargins(0, 0, 0, 0);
    m_sheetLayout->setSpacing(0);
    layout->addWidget(m_sheet, 1);

    // The endpapers: each spread with its settings
    m_endpaperSheet = new QWidget(this);
    m_endpaperSheet->setObjectName(QStringLiteral("coverPlanEndpaperSheet"));
    auto* endpapersLayout = new QVBoxLayout(m_endpaperSheet);
    endpapersLayout->setContentsMargins(0, 0, 0, 0);
    endpapersLayout->setSpacing(14);
    endpapersLayout->addWidget(newEndpaperRow(QString::fromLatin1(FRONT_ENDPAPER)));
    endpapersLayout->addWidget(newEndpaperRow(QString::fromLatin1(BACK_ENDPAPER)));
    endpapersLayout->addStretch();
    layout->addWidget(m_endpaperSheet, 1);

    m_unplacedLabel = new QLabel(this);
    m_unplacedLabel->setObjectName(QStringLiteral("coverPlanUnplacedLabel"));
    layout->addWidget(m_unplacedLabel);
    m_unplaced = new QWidget(this);
    m_unplaced->setObjectName(QStringLiteral("coverPlanUnplaced"));
    m_unplacedLayout = new FlowLayout(m_unplaced, 6, 6);
    m_unplacedLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_unplaced);
    //: Under the plan of the cover. In Polish: Miejsce zmienia pole „Miejsce na okładce” we
    //: Właściwościach albo na karcie materiału. Ilustracje referencyjne i notatki nie trafiają
    //: na okładkę, więc nie ma ich na planie.
    m_note = new QLabel(tr("The field \"Place on the cover\" in the Properties or on the card "
                           "of a material changes the place. Reference pictures and notes do "
                           "not go on the cover, so the plan does not show them."),
                        this);
    m_note->setWordWrap(true);
    m_note->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_note);

    connect(m_flaps, &QCheckBox::toggled, this,
            [this](bool on) { changePart(&CoverParts::flaps, on); });
    connect(m_jacket, &QCheckBox::toggled, this,
            [this](bool on) { changePart(&CoverParts::jacket, on); });
    connect(m_endpapers, &QCheckBox::toggled, this,
            [this](bool on) { changePart(&CoverParts::endpapers, on); });
    connect(m_rejected, &QCheckBox::toggled, this, [this]() { rebuild(); });
    connect(m_tabs, &QTabBar::currentChanged, this, [this](int index) {
        if (m_updating || index < 0) {
            return;
        }
        m_sheetShown = m_tabs->tabData(index).toString();
        m_designPicked = false;
        rebuild();
    });
    // Ctrl+Page Down and Ctrl+Page Up: the next and the previous sheet, as the tabs of the
    // Settings
    for (const auto& [keys, step] :
         {std::pair{QKeyCombination(Qt::ControlModifier, Qt::Key_PageDown), 1},
          std::pair{QKeyCombination(Qt::ControlModifier, Qt::Key_PageUp), -1}}) {
        auto* shortcut = new QShortcut(QKeySequence(keys), this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, step = step]() {
            const QStringList shown = sheets();
            const qsizetype at = std::max<qsizetype>(0, shown.indexOf(m_sheetShown));
            const qsizetype count = shown.size();
            showSheet(shown.at((at + step + count) % count));
            m_tabs->setFocus(Qt::ShortcutFocusReason);
        });
    }
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            [this]() { rebuild(); });
    // The colors of a new pattern and of white paper come from the theme
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            [this]() { updateEndpapers(); });
    rebuild();
}

QWidget* CoverPlan::newEndpaperRow(const QString& side) {
    const bool front = side == QLatin1String(FRONT_ENDPAPER);
    EndpaperControls& controls = controlsOf(side);
    auto* row = new QWidget(m_endpaperSheet);
    row->setObjectName(QStringLiteral("coverPlanEndpaper_") + side);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(12);

    // The spread: its name and where it is, the two pages
    auto* spreadColumn = new QWidget(row);
    auto* spreadLayout = new QVBoxLayout(spreadColumn);
    spreadLayout->setContentsMargins(0, 0, 0, 0);
    spreadLayout->setSpacing(4);
    auto* heading = new QLabel(spreadColumn);
    heading->setWordWrap(true);
    const QString name = front
                             //: The endpaper between the cover and the first page. In Polish:
                             //: Wyklejka przednia
                             ? tr("Front endpaper")
                             //: The endpaper between the last page and the cover. In Polish:
                             //: Wyklejka tylna
                             : tr("Back endpaper");
    const QString where = front
                              //: Where the front endpaper is. In Polish: między okładką a
                              //: pierwszą stroną
                              ? tr("between the cover and the first page")
                              //: Where the back endpaper is. In Polish: między ostatnią stroną a
                              //: okładką
                              : tr("between the last page and the cover");
    heading->setText(QStringLiteral("<b>%1</b> &nbsp;<small>%2</small>")
                         .arg(name.toHtmlEscaped(), where.toHtmlEscaped()));
    spreadLayout->addWidget(heading);
    controls.spread = new EndpaperSpread(spreadColumn);
    controls.spread->setObjectName(QStringLiteral("coverPlanSpread_") + side);
    controls.spread->setAccessibleName(name);
    controls.spread->installEventFilter(this);
    spreadLayout->addWidget(controls.spread);
    //: The page of an endpaper glued to the inside of the cover. In Polish: przyklejona do
    //: okładki
    const QString pasted = tr("pasted to the cover");
    //: The page of an endpaper that turns as a leaf. In Polish: wolna karta
    const QString free = tr("free leaf");
    auto* pages = new QWidget(spreadColumn);
    auto* pagesLayout = new QHBoxLayout(pages);
    pagesLayout->setContentsMargins(0, 0, 0, 0);
    for (const QString& page : front ? QStringList{pasted, free} : QStringList{free, pasted}) {
        auto* label = new QLabel(page, pages);
        label->setAlignment(Qt::AlignHCenter);
        label->setForegroundRole(QPalette::PlaceholderText);
        QFont small = label->font();
        small.setPointSizeF(small.pointSizeF() * 0.9);
        label->setFont(small);
        pagesLayout->addWidget(label, 1);
    }
    spreadLayout->addWidget(pages);
    spreadLayout->addStretch();
    rowLayout->addWidget(spreadColumn, 1);
    connect(controls.spread, &EndpaperSpread::clicked, this,
            [this, side]() { chooseEndpaper(side, false); });

    // Its settings beside it
    controls.settings = new QFrame(row);
    controls.settings->setObjectName(QStringLiteral("coverPlanEndpaperSettings_") + side);
    static_cast<QFrame*>(controls.settings)->setFrameShape(QFrame::StyledPanel);
    controls.settings->setBackgroundRole(QPalette::Window);
    controls.settings->setAutoFillBackground(true);
    auto* settingsLayout = new QVBoxLayout(controls.settings);
    settingsLayout->setContentsMargins(10, 8, 10, 8);
    settingsLayout->setSpacing(6);
    if (!front) {
        //: The back endpaper is filled as the front one. In Polish: Tak jak przednia
        controls.same = new QCheckBox(tr("Same as the front one"), controls.settings);
        controls.same->setObjectName(QStringLiteral("coverPlanSameAsFront"));
        settingsLayout->addWidget(controls.same);
        connect(controls.same, &QCheckBox::toggled, this, [this, side](bool on) {
            changeEndpaper(side, [on](Endpaper& endpaper) { endpaper.sameAsFront = on; });
        });
    }
    controls.form = new QFormLayout();
    controls.form->setContentsMargins(0, 0, 0, 0);
    controls.form->setSpacing(6);
    settingsLayout->addLayout(controls.form);
    settingsLayout->addStretch();

    controls.fill = new QComboBox(controls.settings);
    controls.fill->setObjectName(QStringLiteral("coverPlanFill_") + side);
    //: How an endpaper is filled. In Polish: Wypełnienie:
    controls.form->addRow(tr("Fill:"), controls.fill);

    auto* pictureRow = new QWidget(controls.settings);
    auto* pictureLayout = new QVBoxLayout(pictureRow);
    pictureLayout->setContentsMargins(0, 0, 0, 0);
    pictureLayout->setSpacing(4);
    controls.picture = new QComboBox(pictureRow);
    controls.picture->setObjectName(QStringLiteral("coverPlanPicture_") + side);
    controls.picture->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    controls.picture->setMinimumContentsLength(16);
    //: Adds a picture from the disk to the materials of the cover and puts it on the endpaper.
    //: In Polish: Obraz z dysku...
    controls.fromDisk = new QPushButton(tr("Picture from Disk..."), pictureRow);
    controls.fromDisk->setObjectName(QStringLiteral("coverPlanFromDisk_") + side);
    controls.fromDisk->setIcon(
        core::ArtProvider::getInstance().getIcon("common.image", core::IconContext::Button));
    pictureLayout->addWidget(controls.picture);
    pictureLayout->addWidget(controls.fromDisk, 0, Qt::AlignLeft);
    //: The picture an endpaper is filled with. In Polish: Obraz:
    controls.form->addRow(tr("Picture:"), pictureRow);

    auto* scaleRow = new QWidget(controls.settings);
    auto* scaleLayout = new QHBoxLayout(scaleRow);
    scaleLayout->setContentsMargins(0, 0, 0, 0);
    controls.scale = new QSpinBox(scaleRow);
    controls.scale->setObjectName(QStringLiteral("coverPlanScale_") + side);
    controls.scale->setRange(Endpaper::MIN_SCALE, Endpaper::MAX_SCALE);
    //: After the size of a picture repeated on an endpaper. In Polish: % szerokości strony
    auto* scaleUnit = new QLabel(tr("% of the page width"), scaleRow);
    scaleUnit->setForegroundRole(QPalette::PlaceholderText);
    scaleLayout->addWidget(controls.scale);
    scaleLayout->addWidget(scaleUnit, 1);
    //: The size of a picture repeated on an endpaper. In Polish: Wielkość:
    controls.form->addRow(tr("Size:"), scaleRow);

    controls.patterns = new QWidget(controls.settings);
    auto* patternsLayout = new FlowLayout(controls.patterns, 4, 4);
    patternsLayout->setContentsMargins(0, 0, 0, 0);
    controls.patternButtons = new QButtonGroup(controls.patterns);
    controls.patternButtons->setExclusive(true);
    for (const QString& pattern : Endpaper::patterns()) {
        auto* button = new QToolButton(controls.patterns);
        button->setObjectName(QStringLiteral("coverPlanPattern_%1_%2").arg(side, pattern));
        button->setCheckable(true);
        button->setProperty("pattern", pattern);
        button->setFixedSize(PATTERN_BUTTON, PATTERN_BUTTON);
        button->setIconSize(QSize(PATTERN_BUTTON - 6, PATTERN_BUTTON - 6));
        button->setToolTip(CoverNames::patternName(pattern));
        button->setAccessibleName(CoverNames::patternName(pattern));
        controls.patternButtons->addButton(button);
        patternsLayout->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, side, pattern]() {
            changeEndpaper(side, [&pattern](Endpaper& endpaper) { endpaper.pattern = pattern; });
        });
    }
    //: The pattern of an endpaper. In Polish: Wzór:
    controls.form->addRow(tr("Pattern:"), controls.patterns);

    const auto swatch = [this, &side](const char* id, const QString& text,
                                      QString Endpaper::*field, QWidget* parent,
                                      QColor (*shown)(const Endpaper&)) {
        auto* button = new QToolButton(parent);
        button->setObjectName(QStringLiteral("%1_%2").arg(QLatin1String(id), side));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(SWATCH_WIDTH, SWATCH_HEIGHT));
        button->setText(text);
        button->setAutoRaise(true);
        connect(button, &QToolButton::clicked, this, [this, side, field, shown]() {
            pickColor(side, field, shown(endpaperOf(side)));
        });
        return button;
    };
    controls.colors = new QWidget(controls.settings);
    auto* colorsLayout = new QHBoxLayout(controls.colors);
    colorsLayout->setContentsMargins(0, 0, 0, 0);
    colorsLayout->setSpacing(10);
    //: The color of the pattern of an endpaper, on its button. In Polish: wzór
    controls.patternColor = swatch("coverPlanPatternColor", tr("pattern"),
                                   &Endpaper::patternColor, controls.colors, &patternColorOf);
    //: The color under the pattern of an endpaper, on its button. In Polish: tło
    controls.paperColor = swatch("coverPlanPaperColor", tr("background"), &Endpaper::paperColor,
                                 controls.colors, &paperColorOf);
    colorsLayout->addWidget(controls.patternColor);
    colorsLayout->addWidget(controls.paperColor);
    colorsLayout->addStretch();
    //: The colors of the pattern of an endpaper. In Polish: Kolory:
    controls.form->addRow(tr("Colors:"), controls.colors);
    controls.color = swatch("coverPlanColor", QString(), &Endpaper::color, controls.settings,
                            &colorOf);
    //: The one color of an endpaper. In Polish: Kolor:
    controls.form->addRow(tr("Color:"), controls.color);

    connect(controls.fill, &QComboBox::activated, this, [this, side]() {
        const QString fill = controlsOf(side).fill->currentData().toString();
        changeEndpaper(side, [this, &fill](Endpaper& endpaper) {
            endpaper.fill = fill;
            // A new pattern and a new color take the theme's, kept with the book from then on
            if (fill == QLatin1String(Endpaper::PATTERN)) {
                if (endpaper.patternColor.isEmpty()) {
                    endpaper.patternColor = patternColorOf(endpaper).name();
                }
                if (endpaper.paperColor.isEmpty()) {
                    endpaper.paperColor = paperColorOf(endpaper).name();
                }
            } else if (fill == QLatin1String(Endpaper::COLOR) && endpaper.color.isEmpty()) {
                endpaper.color = colorOf(endpaper).name();
            } else if ((fill == QLatin1String(Endpaper::PICTURE) ||
                        fill == QLatin1String(Endpaper::TILE)) &&
                       endpaper.picture.isEmpty() && !m_pictures.isEmpty()) {
                endpaper.picture = m_pictures.constFirst().id;
            }
        });
    });
    connect(controls.picture, &QComboBox::activated, this, [this, side]() {
        const QString id = controlsOf(side).picture->currentData().toString();
        if (!id.isEmpty()) {
            changeEndpaper(side, [&id](Endpaper& endpaper) { endpaper.picture = id; });
        }
    });
    connect(controls.fromDisk, &QPushButton::clicked, this,
            [this, side]() { emit endpaperPictureRequested(side); });
    connect(controls.scale, &QSpinBox::valueChanged, this, [this, side](int value) {
        if (!m_updating) {
            changeEndpaper(side, [value](Endpaper& endpaper) { endpaper.scale = value; });
        }
    });
    return row;
}

CoverPlan::EndpaperControls& CoverPlan::controlsOf(const QString& side) {
    return m_endpaperControls.at(side == QLatin1String(BACK_ENDPAPER) ? 1 : 0);
}

const CoverPlan::EndpaperControls& CoverPlan::controlsOf(const QString& side) const {
    return m_endpaperControls.at(side == QLatin1String(BACK_ENDPAPER) ? 1 : 0);
}

core::Endpaper& CoverPlan::endpaperOf(const QString& side) {
    return side == QLatin1String(BACK_ENDPAPER) ? m_parts.back : m_parts.front;
}

EndpaperSpread* CoverPlan::spread(const QString& side) const {
    return controlsOf(side).spread;
}

QComboBox* CoverPlan::fillBox(const QString& side) const {
    return controlsOf(side).fill;
}

QComboBox* CoverPlan::pictureBox(const QString& side) const {
    return controlsOf(side).picture;
}

QSpinBox* CoverPlan::scaleBox(const QString& side) const {
    return controlsOf(side).scale;
}

QToolButton* CoverPlan::patternButton(const QString& side, const QString& pattern) const {
    for (QAbstractButton* button : controlsOf(side).patternButtons->buttons()) {
        if (button->property("pattern").toString() == pattern) {
            return static_cast<QToolButton*>(button);
        }
    }
    return nullptr;
}

QCheckBox* CoverPlan::sameAsFrontBox() const {
    return controlsOf(QString::fromLatin1(BACK_ENDPAPER)).same;
}

QColor CoverPlan::patternColorOf(const core::Endpaper& endpaper) {
    return colorOrTheme(endpaper.patternColor, PATTERN_COLOR_KEY,
                        QApplication::palette().color(QPalette::Text));
}

QColor CoverPlan::paperColorOf(const core::Endpaper& endpaper) {
    return colorOrTheme(endpaper.paperColor, PAPER_COLOR_KEY,
                        QApplication::palette().color(QPalette::Base));
}

QColor CoverPlan::colorOf(const core::Endpaper& endpaper) {
    return colorOrTheme(endpaper.color, PAPER_COLOR_KEY,
                        QApplication::palette().color(QPalette::Base));
}

QColor CoverPlan::blankColor() {
    return core::ThemeManager::getInstance().editorColor(
        BLANK_COLOR_KEY, QApplication::palette().color(QPalette::Base));
}

void CoverPlan::setItems(const QList<CoverPlanItem>& items) {
    m_items = items;
    rebuild();
}

void CoverPlan::setParts(const core::CoverParts& parts) {
    m_parts = parts;
    {
        const QSignalBlocker flaps(m_flaps);
        const QSignalBlocker jacket(m_jacket);
        const QSignalBlocker endpapers(m_endpapers);
        m_flaps->setChecked(parts.flaps);
        m_jacket->setChecked(parts.jacket);
        m_endpapers->setChecked(parts.endpapers);
    }
    rebuild();
}

void CoverPlan::setPictures(const QList<CoverPlanPicture>& pictures) {
    if (pictures == m_pictures) {
        return;
    }
    m_pictures = pictures;
    m_pictureCache.clear();
    updateEndpapers();
}

void CoverPlan::setEditable(bool editable) {
    m_editable = editable;
    for (QCheckBox* box : {m_flaps, m_jacket, m_endpapers}) {
        box->setEnabled(editable);
    }
    updateEndpapers();
}

QStringList CoverPlan::sheets() const {
    QStringList shown{QString::fromLatin1(COVER_SHEET)};
    if (m_parts.jacket) {
        shown << QString::fromLatin1(JACKET_SHEET);
    }
    if (m_parts.endpapers) {
        shown << QString::fromLatin1(ENDPAPERS_SHEET);
    }
    return shown;
}

bool CoverPlan::showSheet(const QString& sheet) {
    if (!sheets().contains(sheet)) {
        return false;
    }
    if (m_sheetShown != sheet) {
        m_sheetShown = sheet;
        m_designPicked = false;
        rebuild();
    }
    return true;
}

void CoverPlan::changePart(bool core::CoverParts::*part, bool on) {
    if (m_parts.*part == on) {
        return;
    }
    m_parts.*part = on;
    // A part turned on shows its sheet
    if (on && part == &CoverParts::jacket) {
        m_sheetShown = QString::fromLatin1(JACKET_SHEET);
    } else if (on && part == &CoverParts::endpapers) {
        m_sheetShown = QString::fromLatin1(ENDPAPERS_SHEET);
    }
    m_designPicked = false;
    rebuild();
    if (part == &CoverParts::endpapers) {
        emit message(on
                         //: The status bar after the endpapers are turned on above the plan of
                         //: the cover. In Polish: Wyklejki włączone: arkusz „Wyklejki”
                         ? tr("Endpapers on: the sheet \"Endpapers\"")
                         //: The status bar after the endpapers are turned off. In Polish:
                         //: Wyklejki wyłączone; ich wypełnienie zostaje zapisane.
                         : tr("Endpapers off; their fills are kept."));
    } else {
        emit message(on
                         //: The status bar after a part of the cover is turned on above its
                         //: plan. In Polish: Część włączona; jej materiały wróciły na swoje
                         //: miejsca.
                         ? tr("The part is on; its materials are back in their places.")
                         //: The status bar after a part of the cover is turned off. In
                         //: Polish: Część wyłączona; jej materiały czekają pod planem.
                         : tr("The part is off; its materials wait under the plan."));
    }
    emit partsChanged(m_parts);
}

void CoverPlan::changeEndpaper(const QString& side,
                               const std::function<void(core::Endpaper&)>& change) {
    if (m_updating) {
        return;
    }
    Endpaper& endpaper = endpaperOf(side);
    const Endpaper before = endpaper;
    change(endpaper);
    if (endpaper == before) {
        return;
    }
    updateEndpapers();
    emit partsChanged(m_parts);
}

void CoverPlan::pickColor(const QString& side, QString core::Endpaper::*field,
                          const QColor& shown) {
    const QString title = side == QLatin1String(FRONT_ENDPAPER)
                              //: The heading of the window of a color of the front endpaper.
                              //: In Polish: Wyklejka przednia
                              ? tr("Front endpaper")
                              //: In Polish: Wyklejka tylna
                              : tr("Back endpaper");
    const std::optional<QColor> color = dialogs::ColorDialog::getColor(shown, this, title);
    if (!color) {
        return;
    }
    const QString name = color->name();
    changeEndpaper(side, [field, &name](Endpaper& endpaper) { endpaper.*field = name; });
}

void CoverPlan::setCurrent(const QString& id) {
    m_current = id;
    for (auto it = m_cards.cbegin(); it != m_cards.cend(); ++it) {
        it.value()->setCurrent(it.key() == id);
    }
    for (QAbstractButton* button : m_designButtons->buttons()) {
        if (button->property("materialId").toString() == id) {
            const QSignalBlocker blocker(m_designButtons);
            button->setChecked(true);
        }
    }
    if (!id.isEmpty() && !m_currentEndpaper.isEmpty()) {
        m_currentEndpaper.clear();
        for (const EndpaperControls& controls : m_endpaperControls) {
            controls.spread->setCurrent(false);
        }
    }
}

void CoverPlan::focusCurrent() {
    if (m_sheetShown == QLatin1String(ENDPAPERS_SHEET)) {
        chooseEndpaper(m_currentEndpaper.isEmpty() ? QString::fromLatin1(FRONT_ENDPAPER)
                                                   : m_currentEndpaper,
                       true);
        return;
    }
    if (CoverPlanCard* card = m_cards.value(m_current)) {
        card->setFocus(Qt::OtherFocusReason);
        return;
    }
    for (QAbstractButton* button : m_designButtons->buttons()) {
        if (button->property("materialId").toString() == m_current) {
            button->setFocus(Qt::OtherFocusReason);
            return;
        }
    }
    for (const QList<CoverPlanCard*>& column : std::as_const(m_columns)) {
        if (!column.isEmpty()) {
            choose(column.constFirst()->id(), true);
            return;
        }
    }
    if (!m_designButtons->buttons().isEmpty()) {
        m_designButtons->buttons().constFirst()->setFocus(Qt::OtherFocusReason);
    } else {
        m_tabs->setFocus(Qt::OtherFocusReason);
    }
}

void CoverPlan::choose(const QString& id, bool focus) {
    setCurrent(id);
    if (focus) {
        if (CoverPlanCard* card = m_cards.value(id)) {
            card->setFocus(Qt::OtherFocusReason);
        }
    }
    emit currentChanged(id);
}

void CoverPlan::chooseEndpaper(const QString& side, bool focus) {
    m_currentEndpaper = side;
    for (const char* each : {FRONT_ENDPAPER, BACK_ENDPAPER}) {
        controlsOf(QString::fromLatin1(each))
            .spread->setCurrent(side == QLatin1String(each));
    }
    if (focus) {
        controlsOf(side).spread->setFocus(Qt::OtherFocusReason);
    }
    emit endpaperChosen(side);
}

void CoverPlan::rebuild() {
    const bool focused = isAncestorOf(QApplication::focusWidget()) &&
                         qobject_cast<CoverPlanCard*>(QApplication::focusWidget()) != nullptr;
    if (!sheets().contains(m_sheetShown)) {
        m_sheetShown = QString::fromLatin1(COVER_SHEET);
    }
    rebuildTabs();
    const bool endpapers = m_sheetShown == QLatin1String(ENDPAPERS_SHEET);
    m_designs->setVisible(!endpapers);
    m_sheet->setVisible(!endpapers);
    m_endpaperSheet->setVisible(endpapers);
    m_note->setVisible(!endpapers);
    rebuildSheet();
    updateEndpapers();
    setCurrent(m_current);
    if (focused && !endpapers) {
        focusCurrent();
    }
}

void CoverPlan::rebuildTabs() {
    const bool rejected = m_rejected->isChecked();
    const auto countOn = [this, rejected](const std::array<const char*, 5>& columns,
                                          bool flaps) {
        return std::count_if(m_items.cbegin(), m_items.cend(),
                             [&columns, flaps, rejected](const CoverPlanItem& item) {
                                 return CoverMaterial::goesOnCover(item.part) &&
                                        item.part != QLatin1String(CoverMaterial::DESIGN) &&
                                        (rejected || item.state !=
                                                         QLatin1String(CoverMaterial::REJECTED)) &&
                                        isColumn(columns, item.place) &&
                                        (flaps || !CoverMaterial::isCoverFlap(item.place));
                             });
    };
    m_updating = true;
    while (m_tabs->count() > 0) {
        m_tabs->removeTab(0);
    }
    core::ArtProvider& art = core::ArtProvider::getInstance();
    for (const QString& sheet : sheets()) {
        QString name;
        qsizetype count = 2;
        QString icon = QStringLiteral("cover.sheet");
        if (sheet == QLatin1String(COVER_SHEET)) {
            //: The tab of the sheet of the cover itself on its plan. In Polish: Okładka
            name = tr("Cover");
            count = countOn(COVER_COLUMNS, m_parts.flaps);
        } else if (sheet == QLatin1String(JACKET_SHEET)) {
            //: The tab of the sheet of the jacket on the plan of the cover. In Polish: Obwoluta
            name = tr("Jacket");
            count = countOn(JACKET_COLUMNS, true);
        } else {
            //: The tab of the sheet of the endpapers on the plan of the cover. In Polish:
            //: Wyklejki
            name = tr("Endpapers");
            icon = QStringLiteral("cover.endpapers");
        }
        //: A tab of the plan of the cover: %1 its sheet, %2 the materials on it (of the
        //: endpapers: the two of them). In Polish: %1 · %2
        const int index = m_tabs->addTab(art.getIcon(icon, core::IconContext::Menu),
                                         tr("%1 · %2").arg(name, QLocale().toString(count)));
        m_tabs->setTabData(index, sheet);
        if (sheet == m_sheetShown) {
            m_tabs->setCurrentIndex(index);
        }
    }
    m_updating = false;
}

void CoverPlan::rebuildDesigns(const QList<const CoverPlanItem*>& designs) {
    for (QAbstractButton* button : m_designButtons->buttons()) {
        m_designButtons->removeButton(button);
        button->deleteLater();
    }
    const bool jacket = m_sheetShown == QLatin1String(JACKET_SHEET);
    m_designsLabel->setText(jacket
                                //: Before the designs of the jacket above its sheet. In Polish:
                                //: Projekty obwoluty:
                                ? tr("Jacket designs:")
                                //: Before the designs of the cover above its sheet. In Polish:
                                //: Projekty okładki:
                                : tr("Cover designs:"));
    m_noDesign->setVisible(designs.isEmpty());
    const auto known = [&designs](const QString& id) {
        return std::any_of(designs.cbegin(), designs.cend(),
                           [&id](const CoverPlanItem* design) { return design->id == id; });
    };
    if (!m_designPicked || !known(m_design)) {
        m_designPicked = false;
        m_design.clear();
        for (const CoverPlanItem* design : designs) {
            if (design->state == QLatin1String(CoverMaterial::CHOSEN) && m_design.isEmpty()) {
                m_design = design->id;
            }
        }
        if (m_design.isEmpty() && !designs.isEmpty()) {
            m_design = designs.constFirst()->id;
        }
    }
    QImage picture;
    for (const CoverPlanItem* design : designs) {
        auto* button = new QToolButton(m_designsLayout->parentWidget());
        button->setObjectName(QStringLiteral("coverPlanDesign"));
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setProperty("materialId", design->id);
        const bool chosen = design->state == QLatin1String(CoverMaterial::CHOSEN);
        button->setText(chosen ? QStringLiteral("★ ") + design->title : design->title);
        if (!design->picture.isEmpty()) {
            const QImage thumbnail =
                readPicture(design->picture, design->rotation,
                            QSize(DESIGN_ICON, DESIGN_ICON) * devicePixelRatioF());
            QPixmap pixmap = QPixmap::fromImage(thumbnail);
            pixmap.setDevicePixelRatio(devicePixelRatioF());
            button->setIcon(QIcon(pixmap));
            button->setIconSize(QSize(DESIGN_ICON, DESIGN_ICON));
        } else {
            button->setIcon(core::ArtProvider::getInstance().getIcon(design->iconId,
                                                                     core::IconContext::Menu));
        }
        //: The button of a design above the plan of the cover. In Polish: Pokaż ten projekt
        //: za planem okładki (Enter otwiera jego kartę)
        button->setToolTip(tr("Show this design behind the plan of the cover (Enter opens its "
                              "card)"));
        button->installEventFilter(this);
        m_designButtons->addButton(button);
        m_designsLayout->addWidget(button);
        if (design->id == m_design) {
            button->setChecked(true);
            if (!design->picture.isEmpty()) {
                picture = readPicture(design->picture, design->rotation, QSize(1200, 1200));
            }
        }
        const QString id = design->id;
        connect(button, &QToolButton::clicked, this, [this, id]() {
            m_design = id;
            m_designPicked = true;
            rebuild();
            choose(id, false);
        });
    }
    static_cast<CoverSheet*>(m_sheet)->setPicture(picture);
}

void CoverPlan::rebuildSheet() {
    const bool words = CoverMaterial::countsWords();
    const bool rejected = m_rejected->isChecked();
    const bool jacket = m_sheetShown == QLatin1String(JACKET_SHEET);
    const bool endpapers = m_sheetShown == QLatin1String(ENDPAPERS_SHEET);
    const auto visible = [rejected](const CoverPlanItem& item) {
        return CoverMaterial::goesOnCover(item.part) &&
               (rejected || item.state != QLatin1String(CoverMaterial::REJECTED));
    };
    const auto isDesign = [](const CoverPlanItem& item) {
        return item.part == QLatin1String(CoverMaterial::DESIGN);
    };

    // The designs of the sheet: of the jacket on the jacket, any other above the cover
    QList<const CoverPlanItem*> designs;
    for (const CoverPlanItem& item : std::as_const(m_items)) {
        if (visible(item) && isDesign(item) &&
            CoverMaterial::isJacketPlace(item.place) == jacket &&
            CoverMaterial::isOn(item.place, m_parts)) {
            designs << &item;
        }
    }
    rebuildDesigns(endpapers ? QList<const CoverPlanItem*>{} : designs);

    // The columns, each with its cards; the cards without a place under the plan
    m_cards.clear();
    m_columnOf.clear();
    m_columns.clear();
    while (QLayoutItem* item = m_sheetLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    while (QLayoutItem* item = m_unplacedLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    auto* sheet = static_cast<CoverSheet*>(m_sheet);
    sheet->setFront(nullptr);
    sheet->setCloth(!jacket && m_parts.jacket);
    const auto connectCard = [this](CoverPlanCard* card) {
        card->installEventFilter(this);
        connect(card, &CoverPlanCard::clicked, this,
                [this](const QString& id) { choose(id, false); });
        connect(card, &CoverPlanCard::doubleClicked, this, &CoverPlan::activated);
        connect(card, &CoverPlanCard::menuRequested, this, &CoverPlan::menuRequested);
    };
    if (!endpapers) {
        for (const char* placeName : jacket ? JACKET_COLUMNS : COVER_COLUMNS) {
            const QString place = QString::fromLatin1(placeName);
            if (!CoverMaterial::isOn(place, m_parts)) {
                continue;
            }
            QList<const CoverPlanItem*> here;
            int length = 0;
            bool bio = false;
            for (const CoverPlanItem& item : std::as_const(m_items)) {
                if (visible(item) && !isDesign(item) && item.place == place) {
                    here << &item;
                    length += CoverMaterial::lengthOf(item.text, words);
                    bio = bio || item.part == QLatin1String(CoverMaterial::BIO);
                }
            }
            const bool spine = isSpine(place);
            QWidget* column = newColumn(
                place, static_cast<int>(here.size()),
                length > 0 ? CoverNames::lengthText(length, words) : QString(), m_sheet);
            auto* columnLayout = static_cast<QVBoxLayout*>(column->layout());
            // The back flap of the jacket waits for a note about the author
            if (!bio && place == QLatin1String(CoverMaterial::JACKET_BACK_FLAP)) {
                //: On the back flap of the jacket without a note about the author. In
                //: Polish: Miejsce na notkę biograficzną
                auto* ghost = new QLabel(tr("A place for the author bio"), column);
                ghost->setObjectName(QStringLiteral("coverPlanBioPlace"));
                ghost->setWordWrap(true);
                ghost->setFrameShape(QFrame::StyledPanel);
                ghost->setMargin(6);
                ghost->setForegroundRole(QPalette::PlaceholderText);
                columnLayout->addWidget(ghost);
            }
            QList<CoverPlanCard*> cards;
            for (const CoverPlanItem* item : here) {
                auto* card = new CoverPlanCard(*item, spine, column);
                columnLayout->addWidget(card);
                connectCard(card);
                m_cards.insert(item->id, card);
                m_columnOf.insert(item->id, place);
                cards << card;
            }
            columnLayout->addStretch();
            if (spine) {
                column->setFixedWidth(fontMetrics().averageCharWidth() * 12);
            }
            m_sheetLayout->addWidget(column, spine ? 0 : (isFlap(place) ? 2 : 3));
            if (isFront(place)) {
                sheet->setFront(column);
            }
            m_columns << cards;
        }
    }

    // Under the sheet: the materials without a place, or of a part turned off, which say so
    QList<CoverPlanCard*> unplaced;
    if (!endpapers) {
        for (const CoverPlanItem& item : std::as_const(m_items)) {
            if (!visible(item)) {
                continue;
            }
            const bool known = isColumn(COVER_COLUMNS, item.place) ||
                               isColumn(JACKET_COLUMNS, item.place) ||
                               CoverMaterial::isWhole(item.place);
            const bool off = known && !CoverMaterial::isOn(item.place, m_parts);
            const bool waits = isDesign(item) ? off
                                              : (!isColumn(COVER_COLUMNS, item.place) &&
                                                 !isColumn(JACKET_COLUMNS, item.place)) ||
                                                    off;
            if (!waits) {
                continue;
            }
            auto* card = new CoverPlanCard(item, false, m_unplaced,
                                           off ? CoverNames::partOffText(item.place, m_parts)
                                               : QString());
            card->setMaximumWidth(fontMetrics().averageCharWidth() * 40);
            m_unplacedLayout->addWidget(card);
            connectCard(card);
            m_cards.insert(item.id, card);
            unplaced << card;
        }
    }
    m_columns << unplaced;
    //: The materials of the cover without a place, under its plan: %1 their number. In
    //: Polish: Bez miejsca na okładce · %1
    const QString unplacedText =
        tr("Without a place on the cover · %1").arg(QLocale().toString(unplaced.size()));
    m_unplacedLabel->setText(QStringLiteral("<b>%1</b>").arg(unplacedText.toHtmlEscaped()));
    m_unplacedLabel->setVisible(!unplaced.isEmpty());
    m_unplaced->setVisible(!unplaced.isEmpty());
}

QImage CoverPlan::pictureOf(const QString& id, const QSize& size) const {
    if (const auto cached = m_pictureCache.constFind(id); cached != m_pictureCache.cend()) {
        return cached.value();
    }
    for (const CoverPlanPicture& picture : m_pictures) {
        if (picture.id == id) {
            const QImage image = readPicture(picture.picture, picture.rotation, size);
            m_pictureCache.insert(id, image);
            return image;
        }
    }
    return {};
}

QString CoverPlan::pictureTitleOf(const QString& id) const {
    for (const CoverPlanPicture& picture : m_pictures) {
        if (picture.id == id) {
            return picture.title;
        }
    }
    return {};
}

void CoverPlan::updateEndpapers() {
    if (m_updating) {
        return;
    }
    m_updating = true;
    for (const char* each : {FRONT_ENDPAPER, BACK_ENDPAPER}) {
        const QString side = QString::fromLatin1(each);
        const bool back = side == QLatin1String(BACK_ENDPAPER);
        const Endpaper& shown = back ? m_parts.backShown() : m_parts.front;
        controlsOf(side).spread->setEndpaper(
            shown, shown.picture.isEmpty() ? QImage() : pictureOf(shown.picture, QSize(1600, 1600)));
        showEndpaperSettings(side);
    }
    m_updating = false;
}

void CoverPlan::showEndpaperSettings(const QString& side) {
    EndpaperControls& controls = controlsOf(side);
    const Endpaper& endpaper = endpaperOf(side);
    const bool same = side == QLatin1String(BACK_ENDPAPER) && endpaper.sameAsFront;
    if (controls.same != nullptr) {
        const QSignalBlocker blocker(controls.same);
        controls.same->setChecked(endpaper.sameAsFront);
        controls.same->setEnabled(m_editable);
    }
    // Of the back endpaper as the front one: its own settings wait, greyed out
    for (QWidget* field : {static_cast<QWidget*>(controls.fill),
                           static_cast<QWidget*>(controls.picture),
                           static_cast<QWidget*>(controls.fromDisk),
                           static_cast<QWidget*>(controls.scale), controls.patterns,
                           controls.colors, static_cast<QWidget*>(controls.color)}) {
        field->setEnabled(m_editable && !same);
    }

    const QSignalBlocker fillBlocker(controls.fill);
    controls.fill->clear();
    for (const QString& fill : Endpaper::fills()) {
        controls.fill->addItem(CoverNames::fillName(fill), fill);
    }
    if (controls.fill->findData(endpaper.fill) < 0) {
        // A fill a later version adds is kept as it is
        controls.fill->addItem(CoverNames::fillName(endpaper.fill), endpaper.fill);
    }
    controls.fill->setCurrentIndex(controls.fill->findData(endpaper.fill));

    // The pictures of the book in the groups of their lists
    const QSignalBlocker pictureBlocker(controls.picture);
    controls.picture->clear();
    auto* model = qobject_cast<QStandardItemModel*>(controls.picture->model());
    if (endpaper.picture.isEmpty() || pictureTitleOf(endpaper.picture).isEmpty()) {
        controls.picture->addItem(
            endpaper.picture.isEmpty()
                //: The box of the picture of an endpaper before a picture is chosen. In
                //: Polish: Wybierz obraz
                ? tr("Choose a picture")
                //: The box of the picture of an endpaper whose material is gone. In Polish:
                //: Obraz usunięty z materiałów
                : tr("Picture removed from the materials"),
            endpaper.picture);
    }
    QString group;
    for (const CoverPlanPicture& picture : std::as_const(m_pictures)) {
        if (picture.group != group) {
            group = picture.group;
            controls.picture->addItem(group);
            if (model != nullptr) {
                QStandardItem* heading = model->item(model->rowCount() - 1);
                heading->setFlags(Qt::NoItemFlags);
                QFont bold = controls.picture->font();
                bold.setBold(true);
                heading->setFont(bold);
            }
        }
        controls.picture->addItem(QStringLiteral("    ") + picture.title, picture.id);
    }
    controls.picture->setCurrentIndex(controls.picture->findData(endpaper.picture));

    controls.scale->setValue(endpaper.scale);
    for (QAbstractButton* button : controls.patternButtons->buttons()) {
        const QString pattern = button->property("pattern").toString();
        button->setChecked(pattern == endpaper.shownPattern());
        QPixmap pixmap(button->iconSize());
        QPainter painter(&pixmap);
        fillPattern(painter, pixmap.rect(), pattern, patternColorOf(endpaper),
                    paperColorOf(endpaper), PATTERN_BUTTON_CELL);
        painter.end();
        button->setIcon(QIcon(pixmap));
    }
    const QColor border = palette().color(QPalette::Mid);
    controls.patternColor->setIcon(swatchIcon(patternColorOf(endpaper), border));
    controls.paperColor->setIcon(swatchIcon(paperColorOf(endpaper), border));
    controls.color->setIcon(swatchIcon(colorOf(endpaper), border));
    //: The accessible name of the button of the one color of an endpaper. In Polish: Kolor
    //: wyklejki
    controls.color->setAccessibleName(tr("Color of the endpaper"));

    const QString& fill = endpaper.fill;
    const bool picture =
        fill == QLatin1String(Endpaper::PICTURE) || fill == QLatin1String(Endpaper::TILE);
    controls.form->setRowVisible(controls.picture->parentWidget(), picture);
    controls.form->setRowVisible(controls.scale->parentWidget(),
                                 fill == QLatin1String(Endpaper::TILE));
    controls.form->setRowVisible(controls.patterns, fill == QLatin1String(Endpaper::PATTERN));
    controls.form->setRowVisible(controls.colors, fill == QLatin1String(Endpaper::PATTERN));
    controls.form->setRowVisible(controls.color, fill == QLatin1String(Endpaper::COLOR));

    // The spread says how it is filled
    const Endpaper& shown = side == QLatin1String(BACK_ENDPAPER) ? m_parts.backShown()
                                                                 : m_parts.front;
    controls.spread->setAccessibleDescription(
        CoverNames::fillSummary(shown, pictureTitleOf(shown.picture)));
}

CoverPlanCard* CoverPlan::neighbour(const CoverPlanCard* card, int key) const {
    int columnIndex = -1;
    int row = -1;
    for (int c = 0; c < m_columns.size() && columnIndex < 0; ++c) {
        const int found =
            static_cast<int>(m_columns.at(c).indexOf(const_cast<CoverPlanCard*>(card)));
        if (found >= 0) {
            columnIndex = c;
            row = found;
        }
    }
    if (columnIndex < 0) {
        return nullptr;
    }
    const QList<CoverPlanCard*>& column = m_columns.at(columnIndex);
    switch (key) {
    case Qt::Key_Up:
        return row > 0 ? column.at(row - 1) : nullptr;
    case Qt::Key_Down:
        return row + 1 < column.size() ? column.at(row + 1) : nullptr;
    case Qt::Key_Left:
    case Qt::Key_Right: {
        const int step = key == Qt::Key_Left ? -1 : 1;
        for (int c = columnIndex + step; c >= 0 && c < m_columns.size(); c += step) {
            const QList<CoverPlanCard*>& next = m_columns.at(c);
            if (!next.isEmpty()) {
                return next.at(std::min(row, static_cast<int>(next.size()) - 1));
            }
        }
        return nullptr;
    }
    default:
        return nullptr;
    }
}

bool CoverPlan::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }
    auto* key = static_cast<QKeyEvent*>(event);
    const bool plain = key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::KeypadModifier;
    const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
    const bool menu = key->key() == Qt::Key_Menu ||
                      (key->key() == Qt::Key_F10 && key->modifiers() == Qt::ShiftModifier);
    const bool arrow = key->key() == Qt::Key_Up || key->key() == Qt::Key_Down ||
                       key->key() == Qt::Key_Left || key->key() == Qt::Key_Right;
    if (auto* card = qobject_cast<CoverPlanCard*>(watched)) {
        if (enter && plain) {
            emit activated(card->id());
            return true;
        }
        if (menu) {
            emit menuRequested(card->id(), card->mapToGlobal(card->rect().center()));
            return true;
        }
        if (plain) {
            if (CoverPlanCard* next = neighbour(card, key->key())) {
                choose(next->id(), true);
                return true;
            }
            if (arrow) {
                return true;
            }
        }
    } else if (auto* spread = qobject_cast<EndpaperSpread*>(watched)) {
        // The arrows go between the two spreads, Enter to the fill of the one with the keys
        const QString side = spread == controlsOf(QString::fromLatin1(FRONT_ENDPAPER)).spread
                                 ? QString::fromLatin1(FRONT_ENDPAPER)
                                 : QString::fromLatin1(BACK_ENDPAPER);
        if (plain && arrow) {
            const bool forward = key->key() == Qt::Key_Down || key->key() == Qt::Key_Right;
            chooseEndpaper(QString::fromLatin1(forward ? BACK_ENDPAPER : FRONT_ENDPAPER), true);
            return true;
        }
        if (plain && enter) {
            chooseEndpaper(side, false);
            QWidget* field = controlsOf(side).fill;
            if (!field->isEnabled() && controlsOf(side).same != nullptr) {
                field = controlsOf(side).same;
            }
            field->setFocus(Qt::OtherFocusReason);
            return true;
        }
    } else if (auto* button = qobject_cast<QToolButton*>(watched)) {
        const QString id = button->property("materialId").toString();
        if (enter && plain && !id.isEmpty()) {
            emit activated(id);
            return true;
        }
        if (menu && !id.isEmpty()) {
            choose(id, false);
            emit menuRequested(id, button->mapToGlobal(button->rect().center()));
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CoverPlan::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
        m_sheet->update();
        updateEndpapers();
    }
}

} // namespace kalahari::gui
