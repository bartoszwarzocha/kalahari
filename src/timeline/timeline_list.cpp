// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_list.cpp
/// @brief A timeline as a table

#include <kalahari/timeline/timeline_list.h>

#include <kalahari/timeline/timeline_document.h>
#include <kalahari/timeline/timeline_edits.h>

#include <QAction>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QStyledItemDelegate>
#include <QTimer>

#include <array>
#include <cmath>

namespace kalahari::timeline {

namespace {

/// Days between two events from which the list shows a gap
constexpr double GAP_FROM = 20.0;
constexpr double DIM_SHARE = 0.35;

/// The shares of the width of the columns
constexpr std::array<double, TimelineList::COLUMNS> SHARES{0.20, 0.25, 0.14, 0.15, 0.14, 0.12};

QColor mixed(const QColor& a, const QColor& b, double share) {
    return QColor::fromRgbF(static_cast<float>(a.redF() * share + b.redF() * (1.0 - share)),
                            static_cast<float>(a.greenF() * share + b.greenF() * (1.0 - share)),
                            static_cast<float>(a.blueF() * share + b.blueF() * (1.0 - share)));
}

void showKeys(QAction* action, const QKeySequence& keys) {
    action->setShortcut(keys);
    action->setShortcutContext(Qt::WidgetShortcut);
    action->setShortcutVisibleInContextMenu(true);
}

/// The fields of the cells: a list of the threads, a line for the others
class CellDelegate : public QStyledItemDelegate {
public:
    explicit CellDelegate(TimelineList& list) : QStyledItemDelegate(&list), m_list(list) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        if (index.column() != TimelineList::Thread) {
            return QStyledItemDelegate::createEditor(parent, option, index);
        }
        auto* box = new QComboBox(parent);
        for (const TimelineThread& thread : m_list.document().timeline().threads()) {
            box->addItem(thread.name, thread.id);
        }
        box->addItem(TimelineWords::withoutLane(TimelineView::Lanes::Threads), QString());
        return box;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        const QString id = index.data(TimelineList::ID_ROLE).toString();
        const QString text = m_list.editText(id, index.column());
        if (auto* box = qobject_cast<QComboBox*>(editor); box != nullptr) {
            box->setCurrentIndex(std::max(0, box->findData(text)));
            return;
        }
        if (auto* line = qobject_cast<QLineEdit*>(editor); line != nullptr) {
            // Typing in a cell starts the field with what was typed
            if (line->text().isEmpty()) {
                line->setText(text);
                line->selectAll();
            }
        }
    }

    void setModelData(QWidget* editor, QAbstractItemModel* /*model*/,
                      const QModelIndex& index) const override {
        const QString id = index.data(TimelineList::ID_ROLE).toString();
        QString value;
        if (auto* box = qobject_cast<QComboBox*>(editor); box != nullptr) {
            value = box->currentData().toString();
        } else if (auto* line = qobject_cast<QLineEdit*>(editor); line != nullptr) {
            value = line->text();
        }
        // The list fills again later: the model stays as it is until then
        QTimer::singleShot(0, &m_list, [list = &m_list, id, column = index.column(), value]() {
            list->applyEdit(id, column, value);
        });
    }

private:
    TimelineList& m_list;
};

const TimelineIndex& emptyIndex() {
    static const TimelineIndex index;
    return index;
}

}  // namespace

TimelineList::TimelineList(TimelineDocument& document, QWidget* parent)
    : QTableWidget(parent),
      m_document(document),
      m_index(&emptyIndex()),
      m_look(TimelineLook::current()) {
    setObjectName(QStringLiteral("timelineList"));
    //: The accessible name of the list of a timeline. In Polish: Lista wydarzeń
    setAccessibleName(tr("List of the events"));
    setColumnCount(COLUMNS);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                    QAbstractItemView::AnyKeyPressed);
    setTabKeyNavigation(true);
    setWordWrap(false);
    setAlternatingRowColors(true);
    setFrameShape(QFrame::NoFrame);
    verticalHeader()->hide();
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    horizontalHeader()->setHighlightSections(false);
    setItemDelegate(new CellDelegate(*this));

    connect(&m_document, &TimelineDocument::changed, this, &TimelineList::refresh);
    connect(&m_document, &TimelineDocument::viewChanged, this, &TimelineList::refresh);
    connect(&m_document, &TimelineDocument::selectionChanged, this, &TimelineList::showSelected);
    connect(this, &QTableWidget::currentCellChanged, this, [this](int row) {
        if (m_filling || row < 0) {
            return;
        }
        const QTableWidgetItem* cell = item(row, 0);
        const QString id = cell != nullptr ? cell->data(ID_ROLE).toString() : QString();
        if (!id.isEmpty()) {
            m_document.select(id);
        }
    });
    rebuild();
}

void TimelineList::setIndex(const TimelineIndex* index) {
    m_index = index != nullptr ? index : &emptyIndex();
    refresh();
}

void TimelineList::setLook(const TimelineLook& look) {
    m_look = look;
    refresh();
}

void TimelineList::setHighlight(const QString& filter) {
    m_highlight = filter;
    refresh();
}

void TimelineList::refresh() {
    if (m_rebuildPending) {
        return;
    }
    m_rebuildPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_rebuildPending = false;
        if (state() == QAbstractItemView::EditingState) {
            return;  // closeEditor() fills it again
        }
        rebuild();
    });
}

QString TimelineList::currentId() const {
    const QTableWidgetItem* cell = item(currentRow(), 0);
    return cell != nullptr ? cell->data(ID_ROLE).toString() : QString();
}

// =============================================================================
// Filling the table
// =============================================================================

QString TimelineList::cellText(const TimelineEvent& event, int column) const {
    const Timeline& timeline = m_document.timeline();
    switch (column) {
        case When:
            return TimelineWords::when(timeline, event, m_look);
        case Title:
            return event.title;
        case Thread: {
            const TimelineThread* thread = timeline.thread(event.thread);
            return thread != nullptr ? thread->name : QStringLiteral("–");
        }
        case Characters:
            return event.characters.isEmpty() ? QStringLiteral("–")
                                              : m_index->namesOf(event.characters);
        case Place:
            return event.place.isEmpty() ? QStringLiteral("–") : m_index->nameOf(event.place);
        case Book: {
            if (event.chapters.isEmpty()) {
                //: An event that no chapter tells. In Polish: poza książką
                return tr("not in the book");
            }
            QStringList titles;
            for (const QString& chapter : event.chapters) {
                const TimelineIndex::Entry* entry = m_index->chapter(chapter);
                //: A chapter that was deleted. In Polish: (usunięty)
                titles.append(entry != nullptr ? entry->name : tr("(deleted)"));
            }
            return titles.join(QStringLiteral(", "));
        }
        default:
            break;
    }
    return QString();
}

QString TimelineList::editText(const QString& id, int column) const {
    const TimelineEvent* event = m_document.timeline().event(id);
    if (event == nullptr) {
        return QString();
    }
    switch (column) {
        case When:
            return TimelineWords::whenToEdit(m_document.timeline(), *event, m_look);
        case Title:
            return event->title;
        case Thread:
            return event->thread;
        case Characters:
            return m_index->namesOf(event->characters);
        case Place:
            return m_index->nameOf(event->place);
        default:
            break;
    }
    return QString();
}

void TimelineList::rebuild() {
    m_filling = true;
    const Timeline& timeline = m_document.timeline();
    const bool ordered = timeline.view().scale == TimelineView::Scale::Order;
    //: The columns of the list of a timeline. In Polish: Kiedy (podpis) / Kiedy / Wydarzenie /
    //: Wątek / Postacie / Miejsce / W książce
    setHorizontalHeaderLabels({ordered ? tr("When (label)") : tr("When"), tr("Event"),
                               tr("Thread"), tr("Characters"), tr("Place"), tr("In the book")});
    QList<const TimelineEvent*> rows = ordered ? timeline.eventsInOrder() : timeline.eventsInTime();
    if (!ordered) {
        rows.append(timeline.eventsWithoutTime());
    }
    clearSpans();
    setRowCount(0);
    const QColor dim = mixed(m_look.text, m_look.card, DIM_SHARE);
    const TimelineEvent* previous = nullptr;
    int selectedRow = -1;
    for (const TimelineEvent* event : rows) {
        // A long gap without events
        if (previous != nullptr && timeline.view().compress && !ordered && event->time &&
            previous->time) {
            const double gap = event->time->start() - previous->time->end();
            if (gap > GAP_FROM) {
                const int row = rowCount();
                insertRow(row);
                //: A row of the list of a timeline. %1: "≈ 3 mies.". In Polish: %1 bez
                //: wydarzeń
                auto* cell = new QTableWidgetItem(
                    tr("%1 without events")
                        .arg(StoryTime::lengthText(gap, m_look.style.language)));
                cell->setFlags(Qt::ItemIsEnabled);
                cell->setForeground(m_look.mutedText);
                cell->setBackground(m_look.gap);
                setItem(row, 0, cell);
                setSpan(row, 0, 1, COLUMNS);
            }
        }
        const int row = rowCount();
        insertRow(row);
        const bool matches = TimelineEdits::matches(*event, m_highlight);
        for (int column = 0; column < COLUMNS; ++column) {
            auto* cell = new QTableWidgetItem(cellText(*event, column));
            cell->setData(ID_ROLE, event->id);
            Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            if (column != Book) {
                flags |= Qt::ItemIsEditable;
            }
            cell->setFlags(flags);
            if (!matches) {
                cell->setForeground(dim);
            }
            if (column == When && !ordered && !event->time) {
                QFont font = this->font();
                font.setItalic(true);
                cell->setFont(font);
                cell->setForeground(m_look.flashback);
                bool ok = false;
                cell->setToolTip(TimelineWords::recognized(timeline, *event, m_look, &ok));
            }
            if (column == Book && event->chapters.isEmpty()) {
                QFont font = this->font();
                font.setItalic(true);
                cell->setFont(font);
                cell->setForeground(m_look.mutedText);
            }
            if (column == Thread) {
                const TimelineThread* thread = timeline.thread(event->thread);
                const QColor color = thread != nullptr ? m_look.paletteColor(thread->color)
                                                       : QColor();
                if (color.isValid()) {
                    cell->setIcon(TimelineEdits::swatch(color, m_look.mid));
                }
            }
            if (column == Title && !event->description.isEmpty()) {
                cell->setToolTip(event->description);
            }
            setItem(row, column, cell);
        }
        if (event->id == m_document.selected()) {
            selectedRow = row;
        }
        previous = event;
    }
    sizeColumns();
    if (selectedRow >= 0) {
        const int column = currentColumn() >= 0 ? currentColumn() : static_cast<int>(Title);
        setCurrentCell(selectedRow, column);
    }
    m_filling = false;
    if (!m_editAfter.isEmpty()) {
        const QString id = m_editAfter;
        m_editAfter.clear();
        for (int row = 0; row < rowCount(); ++row) {
            if (item(row, Title) != nullptr && item(row, Title)->data(ID_ROLE).toString() == id) {
                setCurrentCell(row, Title);
                m_newId = id;
                editItem(item(row, Title));
                break;
            }
        }
    }
}

void TimelineList::sizeColumns() {
    const int width = viewport()->width();
    for (int column = 0; column < COLUMNS; ++column) {
        setColumnWidth(column,
                       static_cast<int>(width * SHARES.at(static_cast<std::size_t>(column))));
    }
}

void TimelineList::resizeEvent(QResizeEvent* event) {
    QTableWidget::resizeEvent(event);
    sizeColumns();
}

void TimelineList::showSelected() {
    if (m_filling) {
        return;
    }
    const QString id = m_document.selected();
    for (int row = 0; row < rowCount(); ++row) {
        const QTableWidgetItem* cell = item(row, 0);
        if (cell != nullptr && cell->data(ID_ROLE).toString() == id && !id.isEmpty()) {
            if (currentRow() != row) {
                m_filling = true;
                setCurrentCell(row,
                               currentColumn() >= 0 ? currentColumn() : static_cast<int>(Title));
                m_filling = false;
            }
            return;
        }
    }
}

// =============================================================================
// Changes
// =============================================================================

void TimelineList::applyEdit(const QString& id, int column, const QString& value) {
    const TimelineEvent* event = m_document.timeline().event(id);
    if (event == nullptr) {
        return;
    }
    if (id == m_newId && column == Title) {
        m_newId.clear();
    }
    const QString title = TimelineEdits::quoted(event->title);
    switch (column) {
        case When: {
            if (value.trimmed() == editText(id, When).trimmed()) {
                return;
            }
            TimelineEvent changed = *event;
            const bool known = TimelineWords::setWhen(m_document.timeline(), changed, value);
            //: What Undo undoes in a timeline. In Polish: zmiana czasu „%1”
            m_document.changeEvent(id, tr("changing the time of %1").arg(title),
                                   [&changed](TimelineEvent& own) { own = changed; });
            if (!known) {
                //: In Polish: %1 nie ma już czasu i czeka na pasku „%2”. / Nie rozpoznano „%1” –
                //: wpis zostaje, a wydarzenie czeka na pasku „%2”. Przykłady: %3.
                emit told(value.trimmed().isEmpty()
                              ? tr("%1 has no time any more and waits in “%2”.")
                                    .arg(title, TimelineWords::trayName())
                              : tr("“%1” is not recognized – what you typed stays, and the "
                                   "event waits in “%2”. Examples: %3.")
                                    .arg(value.trimmed(), TimelineWords::trayName(),
                                         TimelineWords::examples()),
                          true);
            }
            break;
        }
        case Title:
            if (!value.trimmed().isEmpty() && value.trimmed() != event->title) {
                //: What Undo undoes in a timeline. In Polish: zmiana tytułu „%1”
                m_document.changeEvent(id, tr("changing the title of %1").arg(title),
                                       [&value](TimelineEvent& own) {
                                           own.title = value.trimmed();
                                       });
            }
            break;
        case Thread:
            //: What Undo undoes in a timeline. In Polish: zmiana wątku „%1”
            m_document.changeEvent(id, tr("changing the thread of %1").arg(title),
                                   [&value](TimelineEvent& own) { own.thread = value; });
            break;
        case Characters: {
            const QStringList characters = m_index->charactersOf(value);
            //: What Undo undoes in a timeline. In Polish: zmiana postaci „%1”
            m_document.changeEvent(id, tr("changing the characters of %1").arg(title),
                                   [&characters](TimelineEvent& own) {
                                       own.characters = characters;
                                   });
            break;
        }
        case Place: {
            const QString place =
                value.trimmed().isEmpty() ? QString() : m_index->placeOf(value.trimmed());
            //: What Undo undoes in a timeline. In Polish: zmiana miejsca „%1”
            m_document.changeEvent(id, tr("changing the place of %1").arg(title),
                                   [&place](TimelineEvent& own) { own.place = place; });
            break;
        }
        default:
            break;
    }
}

void TimelineList::closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint) {
    const QString added = m_newId;
    QTableWidget::closeEditor(editor, hint);
    if (!added.isEmpty() && hint == QAbstractItemDelegate::RevertModelCache) {
        // A new event left without a title goes away
        m_newId.clear();
        m_document.undo();
    }
    refresh();
}

void TimelineList::addAfterSelected() {
    QString message;
    const QString added =
        TimelineEdits::addAfter(m_document, m_document.selected(), std::nullopt, &message);
    if (!added.isEmpty()) {
        m_editAfter = added;
        refresh();
        emit told(message, true);
    }
}

void TimelineList::removeSelected() {
    QString message;
    const bool done = TimelineEdits::remove(m_document, m_document.selected(), &message);
    if (!message.isEmpty()) {
        emit told(message, done);
    }
}

// =============================================================================
// The keys and the menu
// =============================================================================

bool TimelineList::event(QEvent* event) {
    if (event->type() == QEvent::ShortcutOverride && state() != QAbstractItemView::EditingState) {
        auto* key = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers modifiers =
            key->modifiers() & ~(Qt::KeypadModifier | Qt::ShiftModifier);
        const int code = key->key();
        const bool own =
            (modifiers == Qt::NoModifier &&
             (code == Qt::Key_Delete || code == Qt::Key_Insert || code == Qt::Key_F2 ||
              code == Qt::Key_Return || code == Qt::Key_Enter || code == Qt::Key_Home ||
              code == Qt::Key_End)) ||
            (modifiers == Qt::ControlModifier &&
             (code == Qt::Key_Z || code == Qt::Key_Y || code == Qt::Key_Return ||
              code == Qt::Key_Enter));
        if (own) {
            event->accept();
            return true;
        }
    }
    return QTableWidget::event(event);
}

void TimelineList::keyPressEvent(QKeyEvent* event) {
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const bool control = (modifiers & Qt::ControlModifier) != 0;
    const bool shift = (modifiers & Qt::ShiftModifier) != 0;
    const int key = event->key();
    const QString id = currentId();
    if (key == Qt::Key_Insert) {
        addAfterSelected();
        return;
    }
    if (control && key == Qt::Key_Z && !shift) {
        const QString what = m_document.undoText();
        //: In Polish: Nie ma już czego cofnąć. / Cofnięto: %1.
        emit told(what.isEmpty() ? tr("There is nothing more to undo.")
                                 : tr("Undone: %1.").arg(what),
                  false);
        m_document.undo();
        return;
    }
    if (control && (key == Qt::Key_Y || (key == Qt::Key_Z && shift))) {
        const QString what = m_document.redoText();
        //: In Polish: Nie ma czego ponowić. / Ponowiono: %1.
        emit told(what.isEmpty() ? tr("There is nothing to redo.") : tr("Redone: %1.").arg(what),
                  false);
        m_document.redo();
        return;
    }
    if (!id.isEmpty() && (key == Qt::Key_Return || key == Qt::Key_Enter)) {
        if (control) {
            emit chapterRequested(id);
        } else {
            emit eventRequested(id);
        }
        return;
    }
    if (!id.isEmpty() && key == Qt::Key_Delete) {
        removeSelected();
        return;
    }
    if (!id.isEmpty() && currentColumn() == Book &&
        (key == Qt::Key_F2 || (!event->text().isEmpty() && event->text().at(0).isPrint()))) {
        // The chapters are chosen in the window "Event"
        emit eventRequested(id);
        return;
    }
    QTableWidget::keyPressEvent(event);
}

void TimelineList::contextMenuEvent(QContextMenuEvent* event) {
    const QTableWidgetItem* cell = itemAt(event->pos());
    const QString id = cell != nullptr ? cell->data(ID_ROLE).toString() : QString();
    QMenu menu(this);
    if (!id.isEmpty()) {
        m_document.select(id);
        //: In the menu of an event of a timeline. In Polish: Okno „Wydarzenie”…
        QAction* open = menu.addAction(tr("Event…"));
        showKeys(open, QKeySequence(Qt::Key_Return));
        connect(open, &QAction::triggered, this, [this, id]() { emit eventRequested(id); });
        const TimelineEvent* shown = m_document.timeline().event(id);
        //: In Polish: Otwórz rozdział
        QAction* chapter = menu.addAction(tr("Open the Chapter"));
        showKeys(chapter, QKeySequence(Qt::CTRL | Qt::Key_Return));
        chapter->setEnabled(shown != nullptr && !shown->chapters.isEmpty());
        connect(chapter, &QAction::triggered, this, [this, id]() { emit chapterRequested(id); });
        menu.addSeparator();
    }
    //: In Polish: Nowe wydarzenie / Nowe wydarzenie po nim
    QAction* add = menu.addAction(id.isEmpty() ? tr("New Event") : tr("New Event after It"));
    showKeys(add, QKeySequence(Qt::Key_Insert));
    connect(add, &QAction::triggered, this, &TimelineList::addAfterSelected);
    if (!id.isEmpty()) {
        TimelineEdits::addColors(menu, m_document, m_look, id);
        menu.addSeparator();
        //: In Polish: Usuń wydarzenie
        QAction* remove = menu.addAction(tr("Delete the Event"));
        showKeys(remove, QKeySequence(Qt::Key_Delete));
        connect(remove, &QAction::triggered, this, &TimelineList::removeSelected);
    }
    menu.exec(event->globalPos());
}

}  // namespace kalahari::timeline
