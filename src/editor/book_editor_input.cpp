/// @file book_editor_input.cpp
/// @brief BookEditor: keyboard, input method, mouse, drag and drop and context menu events

#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/buffer_commands.h>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QHash>
#include <QScopedValueRollback>
#include <QSet>
#include <QTimer>
#include <algorithm>
#include <optional>
#include <tuple>
#include <utility>

namespace kalahari::editor {

// Automatic scrolling while selecting with the mouse or dragging text: one step per
// interval, longer the further the mouse is past the edge (or into the edge band, for
// dragged text)
constexpr int AUTO_SCROLL_INTERVAL = 25;           // ms
constexpr qreal AUTO_SCROLL_DROP_BAND = 24.0;      // px at the top and bottom edges
constexpr qreal AUTO_SCROLL_MIN_STEP = 2.0;        // px
constexpr qreal AUTO_SCROLL_MAX_STEP = 60.0;       // px

namespace {

/// @brief Whether a key types: a character (also with AltGr), Backspace or Delete. A
///        character as keyPressEvent() types it (typesText())
bool isTypingKey(const QKeyEvent* event) {
    if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
        return true;
    }
    return typesText(event->key(), event->modifiers(), isMacOS()) && !event->text().isEmpty() &&
           event->text().at(0).isPrint();
}

/// @brief Watches a context menu for the keys of a command: they close the menu, and the
///        command is to run once the menu is gone (pressed())
class MenuCommandKeys : public QObject {
public:
    explicit MenuCommandKeys(const QAction* command) : m_command(command) {}

    bool pressed() const { return m_pressed; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        const QEvent::Type type = event->type();
        if ((type == QEvent::ShortcutOverride || type == QEvent::KeyPress) &&
            isCommandKey(*static_cast<QKeyEvent*>(event))) {
            if (type == QEvent::KeyPress) {
                m_pressed = true;
                if (auto* menu = qobject_cast<QWidget*>(watched)) {
                    menu->close();
                }
            }
            event->accept();  // the menu takes the keys before any shortcut
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    bool isCommandKey(const QKeyEvent& event) const {
        if (m_command == nullptr || !m_command->isEnabled()) {
            return false;
        }
        const QList<QKeySequence> shortcuts = m_command->shortcuts();
        return shortcuts.contains(QKeySequence(event.keyCombination()));
    }

    const QAction* m_command;
    bool m_pressed = false;
};

/// @brief The access key of a menu text in upper case: the character after a single '&'
///        ("&&" is an ampersand); a null QChar for a text without one
QChar accessKeyOf(const QString& text) {
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&')) {
            if (text.at(i + 1) != QLatin1Char('&')) {
                return text.at(i + 1).toUpper();
            }
            ++i;
        }
    }
    return {};
}

/// @brief A menu text that shows @p shown (its ampersands doubled), with the access key on
///        its character at @p place
QString withAccessKeyAt(const QString& shown, qsizetype place) {
    QString text;
    for (qsizetype i = 0; i < shown.size(); ++i) {
        if (i == place) {
            text += QLatin1Char('&');
        }
        text += shown.at(i);
        if (shown.at(i) == QLatin1Char('&')) {
            text += QLatin1Char('&');
        }
    }
    return text;
}

/// @brief An item of a context menu that can get another access key
struct MenuKeyItem {
    QAction* action = nullptr;
    QString shown;       ///< Its text as the menu shows it, without the keys after a tab
    QString keys;        ///< The keys after a tab, with the tab
    QList<QChar> keyed;  ///< The letters and digits of the text it can have as its key
    QChar key;           ///< Its key, in upper case
};

/// @brief The keys of the items of a context menu: each its own when it can
class MenuKeys {
public:
    /// @param fixed The keys that stay, of the commands of the window
    /// @param items The items of the menu's own, with their keys
    MenuKeys(QSet<QChar> fixed, QList<MenuKeyItem>& items)
        : m_fixed(std::move(fixed)), m_items(items) {}

    /// @brief Each item keeps its key unless a command or an item above has it; then it takes
    ///        another letter of its text (the first of a word first), and when none is free, an
    ///        item that has one takes another of its own. An item without any keeps its key
    void assign() {
        QList<int> moving;
        for (int i = 0; i < m_items.size(); ++i) {
            const QChar key = m_items.at(i).key;
            if (m_fixed.contains(key) || m_holders.contains(key)) {
                moving.append(i);
            } else {
                m_holders.insert(key, i);
            }
        }
        for (const int i : std::as_const(moving)) {
            QSet<QChar> tried;
            place(i, tried);
        }
    }

private:
    /// @brief Give item @p index a key no other item has, moving the items that have the
    ///        letters it can have to others of theirs; @p tried: the letters asked for so far
    bool place(int index, QSet<QChar>& tried) {
        const QList<QChar>& letters = m_items.at(index).keyed;
        for (const QChar letter : letters) {
            if (!m_fixed.contains(letter) && !m_holders.contains(letter)) {
                take(index, letter);
                return true;
            }
        }
        for (const QChar letter : letters) {
            if (m_fixed.contains(letter) || tried.contains(letter)) {
                continue;
            }
            tried.insert(letter);
            const int holder = m_holders.value(letter, -1);
            if (holder != index && holder >= 0 && place(holder, tried)) {
                take(index, letter);
                return true;
            }
        }
        return false;
    }

    /// @brief Item @p index has @p letter as its key from now on
    void take(int index, QChar letter) {
        MenuKeyItem& item = m_items[index];
        if (m_holders.value(item.key, -1) == index) {
            m_holders.remove(item.key);
        }
        item.key = letter;
        m_holders.insert(letter, index);
    }

    QSet<QChar> m_fixed;
    QList<MenuKeyItem>& m_items;
    QHash<QChar, int> m_holders;  ///< The item with each key
};

/// @brief Give each item of a context menu put together from its parts (those of a word, of
///        a link, of an annotation, of a list item, the editing commands, the linked
///        materials) an access key no other item has, in English and in any translation
///
/// The commands of the window keep their keys: they are shared with its menus. The items of
/// the menu's own keep theirs where they can (MenuKeys::assign()). Each part has its own keys
/// unique; only parts shown together can share one.
void giveUniqueAccessKeys(QMenu& menu) {
    QSet<QChar> fixed;
    QList<MenuKeyItem> items;
    for (QAction* action : menu.actions()) {
        if (action->isSeparator()) {
            continue;
        }
        const QString text = action->text();
        const QChar key = accessKeyOf(text);
        if (key.isNull()) {
            continue;  // a line that only tells something, or a word the menu gives a key
        }
        const QMenu* submenu = action->menu();
        const QObject* owner = submenu != nullptr ? submenu->parent() : action->parent();
        if (owner != &menu) {
            fixed.insert(key);
            continue;
        }
        MenuKeyItem item;
        item.action = action;
        item.key = key;
        const qsizetype tab = text.indexOf(QLatin1Char('\t'));
        const QString label = tab < 0 ? text : text.left(tab);
        item.keys = tab < 0 ? QString() : text.mid(tab);
        for (qsizetype i = 0; i < label.size(); ++i) {
            if (label.at(i) == QLatin1Char('&') && i + 1 < label.size()) {
                ++i;  // "&x" is the mark of x, "&&" an ampersand
            }
            item.shown += label.at(i);
        }
        // The first letters of the words first, then the others
        for (const bool wordStarts : {true, false}) {
            for (qsizetype i = 0; i < item.shown.size(); ++i) {
                const QChar ch = item.shown.at(i);
                const bool wordStart = i == 0 || !item.shown.at(i - 1).isLetterOrNumber();
                if (wordStart == wordStarts && ch.unicode() < 0x80 && ch.isLetterOrNumber() &&
                    !item.keyed.contains(ch.toUpper())) {
                    item.keyed.append(ch.toUpper());
                }
            }
        }
        items.append(item);
    }

    QList<QChar> before;
    before.reserve(items.size());
    for (const MenuKeyItem& item : std::as_const(items)) {
        before.append(item.key);
    }
    MenuKeys(std::move(fixed), items).assign();

    // The items with another key: on its letter, the first of a word first
    for (qsizetype i = 0; i < items.size(); ++i) {
        const MenuKeyItem& item = items.at(i);
        if (item.key == before.at(i)) {
            continue;
        }
        qsizetype place = -1;
        for (const bool wordStarts : {true, false}) {
            for (qsizetype at = 0; at < item.shown.size() && place < 0; ++at) {
                const bool wordStart = at == 0 || !item.shown.at(at - 1).isLetterOrNumber();
                if (wordStart == wordStarts && item.shown.at(at).toUpper() == item.key) {
                    place = at;
                }
            }
        }
        item.action->setText(withAccessKeyAt(item.shown, place) + item.keys);
    }
}

}  // namespace

void BookEditor::keyPressEvent(QKeyEvent* event)
{
    // Ctrl pressed over a link: the hand of Ctrl+click at once, as the mouse stays
    if (event->key() == Qt::Key_Control && underMouse()) {
        updatePointerShape(mapFromGlobal(QCursor::pos()), event->modifiers());
    }

    // The word being typed gets no spelling wave until the cursor leaves it
    const QScopedValueRollback<bool> typing(m_spellTypingEdit, isTypingKey(event));

    // The keys of the text, those of the system the program runs on
    bool handled = false;
    const TextKey textKey = textKeyFor(event->key(), event->modifiers(), isMacOS());
    if (textKey.action != TextKeyAction::None) {
        performTextKey(textKey.action, textKey.extendSelection);
        handled = true;
    }

    // Type the character of the key (if not already handled), unless its modifiers give a
    // command or make it the access key of a menu item (typesText())
    if (!handled && typesText(event->key(), event->modifiers(), isMacOS()) &&
        !event->text().isEmpty()) {
        QString text = event->text();
        // Only handle printable characters
        if (!text.isEmpty() && text.at(0).isPrint()) {
            // Caps Lock: a letter typed small with Shift, or a capital without it, shows
            // it on (autocorrect changes a word typed with it on by mistake)
            if (const QChar first = text.at(0);
                text.size() == 1 && first.isUpper() != first.isLower()) {
                m_capsLockOn = first.isUpper() != event->modifiers().testFlag(Qt::ShiftModifier);
            }
            typeText(text);
            handled = true;
        }
    }

    if (handled) {
        if (m_spellTypingEdit) {
            noteSpellingTyping();
        }
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void BookEditor::keyReleaseEvent(QKeyEvent* event)
{
    // Ctrl let go over a link: the I-beam again
    if (event->key() == Qt::Key_Control && underMouse()) {
        updatePointerShape(mapFromGlobal(QCursor::pos()), event->modifiers());
    }
    QWidget::keyReleaseEvent(event);
}

void BookEditor::inputMethodEvent(QInputMethodEvent* event)
{
    if (!m_textBuffer || m_readOnly) {
        event->ignore();
        return;
    }

    // Composed text is typed too (see keyPressEvent())
    const QScopedValueRollback<bool> typing(
        m_spellTypingEdit, !event->commitString().isEmpty() || !event->preeditString().isEmpty() ||
                               m_hasComposition);

    // Phase 11: Use QTextCursor for IME operations
    if (!m_textBuffer) return;

    // Handle committed text (final input)
    const QString& commitString = event->commitString();
    if (!commitString.isEmpty()) {
        // If we had composition, it's been replaced by commit
        if (m_hasComposition) {
            // The preedit text is already in the document, delete it first
            if (!m_preeditString.isEmpty()) {
                QTextBlock block = m_textBuffer->findBlockByNumber(m_preeditStart.paragraph);
                if (block.isValid()) {
                    QTextCursor cursor(m_textBuffer.get());
                    int startPos = block.position() + m_preeditStart.offset;
                    cursor.setPosition(startPos);
                    cursor.setPosition(startPos + m_preeditString.length(), QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                }
                setCursorPosition(m_preeditStart);
            }
            m_preeditString.clear();
            m_hasComposition = false;
        }

        // Insert committed text (this handles selection deletion too)
        typeText(commitString);
    }

    // Handle preedit text (composition in progress)
    const QString& preeditString = event->preeditString();
    if (m_hasComposition) {
        // Remove old preedit text
        if (!m_preeditString.isEmpty()) {
            QTextBlock block = m_textBuffer->findBlockByNumber(m_preeditStart.paragraph);
            if (block.isValid()) {
                QTextCursor cursor(m_textBuffer.get());
                int startPos = block.position() + m_preeditStart.offset;
                cursor.setPosition(startPos);
                cursor.setPosition(startPos + m_preeditString.length(), QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
            setCursorPosition(m_preeditStart);
        }
    }

    if (!preeditString.isEmpty()) {
        // Store composition state
        m_preeditStart = m_cursorPosition;
        m_preeditString = preeditString;
        m_hasComposition = true;

        // Insert preedit text using QTextCursor, with the format typed text gets
        QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
        if (block.isValid()) {
            QTextCursor cursor(m_textBuffer.get());
            cursor.setPosition(block.position() + m_cursorPosition.offset);
            cursor.insertText(preeditString, typingFormat());
        }

        // Move cursor to end of preedit
        CursorPosition newPos = m_cursorPosition;
        newPos.offset += preeditString.length();
        setCursorPosition(newPos);
    } else {
        // No preedit, clear composition state
        m_preeditString.clear();
        m_hasComposition = false;
    }

    if (m_spellTypingEdit) {
        noteSpellingTyping();
    }
    event->accept();
    update();
}

QVariant BookEditor::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
        case Qt::ImEnabled:
            return !m_readOnly;

        case Qt::ImCursorRectangle: {
            // Cursor rectangle for IME positioning, from the pipeline that paints the cursor
            const QRectF rect = m_renderPipeline ? m_renderPipeline->cursorRect() : QRectF();
            if (rect.isEmpty()) {
                // Default position at top-left with some offset
                return QRectF(10, 10, 2, 20);
            }
            return rect;
        }

        case Qt::ImFont:
            return font();

        case Qt::ImCursorPosition:
            return m_cursorPosition.offset;

        case Qt::ImSurroundingText: {
            // Phase 11: Return text of current paragraph using QTextBlock
            if (m_textBuffer) {
                QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
                if (block.isValid()) {
                    return block.text();
                }
            }
            return QString();
        }

        case Qt::ImCurrentSelection: {
            if (hasSelection()) {
                return selectedText();
            }
            return QString();
        }

        case Qt::ImAnchorPosition:
            return m_selectionAnchor.offset;

        case Qt::ImHints:
            return static_cast<int>(Qt::ImhMultiLine);

        default:
            break;
    }

    return QWidget::inputMethodQuery(query);
}

// =============================================================================
// Mouse Event Handlers (Phase 3.9/3.10/3.11)
// =============================================================================

void BookEditor::mousePressEvent(QMouseEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // A click on an annotation's card does what its part does; one on an annotation's mark
    // collapses or expands its card, or opens the annotation without cards; one on the mark
    // beside a paragraph chooses a material. The cursor stays.
    if (!(event->modifiers() & Qt::ShiftModifier)) {
        if (pressAnnotationCard(event->position())) {
            event->accept();
            return;
        }
        if (const std::optional<ParagraphMark> marked = paragraphMarkAt(event->position())) {
            openParagraphMark(*marked, event->globalPosition().toPoint());
            event->accept();
            return;
        }
        const std::optional<AnnotationMark> mark =
            m_renderPipeline ? m_renderPipeline->annotationMarkDetailsAt(event->position())
                             : std::nullopt;
        if (mark) {
            if (m_annotationCards.hasCard(mark->kind)) {
                setAnnotationCollapsed(mark->id, !m_annotationCards.isCollapsed(mark->id));
            } else {
                emit annotationMarkClicked(mark->id);
            }
            event->accept();
            return;
        }
    }

    // Ctrl+click on a link (Cmd+click on macOS) opens it, as in Word; the cursor stays
    if ((event->modifiers() & Qt::ControlModifier) && !(event->modifiers() & Qt::ShiftModifier)) {
        if (const std::optional<TextLink> link = linkAt(event->position())) {
            emit linkOpenAsked(link->address);
            event->accept();
            return;
        }
    }

    // Set focus on click
    setFocus();

    QPointF clickPos = event->position();

    // Check for multi-click (double/triple)
    bool isMultiClick = false;
    if (m_clickTimer != nullptr && m_clickTimer->isActive()) {
        // Check distance from last click
        qreal distance = (clickPos - m_lastClickPos).manhattanLength();
        if (distance <= MULTI_CLICK_DISTANCE) {
            ++m_clickCount;
            isMultiClick = true;
        } else {
            m_clickCount = 1;
        }
    } else {
        m_clickCount = 1;
    }

    m_lastClickPos = clickPos;

    // Start/restart click timer
    if (m_clickTimer == nullptr) {
        m_clickTimer = new QTimer(this);
        m_clickTimer->setSingleShot(true);
        connect(m_clickTimer, &QTimer::timeout, this, [this]() {
            m_clickCount = 0;
        });
    }
    m_clickTimer->start(MULTI_CLICK_INTERVAL);

    // Convert click to cursor position
    CursorPosition clickPosition = positionFromPoint(clickPos);

    if (m_clickCount == 3) {
        // Triple click - select paragraph
        m_cursorPosition = clickPosition;
        selectParagraphAtCursor();
    } else if (m_clickCount == 2 || isMultiClick) {
        // Double click - select word (handled in mouseDoubleClickEvent)
        // But we still need to set position for the case when double-click
        // is detected through our click counting
        m_cursorPosition = clickPosition;
        selectWordAtCursor();
    } else if (!(event->modifiers() & Qt::ShiftModifier) && isOverSelectedText(clickPos)) {
        // A press on the selected text drags it once the mouse moves far enough; released
        // without moving, it places the cursor (mouseReleaseEvent())
        m_textDragPending = true;
        m_textDragStartPos = clickPos;
    } else {
        // Single click - position cursor
        if (event->modifiers() & Qt::ShiftModifier) {
            // Shift+click extends selection
            extendSelection(clickPosition);
            update();
        } else {
            // Normal click clears selection and positions cursor
            clearSelection();
            m_selectionAnchor = clickPosition;
            setCursorPosition(clickPosition);
        }

        // Start drag selection
        m_isDragging = true;
    }

    event->accept();
}

void BookEditor::mouseMoveEvent(QMouseEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    // In Distraction-Free, the texts at the edges come back when the mouse nears an edge
    if (m_distractionFree) {
        // Check if mouse is near edges for fade trigger
        QPointF pos = event->position();
        qreal edgeThreshold = 50.0;  // Pixels from edge
        bool nearEdge = (pos.y() < edgeThreshold ||
                         pos.y() > height() - edgeThreshold ||
                         pos.x() < edgeThreshold ||
                         pos.x() > width() - edgeThreshold);

        if (nearEdge && m_appearance.distractionFree.fadeOnMouseMove) {
            // With mouse tracking this runs on every move: repaint only to show faded UI
            const bool wasFaded = m_uiOpacity < 1.0;
            m_uiOpacity = 1.0;
            startUiFade();
            if (wasFaded) {
                update();
            }
        }
    }

    const QPointF pos = event->position();
    if (!(event->buttons() & Qt::LeftButton)) {
        updatePointerShape(pos, event->modifiers());
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (m_textDragPending) {
        if ((pos - m_textDragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_textDragPending = false;
            startTextDrag();
        }
        event->accept();
        return;
    }

    if (!m_isDragging) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // The selection follows the mouse; past the top or bottom edge the view scrolls
    extendMouseSelection(pos);
    ensureCursorVisible();
    updateAutoScroll(pos, false);

    event->accept();
}

void BookEditor::updatePointerShape(const QPointF& pos, Qt::KeyboardModifiers modifiers)
{
    // A hand over an annotation's mark, which opens it, over the mark beside a paragraph, over
    // the buttons of a card and with Ctrl over a link, which Ctrl+click opens; an arrow over a
    // card and over the selected text, which can be dragged; an I-beam elsewhere
    Qt::CursorShape shape = Qt::IBeamCursor;
    if (hoverAnnotationCard(pos)) {
        shape = m_annotationCards.hoveredPart() == AnnotationCardPart::Card
                    ? Qt::ArrowCursor
                    : Qt::PointingHandCursor;
    } else if (!annotationMarkAt(pos).isEmpty() || paragraphMarkAt(pos) ||
               ((modifiers & Qt::ControlModifier) && linkAt(pos).has_value())) {
        shape = Qt::PointingHandCursor;
    } else if (isOverSelectedText(pos)) {
        shape = Qt::ArrowCursor;
    }
    if (cursor().shape() != shape) {
        setCursor(shape);
    }
}

void BookEditor::mouseReleaseEvent(QMouseEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    stopAutoScroll();
    m_isDragging = false;
    if (m_textDragPending) {
        // A click on the selected text without dragging places the cursor there
        m_textDragPending = false;
        clearSelection();
        const CursorPosition position = positionFromPoint(event->position());
        m_selectionAnchor = position;
        setCursorPosition(position);
    }
    event->accept();
}

void BookEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    // A double click on a card edits its annotation; on its buttons it does no more than
    // the first click
    if (const std::optional<AnnotationCardHit> card = annotationCardAt(event->position())) {
        if (card->part == AnnotationCardPart::Card && card->id == m_pressedCard) {
            emit annotationEditRequested(card->id);
        }
        m_pressedCard.clear();
        event->accept();
        return;
    }

    // Position cursor at double-click location
    CursorPosition clickPosition = positionFromPoint(event->position());
    m_cursorPosition = clickPosition;

    // Select word at cursor
    selectWordAtCursor();

    // Update click count for triple-click detection
    m_clickCount = 2;
    m_lastClickPos = event->position();
    if (m_clickTimer != nullptr) {
        m_clickTimer->start(MULTI_CLICK_INTERVAL);
    }

    event->accept();
}

// =============================================================================
// Drag and drop of text (stage 3)
// =============================================================================

void BookEditor::dragEnterEvent(QDragEnterEvent* event)
{
    if (!m_textBuffer || m_readOnly || !canInsertFromMimeData(event->mimeData())) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
}

void BookEditor::dragMoveEvent(QDragMoveEvent* event)
{
    if (!m_textBuffer || m_readOnly || !canInsertFromMimeData(event->mimeData())) {
        event->ignore();
        return;
    }

    const QPointF pos = event->position();
    updateAutoScroll(pos, true);

    // The selected text is not moved onto itself
    const CursorPosition position = positionFromPoint(pos);
    if (event->source() == this && event->proposedAction() == Qt::MoveAction &&
        isInSelection(position)) {
        m_renderPipeline->setDropCaret(std::nullopt);
        event->ignore();
        return;
    }

    m_renderPipeline->setDropCaret(position);
    event->acceptProposedAction();
}

void BookEditor::dragLeaveEvent(QDragLeaveEvent* event)
{
    stopAutoScroll();
    m_renderPipeline->setDropCaret(std::nullopt);
    event->accept();
}

void BookEditor::dropEvent(QDropEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    stopAutoScroll();
    m_renderPipeline->setDropCaret(std::nullopt);

    const bool moveSelection = event->source() == this && event->dropAction() == Qt::MoveAction;
    if (!dropMimeData(event->mimeData(), positionFromPoint(event->position()), moveSelection)) {
        event->ignore();
        return;
    }
    setFocus();
    event->accept();
}

bool BookEditor::isOverSelectedText(const QPointF& widgetPos) const
{
    if (!hasSelection() || !m_textBuffer) {
        return false;
    }

    // The character under the point: the layout's exact hit, which misses the space around
    // the text and the gaps between pages
    const QPointF docPoint = m_renderPipeline->widgetToDocument(widgetPos);
    const int hit = m_textBuffer->documentLayout()->hitTest(docPoint, Qt::ExactHit);
    if (hit < 0) {
        return false;
    }
    const QTextBlock block = m_textBuffer->findBlock(hit);
    const CursorPosition position{block.blockNumber(), hit - block.position()};

    const SelectionRange sel = m_selection.normalized();
    return sel.start <= position && position < sel.end;
}

bool BookEditor::isInSelection(const CursorPosition& position) const
{
    const SelectionRange sel = m_selection.normalized();
    return hasSelection() && sel.start <= position && position <= sel.end;
}

void BookEditor::startTextDrag()
{
    std::unique_ptr<QMimeData> mimeData = createMimeDataFromSelection();
    if (!mimeData) {
        return;
    }

    // Stays at the dragged text if the drop target edits the document before it
    const SelectionRange sel = m_selection.normalized();
    QTextCursor dragged = createCursor(m_textBuffer.get(), sel.start, sel.end);

    auto* drag = new QDrag(this);  // Qt deletes it once the drag is over
    drag->setMimeData(mimeData.release());
    m_draggingText = true;
    // A read-only text is only copied
    const Qt::DropAction action =
        m_readOnly ? drag->exec(Qt::CopyAction, Qt::CopyAction)
                   : drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::MoveAction);
    m_draggingText = false;
    stopAutoScroll();

    // Moved to another widget or program: the text leaves the editor (dropMimeData()
    // moves it within the editor). The editor's own find bar only takes a copy.
    const auto* target = qobject_cast<QWidget*>(drag->target());
    const bool movedAway = !m_readOnly && action == Qt::MoveAction && target != this &&
                           (target == nullptr || !isAncestorOf(target));
    if (movedAway && dragged.hasSelection()) {
        clearSelection();
        dragged.removeSelectedText();
        finishEdit(dragged);
    }
}

void BookEditor::extendMouseSelection(const QPointF& widgetPos)
{
    // Past the top or bottom edge, the selection ends in the first or last visible line;
    // automatic scrolling brings the next lines in
    const double y = std::clamp(widgetPos.y(), 0.0, std::max(0.0, height() - 1.0));
    const CursorPosition position = positionFromPoint(QPointF(widgetPos.x(), y));

    m_cursorPosition = position;
    setSelection({m_selectionAnchor, position});
    syncPipelineCursor();
    update();
}

void BookEditor::updateAutoScroll(const QPointF& widgetPos, bool forDrop)
{
    m_autoScrollPos = widgetPos;
    m_autoScrollForDrop = forDrop;
    if (autoScrollStep() == 0.0) {
        stopAutoScroll();
        return;
    }

    if (m_autoScrollTimer == nullptr) {
        m_autoScrollTimer = new QTimer(this);
        m_autoScrollTimer->setInterval(AUTO_SCROLL_INTERVAL);
        connect(m_autoScrollTimer, &QTimer::timeout, this, &BookEditor::onAutoScrollTimeout);
    }
    if (!m_autoScrollTimer->isActive()) {
        m_autoScrollTimer->start();
    }
}

void BookEditor::stopAutoScroll()
{
    if (m_autoScrollTimer != nullptr) {
        m_autoScrollTimer->stop();
    }
}

double BookEditor::autoScrollStep() const
{
    // Dragged text scrolls the view in a band along the top and bottom edges, a mouse
    // selection past them
    const double band = m_autoScrollForDrop ? AUTO_SCROLL_DROP_BAND : 0.0;
    const double y = m_autoScrollPos.y();
    double distance = 0.0;
    if (y < band) {
        distance = y - band;
    } else if (y > height() - band) {
        distance = y - (height() - band);
    }
    if (distance == 0.0) {
        return 0.0;
    }
    const double step =
        std::clamp(std::abs(distance) / 2.0, AUTO_SCROLL_MIN_STEP, AUTO_SCROLL_MAX_STEP);
    return distance < 0.0 ? -step : step;
}

void BookEditor::onAutoScrollTimeout()
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    // The step is in view pixels (page mode zooms the document)
    const double scale = m_viewportManager ? m_viewportManager->viewScale() : 1.0;
    const double step = autoScrollStep() / scale;
    const qreal before = scrollOffset();
    if (step != 0.0) {
        scrollBy(step, false);
    }
    if (scrollOffset() == before) {
        stopAutoScroll();  // the mouse is back inside, or the view is at the end
        return;
    }

    // The text under the mouse has moved: the selection or the drop caret follows it
    if (m_autoScrollForDrop) {
        const CursorPosition position = positionFromPoint(m_autoScrollPos);
        if (m_draggingText && isInSelection(position)) {
            m_renderPipeline->setDropCaret(std::nullopt);
        } else {
            m_renderPipeline->setDropCaret(position);
        }
    } else if (m_isDragging) {
        extendMouseSelection(m_autoScrollPos);
    }
}

void BookEditor::setContextMenuActions(const QList<QAction*>& actions)
{
    m_contextMenuActions = actions;
}

void BookEditor::setContextMenuEditActions(const ContextMenuEditActions& actions)
{
    m_contextMenuEditActions = actions;
}

void BookEditor::setNextIssueAction(QAction* action)
{
    m_nextIssueAction = action;
}

void BookEditor::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_textBuffer) {
        QWidget::contextMenuEvent(event);
        return;
    }

    // From the keyboard (the menu key, Shift+F10) the menu is for the cursor's place
    if (event->reason() == QContextMenuEvent::Keyboard) {
        showContextMenuAtCursor();
        return;
    }

    // On an annotation's card: the menu of its annotation
    if (event->reason() == QContextMenuEvent::Mouse) {
        if (const std::optional<AnnotationCardHit> card = annotationCardAt(event->pos())) {
            showAnnotationCardMenu(card->id, event->globalPos());
            return;
        }
    }

    const CursorPosition pos = positionFromPoint(event->pos());
    if (pos.paragraph < 0) {
        QWidget::contextMenuEvent(event);
        return;
    }
    showContextMenu(pos, event->globalPos(), event->reason() == QContextMenuEvent::Mouse);
}

void BookEditor::showContextMenuAtCursor()
{
    if (!m_textBuffer || m_cursorPosition.paragraph < 0) {
        return;
    }
    checkSpellingAtCursor();  // also the word just typed, without its wave yet

    // Under the cursor, so the word the menu is for stays in sight
    QPoint menuPos = mapToGlobal(rect().center());
    if (m_renderPipeline) {
        const QRectF caret = m_renderPipeline->cursorRect();
        if (!caret.isEmpty()) {
            menuPos = mapToGlobal(caret.bottomLeft().toPoint());
        }
    }
    showContextMenu(m_cursorPosition, menuPos, false);
}

bool BookEditor::showIssueMenu(const TextIssue& issue, const QPoint& globalPos)
{
    if (!isIssueAt(issue)) {
        return false;
    }
    // Inside the issue: a word that ends where it starts is not taken for it
    showContextMenu({issue.paragraph, issue.start + issue.length / 2}, globalPos, false,
                    issue.kind);
    return true;
}

void BookEditor::showContextMenu(const CursorPosition& pos, const QPoint& globalPos,
                                 bool fromMouse, std::optional<TextIssueKind> kind)
{
    // A misspelled word or a grammar issue: what to put in its place, on top of the usual
    // menu (the spelling first, where both are)
    const auto [word, startOffset, endOffset] =
        kind != TextIssueKind::Grammar ? getMisspelledWordAt(pos.paragraph, pos.offset)
                                       : std::tuple<QString, int, int>{QString(), 0, 0};
    const std::optional<GrammarError> grammarError =
        word.isEmpty() && kind != TextIssueKind::Spelling
            ? getGrammarErrorAt(pos.paragraph, pos.offset)
            : std::nullopt;

    // A right click outside the selection puts the cursor there, as a click does, so what
    // the menu does (pasting, adding an annotation) happens where the writer clicked
    if (fromMouse && !isInSelection(pos)) {
        const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);
        clearSelection();
        m_selectionAnchor = pos;
        setCursorPosition(pos);
    }

    // Default context menu. Each item with its access key, unique in the menu with the
    // commands below and the items of spelling (test_menu_access_keys). A greyed-out item says
    // why in its tooltip (a read-only text), as in the program's menus
    QMenu menu(this);
    menu.setToolTipsVisible(true);
    if (!word.isEmpty()) {
        addSpellingActions(menu, word, pos.paragraph, startOffset, endOffset);
    } else if (grammarError.has_value()) {
        addGrammarActions(menu, *grammarError, pos.paragraph);
    }

    // A link: edit it (the window of Insert > Link, with its keys), open it, copy its address
    // or take it off the text, as in Word
    if (const LinkTarget link = linkTarget(); link.existing) {
        if (!menu.isEmpty()) {
            menu.addSeparator();
        }
        const QString keys =
            m_contextMenuEditActions.link != nullptr
                ? m_contextMenuEditActions.link->shortcut().toString(QKeySequence::NativeText)
                : QString();
        // The keys only shown (after a tab), so they stay those of the command
        QString editText = tr("Edit &Hyperlink...");
        if (!keys.isEmpty()) {
            editText += QLatin1Char('\t') + keys;
        }
        disableWhileReadOnly(menu.addAction(editText, this, &BookEditor::linkDialogAsked));
        menu.addAction(tr("&Open Hyperlink"), this,
                       [this, address = link.address]() { emit linkOpenAsked(address); });
        menu.addAction(tr("Cop&y Hyperlink"), this, [address = link.address]() {
            if (QClipboard* clipboard = QGuiApplication::clipboard()) {
                clipboard->setText(address);
            }
        });
        disableWhileReadOnly(menu.addAction(tr("Remove Hyperl&ink"), this,
                                            [this, link]() { removeLink(link); }));
        menu.addSeparator();
    }

    // The annotation whose fragment or place is there: what can be done with it
    if (const QString annotation = annotationAt(pos); !annotation.isEmpty()) {
        addAnnotationActions(menu, annotation, false);
        menu.addSeparator();
    }

    // A list item: its numbering and its level, as in Word
    if (const std::optional<ListItem> item = currentListItem()) {
        // (after the annotation's items, which end with a line)
        if (!menu.isEmpty() && !menu.actions().constLast()->isSeparator()) {
            menu.addSeparator();
        }
        if (!item->levelFormat().isBullet()) {
            const ListLevel& level = item->levelFormat();
            //: %1 is the first number of the level of the list item: 1, a, i
            QAction* restart = menu.addAction(
                tr("&Restart at %1").arg(listNumberText(level.start, level.format)), this,
                &BookEditor::restartNumbering);
            disableWhileReadOnly(restart);
            restart->setEnabled(restart->isEnabled() &&
                                (!item->numbered || currentListNumber() != level.start));
            QAction* goOn =
                menu.addAction(tr("Contin&ue Numbering"), this, &BookEditor::continueNumbering);
            disableWhileReadOnly(goOn);
            goOn->setEnabled(goOn->isEnabled() && canContinueNumbering());
            disableWhileReadOnly(menu.addAction(tr("Set Numbering &Value..."), this,
                                                &BookEditor::numberingValueAsked));
            menu.addSeparator();
        }
        QAction* deeper = menu.addAction(tr("Incr&ease Indent"), this, &BookEditor::increaseIndent);
        disableWhileReadOnly(deeper);
        deeper->setEnabled(deeper->isEnabled() && item->level < LIST_LEVELS - 1);
        QAction* higher = menu.addAction(tr("Decrea&se Indent"), this, &BookEditor::decreaseIndent);
        disableWhileReadOnly(higher);
        higher->setEnabled(higher->isEnabled() && item->level > 0);
        disableWhileReadOnly(menu.addAction(tr("Bullets and Numberin&g..."), this,
                                            &BookEditor::bulletsAndNumberingAsked));
        menu.addSeparator();
    }

    // The editing items: the Edit menu's commands with their icons and keys, where the
    // application gives them; else the editor's own, those that change the text greyed out in
    // a read-only text
    const auto addEditItem = [this, &menu](QAction* command, const QString& text,
                                           void (BookEditor::*edit)(), bool changes) {
        if (command != nullptr) {
            menu.addAction(command);
        } else if (changes) {
            disableWhileReadOnly(menu.addAction(text, this, edit));
        } else {
            menu.addAction(text, this, edit);
        }
    };
    const ContextMenuEditActions& commands = m_contextMenuEditActions;
    if (hasSelection()) {
        addEditItem(commands.cut, tr("Cu&t"), &BookEditor::cut, true);
        addEditItem(commands.copy, tr("&Copy"), &BookEditor::copy, false);
    }
    addEditItem(commands.paste, tr("&Paste"), &BookEditor::paste, true);

    // Also without a selection, as in the program's text fields
    menu.addSeparator();
    addEditItem(commands.selectAll, tr("Select &All"), &BookEditor::selectAll, false);

    // The application's commands for the text (adding annotations)
    bool separated = false;
    for (QAction* action : std::as_const(m_contextMenuActions)) {
        if (action == nullptr) {
            continue;
        }
        if (!separated) {
            menu.addSeparator();
            separated = true;
        }
        menu.addAction(action);
    }

    // The materials linked with the paragraph, whose mark is beside it: from the keyboard too
    const QList<ParagraphMark> marks = paragraphMarks();
    const auto marked =
        std::find_if(marks.cbegin(), marks.cend(),
                     [&pos](const ParagraphMark& mark) { return mark.paragraph == pos.paragraph; });
    if (marked != marks.cend()) {
        menu.addSeparator();
        //: The submenu of the materials linked with the paragraph. In Polish: Powiązane
        //: mate&riały
        addParagraphMarkItems(*menu.addMenu(m_paragraphMarkIcon, tr("Linked Mate&rials")),
                              *marked);
    }

    // Color mode toggle
    menu.addSeparator();
    QString colorModeText = (m_appearance.colorMode == EditorColorMode::Light)
        ? tr("Switch to Dar&k Mode")
        : tr("Switch to &Light Mode");
    menu.addAction(colorModeText, this, &BookEditor::toggleEditorColorMode);

    // The parts shown together (a word's, an annotation's, the linked materials) can have
    // the same keys: the items below take others
    giveUniqueAccessKeys(menu);

    // The keys going to the next issue (F7) close the menu and go on to it, without Esc
    // first: from the event loop, so the next issue's menu does not open inside this one
    MenuCommandKeys nextIssueKeys(m_nextIssueAction);
    menu.installEventFilter(&nextIssueKeys);
    menu.exec(globalPos);
    if (nextIssueKeys.pressed()) {
        QTimer::singleShot(0, m_nextIssueAction, &QAction::trigger);
    }
}

}  // namespace kalahari::editor
