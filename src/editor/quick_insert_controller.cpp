/// @file quick_insert_controller.cpp
/// @brief Quick insert in a BookEditor: the list at the cursor

#include <kalahari/editor/quick_insert_controller.h>
#include <kalahari/editor/book_editor.h>

#include <QApplication>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::editor {

namespace {

/// @brief How many rows the list shows at most before it scrolls
constexpr int VISIBLE_ROWS = 9;

/// @brief How wide the list is, in average characters of its font
constexpr int WIDTH_IN_CHARACTERS = 46;

/// @brief The space between the cursor and the list, in pixels
constexpr int CURSOR_GAP = 3;

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
    m_items = items;
    m_rows.clear();
    m_tree->clear();

    QFont groupFont = m_tree->font();
    groupFont.setBold(true);
    groupFont.setPointSizeF(groupFont.pointSizeF() * 0.85);
    QString group;
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
        auto* row = new QTreeWidgetItem(m_tree, {item.name, shownText(item.detail)});
        row->setForeground(1, palette().brush(QPalette::PlaceholderText));
        row->setToolTip(0, item.name);
        row->setToolTip(1, item.detail);
        m_rows.append(row);
    }

    if (items.isEmpty()) {
        //: Quick insert: the list after the letters typed in the text (%1, "@xyz") found nothing
        m_preview->setText(tr("Nothing matches “%1”. The next character closes the list.").arg(typed));
        m_tree->hide();
    } else {
        m_tree->show();
    }
    //: Quick insert: the keys of the list; %1 is what was typed in the text ("@an")
    m_keys->setText(tr("↑↓ choose · Enter, Tab insert · Esc leaves “%1”").arg(typed));

    // As high as its rows, up to VISIBLE_ROWS of them
    const int rowHeight = std::max(m_tree->sizeHintForRow(0), QFontMetrics(font()).height() + 4);
    const int rows = std::min(m_tree->topLevelItemCount(), VISIBLE_ROWS);
    m_tree->setFixedHeight(rows * rowHeight + 2 * m_tree->frameWidth() + 2);

    m_current = -1;
    setCurrent(items.isEmpty() ? -1 : 0);
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

void QuickInsertList::showChoice() {
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

bool QuickInsertController::handleKey(int key) {
    switch (key) {
        case Qt::Key_Up:
            m_list->move(-1);
            return true;
        case Qt::Key_Down:
            m_list->move(1);
            return true;
        case Qt::Key_PageUp:
            m_list->move(-m_list->pageSize());
            return true;
        case Qt::Key_PageDown:
            m_list->move(m_list->pageSize());
            return true;
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
                        static const QList<int> listKeys = {Qt::Key_Up,     Qt::Key_Down,
                                                            Qt::Key_PageUp, Qt::Key_PageDown,
                                                            Qt::Key_Return, Qt::Key_Enter,
                                                            Qt::Key_Tab,    Qt::Key_Escape};
                        if (listKeys.contains(key->key())) {
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
