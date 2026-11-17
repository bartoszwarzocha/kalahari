// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_view.cpp
/// @brief The cards of the plot board: the texts of the book in the order of the book, in
/// columns of the parts or in rows, moved with the keys or the mouse. Its keys and the mouse,
/// where the cards are and how they are painted are in plot_board_view_*.cpp.

#include "kalahari/gui/widgets/plot_board_view.h"

#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/widgets/plot_board_view_internal.h"
#include "kalahari/gui/widgets/plot_card_editor.h"

#include <QAbstractScrollArea>
#include <QAccessible>
#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QFrame>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QList>
#include <QPalette>
#include <QPoint>
#include <QRect>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QToolTip>
#include <QWidget>

#include <algorithm>

namespace kalahari::gui {

using board_view::DEFAULT_HEIGHT;
using board_view::DEFAULT_WIDTH;
using board_view::MARGIN;
using board_view::SCROLL_STEP;
using board_view::textOfCard;

namespace {

/// @brief The kind and the title of @p card, as the screen reader says them
QString nameOf(const PlotCard& card) {
    if (card.kindName.isEmpty()) {
        return card.title;
    }
    if (card.title.isEmpty()) {
        return card.kindName;
    }
    return QStringLiteral("%1: %2").arg(card.kindName, card.title);
}

}  // namespace

PlotBoardView::PlotBoardView(QWidget* parent)
    : QAbstractScrollArea(parent), m_colors(PlotColors::current()) {
    setObjectName(QStringLiteral("plotBoardView"));
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    viewport()->setBackgroundRole(QPalette::Window);
    viewport()->setAutoFillBackground(true);
    viewport()->setAcceptDrops(true);
    viewport()->setMouseTracking(false);
    horizontalScrollBar()->setSingleStep(SCROLL_STEP);
    verticalScrollBar()->setSingleStep(SCROLL_STEP);
    //: The cards of the texts of the book on the plot board, for the screen reader. In
    //: Polish: Tablica fabuły
    setAccessibleName(tr("Plot Board"));
}

PlotBoardView::~PlotBoardView() = default;

void PlotBoardView::setBands(const QList<PlotBand>& bands) {
    // The card at the place of the one chosen, when the board no longer has it; for a scene,
    // its text
    const QString currentText = textOfCard(m_current);
    const qsizetype at = indexOfText(currentText);
    m_bands = bands;
    m_povs = povsOf(PlotBoard::allCardsOf(m_bands));
    relayout();
    if (m_editor != nullptr && placeOf(m_editor->elementId()) == nullptr) {
        closeEditor(false);
    }
    if (!m_marked.isEmpty() && placeOf(m_marked) == nullptr) {
        m_marked.clear();
    }
    if (!m_current.isEmpty() && placeOf(m_current) == nullptr) {
        setCurrent(placeOf(currentText) != nullptr ? currentText : textAt(at));
    }
    placeEditor();
    viewport()->update();
}

void PlotBoardView::setSections(const PlotBand& front, const PlotBand& back) {
    m_front = front;
    m_back = back;
    relayout();
    placeEditor();
    viewport()->update();
}

void PlotBoardView::setShowSections(bool show) {
    if (m_showSections == show) {
        return;
    }
    m_showSections = show;
    relayout();
    showCurrent();
}

qsizetype PlotBoardView::indexOfText(const QString& textId) const {
    const QList<PlotCard> texts = PlotBoard::cardsOf(m_bands);
    for (qsizetype i = 0; i < texts.size(); ++i) {
        if (texts.at(i).id == textId) {
            return i;
        }
    }
    return -1;
}

QString PlotBoardView::textAt(qsizetype at) const {
    const QList<PlotCard> texts = PlotBoard::cardsOf(m_bands);
    if (texts.isEmpty()) {
        return {};
    }
    return texts.at(std::clamp<qsizetype>(at, 0, texts.size() - 1)).id;
}

void PlotBoardView::setCurrent(const QString& elementId) {
    if (m_current == elementId) {
        return;
    }
    m_current = elementId;
    // A copy: whoever takes the signal may choose another card
    const QString current = m_current;
    emit currentCardChanged(current);
}

void PlotBoardView::showCurrent() {
    if (const CardPlace* place = placeOf(m_current)) {
        ensureVisible(place->rect);
    }
    placeEditor();
    viewport()->update();
}

void PlotBoardView::setArrangement(Arrangement arrangement) {
    if (m_arrangement == arrangement) {
        return;
    }
    m_arrangement = arrangement;
    relayout();
    showCurrent();
}

void PlotBoardView::setCardSize(CardSize size) {
    if (m_cardSize == size) {
        return;
    }
    m_cardSize = size;
    relayout();
    showCurrent();
}

void PlotBoardView::setFields(const Fields& fields) {
    if (m_fields == fields) {
        return;
    }
    m_fields = fields;
    relayout();
    showCurrent();
}

void PlotBoardView::setShowScenes(bool show) {
    if (m_showScenes == show) {
        return;
    }
    m_showScenes = show;
    m_toggled.clear();
    relayout();
    // The card of a scene hidden: its text is chosen
    if (placeOf(m_current) == nullptr) {
        choose(textOfCard(m_current));
    }
    placeEditor();
    viewport()->update();
}

bool PlotBoardView::scenesShown(const QString& textId) const {
    return m_showScenes != m_toggled.contains(textId);
}

bool PlotBoardView::toggleScenes(const QString& textId) {
    const PlotCard* card = cardOf(textId);
    if (m_arrangement != Arrangement::Grid || card == nullptr || card->scenes.isEmpty()) {
        return false;
    }
    if (!m_toggled.remove(textId)) {
        m_toggled.insert(textId);
    }
    relayout();
    if (placeOf(m_current) == nullptr) {
        choose(textId);  // a scene of the drawer closed: its text
    }
    showCurrent();
    return true;
}

void PlotBoardView::setColoring(const QString& colorBy, const PlotColors& colors) {
    m_colorBy = colorBy;
    m_colors = colors;
    updateBackground();
    viewport()->update();
}

void PlotBoardView::setTint(bool tint) {
    m_tint = tint;
    viewport()->update();
}

void PlotBoardView::setBackground(Background background) {
    m_background = background;
    updateBackground();
    viewport()->update();
}

void PlotBoardView::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
    if (readOnly) {
        closeEditor(false);
        m_marked.clear();
    }
    viewport()->update();
}

void PlotBoardView::setMarkedCard(const QString& elementId) {
    m_marked = placeOf(elementId) != nullptr ? elementId : QString();
    viewport()->update();
}

bool PlotBoardView::setCurrentCard(const QString& elementId) {
    if (placeOf(elementId) == nullptr) {
        return false;
    }
    choose(elementId);
    return true;
}

bool PlotBoardView::editCard(const QString& elementId, const QStringList& povs,
                             const QStringList& places) {
    const PlotCard* card = cardOf(elementId);
    if (card == nullptr || m_readOnly) {
        return false;
    }
    closeEditor(false);
    choose(elementId);
    m_editor = new PlotCardEditor(*card, povs, places, viewport());
    connect(m_editor, &PlotCardEditor::accepted, this, [this]() { closeEditor(true); });
    connect(m_editor, &PlotCardEditor::rejected, this, [this]() { closeEditor(false); });
    placeEditor();
    m_editor->show();
    m_editor->raise();
    m_editor->focusTitle();
    return true;
}

QStringList PlotBoardView::plotTexts(const PlotCard& card) {
    QStringList texts;
    if (!card.plot.pov.isEmpty()) {
        //: Read by the screen reader on a card of the plot board. In Polish: Punkt widzenia: %1
        texts << tr("Point of view: %1").arg(card.plot.pov);
    }
    if (!card.plot.place.isEmpty()) {
        //: In Polish: Miejsce: %1
        texts << tr("Place: %1").arg(card.plot.place);
    }
    if (!card.plot.time.isEmpty()) {
        //: In Polish: Czas akcji: %1
        texts << tr("Time of the action: %1").arg(card.plot.time);
    }
    return texts;
}

QString PlotBoardView::spokenText(const QString& elementId) const {
    const PlotCard* card = cardOf(elementId);
    if (card == nullptr) {
        return {};
    }
    QStringList parts{nameOf(*card)};
    if (const CardPlace* place = placeOf(elementId); place != nullptr && isPlanBesideText(*place)) {
        //: Read by the screen reader on a card of a scene in the plan beside the scenes of its
        //: text, after "In the Plan: The Storm". In Polish: Enter wpisuje ją do tekstu
        parts << tr("Enter writes it in the text");
    } else if (!card->planned) {
        // A scene in the plan has no state and no words
        parts << PlotBoard::statusName(card->status) << PlotBoard::wordsText(card->words);
    }
    if (!card->description.isEmpty()) {
        parts << card->description;
    }
    // A text without its own point of view has those of its scenes
    if (const QStringList povs = card->scenePovs(); card->plot.pov.isEmpty() && !povs.isEmpty()) {
        parts << tr("Point of view: %1").arg(povs.join(QStringLiteral(", ")));
    }
    parts << plotTexts(*card);
    if (!card->scenes.isEmpty()) {
        parts << PlotBoard::scenesText(static_cast<int>(card->scenes.size()));
    }
    if (card->id == m_marked) {
        //: A card marked to move with Ctrl+X. In Polish: Wycięta – Ctrl+V wstawia ją za
        //: bieżącą kartą
        parts << tr("Cut – Ctrl+V puts it after the current card");
    }
    return parts.join(QStringLiteral(". "));
}

QString PlotBoardView::toolTipOf(const PlotCard& card, const QString& note) {
    QStringList lines{QStringLiteral("<b>%1</b>").arg(
        (card.title.isEmpty() ? card.kindName : card.title).toHtmlEscaped())};
    if (!card.description.isEmpty()) {
        lines << card.description.toHtmlEscaped();
    }
    QStringList plot = plotTexts(card);
    if (!plot.isEmpty()) {
        for (QString& text : plot) {
            text = text.toHtmlEscaped();
        }
        lines << plot.join(QStringLiteral("<br>"));
    }
    if (!note.isEmpty()) {
        lines << QStringLiteral("<i>%1</i>").arg(note.toHtmlEscaped());
    }
    return QStringLiteral("<p style='white-space:pre-wrap'>%1</p>")
        .arg(lines.join(QStringLiteral("<br>")));
}

QSize PlotBoardView::sizeHint() const {
    return {DEFAULT_WIDTH, DEFAULT_HEIGHT};
}

void PlotBoardView::choose(const QString& elementId) {
    const CardPlace* place = placeOf(elementId);
    if (place == nullptr) {
        return;
    }
    ensureVisible(place->rect);
    if (m_current != elementId) {
        setCurrent(elementId);
        tellScreenReader();
    }
    viewport()->update();
}

void PlotBoardView::ensureVisible(const QRect& rect) {
    const QRect shown(QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value()),
                      viewport()->size());
    const QRect wanted = rect.adjusted(-MARGIN, -MARGIN, MARGIN, MARGIN);
    if (wanted.left() < shown.left()) {
        horizontalScrollBar()->setValue(wanted.left());
    } else if (wanted.right() > shown.right()) {
        horizontalScrollBar()->setValue(wanted.right() - shown.width() + 1);
    }
    if (wanted.top() < shown.top()) {
        verticalScrollBar()->setValue(wanted.top());
    } else if (wanted.bottom() > shown.bottom()) {
        verticalScrollBar()->setValue(wanted.bottom() - shown.height() + 1);
    }
}

void PlotBoardView::tellScreenReader() {
    setAccessibleDescription(spokenText(m_current));
    if (QAccessible::isActive()) {
        QAccessibleEvent event(this, QAccessible::DescriptionChanged);
        QAccessible::updateAccessibility(&event);
    }
}

// =============================================================================
// Events
// =============================================================================

bool PlotBoardView::event(QEvent* event) {
    if (event->type() == QEvent::ShortcutOverride &&
        keyOf(static_cast<QKeyEvent*>(event)) != Key::None) {
        // The keys of the board are its own, not the window's commands
        event->accept();
        return true;
    }
    return QAbstractScrollArea::event(event);
}

bool PlotBoardView::viewportEvent(QEvent* event) {
    if (event->type() != QEvent::ToolTip) {
        return QAbstractScrollArea::viewportEvent(event);
    }
    const auto* help = static_cast<QHelpEvent*>(event);
    const CardPlace* place = cardAt(contentsPos(help->pos()));
    if (place != nullptr) {
        QToolTip::showText(help->globalPos(), toolTipOf(cardOfPlace(*place)), viewport(),
                           cardRect(place->id));
        return true;
    }
    if (const PlotCard* text = sectionCardAt(contentsPos(help->pos()))) {
        //: The tooltip of a text of the front or the back section on the plot board, after its
        //: title. In Polish: Tekst sekcji – otwiera się z Nawigatora.
        const QString note = tr("A text of the section – it opens from the Navigator.");
        QToolTip::showText(help->globalPos(), toolTipOf(*text, note), viewport(),
                           sectionCardRect(text->id));
        return true;
    }
    QToolTip::hideText();
    event->ignore();
    return true;
}

void PlotBoardView::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    relayout();
    placeEditor();
}

void PlotBoardView::scrollContentsBy(int /*dx*/, int /*dy*/) {
    placeEditor();
    viewport()->update();
}

void PlotBoardView::changeEvent(QEvent* event) {
    QAbstractScrollArea::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        relayout();
        placeEditor();
        viewport()->update();
    } else if (event->type() == QEvent::PaletteChange) {
        viewport()->update();
    }
}

void PlotBoardView::focusInEvent(QFocusEvent* event) {
    QAbstractScrollArea::focusInEvent(event);
    if (placeOf(m_current) == nullptr && !m_cards.isEmpty()) {
        choose(m_cards.constFirst().id);
    }
    tellScreenReader();
    viewport()->update();
}

void PlotBoardView::focusOutEvent(QFocusEvent* event) {
    QAbstractScrollArea::focusOutEvent(event);
    viewport()->update();
}

// =============================================================================
// The editor of a card
// =============================================================================

void PlotBoardView::closeEditor(bool save) {
    if (m_editor == nullptr) {
        return;
    }
    PlotCardEditor* editor = m_editor;
    m_editor = nullptr;
    const QString elementId = editor->elementId();
    const PlotCardEdit edit = editor->edit();
    const bool hadKeys = editor->isAncestorOf(QApplication::focusWidget());
    editor->hide();
    editor->deleteLater();
    if (hadKeys) {
        setFocus(Qt::OtherFocusReason);
    }
    viewport()->update();
    if (save) {
        emit editFinished(elementId, edit);
    }
}

void PlotBoardView::placeEditor() {
    if (m_editor == nullptr) {
        return;
    }
    const QRect rect = cardRect(m_editor->elementId());
    if (!rect.isValid()) {
        return;
    }
    const QSize hint = m_editor->sizeHint();
    const int width = std::max(rect.width(), hint.width());
    const int height = hint.height();
    const QRect area = viewport()->rect();
    const int x = std::max(std::min(rect.left(), area.right() - width), area.left());
    const int y = std::max(std::min(rect.top(), area.bottom() - height), area.top());
    m_editor->setGeometry(x, y, width, height);
}

}  // namespace kalahari::gui
