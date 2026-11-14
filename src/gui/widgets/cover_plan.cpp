// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cover_plan.cpp
/// @brief The plan of the cover: the cover unfolded with the materials in their places

#include "kalahari/gui/widgets/cover_plan.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cstdlib>

namespace kalahari {
namespace gui {

namespace {

using core::CoverMaterial;

/// Lines of the text of a card at most; the rest is cut
constexpr int CARD_LINES = 6;

/// Size of the picture of a design on its button
constexpr int DESIGN_ICON = 24;

/// The columns of the plan from the left: the back flap, the back, the spine, the front and
/// the front flap
const std::array<const char*, 5> COLUMNS{CoverMaterial::BACK_FLAP, CoverMaterial::BACK,
                                         CoverMaterial::SPINE, CoverMaterial::FRONT,
                                         CoverMaterial::FRONT_FLAP};

/// The cover unfolded: the columns on a background, the design behind the front
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

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        const QPalette& colors = palette();
        painter.fillRect(rect(), colors.color(QPalette::AlternateBase));
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
            painter.setOpacity(0.35);
            painter.drawImage(target, m_picture);
            painter.restore();
        }
        // The folds between the columns
        QPen fold(colors.color(QPalette::Mid));
        fold.setStyle(Qt::DashLine);
        painter.setPen(fold);
        const QList<QWidget*> columns =
            findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
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
};

/// The column of @p place on the plan: its name above its cards
QWidget* newColumn(const QString& place, int count, const QString& length, QWidget* parent) {
    auto* column = new QWidget(parent);
    column->setObjectName(QStringLiteral("coverPlanColumn_") + place);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    auto* heading = new QLabel(column);
    heading->setObjectName(QStringLiteral("coverPlanHeading"));
    heading->setWordWrap(true);
    QString text =
        QStringLiteral("<b>%1</b>").arg(CoverNames::placeName(place).toUpper().toHtmlEscaped());
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

CoverPlanCard::CoverPlanCard(const CoverPlanItem& item, bool narrow, QWidget* parent)
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
    setToolTip(shown);
    setAccessibleName(QStringLiteral("%1: %2 (%3)").arg(CoverNames::partName(item.part), shown,
                                                       CoverNames::stateName(item.state)));
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
// The plan
// =============================================================================

CoverPlan::CoverPlan(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("coverPlan"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // What the plan shows, and the designs to put behind it
    auto* options = new QWidget(this);
    auto* optionsLayout = new FlowLayout(options, 12, 4);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    //: Shows the flaps of the cover on its plan. In Polish: Skrzydełka
    m_flaps = new QCheckBox(tr("Flaps"), options);
    m_flaps->setObjectName(QStringLiteral("coverPlanFlaps"));
    m_flaps->setChecked(true);
    //: Shows the materials rejected on the plan of the cover. In Polish: Pokaż odrzucone
    m_rejected = new QCheckBox(tr("Show rejected"), options);
    m_rejected->setObjectName(QStringLiteral("coverPlanRejected"));
    //: The designs of the cover above its plan, one of them behind it. In Polish: Projekty:
    m_designsLabel = new QLabel(tr("Designs:"), options);
    auto* designs = new QWidget(options);
    m_designsLayout = new QHBoxLayout(designs);
    m_designsLayout->setContentsMargins(0, 0, 0, 0);
    m_designsLayout->setSpacing(4);
    m_designButtons = new QButtonGroup(this);
    m_designButtons->setExclusive(true);
    optionsLayout->addWidget(m_flaps);
    optionsLayout->addWidget(m_rejected);
    optionsLayout->addWidget(m_designsLabel);
    optionsLayout->addWidget(designs);
    layout->addWidget(options);

    auto* sheet = new CoverSheet(this);
    m_sheet = sheet;
    m_sheet->setObjectName(QStringLiteral("coverPlanSheet"));
    m_sheet->setAutoFillBackground(false);
    m_sheetLayout = new QHBoxLayout(m_sheet);
    m_sheetLayout->setContentsMargins(0, 0, 0, 0);
    m_sheetLayout->setSpacing(0);
    layout->addWidget(m_sheet, 1);

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
    auto* note = new QLabel(tr("The field \"Place on the cover\" in the Properties or on the "
                               "card of a material changes the place. Reference pictures and "
                               "notes do not go on the cover, so the plan does not show them."),
                            this);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(note);

    connect(m_flaps, &QCheckBox::toggled, this, [this]() { rebuild(); });
    connect(m_rejected, &QCheckBox::toggled, this, [this]() { rebuild(); });
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            [this]() { rebuild(); });
}

void CoverPlan::setItems(const QList<CoverPlanItem>& items) {
    m_items = items;
    rebuild();
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
}

void CoverPlan::focusCurrent() {
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
        m_flaps->setFocus(Qt::OtherFocusReason);
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

void CoverPlan::rebuild() {
    const bool focused = isAncestorOf(QApplication::focusWidget()) &&
                         qobject_cast<CoverPlanCard*>(QApplication::focusWidget()) != nullptr;
    const bool words = CoverMaterial::countsWords();

    // The designs: the one chosen behind the plan, unless the writer picked another
    for (QAbstractButton* button : m_designButtons->buttons()) {
        m_designButtons->removeButton(button);
        button->deleteLater();
    }
    QList<const CoverPlanItem*> designs;
    for (const CoverPlanItem& item : std::as_const(m_items)) {
        if (item.part == QLatin1String(CoverMaterial::DESIGN) &&
            (m_rejected->isChecked() || item.state != QLatin1String(CoverMaterial::REJECTED))) {
            designs << &item;
        }
    }
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
    m_designsLabel->setVisible(!designs.isEmpty());

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
    const auto shown = [this](const CoverPlanItem& item) {
        return CoverMaterial::goesOnCover(item.part) &&
               item.part != QLatin1String(CoverMaterial::DESIGN) &&
               (m_rejected->isChecked() || item.state != QLatin1String(CoverMaterial::REJECTED));
    };
    const auto connectCard = [this](CoverPlanCard* card) {
        card->installEventFilter(this);
        connect(card, &CoverPlanCard::clicked, this,
                [this](const QString& id) { choose(id, false); });
        connect(card, &CoverPlanCard::doubleClicked, this, &CoverPlan::activated);
        connect(card, &CoverPlanCard::menuRequested, this, &CoverPlan::menuRequested);
    };
    for (const char* placeName : COLUMNS) {
        const QString place = QString::fromLatin1(placeName);
        const bool flap = place == QLatin1String(CoverMaterial::FRONT_FLAP) ||
                          place == QLatin1String(CoverMaterial::BACK_FLAP);
        if (flap && !m_flaps->isChecked()) {
            continue;
        }
        QList<const CoverPlanItem*> here;
        int length = 0;
        for (const CoverPlanItem& item : std::as_const(m_items)) {
            if (shown(item) && item.place == place) {
                here << &item;
                length += CoverMaterial::lengthOf(item.text, words);
            }
        }
        const bool spine = place == QLatin1String(CoverMaterial::SPINE);
        QWidget* column = newColumn(place, static_cast<int>(here.size()),
                                    length > 0 ? CoverNames::lengthText(length, words) : QString(),
                                    m_sheet);
        auto* columnLayout = static_cast<QVBoxLayout*>(column->layout());
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
        m_sheetLayout->addWidget(column, spine ? 0 : (flap ? 2 : 3));
        if (place == QLatin1String(CoverMaterial::FRONT)) {
            sheet->setFront(column);
        }
        m_columns << cards;
    }
    QList<CoverPlanCard*> unplaced;
    for (const CoverPlanItem& item : std::as_const(m_items)) {
        const bool flap = item.place == QLatin1String(CoverMaterial::FRONT_FLAP) ||
                          item.place == QLatin1String(CoverMaterial::BACK_FLAP);
        const bool inColumn =
            std::any_of(COLUMNS.cbegin(), COLUMNS.cend(),
                        [&item](const char* place) {
                            return item.place == QLatin1String(place);
                        }) &&
            (!flap || m_flaps->isChecked());
        if (!shown(item) || inColumn) {
            continue;
        }
        // A material for a flap, while the flaps are hidden, waits under the plan too
        auto* card = new CoverPlanCard(item, false, m_unplaced);
        card->setMaximumWidth(fontMetrics().averageCharWidth() * 40);
        m_unplacedLayout->addWidget(card);
        connectCard(card);
        m_cards.insert(item.id, card);
        unplaced << card;
    }
    m_columns << unplaced;
    //: The materials of the cover without a place, under its plan: %1 their number. In
    //: Polish: Bez miejsca na okładce · %1
    const QString unplacedText =
        tr("Without a place on the cover · %1").arg(QLocale().toString(unplaced.size()));
    m_unplacedLabel->setText(QStringLiteral("<b>%1</b>").arg(unplacedText.toHtmlEscaped()));
    m_unplacedLabel->setVisible(!unplaced.isEmpty());
    m_unplaced->setVisible(!unplaced.isEmpty());
    sheet->setPicture(picture);

    setCurrent(m_current);
    if (focused) {
        focusCurrent();
    }
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
            if (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down ||
                key->key() == Qt::Key_Left || key->key() == Qt::Key_Right) {
                return true;
            }
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
    }
}

} // namespace gui
} // namespace kalahari
