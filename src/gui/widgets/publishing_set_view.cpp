// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_set_view.cpp
/// @brief The set for a publisher as a page, its parts one under another

#include "kalahari/gui/widgets/publishing_set_view.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::gui {

namespace {

constexpr int CARD_MARGIN_X = 10;
constexpr int CARD_MARGIN_Y = 8;
constexpr int CARD_SPACING = 6;
constexpr int PAGE_MARGIN_X = 24;
constexpr int PAGE_MARGIN_Y = 20;
constexpr int PAGE_SPACING = 14;
constexpr int VERSIONS_CHARACTERS = 28;  ///< The narrowest list of versions, in characters
constexpr double TITLE_SCALE = 1.6;
constexpr double CURRENT_BORDER = 2.0;
constexpr double PALE = 0.5;  ///< The opacity of a part left out of the set

/// Whether @p key is the menu key or Shift+F10
bool isMenuKey(const QKeyEvent& key) {
    return key.key() == Qt::Key_Menu ||
           (key.key() == Qt::Key_F10 && key.modifiers() == Qt::ShiftModifier);
}

/// Whether @p key has no modifiers but those of the keypad
bool isPlain(const QKeyEvent& key) {
    return key.modifiers() == Qt::NoModifier || key.modifiers() == Qt::KeypadModifier;
}

/// A button of a card with its icon and text
QToolButton* cardButton(const char* name, const char* iconId, const QString& text,
                        QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QString::fromLatin1(name));
    button->setText(text);
    button->setIcon(core::ArtProvider::getInstance().getIcon(QString::fromLatin1(iconId),
                                                             core::IconContext::Menu));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(true);
    return button;
}

}  // namespace

// ============================================================================
// PublishingSetCard
// ============================================================================

PublishingSetCard::PublishingSetCard(const PublishingSetItem& item, QWidget* parent)
    : QFrame(parent)
    , m_part(item.part)
    , m_on(item.on)
    , m_text(new QLabel(this)) {
    setObjectName(QStringLiteral("publishingSetCard_") + item.part);
    setFocusPolicy(Qt::StrongFocus);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(CARD_MARGIN_X, CARD_MARGIN_Y, CARD_MARGIN_X, CARD_MARGIN_Y);
    layout->setSpacing(CARD_SPACING);
    layout->addWidget(createHeading(item));
    layout->addWidget(createButtons(item));

    m_text->setObjectName(QStringLiteral("publishingSetText"));
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::PlainText);
    m_text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    if (item.text.isEmpty()) {
        //: A part of the set for a publisher that the book has no text of yet. In Polish:
        //: Książka nie ma jeszcze tego tekstu.
        m_text->setText(PublishingSetView::tr("The book has no such text yet."));
        m_text->setForegroundRole(QPalette::PlaceholderText);
    } else {
        m_text->setText(item.text);
        QFont font = m_text->font();
        font.setItalic(item.italic);
        m_text->setFont(font);
    }
    layout->addWidget(m_text);

    QString state;
    if (!item.on) {
        //: A part left out of the set for a publisher, for a screen reader. In Polish: poza
        //: zestawem
        state = PublishingSetView::tr("left out of the set");
    }
    setAccessibleName(state.isEmpty() ? item.name
                                      : QStringLiteral("%1 (%2)").arg(item.name, state));
    setAccessibleDescription(item.text);
}

QWidget* PublishingSetCard::createHeading(const PublishingSetItem& item) {
    auto* heading = new QWidget(this);
    auto* layout = new FlowLayout(heading, CARD_SPACING, CARD_SPACING);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* name = new QLabel(item.name.toUpper(), heading);
    name->setObjectName(QStringLiteral("publishingSetName"));
    QFont bold = name->font();
    bold.setBold(true);
    name->setFont(bold);
    layout->addWidget(name);
    if (item.fromCover) {
        //: The mark of a description or a tagline of the set for a publisher, which the cover
        //: has. In Polish: z Okładki
        auto* cover = new QLabel(PublishingSetView::tr("from the Cover"), heading);
        cover->setObjectName(QStringLiteral("publishingSetFromCover"));
        cover->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(cover);
    }
    m_versions = new QComboBox(heading);
    m_versions->setObjectName(QStringLiteral("publishingSetVersions"));
    m_versions->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_versions->setMinimumContentsLength(VERSIONS_CHARACTERS);
    //: The list of the versions of a part of the set for a publisher, for a screen reader.
    //: In Polish: Wersja
    m_versions->setAccessibleName(PublishingSetView::tr("Version"));
    for (const PublishingSetVersion& version : item.versions) {
        m_versions->addItem(version.title, version.id);
    }
    m_versions->setCurrentIndex(std::max(0, m_versions->findData(item.elementId)));
    m_versions->setEnabled(m_versions->count() > 1);
    m_versions->setVisible(m_versions->count() > 0);
    connect(m_versions, &QComboBox::activated, this, [this](int index) {
        emit versionChosen(m_part, m_versions->itemData(index).toString());
    });
    layout->addWidget(m_versions);
    auto* length = new QLabel(item.length, heading);
    length->setObjectName(QStringLiteral("publishingSetLength"));
    length->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(length);
    return heading;
}

QWidget* PublishingSetCard::createButtons(const PublishingSetItem& item) {
    auto* buttons = new QWidget(this);
    auto* layout = new QHBoxLayout(buttons);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(CARD_SPACING);
    //: Copies the text of a part of the set for a publisher. In Polish: Kopiuj
    m_copy = cardButton("publishingSetCopy", "edit.copy", PublishingSetView::tr("Copy"),
                        buttons);
    //: Opens the text of a part of the set for a publisher where it is written. In Polish:
    //: Otwórz
    m_open = cardButton("publishingSetOpen", "common.openExternal",
                        PublishingSetView::tr("Open"), buttons);
    m_copy->setEnabled(!item.text.isEmpty());
    m_open->setEnabled(!item.elementId.isEmpty());
    connect(m_copy, &QToolButton::clicked, this, [this]() { emit copyRequested(m_part); });
    connect(m_open, &QToolButton::clicked, this, [this]() { emit openRequested(m_part); });
    layout->addWidget(m_copy);
    layout->addWidget(m_open);
    layout->addStretch();
    return buttons;
}

void PublishingSetCard::setCurrent(bool current) {
    if (m_current != current) {
        m_current = current;
        update();
    }
}

void PublishingSetCard::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!m_on) {
        painter.setOpacity(PALE);
    }
    const QPalette& colors = palette();
    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 4, 4);
    painter.fillPath(shape, colors.color(QPalette::Base));
    const bool marked = m_current || hasFocus();
    QPen border(marked ? colors.color(QPalette::Highlight) : colors.color(QPalette::Midlight));
    border.setWidthF(marked ? CURRENT_BORDER : 1.0);
    if (!m_on) {
        border.setStyle(Qt::DashLine);
    }
    painter.setPen(border);
    painter.drawPath(shape);
}

void PublishingSetCard::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    emit clicked(m_part);
    QFrame::mousePressEvent(event);
}

void PublishingSetCard::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(m_part);
    }
}

void PublishingSetCard::contextMenuEvent(QContextMenuEvent* event) {
    setFocus(Qt::MouseFocusReason);
    emit clicked(m_part);
    emit menuRequested(m_part, event->globalPos());
}

void PublishingSetCard::focusInEvent(QFocusEvent* event) {
    QFrame::focusInEvent(event);
    update();
}

void PublishingSetCard::focusOutEvent(QFocusEvent* event) {
    QFrame::focusOutEvent(event);
    update();
}

// ============================================================================
// PublishingSetView
// ============================================================================

PublishingSetView::PublishingSetView(QWidget* parent)
    : QWidget(parent)
    , m_page(new QFrame(this))
    , m_title(new QLabel(m_page))
    , m_about(new QLabel(m_page)) {
    setObjectName(QStringLiteral("publishingSetView"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    m_page->setObjectName(QStringLiteral("publishingSetPage"));
    m_page->setFrameShape(QFrame::StyledPanel);
    m_page->setAutoFillBackground(true);
    m_page->setBackgroundRole(QPalette::Base);
    auto* pageLayout = new QVBoxLayout(m_page);
    pageLayout->setContentsMargins(PAGE_MARGIN_X, PAGE_MARGIN_Y, PAGE_MARGIN_X, PAGE_MARGIN_Y);
    pageLayout->setSpacing(PAGE_SPACING);

    m_title->setObjectName(QStringLiteral("publishingSetTitle"));
    m_title->setWordWrap(true);
    m_title->setTextFormat(Qt::PlainText);
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * TITLE_SCALE);
    m_title->setFont(titleFont);
    m_about->setObjectName(QStringLiteral("publishingSetAbout"));
    m_about->setWordWrap(true);
    m_about->setTextFormat(Qt::PlainText);
    m_about->setForegroundRole(QPalette::PlaceholderText);
    auto* title = new QVBoxLayout;
    title->setSpacing(2);
    title->addWidget(m_title);
    title->addWidget(m_about);
    pageLayout->addLayout(title);

    m_partsLayout = new QVBoxLayout;
    m_partsLayout->setSpacing(PAGE_SPACING);
    pageLayout->addLayout(m_partsLayout);
    pageLayout->addStretch();
    layout->addWidget(m_page);
}

void PublishingSetView::setHeader(const QString& title, const QString& about) {
    m_title->setText(title.toUpper());
    m_about->setText(about);
    m_about->setVisible(!about.isEmpty());
}

void PublishingSetView::setItems(const QList<PublishingSetItem>& items) {
    const bool focused = isAncestorOf(QApplication::focusWidget());
    for (PublishingSetCard* card : std::as_const(m_cards)) {
        card->deleteLater();
        card->hide();
    }
    m_cards.clear();
    m_order.clear();
    for (const PublishingSetItem& item : items) {
        auto* card = new PublishingSetCard(item, m_page);
        m_partsLayout->addWidget(card);
        connectCard(card);
        m_cards.insert(item.part, card);
        m_order << item.part;
    }
    if (!m_order.contains(m_current)) {
        m_current = m_order.isEmpty() ? QString() : m_order.constFirst();
    }
    setCurrent(m_current);
    if (focused) {
        focusCurrent();
    }
}

void PublishingSetView::connectCard(PublishingSetCard* card) {
    card->installEventFilter(this);
    connect(card, &PublishingSetCard::clicked, this,
            [this](const QString& part) { choose(part, false); });
    connect(card, &PublishingSetCard::doubleClicked, this, &PublishingSetView::activated);
    connect(card, &PublishingSetCard::openRequested, this, &PublishingSetView::activated);
    connect(card, &PublishingSetCard::copyRequested, this, &PublishingSetView::copyRequested);
    connect(card, &PublishingSetCard::versionChosen, this, &PublishingSetView::versionChosen);
    connect(card, &PublishingSetCard::menuRequested, this, &PublishingSetView::menuRequested);
}

void PublishingSetView::setCurrent(const QString& part) {
    m_current = m_cards.contains(part) ? part : QString();
    for (auto it = m_cards.cbegin(); it != m_cards.cend(); ++it) {
        it.value()->setCurrent(it.key() == m_current);
    }
}

void PublishingSetView::focusCurrent() {
    if (PublishingSetCard* card = m_cards.value(m_current)) {
        card->setFocus(Qt::OtherFocusReason);
    } else if (!m_order.isEmpty()) {
        choose(m_order.constFirst(), true);
    }
}

void PublishingSetView::choose(const QString& part, bool focus) {
    const bool changed = part != m_current;
    setCurrent(part);
    if (focus) {
        if (PublishingSetCard* card = m_cards.value(m_current)) {
            card->setFocus(Qt::OtherFocusReason);
        }
    }
    if (changed && !m_current.isEmpty()) {
        emit currentChanged(m_current);
    }
}

bool PublishingSetView::cardKey(const PublishingSetCard* card, const QKeyEvent& key) {
    const QString part = card->part();
    if (isMenuKey(key)) {
        emit menuRequested(part, card->mapToGlobal(card->rect().center()));
        return true;
    }
    if (!isPlain(key)) {
        return false;
    }
    const auto index = m_order.indexOf(part);
    switch (key.key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        emit activated(part);
        return true;
    case Qt::Key_Space:
        emit toggled(part);
        return true;
    case Qt::Key_Up:
        if (index > 0) {
            choose(m_order.at(index - 1), true);
        }
        return true;
    case Qt::Key_Down:
        if (index + 1 < m_order.size()) {
            choose(m_order.at(index + 1), true);
        }
        return true;
    default:
        return false;
    }
}

bool PublishingSetView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        if (const auto* card = qobject_cast<PublishingSetCard*>(watched);
            card != nullptr && cardKey(card, *static_cast<QKeyEvent*>(event))) {
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace kalahari::gui
