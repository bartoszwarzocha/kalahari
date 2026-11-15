// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file palette_edit.cpp
/// @brief A palette of colors to change in the Settings

#include "kalahari/gui/widgets/palette_edit.h"

#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/dialogs/color_dialog.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/list_parts.h"

#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QPainter>
#include <QRectF>
#include <QToolButton>

#include <algorithm>
#include <optional>
#include <utility>

namespace kalahari::gui {

namespace {

/// Side of a swatch
constexpr int SWATCH_SIDE = 20;
/// Room between the swatches
constexpr int SWATCH_SPACING = 4;
/// The frame of a swatch is drawn inside its half pixel
constexpr qreal FRAME_INSET = 0.5;

/// What a key does on a palette
enum class PaletteKey {
    None,      ///< Nothing of the palette
    Add,       ///< Insert: a new color
    Remove,    ///< Delete: the color goes
    Change,    ///< Enter: the color window
    Previous,  ///< Left: the swatch before
    Next       ///< Right: the swatch after, or "+"
};

/// What @p key does, on a swatch when @p onSwatch, else on "+"
PaletteKey keyOf(const QKeyEvent* key, bool onSwatch) {
    if (list_parts::isListKey(key, Qt::Key_Insert)) {
        return PaletteKey::Add;
    }
    if (!onSwatch) {
        return PaletteKey::None;
    }
    if (list_parts::isListKey(key, Qt::Key_Delete)) {
        return PaletteKey::Remove;
    }
    if (list_parts::isOpenKey(key)) {
        return PaletteKey::Change;
    }
    if (list_parts::isListKey(key, Qt::Key_Left)) {
        return PaletteKey::Previous;
    }
    return list_parts::isListKey(key, Qt::Key_Right) ? PaletteKey::Next : PaletteKey::None;
}

}  // namespace

PaletteEdit::PaletteEdit(QString description, QWidget* parent)
    : QWidget(parent),
      m_description(std::move(description)),
      m_layout(new FlowLayout(this, SWATCH_SPACING, SWATCH_SPACING)) {
    m_layout->setContentsMargins(0, 0, 0, 0);
    //: The button after the swatches of a palette in the Settings. In Polish: Dodaj kolor...
    //: (Insert)
    m_addButton = list_parts::listButton(this, "common.add", tr("Add Color... (Insert)"),
                                         QStringLiteral("paletteAddColor"));
    m_addButton->installEventFilter(this);
    connect(m_addButton, &QToolButton::clicked, this, &PaletteEdit::addColor);
    m_layout->addWidget(m_addButton);
    // The frames of the swatches in the colors of the theme
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            &PaletteEdit::rebuild);
}

void PaletteEdit::setColors(const QStringList& colors) {
    m_colors.clear();
    for (const QString& color : colors) {
        if (QColor::isValidColorName(color)) {
            m_colors << QColor(color).name();
        }
    }
    rebuild();
}

QPixmap PaletteEdit::swatchOf(const QColor& color, int side, qreal pixelRatio,
                              const QColor& rim) {
    QPixmap pixmap(QSize(side, side) * pixelRatio);
    pixmap.setDevicePixelRatio(pixelRatio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF box(FRAME_INSET, FRAME_INSET, side - (2 * FRAME_INSET), side - (2 * FRAME_INSET));
    painter.setPen(rim);
    if (color.isValid()) {
        painter.setBrush(color);
    }
    painter.drawRect(box);
    if (!color.isValid()) {
        painter.drawLine(box.bottomLeft(), box.topRight());
    }
    return pixmap;
}

void PaletteEdit::addColor() {
    const QColor initial = m_colors.isEmpty() ? palette().color(QPalette::Highlight)
                                              : QColor(m_colors.constLast());
    const std::optional<QColor> chosen =
        dialogs::ColorDialog::getColor(initial, window(), m_description);
    if (!chosen.has_value() || !chosen->isValid()) {
        return;
    }
    m_colors << chosen->name();
    rebuild();
    focusSwatch(static_cast<int>(m_colors.size()) - 1);
    emit colorsChanged();
}

void PaletteEdit::changeColor(int index) {
    if (index < 0 || index >= m_colors.size()) {
        return;
    }
    const std::optional<QColor> chosen =
        dialogs::ColorDialog::getColor(QColor(m_colors.at(index)), window(), m_description);
    if (!chosen.has_value() || !chosen->isValid() || chosen->name() == m_colors.at(index)) {
        return;
    }
    m_colors[index] = chosen->name();
    rebuild();
    focusSwatch(index);
    emit colorsChanged();
}

void PaletteEdit::removeColor(int index) {
    if (index < 0 || index >= m_colors.size() || m_colors.size() <= 1) {
        return;
    }
    m_colors.removeAt(index);
    rebuild();
    focusSwatch(std::min(index, static_cast<int>(m_colors.size()) - 1));
    emit colorsChanged();
}

void PaletteEdit::rebuild() {
    // The swatches past the colors go later: one of them may be what calls
    while (m_swatches.size() > m_colors.size()) {
        QToolButton* extra = m_swatches.takeLast();
        m_layout->removeWidget(extra);
        extra->hide();
        extra->deleteLater();
    }
    m_layout->removeWidget(m_addButton);
    while (m_swatches.size() < m_colors.size()) {
        m_swatches << newSwatch(static_cast<int>(m_swatches.size()));
        m_layout->addWidget(m_swatches.constLast());
    }
    m_layout->addWidget(m_addButton);
    for (qsizetype i = 0; i < m_swatches.size(); ++i) {
        showSwatch(static_cast<int>(i));
    }
    setTabOrder(m_swatches.isEmpty() ? m_addButton : m_swatches.constLast(), m_addButton);
}

QToolButton* PaletteEdit::newSwatch(int index) {
    auto* swatch = new QToolButton(this);
    swatch->setObjectName(QStringLiteral("paletteColor%1").arg(index));
    swatch->setAutoRaise(true);
    swatch->setIconSize(QSize(SWATCH_SIDE, SWATCH_SIDE));
    swatch->installEventFilter(this);
    connect(swatch, &QToolButton::clicked, this, [this, index]() { changeColor(index); });
    if (!m_swatches.isEmpty()) {
        setTabOrder(m_swatches.constLast(), swatch);
    }
    return swatch;
}

void PaletteEdit::showSwatch(int index) {
    QToolButton* swatch = m_swatches.at(index);
    const QString color = m_colors.at(index).toUpper();
    swatch->setIcon(QIcon(swatchOf(QColor(color), SWATCH_SIDE, devicePixelRatioF(),
                                   palette().color(QPalette::Mid))));
    //: A swatch of a palette in the Settings: %1 its number, %2 its color, "#C0504D". In
    //: Polish: Kolor %1: %2
    swatch->setAccessibleName(tr("Color %1: %2").arg(QString::number(index + 1), color));
    //: The tooltip of a swatch of a palette in the Settings: %1 its number, %2 its color. In
    //: Polish: Kolor %1: %2. Kliknięcie go zmienia, Delete usuwa.
    swatch->setToolTip(tr("Color %1: %2. A click changes it, Delete takes it out.")
                           .arg(QString::number(index + 1), color));
}

void PaletteEdit::focusSwatch(int index) {
    QWidget* target = index >= 0 && index < m_swatches.size()
                          ? static_cast<QWidget*>(m_swatches.at(index))
                          : m_addButton;
    target->setFocus(Qt::OtherFocusReason);
}

bool PaletteEdit::eventFilter(QObject* watched, QEvent* event) {
    const bool press = event->type() == QEvent::KeyPress;
    if (!press && event->type() != QEvent::ShortcutOverride) {
        return QWidget::eventFilter(watched, event);
    }
    const auto index = static_cast<int>(m_swatches.indexOf(qobject_cast<QToolButton*>(watched)));
    const PaletteKey action = keyOf(static_cast<const QKeyEvent*>(event), index >= 0);
    if (action == PaletteKey::None) {
        return QWidget::eventFilter(watched, event);
    }
    // The same keys of the window's commands wait while a swatch has the keys
    event->accept();
    if (press) {
        act(static_cast<int>(action), index);
    }
    return true;
}

void PaletteEdit::act(int action, int index) {
    switch (static_cast<PaletteKey>(action)) {
    case PaletteKey::Add:
        addColor();
        break;
    case PaletteKey::Remove:
        removeColor(index);
        break;
    case PaletteKey::Change:
        changeColor(index);
        break;
    case PaletteKey::Previous:
        focusSwatch(std::max(0, index - 1));
        break;
    case PaletteKey::Next:
        focusSwatch(index + 1);
        break;
    case PaletteKey::None:
        break;
    }
}

}  // namespace kalahari::gui
