/// @file annotation_frame.cpp
/// @brief The frame an annotation's text is written in, at its place in the text

#include "kalahari/gui/panels/annotation_frame.h"
#include "kalahari/gui/panels/annotation_card.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace kalahari::gui {

namespace {

constexpr int BAR_WIDTH = 4;          ///< The bar in the kind's color on the left, as on the cards
constexpr qreal CORNER_RADIUS = 8.0;  ///< The rounded corners, as on the cards
constexpr int MIN_WIDTH = 300;        ///< The narrowest frame (when the editor has room)
constexpr int MAX_WIDTH = 560;        ///< The widest frame
constexpr qreal GAP = 4.0;            ///< Between the place and the frame
constexpr qreal EDGE = 8.0;           ///< The least room between the frame and the editor's edges
constexpr qreal LEAD = 16.0;          ///< The frame starts this far left of its place
constexpr int MIN_LINES = 3;          ///< The text box is at least this many lines high...
constexpr int MAX_LINES = 8;          ///< ...and at most this many (then it scrolls)

/// @brief How much of the kind's color the frame's border has
constexpr double BORDER_SHARE = 0.55;

/// @brief Ctrl+Enter (Cmd+Return on macOS): keep the text
bool isSaveKey(const QKeyEvent* event) {
    const bool enter = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
    return enter && (event->modifiers() & Qt::ControlModifier);
}

/// @brief Esc: drop the text
bool isCancelKey(const QKeyEvent* event) {
    return event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier;
}

/// @brief A shortcut of the window that works while the frame has the keys: saving, closing
/// and quitting (what is written is kept before)
bool reachesWindow(const QKeyEvent* event) {
    return event->matches(QKeySequence::Save) || event->matches(QKeySequence::SaveAs) ||
           event->matches(QKeySequence::Close) || event->matches(QKeySequence::Quit);
}

}  // namespace

AnnotationFrame::AnnotationFrame(QWidget* editor)
    : QFrame(editor)
{
    setObjectName(QStringLiteral("annotationFrame"));
    setAttribute(Qt::WA_NoMousePropagation);  // a click on it does not move the editor's cursor
    setFont(QApplication::font());            // not the font of the text under it
    setCursor(Qt::ArrowCursor);
    setFocusPolicy(Qt::ClickFocus);
    setAutoFillBackground(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(BAR_WIDTH + 8, 6, 8, 8);
    layout->setSpacing(4);

    QFont kindFont = font();
    kindFont.setPointSizeF(kindFont.pointSizeF() * 0.9);
    kindFont.setBold(true);
    m_kindLabel = new QLabel(this);
    m_kindLabel->setObjectName(QStringLiteral("annotationFrameKind"));
    m_kindLabel->setFont(kindFont);
    layout->addWidget(m_kindLabel);

    // Enter starts a new line; Tab goes to the button
    m_textEdit = new QTextEdit(this);
    m_textEdit->setAcceptRichText(false);
    m_textEdit->setTabChangesFocus(true);
    m_textEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textEdit->setPlaceholderText(tr("Type the text of the annotation..."));
    m_textEdit->installEventFilter(this);
    connect(m_textEdit, &QTextEdit::textChanged, this,
            [this]() { m_saveButton->setEnabled(canSave()); });
    connect(m_textEdit->document()->documentLayout(),
            &QAbstractTextDocumentLayout::documentSizeChanged, this, [this]() {
                updateTextHeight();
                schedulePlacement();
            });
    layout->addWidget(m_textEdit);
    setFocusProxy(m_textEdit);

    // What the keys do, and the button
    auto* footer = new QHBoxLayout();
    footer->setSpacing(8);
    QFont hintFont = font();
    hintFont.setPointSizeF(hintFont.pointSizeF() * 0.9);
    m_hintLabel = new QLabel(tr("%1 to save, Esc to cancel").arg(saveKeysText()), this);
    m_hintLabel->setObjectName(QStringLiteral("annotationFrameHint"));
    m_hintLabel->setFont(hintFont);
    footer->addWidget(m_hintLabel, 1);

    m_saveButton = new QPushButton(tr("Save"), this);
    m_saveButton->setToolTip(tr("Save (%1)").arg(saveKeysText()));
    m_saveButton->setEnabled(false);
    m_saveButton->installEventFilter(this);
    connect(m_saveButton, &QPushButton::clicked, this, &AnnotationFrame::requestSave);
    footer->addWidget(m_saveButton);
    layout->addLayout(footer);

    // The frame follows its place as the editor changes
    editor->installEventFilter(this);
    updateTextHeight();
}

QString AnnotationFrame::saveKeysText() {
    return QKeySequence(Qt::CTRL | Qt::Key_Enter).toString(QKeySequence::NativeText);
}

void AnnotationFrame::setKind(editor::AnnotationKind kind) {
    const QString title = AnnotationCard::kindTitle(kind);
    m_kindLabel->setText(title);
    m_textEdit->setAccessibleName(title);
}

void AnnotationFrame::setText(const QString& text) {
    m_textEdit->setPlainText(text);
    m_saveButton->setEnabled(canSave());
}

QString AnnotationFrame::text() const {
    return m_textEdit->toPlainText();
}

bool AnnotationFrame::canSave() const {
    return !text().trimmed().isEmpty();
}

void AnnotationFrame::setColors(const AnnotationCardColors& colors) {
    m_colors = colors;
    updateStyle();  // also when they are the same: a new palette may have taken the old
}

void AnnotationFrame::setPlacement(PlacementProvider provider) {
    m_placement = std::move(provider);
    place();
}

void AnnotationFrame::place() {
    m_placementPending = false;
    const QWidget* editorWidget = parentWidget();
    if (!m_placement || editorWidget == nullptr) {
        return;
    }
    const QRectF area = QRectF(editorWidget->rect()).adjusted(EDGE, EDGE, -EDGE, -EDGE);
    if (area.width() < 1.0 || area.height() < 1.0) {
        return;
    }
    const Placement placement = m_placement();
    QRectF column = placement.column.intersected(area);
    if (column.width() < 1.0) {
        column = area;
    }

    // Two thirds of the column, within limits and within the editor; the text wraps anew
    const int frameWidth =
        std::min(std::clamp(static_cast<int>(std::lround(column.width() * 2.0 / 3.0)), MIN_WIDTH,
                            MAX_WIDTH),
                 static_cast<int>(area.width()));
    if (frameWidth != width()) {
        resize(frameWidth, height());
        updateTextHeight();
    }
    const int frameHeight = sizeHint().height();

    // Under the place; above it when there is no room below; over its line when there is
    // room neither below nor above. A place out of view takes the frame along.
    const QRectF place = placement.place;
    qreal y = place.bottom() + GAP;
    if (y + frameHeight > area.bottom()) {
        const qreal above = place.top() - GAP - frameHeight;
        const bool placeShown = place.bottom() >= area.top() && place.top() <= area.bottom();
        if (above >= area.top()) {
            y = above;
        } else if (placeShown) {
            y = std::max(area.top(), area.bottom() - frameHeight);
        }
    }

    // From a little left of the place, within the column and the editor
    qreal x = std::min(place.left() - LEAD, column.right() - frameWidth);
    x = std::max(x, column.left());
    x = std::clamp(x, area.left(), std::max(area.left(), area.right() - frameWidth));

    setGeometry(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)), frameWidth,
                frameHeight);
}

void AnnotationFrame::startTyping() {
    m_textEdit->setFocus(Qt::OtherFocusReason);
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_textEdit->setTextCursor(cursor);
    m_textEdit->ensureCursorVisible();
}

void AnnotationFrame::requestSave() {
    if (canSave()) {
        emit saveRequested();
    }
}

void AnnotationFrame::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor background =
        m_colors.background.isValid() ? m_colors.background : palette().color(QPalette::Base);
    const QColor kind = m_colors.kind.isValid() ? m_colors.kind : palette().color(QPalette::Mid);

    // As a card of the panel: a rounded card, the bar in the kind's color on the left; a
    // border sets it off from the paper
    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath shape;
    shape.addRoundedRect(card, CORNER_RADIUS, CORNER_RADIUS);
    painter.fillPath(shape, background);
    painter.save();
    painter.setClipPath(shape);
    painter.fillRect(QRectF(0, 0, BAR_WIDTH, height()), kind);
    painter.restore();
    painter.setPen(QPen(mixedColor(background, kind, BORDER_SHARE), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);
}

void AnnotationFrame::keyPressEvent(QKeyEvent* event) {
    if (isCancelKey(event)) {
        emit cancelRequested();
    } else if (isSaveKey(event)) {
        requestSave();
    }
    event->accept();  // no key goes on to the editor under the frame
}

void AnnotationFrame::keyReleaseEvent(QKeyEvent* event) {
    event->accept();
}

bool AnnotationFrame::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget()) {
        if (event->type() == QEvent::Paint || event->type() == QEvent::Resize) {
            schedulePlacement();
        }
        return false;
    }
    if (watched != m_textEdit && watched != m_saveButton) {
        return false;
    }

    switch (event->type()) {
    case QEvent::ShortcutOverride: {
        // The keys stay in the frame: no shortcut of the window but saving, closing and
        // quitting
        auto* key = static_cast<QKeyEvent*>(event);
        if (reachesWindow(key)) {
            return false;
        }
        key->accept();
        return true;
    }
    case QEvent::KeyPress: {
        auto* key = static_cast<QKeyEvent*>(event);
        if (isCancelKey(key)) {
            emit cancelRequested();
            return true;
        }
        if (isSaveKey(key) ||
            (watched == m_saveButton && key->modifiers() == Qt::NoModifier &&
             (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter))) {
            requestSave();
            return true;
        }
        return false;
    }
    default:
        return false;
    }
}

bool AnnotationFrame::focusNextPrevChild(bool next) {
    // Tab and Shift+Tab go between the text and the button, never out of the frame
    const Qt::FocusReason reason = next ? Qt::TabFocusReason : Qt::BacktabFocusReason;
    if (m_textEdit->hasFocus() && m_saveButton->isEnabled()) {
        m_saveButton->setFocus(reason);
    } else {
        m_textEdit->setFocus(reason);
    }
    return true;
}

void AnnotationFrame::updateTextHeight() {
    const QFontMetricsF metrics(m_textEdit->font());
    const qreal margins = 2.0 * m_textEdit->document()->documentMargin();
    const int frameHeight = 2 * m_textEdit->frameWidth();
    const auto heightOf = [&](qreal content) {
        return static_cast<int>(std::ceil(content + margins)) + frameHeight;
    };
    const int textHeight = heightOf(m_textEdit->document()->size().height() - margins);
    m_textEdit->setFixedHeight(std::clamp(textHeight, heightOf(metrics.lineSpacing() * MIN_LINES),
                                          heightOf(metrics.lineSpacing() * MAX_LINES)));
}

void AnnotationFrame::updateStyle() {
    if (!m_colors.text.isValid()) {
        return;
    }

    // The colors are set in a style sheet: they stay through every polish of the editor's
    // style
    const QColor fieldBorder = mixedColor(m_colors.background, m_colors.secondary, 0.5);
    const QColor hover = mixedColor(m_colors.background, m_colors.kind, 0.2);
    const QColor disabled = mixedColor(m_colors.background, m_colors.secondary, 0.6);
    setStyleSheet(QStringLiteral("QLabel#annotationFrameKind { color: %1; }"
                                 "QLabel#annotationFrameHint { color: %2; }"
                                 "QTextEdit { color: %3; background: transparent;"
                                 " border: 1px solid %4; border-radius: 4px; }"
                                 "QTextEdit:focus { border: 1px solid %1; }"
                                 "QPushButton { color: %3; background: transparent;"
                                 " border: 1px solid %1; border-radius: 4px; padding: 3px 14px; }"
                                 "QPushButton:hover { background: %5; }"
                                 "QPushButton:focus { border: 2px solid %1; }"
                                 "QPushButton:disabled { color: %6; border-color: %6; }")
                      .arg(m_colors.kindName.name(), m_colors.secondary.name(),
                           m_colors.text.name(), fieldBorder.name(), hover.name(),
                           disabled.name()));

    // The placeholder is drawn in the palette's color
    QPalette textPalette = m_textEdit->palette();
    textPalette.setColor(QPalette::PlaceholderText, m_colors.secondary);
    m_textEdit->setPalette(textPalette);
    updateTextHeight();
    update();
}

void AnnotationFrame::schedulePlacement() {
    if (!m_placementPending && m_placement) {
        m_placementPending = true;
        QTimer::singleShot(0, this, &AnnotationFrame::place);
    }
}

}  // namespace kalahari::gui
