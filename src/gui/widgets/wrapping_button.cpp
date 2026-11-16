// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file wrapping_button.cpp
/// @brief A button in one line where it has room, which wraps its text where it has less

#include "kalahari/gui/widgets/wrapping_button.h"

#include <QEvent>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionButton>

#include <algorithm>

namespace kalahari {
namespace gui {

namespace {

/// @brief The room between the icon and the text (as in QPushButton::sizeHint)
constexpr int ICON_SPACING = 4;

/// @brief A width of the contents the style only adds its frame and margins to (a narrow
/// one it may widen to its least width of a button)
constexpr int MEASURE_WIDTH = 1000;

/// @brief The words of @p text, where its lines may end
QStringList wordsOf(const QString& text) {
    return text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

} // namespace

WrappingButton::WrappingButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent), m_fullText(text) {
    // Narrower than its text where it must, taller where it is narrower
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
}

void WrappingButton::setFullText(const QString& text) {
    m_fullText = text;
    QPushButton::setText(text);
    updateGeometry();
    // Laid out already: its lines now, else at its first size
    if (testAttribute(Qt::WA_Resized)) {
        wrap();
    }
}

QSize WrappingButton::sizeHint() const {
    return sizeFor(m_fullText);
}

QSize WrappingButton::minimumSizeHint() const {
    const QSize line = sizeFor(m_fullText);
    int widest = 0;
    for (const QString& word : wordsOf(m_fullText)) {
        widest = std::max(widest, sizeFor(word).width());
    }
    return {widest > 0 ? std::min(widest, line.width()) : line.width(), line.height()};
}

bool WrappingButton::hasHeightForWidth() const {
    return true;
}

int WrappingButton::heightForWidth(int width) const {
    return sizeFor(linesFor(width)).height();
}

void WrappingButton::resizeEvent(QResizeEvent* event) {
    QPushButton::resizeEvent(event);
    wrap();
}

void WrappingButton::changeEvent(QEvent* event) {
    QPushButton::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        updateGeometry();
        wrap();
    }
}

QSize WrappingButton::sizeFor(const QString& shown) const {
    // As QPushButton::sizeHint() measures its text, in all its lines
    ensurePolished();
    QStyleOptionButton option;
    initStyleOption(&option);
    option.text = shown;
    int width = 0;
    int height = 0;
    if (!icon().isNull()) {
        width += option.iconSize.width() + ICON_SPACING;
        height = option.iconSize.height();
    }
    const QSize text = fontMetrics().size(
        Qt::TextShowMnemonic, shown.isEmpty() ? QStringLiteral("XXXX") : shown);
    width += text.width();
    height = std::max(height, text.height());
    option.rect.setSize(QSize(width, height));
    return style()->sizeFromContents(QStyle::CT_PushButton, &option, QSize(width, height), this);
}

QString WrappingButton::linesFor(int width) const {
    if (m_fullText.isEmpty() || sizeFor(m_fullText).width() <= width) {
        return m_fullText;
    }
    // The room of the text: the width less the style's frame and margins, and the icon
    QStyleOptionButton option;
    initStyleOption(&option);
    const int height = fontMetrics().height();
    const int frame =
        style()
            ->sizeFromContents(QStyle::CT_PushButton, &option, QSize(MEASURE_WIDTH, height), this)
            .width() -
        MEASURE_WIDTH;
    const int iconWidth = icon().isNull() ? 0 : option.iconSize.width() + ICON_SPACING;
    const int room = width - frame - iconWidth;

    // As many words in a line as fit it; a word longer than the room has a line of its own
    const QFontMetrics metrics = fontMetrics();
    QStringList lines;
    QString line;
    for (const QString& word : wordsOf(m_fullText)) {
        const QString longer = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
        if (!line.isEmpty() && metrics.size(Qt::TextShowMnemonic, longer).width() > room) {
            lines.append(line);
            line = word;
        } else {
            line = longer;
        }
    }
    lines.append(line);
    return lines.join(QLatin1Char('\n'));
}

void WrappingButton::wrap() {
    const QString shown = linesFor(width());
    if (shown != text()) {
        QPushButton::setText(shown);
    }
}

} // namespace gui
} // namespace kalahari
