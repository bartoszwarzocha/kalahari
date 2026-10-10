/// @file annotation_frame.cpp
/// @brief The frame an annotation's text is written in, at its place in the text

#include "kalahari/gui/panels/annotation_frame.h"
#include "kalahari/gui/panels/annotation_card.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

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

/// @brief Tells of every mouse press of the program before it goes on, the second press of
/// a double click too (installed on the application)
class PressWatcher : public QObject {
public:
    using Handler = std::function<void(QObject* receiver)>;

    PressWatcher(Handler handler, QObject* parent)
        : QObject(parent)
        , m_handler(std::move(handler)) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseButtonDblClick) {
            m_handler(watched);
        }
        return false;
    }

private:
    Handler m_handler;
};

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

    // Header: the kind, and who the annotation is by on the right
    auto* header = new QHBoxLayout();
    header->setSpacing(8);
    QFont kindFont = font();
    kindFont.setPointSizeF(kindFont.pointSizeF() * 0.9);
    kindFont.setBold(true);
    m_kindLabel = new QLabel(this);
    m_kindLabel->setObjectName(QStringLiteral("annotationFrameKind"));
    m_kindLabel->setFont(kindFont);
    header->addWidget(m_kindLabel);

    // Its room stays when no one is shown: the X stays in the corner
    m_authorLabel = new QLabel(this);
    m_authorLabel->setObjectName(QStringLiteral("annotationFrameAuthor"));
    m_authorLabel->setFont(kindFont);
    m_authorLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QSizePolicy authorPolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    authorPolicy.setRetainSizeWhenHidden(true);
    m_authorLabel->setSizePolicy(authorPolicy);
    m_authorLabel->hide();
    header->addWidget(m_authorLabel, 1);

    // The X closes the frame for the mouse, the text kept; the keys have Esc and Ctrl+Enter
    m_closeButton = new QToolButton(this);
    m_closeButton->setObjectName(QStringLiteral("annotationFrameClose"));
    m_closeButton->setText(QString(QChar(0x00D7)));  // the multiplication sign: an X
    QFont closeFont = font();
    closeFont.setPointSizeF(closeFont.pointSizeF() * 1.2);
    m_closeButton->setFont(closeFont);
    m_closeButton->setToolTip(tr("Close (the text is kept)"));
    m_closeButton->setAccessibleName(tr("Close"));
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    m_closeButton->setAutoRaise(true);
    connect(m_closeButton, &QToolButton::clicked, this, &AnnotationFrame::requestClose);
    header->addWidget(m_closeButton);
    layout->addLayout(header);

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

    // While it is shown, a click anywhere else in the program closes it
    m_pressWatcher = new PressWatcher([this](QObject* receiver) { onPress(receiver); }, this);
}

AnnotationFrame::~AnnotationFrame() {
    stopWatching();  // nothing of the program reaches a frame being destroyed
}

QString AnnotationFrame::saveKeysText() {
    return QKeySequence(Qt::CTRL | Qt::Key_Enter).toString(QKeySequence::NativeText);
}

void AnnotationFrame::setKind(editor::AnnotationKind kind) {
    const QString title = AnnotationCard::kindTitle(kind);
    m_kindLabel->setText(title);
    m_textEdit->setAccessibleName(title);
}

void AnnotationFrame::setAuthor(const QString& author) {
    m_author = author.trimmed();
    m_authorLabel->setVisible(!m_author.isEmpty());
    updateAuthorLabel();
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

void AnnotationFrame::requestCancel() {
    m_closing = true;  // what was written is not kept afterwards
    emit cancelRequested();
}

void AnnotationFrame::requestClose() {
    if (!m_closing) {
        m_closing = true;
        emit closeRequested();
    }
}

void AnnotationFrame::startWatching() {
    if (m_watching) {
        return;
    }
    m_watching = true;
    qApp->installEventFilter(m_pressWatcher);
    connect(qApp, &QApplication::focusChanged, this, &AnnotationFrame::onFocusChanged);
}

void AnnotationFrame::stopWatching() {
    if (!m_watching) {
        return;
    }
    m_watching = false;
    qApp->removeEventFilter(m_pressWatcher);
    disconnect(qApp, &QApplication::focusChanged, this, &AnnotationFrame::onFocusChanged);
}

void AnnotationFrame::onPress(QObject* receiver) {
    if (!receiver->isWidgetType()) {
        return;
    }
    const auto* widget = static_cast<const QWidget*>(receiver);

    // Closed before the click goes on: the click acts on what the frame kept
    if (leavesFrame(widget)) {
        requestClose();
        return;
    }

    // Qt gives the keys to the first widget under a click that takes them, before the click
    // arrives: a click on the editor's scroll bars gives them to the editor. They come back
    // to where they were in the frame (also while the window is not in front yet)
    if (!isFrameWidget(widget) && !isFrameWidget(window()->focusWidget())) {
        QWidget* keys = focusWidget();
        if (keys == nullptr || !isAncestorOf(keys)) {
            keys = m_textEdit;
        }
        keys->setFocus(Qt::OtherFocusReason);
    }
}

void AnnotationFrame::onFocusChanged(QWidget* old, QWidget* now) {
    if (isFrameWidget(now)) {
        m_keysAway = false;
        m_keysLeftByMouse = false;
        return;
    }
    if (old != nullptr ? !isFrameWidget(old) : !m_keysAway) {
        return;  // the keys were not in the frame
    }
    const bool byMouse = std::exchange(m_keysLeftByMouse, false);

    // No window of the program has the keys: another application is in front. The frame
    // stays open; the keys come back to it with the program's window
    if (now == nullptr) {
        m_keysAway = true;
        return;
    }
    m_keysAway = false;

    // Taken by a click before it arrives: the click decides (see onPress). The wheel takes
    // them to a field it turns, with no click to decide: looked at once the event is over
    if (byMouse) {
        QTimer::singleShot(0, this, &AnnotationFrame::checkKeys);
        return;
    }
    if (leavesFrame(now)) {
        requestClose();
    }
}

void AnnotationFrame::checkKeys() {
    const QWidget* keys = QApplication::focusWidget();
    if (m_watching && keys != nullptr && leavesFrame(keys)) {
        requestClose();
    }
}

bool AnnotationFrame::isFrameWidget(const QWidget* widget) const {
    return widget != nullptr && (widget == this || isAncestorOf(widget));
}

bool AnnotationFrame::leavesFrame(const QWidget* widget) const {
    if (isFrameWidget(widget)) {
        return false;
    }

    // A menu or a list over everything (the frame's own context menu among them) goes and
    // gives the keys back; a tooltip goes at a click
    const Qt::WindowType type = widget->window()->windowType();
    if (type == Qt::Popup || type == Qt::ToolTip) {
        return false;
    }

    // The editor's scroll bars only move the view: the frame goes along with its place
    const QWidget* editorWidget = parentWidget();
    return qobject_cast<const QScrollBar*>(widget) == nullptr || editorWidget == nullptr ||
           !editorWidget->isAncestorOf(widget);
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

void AnnotationFrame::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    updateAuthorLabel();
}

void AnnotationFrame::keyPressEvent(QKeyEvent* event) {
    if (isCancelKey(event)) {
        requestCancel();
    } else if (isSaveKey(event)) {
        requestSave();
    }
    event->accept();  // no key goes on to the editor under the frame
}

void AnnotationFrame::keyReleaseEvent(QKeyEvent* event) {
    event->accept();
}

void AnnotationFrame::showEvent(QShowEvent* event) {
    QFrame::showEvent(event);
    startWatching();
}

void AnnotationFrame::hideEvent(QHideEvent* event) {
    stopWatching();
    QFrame::hideEvent(event);
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
    case QEvent::FocusOut:
        // A click or the wheel taking the keys elsewhere (see onFocusChanged)
        m_keysLeftByMouse = static_cast<QFocusEvent*>(event)->reason() == Qt::MouseFocusReason;
        return false;
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
            requestCancel();
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

void AnnotationFrame::updateAuthorLabel() {
    m_authorLabel->setText(m_authorLabel->fontMetrics().elidedText(m_author, Qt::ElideRight,
                                                                   m_authorLabel->width()));
    m_authorLabel->setToolTip(m_authorLabel->text() != m_author ? m_author : QString());
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
                                 "QLabel#annotationFrameAuthor { color: %3; }"
                                 "QTextEdit { color: %3; background: transparent;"
                                 " border: 1px solid %4; border-radius: 4px; }"
                                 "QTextEdit:focus { border: 1px solid %1; }"
                                 "QPushButton { color: %3; background: transparent;"
                                 " border: 1px solid %1; border-radius: 4px; padding: 3px 14px; }"
                                 "QPushButton:hover { background: %5; }"
                                 "QPushButton:focus { border: 2px solid %1; }"
                                 "QPushButton:disabled { color: %6; border-color: %6; }"
                                 "QToolButton#annotationFrameClose { color: %2; background:"
                                 " transparent; border: none; border-radius: 4px;"
                                 " padding: 0px 4px; }"
                                 "QToolButton#annotationFrameClose:hover { color: %3;"
                                 " background: %5; }")
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
