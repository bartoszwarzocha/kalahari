// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file quick_insert_controller.cpp
/// @brief Quick insert in a BookEditor: the list at the cursor

#include <kalahari/editor/quick_insert_controller.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/quick_insert.h>

#include <QApplication>
#include <QColor>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QStringView>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace kalahari::editor {

namespace {

/// @brief How many rows the list shows at most before it scrolls
constexpr int VISIBLE_ROWS = 9;

/// @brief How wide the list is, in average characters of its font
constexpr int WIDTH_IN_CHARACTERS = 46;

/// @brief The space between the cursor and the list, in pixels
constexpr int CURSOR_GAP = 3;

/// @brief Beside the detail of an item with forms: Right shows them
constexpr QStringView FORMS_MARK = u"  ▸";

/// @brief The keys of the list taken before any shortcut that has them (Esc), beside Right
///        and Left (QuickInsertController::takesKey())
constexpr std::array<int, 8> LIST_KEYS = {Qt::Key_Up,     Qt::Key_Down,  Qt::Key_PageUp,
                                          Qt::Key_PageDown, Qt::Key_Return, Qt::Key_Enter,
                                          Qt::Key_Tab,    Qt::Key_Escape};

/// @brief How far Up, Down, Page Up and Page Down move the choice (@p page: a page); 0 for
///        another key
int stepsOf(int key, int page) {
    switch (key) {
        case Qt::Key_Up:
            return -1;
        case Qt::Key_Down:
            return 1;
        case Qt::Key_PageUp:
            return -page;
        case Qt::Key_PageDown:
            return page;
        default:
            return 0;
    }
}

/// @brief The share of the height of a line the dot of the color of a card takes
constexpr qreal COLOR_DOT_SHARE = 0.6;

/// @brief The dot of color @p color (none: an empty place of its size) beside a name of a
///        card, @p side pixels wide for the screen's @p ratio, with a line of @p border
QIcon colorDot(const QString& color, int side, qreal ratio, const QColor& border) {
    QPixmap pixmap(QSize(side, side) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    if (const QColor fill(color); fill.isValid()) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(border, 1.0));
        painter.setBrush(fill);
        painter.drawEllipse(QRectF(0.5, 0.5, side - 1.0, side - 1.0));
    }
    return QIcon(pixmap);
}

/// @brief The text an item inserts as the list shows it: one line, the paragraph breaks as ¶
QString shownText(const QString& text) {
    QString shown = text;
    shown.replace(QLatin1Char('\n'), QStringLiteral(" ¶ "));
    if (shown.trimmed().isEmpty()) {
        return QStringLiteral("␣");  // a space of some kind
    }
    return shown;
}

}  // namespace

// =============================================================================
// QuickInsertList
// =============================================================================

QuickInsertList::QuickInsertList(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("quickInsertList"));
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    // The colors of the program's windows, not those of the paper of the text
    setPalette(QApplication::palette());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    m_tree = new QTreeWidget(this);
    m_tree->setFocusPolicy(Qt::NoFocus);
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setIndentation(0);
    m_tree->setUniformRowHeights(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* row) {
        const int index = static_cast<int>(m_rows.indexOf(row));
        if (index >= 0) {
            setCurrent(index);
            emit activated(index);
        }
    });
    layout->addWidget(m_tree);

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("quickInsertPreview"));
    m_preview->setContentsMargins(8, 4, 8, 4);
    m_preview->setTextFormat(Qt::PlainText);
    layout->addWidget(m_preview);

    m_keys = new QLabel(this);
    m_keys->setObjectName(QStringLiteral("quickInsertKeys"));
    m_keys->setContentsMargins(8, 2, 8, 4);
    m_keys->setTextFormat(Qt::PlainText);
    m_keys->setWordWrap(true);
    m_keys->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_keys);

    const QFontMetrics metrics(font());
    setFixedWidth(metrics.averageCharWidth() * WIDTH_IN_CHARACTERS);
    hide();
}

void QuickInsertList::setItems(const QList<QuickInsertItem>& items, const QString& typed) {
    m_typed = typed;
    m_listed.clear();
    m_formsOf = -1;
    fill(items, items.isEmpty() ? -1 : 0);
}

void QuickInsertList::fill(const QList<QuickInsertItem>& items, int current) {
    m_items = items;
    m_rows.clear();
    m_tree->clear();

    QFont groupFont = m_tree->font();
    groupFont.setBold(true);
    groupFont.setPointSizeF(groupFont.pointSizeF() * 0.85);
    QString group;
    // The names of the cards with their colors, the other rows of the list in line with them
    const bool dots = std::ranges::any_of(
        items, [](const QuickInsertItem& item) { return !item.color.isEmpty(); });
    const int side = static_cast<int>(QFontMetrics(font()).height() * COLOR_DOT_SHARE);
    m_tree->setIconSize(QSize(side, side));
    for (const QuickInsertItem& item : items) {
        if (item.group != group || m_rows.isEmpty()) {
            group = item.group;
            if (!group.isEmpty()) {
                auto* heading = new QTreeWidgetItem(m_tree, {group.toUpper()});
                heading->setFlags(Qt::NoItemFlags);
                heading->setFont(0, groupFont);
                heading->setFirstColumnSpanned(true);
            }
        }
        // ▸: Right shows its forms
        QString detail = shownText(item.detail);
        if (!item.forms.isEmpty()) {
            detail.append(FORMS_MARK);
        }
        auto* row = new QTreeWidgetItem(m_tree, {item.name, detail});
        row->setForeground(1, palette().brush(QPalette::PlaceholderText));
        row->setToolTip(0, item.name);
        row->setToolTip(1, item.detail);
        if (dots) {
            row->setIcon(0, colorDot(item.color, side, devicePixelRatioF(),
                                     palette().color(QPalette::Mid)));
        }
        m_rows.append(row);
    }

    if (items.isEmpty()) {
        //: Quick insert: the list after the letters typed in the text (%1, "@xyz") found nothing
        m_preview->setText(tr("Nothing matches “%1”. The next character closes the list.").arg(m_typed));
        m_tree->hide();
    } else {
        m_tree->show();
    }

    // As high as its rows, up to VISIBLE_ROWS of them
    const int rowHeight = std::max(m_tree->sizeHintForRow(0), QFontMetrics(font()).height() + 4);
    const int rows = std::min(m_tree->topLevelItemCount(), VISIBLE_ROWS);
    m_tree->setFixedHeight(rows * rowHeight + 2 * m_tree->frameWidth() + 2);

    m_current = -1;
    setCurrent(current);
    adjustSize();
}

void QuickInsertList::setCurrent(int index) {
    if (index < -1 || index >= m_items.size()) {
        return;
    }
    m_current = index;
    if (index >= 0) {
        m_tree->setCurrentItem(m_rows.at(index));
        m_tree->scrollToItem(m_rows.at(index));
        // The heading of the first group stays in view with its first item
        if (index == 0) {
            m_tree->scrollToTop();
        }
    } else {
        m_tree->setCurrentItem(nullptr);
    }
    showChoice();
}

void QuickInsertList::move(int steps) {
    const int count = static_cast<int>(m_items.size());
    if (count == 0) {
        return;
    }
    int index = m_current + steps;
    if (std::abs(steps) == 1) {
        index = (index + count) % count;
    } else {
        index = std::clamp(index, 0, count - 1);
    }
    setCurrent(index);
}

int QuickInsertList::pageSize() const {
    return VISIBLE_ROWS - 1;
}

bool QuickInsertList::hasForms() const {
    return std::ranges::any_of(m_items,
                               [](const QuickInsertItem& item) { return !item.forms.isEmpty(); });
}

bool QuickInsertList::showForms() {
    if (showsForms() || m_current < 0 || m_items.at(m_current).forms.isEmpty()) {
        return false;
    }
    const QuickInsertItem& item = m_items.at(m_current);
    QList<QuickInsertItem> forms;
    forms.reserve(item.forms.size());
    for (const QuickInsertForm& form : item.forms) {
        QuickInsertItem shown = formItem(item, form);
        shown.detail = form.cases;
        //: Quick insert: the heading of the forms of a name (%1, "Anna Wolska") in the list
        shown.group = tr("%1 – forms").arg(item.name);
        forms.append(shown);
    }
    const int formsOf = m_current;
    m_listed = m_items;
    m_formsOf = formsOf;
    fill(forms, 0);
    return true;
}

bool QuickInsertList::showItems() {
    if (!showsForms()) {
        return false;
    }
    const int chosen = m_formsOf;
    m_formsOf = -1;
    const QList<QuickInsertItem> items = std::exchange(m_listed, {});
    fill(items, chosen);
    return true;
}

void QuickInsertList::showChoice() {
    if (showsForms()) {
        //: Quick insert: the keys of the forms of a name; %1 is what was typed in the text ("@an")
        m_keys->setText(tr("↑↓ choose · ← back to the list · Enter, Tab insert · Esc leaves “%1”")
                            .arg(m_typed));
    } else if (hasForms()) {
        //: Quick insert: the keys of a list with names that have forms (▸); %1 is what was typed
        //: in the text ("@an")
        m_keys->setText(tr("↑↓ choose · → forms · Enter, Tab insert · Esc leaves “%1”")
                            .arg(m_typed));
    } else {
        //: Quick insert: the keys of the list; %1 is what was typed in the text ("@an")
        m_keys->setText(tr("↑↓ choose · Enter, Tab insert · Esc leaves “%1”").arg(m_typed));
    }
    if (m_current < 0) {
        return;
    }
    //: Quick insert: what the chosen item puts in the text
    m_preview->setText(tr("Inserts: %1").arg(shownText(m_items.at(m_current).text)));
}

// =============================================================================
// QuickInsertController
// =============================================================================

QuickInsertController::QuickInsertController(BookEditor* editor, ItemsFor items, QObject* parent)
    : QObject(parent != nullptr ? parent : editor), m_editor(editor), m_itemsFor(std::move(items)) {
    m_list = new QuickInsertList(editor);
    connect(m_list, &QuickInsertList::activated, this, [this]() { insertCurrent(); });
    editor->installEventFilter(this);
    connect(editor, &BookEditor::contentChanged, this, &QuickInsertController::onTextChanged);
    connect(editor, &BookEditor::cursorPositionChanged, this, [this]() {
        if (m_open && !m_inserting) {
            follow();
        }
    });
    connect(editor, &BookEditor::scrollOffsetChanged, this, [this]() {
        if (m_open) {
            place();
        }
    });
    connect(editor, &BookEditor::documentChanged, this, &QuickInsertController::close);
}

void QuickInsertController::setTriggers(const QString& triggers) {
    m_triggers = triggers;
}

void QuickInsertController::openAtCursor() {
    if (m_editor == nullptr || m_editor->isReadOnly()) {
        return;
    }
    if (m_editor->hasSelection()) {
        m_editor->clearSelection();
    }
    open(QChar(), m_editor->cursorPosition());
}

void QuickInsertController::open(QChar trigger, const CursorPosition& start) {
    QList<QuickInsertItem> all = m_itemsFor ? m_itemsFor(trigger) : QList<QuickInsertItem>();
    // Typed in the text, a character without items stays a character: nothing to show
    if (all.isEmpty() && !trigger.isNull()) {
        return;
    }
    m_all = std::move(all);
    m_trigger = trigger;
    m_start = start;
    m_open = true;
    m_lastFound = 0;
    follow();
}

void QuickInsertController::close() {
    if (!m_open) {
        return;
    }
    m_open = false;
    m_all.clear();
    m_list->hide();
}

void QuickInsertController::onTextChanged() {
    if (m_inserting || m_editor == nullptr) {
        return;
    }
    const QChar typed = m_typedTrigger;
    m_typedTrigger = QChar();
    if (m_open) {
        follow();
        return;
    }
    if (typed.isNull() || m_editor->hasSelection()) {
        return;
    }
    // The character just typed, at the start of a word
    const CursorPosition cursor = m_editor->cursorPosition();
    const QString text = m_editor->paragraphPlainText(static_cast<size_t>(cursor.paragraph));
    if (cursor.offset < 1 || cursor.offset > text.size() || text.at(cursor.offset - 1) != typed ||
        !startsQuickInsertWord(text.left(cursor.offset - 1))) {
        return;
    }
    open(typed, cursor);
}

void QuickInsertController::follow() {
    if (!m_open || m_editor == nullptr) {
        return;
    }
    const CursorPosition cursor = m_editor->cursorPosition();
    if (cursor.paragraph != m_start.paragraph || cursor.offset < m_start.offset ||
        m_editor->hasSelection()) {
        close();
        return;
    }
    const QString text = m_editor->paragraphPlainText(static_cast<size_t>(cursor.paragraph));
    // The character that opened the list is still before the letters
    if (!m_trigger.isNull() &&
        (m_start.offset < 1 || m_start.offset > text.size() || text.at(m_start.offset - 1) != m_trigger)) {
        close();
        return;
    }
    const QString query = text.mid(m_start.offset, cursor.offset - m_start.offset);
    if (!std::all_of(query.cbegin(), query.cend(), continuesQuickInsertQuery)) {
        close();
        return;
    }
    const QList<QuickInsertItem> found = matchQuickInsert(m_all, query);
    if (!found.isEmpty()) {
        m_lastFound = static_cast<int>(query.size());
    } else if (query.size() > m_lastFound + 1) {
        close();  // Nothing found after the character before either
        return;
    }
    const QString typed = m_trigger.isNull() ? query : QString(m_trigger) + query;
    m_list->setItems(found, typed);
    place();
    m_list->show();
    m_list->raise();
}

void QuickInsertController::place() {
    if (m_editor == nullptr) {
        return;
    }
    const QRect cursor = m_editor->inputMethodQuery(Qt::ImCursorRectangle).toRect();
    const QSize size = m_list->sizeHint().expandedTo(m_list->minimumSize());
    const QRect area = m_editor->rect();
    int x = std::clamp(cursor.left(), 0, std::max(0, area.width() - size.width()));
    int y = cursor.bottom() + CURSOR_GAP;
    if (y + size.height() > area.height() && cursor.top() - CURSOR_GAP - size.height() >= 0) {
        y = cursor.top() - CURSOR_GAP - size.height();  // No room under the line: above it
    }
    m_list->setGeometry(QRect(QPoint(x, y), size));
}

bool QuickInsertController::insertCurrent() {
    if (!m_open || m_editor == nullptr || m_list->current() < 0) {
        return false;
    }
    const QuickInsertItem item = m_list->items().at(m_list->current());
    const CursorPosition cursor = m_editor->cursorPosition();
    const int from = m_start.offset - (m_trigger.isNull() ? 0 : 1);

    m_inserting = true;
    m_editor->setSelection({CursorPosition{cursor.paragraph, from}, cursor});
    if (item.text.contains(QLatin1Char('\n'))) {
        // The paragraphs of a fragment, as pasted text without formatting
        QMimeData paragraphs;
        paragraphs.setText(item.text);
        m_editor->insertFromMimeData(&paragraphs);
    } else if (from == cursor.offset) {
        m_editor->clearSelection();
        m_editor->insertText(item.text);
    } else {
        m_editor->insertText(item.text);
    }
    if (isEnclosingPair(item.text)) {
        CursorPosition between = m_editor->cursorPosition();
        between.offset -= 1;
        m_editor->setCursorPosition(between);
    }
    m_inserting = false;
    close();
    return true;
}

bool QuickInsertController::takesKey(int key) const {
    switch (key) {
        case Qt::Key_Right:
            return m_list->hasForms() || m_list->showsForms();
        case Qt::Key_Left:
            return m_list->showsForms();
        default:
            return std::ranges::find(LIST_KEYS, key) != LIST_KEYS.end();
    }
}

bool QuickInsertController::handleKey(int key) {
    if (const int steps = stepsOf(key, m_list->pageSize()); steps != 0) {
        m_list->move(steps);
        return true;
    }
    switch (key) {
        case Qt::Key_Right:
            if (!takesKey(key)) {
                return false;  // The cursor moves in the text
            }
            m_list->showForms();
            return true;
        case Qt::Key_Left:
            return m_list->showItems();
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
            if (m_list->current() >= 0) {
                insertCurrent();
                return true;
            }
            close();  // Nothing to insert: the key does what it does in the text
            return false;
        case Qt::Key_Escape:
            close();
            return true;
        default:
            return false;
    }
}

bool QuickInsertController::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_editor) {
        return QObject::eventFilter(watched, event);
    }
    switch (event->type()) {
        case QEvent::ShortcutOverride:
        case QEvent::KeyPress: {
            auto* key = static_cast<QKeyEvent*>(event);
            if (m_open) {
                const bool plain = (key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
                if (plain) {
                    if (event->type() == QEvent::ShortcutOverride) {
                        // The keys of the list before any shortcut that has them (Esc)
                        if (takesKey(key->key())) {
                            event->accept();
                            return true;
                        }
                    } else if (handleKey(key->key())) {
                        return true;
                    }
                }
            } else if (event->type() == QEvent::KeyPress) {
                // A character of the triggers: onTextChanged() opens the list once it is typed
                const bool trigger = key->text().size() == 1 && !m_triggers.isEmpty() &&
                                     m_triggers.contains(key->text().at(0)) &&
                                     !m_editor->isReadOnly();
                m_typedTrigger = trigger ? key->text().at(0) : QChar();
            }
            break;
        }
        case QEvent::FocusOut:
            close();
            break;
        case QEvent::Resize:
            if (m_open) {
                place();
            }
            break;
        default:
            break;
    }
    return QObject::eventFilter(watched, event);
}

}  // namespace kalahari::editor
