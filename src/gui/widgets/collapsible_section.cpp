// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file collapsible_section.cpp
/// @brief A section of fields under a header that opens and closes it, with a summary of the
///        fields in the header while it is closed

#include "kalahari/gui/widgets/collapsible_section.h"

#include <QFontMetrics>
#include <QGridLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QRadioButton>
#include <QShortcutEvent>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// The room between the arrow, the title and the summary, in average letters
constexpr int GAP_LETTERS = 1;
/// The width of the summary the header asks for, in average letters
constexpr int SUMMARY_LETTERS = 14;
/// The transparency of the color of a button under the mouse (0-255)
constexpr int HOVER_ALPHA = 110;

/// @p text without the mark of its access key: "&Printer" is "Printer", "&&" is "&"
QString withoutAccessKey(const QString& text) {
    QString plain;
    plain.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&') && i + 1 < text.size()) {
            ++i;  // the next character as it is: the key, or the second '&'
        }
        plain += text.at(i);
    }
    return plain;
}

/// @p font in bold
QFont boldOf(QFont font) {
    font.setBold(true);
    return font;
}

}  // namespace

// =============================================================================
// SectionHeader
// =============================================================================

SectionHeader::SectionHeader(const QString& title, QWidget* parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setChecked(true);
    setText(title);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setAccessibleName(withoutAccessKey(title));
    connect(this, &QAbstractButton::toggled, this, [this]() { update(); });
}

void SectionHeader::setSummary(const QString& summary)
{
    if (summary == m_summary) {
        return;
    }
    m_summary = summary;
    setAccessibleDescription(summary);
    update();
}

int SectionHeader::padding() const
{
    return fontMetrics().height() / 3;
}

int SectionHeader::arrowSize() const
{
    return fontMetrics().height() * 2 / 3;
}

int SectionHeader::titleWidth() const
{
    return QFontMetrics(boldOf(font())).horizontalAdvance(withoutAccessKey(text()));
}

int SectionHeader::titleIndent() const
{
    return padding() + arrowSize() + fontMetrics().averageCharWidth() * GAP_LETTERS;
}

QSize SectionHeader::sizeHint() const
{
    const int gap = fontMetrics().averageCharWidth() * GAP_LETTERS;
    return {titleIndent() + titleWidth() + gap * 2 +
                fontMetrics().averageCharWidth() * SUMMARY_LETTERS + padding(),
            QFontMetrics(boldOf(font())).height() + padding() * 2};
}

QSize SectionHeader::minimumSizeHint() const
{
    return {titleIndent() + titleWidth() + padding(), sizeHint().height()};
}

bool SectionHeader::event(QEvent* event)
{
    // Alt and the access key open the section and go to its first field; a key that other
    // fields of the window have too only comes here in turn
    if (event->type() == QEvent::Shortcut) {
        if (static_cast<QShortcutEvent*>(event)->isAmbiguous()) {
            setFocus(Qt::ShortcutFocusReason);
        } else {
            emit accessKeyPressed();
        }
        return true;
    }
    return QAbstractButton::event(event);
}

void SectionHeader::keyPressEvent(QKeyEvent* event)
{
    // Enter opens and closes the section, as Space does, and does not press the button of
    // the window
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        click();
        event->accept();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void SectionHeader::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    QStyleOption option;
    option.initFrom(this);
    if (isDown() || (underMouse() && isEnabled())) {
        QColor hover = palette().color(QPalette::Button);
        hover.setAlpha(HOVER_ALPHA);
        painter.fillRect(rect(), hover);
    }

    // The arrow: down while the section is open
    const int pad = padding();
    const int arrow = arrowSize();
    QStyleOption arrowOption = option;
    arrowOption.rect = QRect(pad, (height() - arrow) / 2, arrow, arrow);
    style()->drawPrimitive(isChecked() ? QStyle::PE_IndicatorArrowDown
                                       : QStyle::PE_IndicatorArrowRight,
                           &arrowOption, &painter, this);

    // The title in bold, with its access key
    const int gap = fontMetrics().averageCharWidth() * GAP_LETTERS;
    const QFont bold = boldOf(font());
    painter.setFont(bold);
    const int titleStart = titleIndent();
    const int width = std::min(titleWidth(), std::max(0, this->width() - titleStart - pad));
    const QRect titleRect(titleStart, 0, width, height());
    const int mnemonic = style()->styleHint(QStyle::SH_UnderlineShortcut, &option, this) != 0
                             ? Qt::TextShowMnemonic
                             : Qt::TextHideMnemonic;
    style()->drawItemText(&painter, titleRect,
                          static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter) | Qt::TextSingleLine |
                              mnemonic,
                          palette(), isEnabled(), text(), QPalette::WindowText);

    // The summary of a closed section at the right, cut where it does not fit
    if (!isChecked() && !m_summary.isEmpty()) {
        painter.setFont(font());
        const QRect summaryRect(titleRect.right() + gap * 2, 0,
                                this->width() - titleRect.right() - gap * 2 - pad, height());
        if (summaryRect.width() > 0) {
            painter.setPen(palette().color(QPalette::PlaceholderText));
            painter.drawText(summaryRect, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
                             fontMetrics().elidedText(m_summary, Qt::ElideRight,
                                                      summaryRect.width()));
        }
    }

    if (hasFocus()) {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);
        focus.rect = rect().adjusted(1, 1, -1, -1);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
    }
}

// =============================================================================
// CollapsibleSection
// =============================================================================

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    // Room for the line under the section
    layout->setContentsMargins(0, 0, 0, 1);
    layout->setSpacing(0);
    m_header = new SectionHeader(title, this);
    layout->addWidget(m_header);
    m_body = new QWidget(this);
    m_grid = new QGridLayout(m_body);
    m_grid->setColumnStretch(1, 1);
    // The fields under the title, with the room of the header around them
    const int pad = fontMetrics().height() / 3;
    m_grid->setContentsMargins(m_header->titleIndent(), 0, pad, pad * 2);
    layout->addWidget(m_body);
    connect(m_header, &QAbstractButton::toggled, this, [this](bool open) {
        m_body->setVisible(open);
        emit expandedChanged(open);
    });
    connect(m_header, &SectionHeader::accessKeyPressed, this, &CollapsibleSection::openAndFocus);
}

bool CollapsibleSection::isExpanded() const
{
    return m_header->isChecked();
}

void CollapsibleSection::setExpanded(bool expanded)
{
    m_header->setChecked(expanded);
}

void CollapsibleSection::setSummary(const QString& summary)
{
    m_header->setSummary(summary);
}

QString CollapsibleSection::summary() const
{
    return m_header->summary();
}

QWidget* CollapsibleSection::firstField() const
{
    // In the order Tab goes, from the header
    for (QWidget* each = m_header->nextInFocusChain(); each != nullptr && each != m_header;
         each = each->nextInFocusChain()) {
        if (!m_body->isAncestorOf(each) || !each->isVisibleTo(m_body) || !each->isEnabled() ||
            (each->focusPolicy() & Qt::TabFocus) == 0) {
            continue;
        }
        // Of radio buttons that go together, Tab reaches the one checked
        if (const auto* radio = qobject_cast<const QRadioButton*>(each);
            radio != nullptr && !radio->isChecked() && radio->autoExclusive()) {
            const QList<QRadioButton*> together =
                radio->parentWidget()->findChildren<QRadioButton*>(Qt::FindDirectChildrenOnly);
            if (std::any_of(together.cbegin(), together.cend(), [](const QRadioButton* other) {
                    return other->isChecked() && other->autoExclusive() && other->isEnabled() &&
                           !other->isHidden();
                })) {
                continue;
            }
        }
        return each;
    }
    return nullptr;
}

void CollapsibleSection::openAndFocus()
{
    setExpanded(true);
    QWidget* field = firstField();
    (field != nullptr ? field : static_cast<QWidget*>(m_header))->setFocus(Qt::ShortcutFocusReason);
}

void CollapsibleSection::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(0, height() - 1, width(), height() - 1);
}

} // namespace kalahari::gui
