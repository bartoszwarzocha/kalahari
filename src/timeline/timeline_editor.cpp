// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_editor.cpp
/// @brief The tab of a timeline

#include <kalahari/timeline/timeline_editor.h>

#include <kalahari/core/art_provider.h>
#include <kalahari/core/book_project.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/theme_manager.h>
#include <kalahari/gui/widgets/card_bar.h>
#include <kalahari/gui/widgets/flow_layout.h>
#include <kalahari/gui/widgets/info_bar.h>
#include <kalahari/timeline/timeline_axis_view.h>
#include <kalahari/timeline/timeline_book_view.h>
#include <kalahari/timeline/timeline_dialogs.h>
#include <kalahari/timeline/timeline_document.h>
#include <kalahari/timeline/timeline_edits.h>
#include <kalahari/timeline/timeline_list.h>
#include <kalahari/timeline/timeline_look.h>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace kalahari::timeline {

namespace {

/// How long a message stays in the bar under the timeline
constexpr int MESSAGE_TIME = 10000;

/// The zoom of the axis that Ctrl+0 brings back, in percent
constexpr int FULL_ZOOM = 100;

constexpr int BAR_MARGIN = 6;
constexpr int BAR_SPACING = 6;
constexpr int GROUP_SPACING = 14;

/// A group of the bar: widgets that stay together when the bar wraps
QWidget* group(QWidget* parent, QHBoxLayout** row) {
    auto* box = new QWidget(parent);
    *row = new QHBoxLayout(box);
    (*row)->setContentsMargins(0, 0, 0, 0);
    (*row)->setSpacing(BAR_SPACING / 2);
    return box;
}

QLabel* barLabel(const QString& text, QWidget* field, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setBuddy(field);
    return label;
}

QToolButton* modeButton(const QString& text, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setCheckable(true);
    return button;
}

}  // namespace

TimelineEditor::TimelineEditor(const core::WindowEditorContext& context, QWidget* parent)
    : QWidget(parent),
      m_book(context.book),
      m_elementId(context.elementId),
      m_openSettings(context.openSettings),
      m_openElement(context.openElement),
      m_index(TimelineIndex::of(context.book)) {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    m_document = new TimelineDocument(
        element != nullptr ? m_book.filePathOf(*element) : QString(), this);
    QString error;
    if (!m_document->load(&error)) {
        core::Logger::getInstance().warn("TimelineEditor: {}", error.toStdString());
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_commands = new QWidget(this);
    m_commands->setObjectName(QStringLiteral("timelineCommands"));
    buildCommands(m_commands);
    layout->addWidget(m_commands);
    auto* bar = new QWidget(this);
    buildBar(bar);
    layout->addWidget(bar);

    m_stack = new QStackedWidget(this);
    m_axis = new TimelineAxisView(*m_document, m_stack);
    m_list = new TimelineList(*m_document, m_stack);
    m_bookView = new TimelineBookView(*m_document, m_stack);
    m_axis->setIndex(&m_index);
    m_list->setIndex(&m_index);
    m_bookView->setIndex(&m_index);
    m_stack->addWidget(m_axis);
    m_stack->addWidget(m_list);
    m_stack->addWidget(m_bookView);
    layout->addWidget(m_stack, 1);

    m_info = new gui::InfoBar(this);
    m_info->hide();
    layout->addWidget(m_info);
    m_infoTimer = new QTimer(this);
    m_infoTimer->setSingleShot(true);
    m_infoTimer->setInterval(MESSAGE_TIME);
    connect(m_infoTimer, &QTimer::timeout, m_info, &QWidget::hide);
    connect(m_info, &gui::InfoBar::dismissed, m_info, &QWidget::hide);
    connect(m_info, &gui::InfoBar::actionClicked, this, [this]() {
        m_info->hide();
        const QString what = m_document->undoText();
        m_document->undo();
        if (!what.isEmpty()) {
            //: In Polish: Cofnięto: %1.
            tell(tr("Undone: %1.").arg(what), false);
        }
    });

    connect(m_axis, &TimelineAxisView::told, this, &TimelineEditor::tell);
    connect(m_list, &TimelineList::told, this, &TimelineEditor::tell);
    connect(m_bookView, &TimelineBookView::told, this, &TimelineEditor::tell);
    connect(m_axis, &TimelineAxisView::eventRequested, this, &TimelineEditor::editEvent);
    connect(m_list, &TimelineList::eventRequested, this, &TimelineEditor::editEvent);
    connect(m_bookView, &TimelineBookView::eventRequested, this, &TimelineEditor::editEvent);
    connect(m_axis, &TimelineAxisView::chapterRequested, this, &TimelineEditor::openChapter);
    connect(m_list, &TimelineList::chapterRequested, this, &TimelineEditor::openChapter);
    connect(m_bookView, &TimelineBookView::chapterRequested, this, &TimelineEditor::openChapter);
    connect(m_bookView, &TimelineBookView::addRequested, this, &TimelineEditor::addEvent);
    connect(m_bookView, &TimelineBookView::deleteRequested, this, &TimelineEditor::deleteEvent);
    connect(m_axis, &TimelineAxisView::timelineRequested, this, &TimelineEditor::editTimeline);
    connect(m_axis, &TimelineAxisView::zoomChanged, this, &TimelineEditor::updateBar);
    connect(m_document, &TimelineDocument::changed, this, &TimelineEditor::updateBar);
    connect(m_document, &TimelineDocument::viewChanged, this, &TimelineEditor::updateBar);
    connect(m_document, &TimelineDocument::selectionChanged, this,
            &TimelineEditor::updateCommands);
    connect(m_document, &TimelineDocument::saveError, this, [this](const QString& why) {
        //: In Polish: Nie udało się zapisać osi czasu: %1
        tell(tr("The timeline could not be saved: %1").arg(why), false);
    });

    // The cards of the book, the settings and the theme
    connect(&m_book, &core::ProjectManager::elementRenamed, this, &TimelineEditor::refreshIndex);
    connect(&m_book, &core::ProjectManager::cardChanged, this, &TimelineEditor::refreshIndex);
    connect(&m_book, &core::ProjectManager::dirtyStateChanged, this,
            &TimelineEditor::refreshIndex);
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [guard = QPointer<TimelineEditor>(this)](const std::string& key) {
            if (key.rfind("timelines.", 0) == 0 || key == "ui.language") {
                QMetaObject::invokeMethod(
                    guard.data(),
                    [guard]() {
                        if (guard) {
                            guard->applyLook();
                        }
                    },
                    Qt::QueuedConnection);
            }
        });
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            &TimelineEditor::applyLook);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            &TimelineEditor::applyLook);

    // The view it opens on
    const OpenMode open = TimelineLook::openMode();
    const TimelineView::Mode mode = open == OpenMode::Axis   ? TimelineView::Mode::Axis
                                    : open == OpenMode::List ? TimelineView::Mode::List
                                    : open == OpenMode::Book ? TimelineView::Mode::Book
                                                             : m_document->timeline().view().mode;
    showMode(mode, false);
    updateBar();
}

TimelineEditor::~TimelineEditor() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

void TimelineEditor::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refreshIndex();
}

void TimelineEditor::refreshIndex() {
    TimelineIndex index = TimelineIndex::of(m_book);
    const auto same = [](const QList<TimelineIndex::Entry>& a,
                         const QList<TimelineIndex::Entry>& b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (qsizetype i = 0; i < a.size(); ++i) {
            if (a.at(i).id != b.at(i).id || a.at(i).name != b.at(i).name ||
                a.at(i).color != b.at(i).color) {
                return false;
            }
        }
        return true;
    };
    if (same(index.characters, m_index.characters) && same(index.places, m_index.places) &&
        same(index.chapters, m_index.chapters)) {
        return;
    }
    m_index = index;
    m_axis->setIndex(&m_index);
    m_list->setIndex(&m_index);
    m_bookView->setIndex(&m_index);
    fillHighlight();
}

// =============================================================================
// The commands above the timeline
// =============================================================================

void TimelineEditor::buildCommands(QWidget* row) {
    auto* flow = new gui::FlowLayout(row, GROUP_SPACING, BAR_SPACING / 2);
    flow->setContentsMargins(BAR_MARGIN, BAR_MARGIN / 2, BAR_MARGIN, 0);
    const auto add = [this](Command which, const char* iconId, const QString& name,
                            QKeyCombination keys) {
        QAction* action = gui::CardBar::command(QString::fromLatin1(iconId), name, keys, this);
        m_actions.at(static_cast<std::size_t>(which)) = action;
        return action;
    };
    const auto key = [](Qt::Key pressed, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        return QKeyCombination(modifiers, pressed);
    };
    //: A command of the bar of a timeline. In Polish: Nowe wydarzenie
    add(Command::New, "common.add", tr("New Event"), key(Qt::Key_Insert));
    //: The command of the bar of a timeline that opens the window "Event" of the event selected.
    //: In Polish: Okno „Wydarzenie”…
    add(Command::Edit, "edit.rename", tr("Event…"), key(Qt::Key_Return));
    //: In Polish: Otwórz rozdział
    add(Command::Chapter, "template.chapter", tr("Open the Chapter"),
        key(Qt::Key_Return, Qt::ControlModifier));
    //: The event a step earlier on the axis. In Polish: Wcześniej
    add(Command::Earlier, "common.arrowBack", tr("Earlier"), key(Qt::Key_Left, Qt::ControlModifier));
    //: The event a step later on the axis. In Polish: Później
    add(Command::Later, "common.arrowForward", tr("Later"), key(Qt::Key_Right, Qt::ControlModifier));
    //: In Polish: Do pasma wyżej
    add(Command::LaneUp, "common.arrowUpward", tr("To the Lane Above"),
        key(Qt::Key_Up, Qt::ControlModifier));
    //: In Polish: Do pasma niżej
    add(Command::LaneDown, "common.arrowDownward", tr("To the Lane Below"),
        key(Qt::Key_Down, Qt::ControlModifier));
    //: In Polish: Usuń wydarzenie
    add(Command::Delete, "common.delete", tr("Delete the Event"), key(Qt::Key_Delete));
    //: The command of the bar of a timeline. In Polish: Cofnij
    add(Command::Undo, "edit.undo", tr("Undo"), key(Qt::Key_Z, Qt::ControlModifier));
    //: The command of the bar of a timeline. In Polish: Ponów
    add(Command::Redo, "edit.redo", tr("Redo"), key(Qt::Key_Y, Qt::ControlModifier));

    QHBoxLayout* line = nullptr;
    QWidget* events = group(row, &line);
    line->addWidget(gui::CardBar::button(command(Command::New), "timelineNewEvent", true, events));
    line->addWidget(gui::CardBar::button(command(Command::Edit), "timelineEditEvent", false, events));
    line->addWidget(
        gui::CardBar::button(command(Command::Chapter), "timelineOpenChapter", false, events));
    flow->addWidget(events);
    m_moveBox = group(row, &line);
    line->addWidget(gui::CardBar::button(command(Command::Earlier), "timelineEarlier", false, m_moveBox));
    line->addWidget(gui::CardBar::button(command(Command::Later), "timelineLater", false, m_moveBox));
    line->addWidget(gui::CardBar::button(command(Command::LaneUp), "timelineLaneUp", false, m_moveBox));
    line->addWidget(
        gui::CardBar::button(command(Command::LaneDown), "timelineLaneDown", false, m_moveBox));
    flow->addWidget(m_moveBox);
    flow->addWidget(gui::CardBar::button(command(Command::Delete), "timelineDeleteEvent", true, row));
    QWidget* steps = group(row, &line);
    line->addWidget(gui::CardBar::button(command(Command::Undo), "timelineUndo", false, steps));
    line->addWidget(gui::CardBar::button(command(Command::Redo), "timelineRedo", false, steps));
    flow->addWidget(steps);

    // A title or a cell being typed is kept before the command
    const auto run = [this](Command which, const std::function<void()>& what) {
        connect(command(which), &QAction::triggered, this, [this, what]() {
            finishTyping();
            what();
        });
    };
    run(Command::New, [this]() { addEvent(); });
    run(Command::Edit, [this]() { editEvent(m_document->selected()); });
    run(Command::Chapter, [this]() { openChapter(m_document->selected()); });
    run(Command::Earlier, [this]() { m_axis->shiftSelected(-1); });
    run(Command::Later, [this]() { m_axis->shiftSelected(1); });
    run(Command::LaneUp, [this]() { m_axis->moveSelectedToLane(-1); });
    run(Command::LaneDown, [this]() { m_axis->moveSelectedToLane(1); });
    run(Command::Delete, [this]() { deleteEvent(); });
    run(Command::Undo, [this]() { undo(true); });
    run(Command::Redo, [this]() { undo(false); });
}

void TimelineEditor::finishTyping() {
    QWidget* shown = m_stack->currentWidget();
    const QWidget* focus = QApplication::focusWidget();
    if (focus != nullptr && focus != shown && shown->isAncestorOf(focus)) {
        shown->setFocus(Qt::OtherFocusReason);
    }
}

QAction* TimelineEditor::command(Command which) const {
    return m_actions.at(static_cast<std::size_t>(which));
}

TimelineView::Mode TimelineEditor::shownMode() const {
    return m_stack->currentWidget() == m_list       ? TimelineView::Mode::List
           : m_stack->currentWidget() == m_bookView ? TimelineView::Mode::Book
                                                    : TimelineView::Mode::Axis;
}

void TimelineEditor::updateCommands() {
    if (m_stack == nullptr) {
        return;  // still building
    }
    const Timeline& timeline = m_document->timeline();
    const TimelineEvent* event = timeline.event(m_document->selected());
    const TimelineView::Mode mode = shownMode();
    const bool ordered = timeline.view().scale == TimelineView::Scale::Order;
    const bool oneLane = timeline.view().lanes == TimelineView::Lanes::One;
    command(Command::Edit)->setEnabled(event != nullptr);
    command(Command::Chapter)->setEnabled(event != nullptr && !event->chapters.isEmpty());
    const bool moves = event != nullptr && (ordered || event->time.has_value());
    command(Command::Earlier)->setEnabled(moves);
    command(Command::Later)->setEnabled(moves);
    const auto name = [](QAction* action, const QString& text, Qt::Key key) {
        action->setText(text);
        action->setToolTip(gui::CardBar::withKeys(text, QKeyCombination(Qt::ControlModifier, key)));
    };
    //: The command of the bar of a timeline in the order of the story. In Polish: Wcześniej w
    //: kolejności / Wcześniej
    name(command(Command::Earlier), ordered ? tr("Earlier in the Order") : tr("Earlier"),
         Qt::Key_Left);
    //: In Polish: Później w kolejności / Później
    name(command(Command::Later), ordered ? tr("Later in the Order") : tr("Later"), Qt::Key_Right);
    command(Command::LaneUp)->setEnabled(event != nullptr && !oneLane);
    command(Command::LaneDown)->setEnabled(event != nullptr && !oneLane);
    command(Command::Delete)->setEnabled(event != nullptr);
    command(Command::Undo)->setEnabled(m_document->canUndo());
    command(Command::Redo)->setEnabled(m_document->canRedo());
    // The steps of Earlier and Later and the lanes are those of the axis
    m_moveBox->setVisible(mode == TimelineView::Mode::Axis);
}

void TimelineEditor::addEvent() {
    switch (shownMode()) {
        case TimelineView::Mode::Axis:
            m_axis->addAfterSelected();
            return;
        case TimelineView::Mode::List:
            m_list->addAfterSelected();
            return;
        case TimelineView::Mode::Book:
            break;
    }
    // The time and the book: the new event in its window, to give it its time and chapters
    QString message;
    const QString added = TimelineEdits::addAfter(*m_document, m_document->selected(),
                                                  std::nullopt, &message);
    if (added.isEmpty()) {
        return;
    }
    if (!editEvent(added)) {
        m_document->undo();
    }
}

void TimelineEditor::deleteEvent() {
    switch (shownMode()) {
        case TimelineView::Mode::Axis:
            m_axis->removeSelected();
            return;
        case TimelineView::Mode::List:
            m_list->removeSelected();
            return;
        case TimelineView::Mode::Book:
            break;
    }
    QString message;
    const bool done = TimelineEdits::remove(*m_document, m_document->selected(), &message);
    tell(message, done);
}

void TimelineEditor::undo(bool back) {
    const QString what = back ? m_document->undoText() : m_document->redoText();
    if (what.isEmpty()) {
        return;
    }
    if (back) {
        m_document->undo();
        //: In Polish: Cofnięto: %1.
        tell(tr("Undone: %1.").arg(what), false);
    } else {
        m_document->redo();
        //: In Polish: Ponowiono: %1.
        tell(tr("Redone: %1.").arg(what), false);
    }
}

// =============================================================================
// The bar above the timeline
// =============================================================================

void TimelineEditor::buildBar(QWidget* bar) {
    auto* flow = new gui::FlowLayout(bar, GROUP_SPACING, BAR_SPACING / 2);
    flow->setContentsMargins(BAR_MARGIN, BAR_MARGIN / 2, BAR_MARGIN, BAR_MARGIN / 2);
    QHBoxLayout* row = nullptr;

    // View: the axis, the list, the time and the book
    QWidget* views = group(bar, &row);
    //: The view of a timeline: its events on the axis. In Polish: Oś
    m_axisButton = modeButton(tr("Axis"), views);
    //: The view of a timeline: a table. In Polish: Lista
    m_listButton = modeButton(tr("List"), views);
    //: The view of a timeline: the chapters against the time. In Polish: Czas i książka
    m_bookButton = modeButton(tr("Time and the Book"), views);
    auto* modes = new QButtonGroup(views);
    modes->setExclusive(true);
    //: In the bar of a timeline. In Polish: Widok:
    row->addWidget(barLabel(tr("View:"), m_axisButton, views));
    for (QToolButton* button : {m_axisButton, m_listButton, m_bookButton}) {
        modes->addButton(button);
        row->addWidget(button);
    }
    flow->addWidget(views);
    connect(m_axisButton, &QToolButton::clicked, this,
            [this]() { showMode(TimelineView::Mode::Axis, true); });
    connect(m_listButton, &QToolButton::clicked, this,
            [this]() { showMode(TimelineView::Mode::List, true); });
    connect(m_bookButton, &QToolButton::clicked, this,
            [this]() { showMode(TimelineView::Mode::Book, true); });

    // Scale
    QWidget* scales = group(bar, &row);
    m_scale = new QComboBox(scales);
    for (const TimelineView::Scale scale : {TimelineView::Scale::Dates, TimelineView::Scale::Days,
                                            TimelineView::Scale::Order}) {
        m_scale->addItem(TimelineWords::scaleName(scale), TimelineViewNames::of(scale));
    }
    //: In the bar of a timeline. In Polish: Skala:
    row->addWidget(barLabel(tr("Scale:"), m_scale, scales));
    row->addWidget(m_scale);
    flow->addWidget(scales);
    connect(m_scale, &QComboBox::activated, this, [this]() {
        if (const auto scale = TimelineViewNames::scale(m_scale->currentData().toString())) {
            changeScale(*scale);
        }
    });

    // Lanes
    m_lanesBox = group(bar, &row);
    m_lanes = new QComboBox(m_lanesBox);
    for (const TimelineView::Lanes lanes :
         {TimelineView::Lanes::Threads, TimelineView::Lanes::Characters,
          TimelineView::Lanes::Places, TimelineView::Lanes::One}) {
        m_lanes->addItem(TimelineWords::lanesName(lanes), TimelineViewNames::of(lanes));
    }
    //: In the bar of a timeline. In Polish: Pasma:
    row->addWidget(barLabel(tr("Lanes:"), m_lanes, m_lanesBox));
    row->addWidget(m_lanes);
    flow->addWidget(m_lanesBox);
    connect(m_lanes, &QComboBox::activated, this, [this]() {
        if (const auto lanes = TimelineViewNames::lanes(m_lanes->currentData().toString())) {
            m_document->changeView([lanes](TimelineView& view) { view.lanes = *lanes; });
        }
    });

    // Cards or labels, and the gaps
    m_lookBox = group(bar, &row);
    //: How a timeline shows its events. In Polish: Karty / Etykiety
    m_cardsButton = modeButton(tr("Cards"), m_lookBox);
    m_labelsButton = modeButton(tr("Labels"), m_lookBox);
    auto* looks = new QButtonGroup(m_lookBox);
    looks->setExclusive(true);
    looks->addButton(m_cardsButton);
    looks->addButton(m_labelsButton);
    row->addWidget(m_cardsButton);
    row->addWidget(m_labelsButton);
    flow->addWidget(m_lookBox);
    connect(m_cardsButton, &QToolButton::clicked, this, [this]() {
        m_document->changeView([](TimelineView& view) { view.look = TimelineView::Look::Cards; });
    });
    connect(m_labelsButton, &QToolButton::clicked, this, [this]() {
        m_document->changeView([](TimelineView& view) { view.look = TimelineView::Look::Labels; });
    });
    //: In the bar of a timeline: long gaps without events are short. In Polish: Skracaj przerwy
    m_compress = new QCheckBox(tr("Compress gaps"), bar);
    flow->addWidget(m_compress);
    connect(m_compress, &QCheckBox::clicked, this, [this](bool on) {
        m_document->changeView([on](TimelineView& view) { view.compress = on; });
    });

    // Highlight
    QWidget* highlights = group(bar, &row);
    m_highlight = new QComboBox(highlights);
    m_highlight->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: In the bar of a timeline. In Polish: Wyróżnij:
    row->addWidget(barLabel(tr("Highlight:"), m_highlight, highlights));
    row->addWidget(m_highlight);
    flow->addWidget(highlights);
    connect(m_highlight, &QComboBox::activated, this, [this]() {
        const QString filter = m_highlight->currentData().toString();
        m_axis->setHighlight(filter);
        m_list->setHighlight(filter);
        m_bookView->setHighlight(filter);
    });

    // Zoom, on the axis of time only
    m_zoomBox = group(bar, &row);
    auto* out = new QToolButton(m_zoomBox);
    out->setText(QStringLiteral("−"));
    //: In Polish: Pomniejsz (Ctrl+−)
    out->setToolTip(tr("Zoom Out (Ctrl+-)"));
    //: In Polish: Pomniejsz
    out->setAccessibleName(tr("Zoom Out"));
    m_zoomLabel = new QLabel(m_zoomBox);
    m_zoomLabel->setMinimumWidth(
        m_zoomLabel->fontMetrics().horizontalAdvance(QStringLiteral("400%")));
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    auto* in = new QToolButton(m_zoomBox);
    in->setText(QStringLiteral("+"));
    //: In Polish: Powiększ (Ctrl++)
    in->setToolTip(tr("Zoom In (Ctrl++)"));
    //: In Polish: Powiększ
    in->setAccessibleName(tr("Zoom In"));
    row->addWidget(out);
    row->addWidget(m_zoomLabel);
    row->addWidget(in);
    flow->addWidget(m_zoomBox);
    connect(out, &QToolButton::clicked, this, [this]() { m_axis->zoomBy(-1); });
    connect(in, &QToolButton::clicked, this, [this]() { m_axis->zoomBy(1); });

    //: The button of the bar of a timeline. In Polish: Dzień 1 i wątki…
    auto* threads = new QToolButton(bar);
    threads->setText(tr("Day One and Threads…"));
    flow->addWidget(threads);
    connect(threads, &QToolButton::clicked, this, &TimelineEditor::editTimeline);

    // The numbers and the gear: Settings > Timelines
    QWidget* tail = group(bar, &row);
    m_counts = new QLabel(tail);
    row->addWidget(m_counts);
    m_gear = new QToolButton(tail);
    m_gear->setAutoRaise(true);
    m_gear->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("dock.settings")));
    //: The gear of a timeline. In Polish: Ustawienia osi czasu
    m_gear->setToolTip(tr("Timeline Settings"));
    m_gear->setAccessibleName(m_gear->toolTip());
    row->addWidget(m_gear);
    flow->addWidget(tail);
    connect(m_gear, &QToolButton::clicked, this, [this]() {
        if (m_openSettings) {
            m_openSettings();
        }
    });
    fillHighlight();
}

void TimelineEditor::fillHighlight() {
    const QString current = m_highlight->currentData().toString();
    m_highlight->clear();
    //: Highlight in a timeline: nothing. In Polish: nic
    m_highlight->addItem(tr("nothing"), QString());
    const Timeline& timeline = m_document->timeline();
    for (const TimelineThread& thread : timeline.threads()) {
        //: Highlight in a timeline. In Polish: wątek: %1
        m_highlight->addItem(tr("thread: %1").arg(thread.name),
                             QStringLiteral("thread:") + thread.id);
    }
    QStringList characters;
    for (const TimelineIndex::Entry& entry : m_index.characters) {
        characters.append(entry.id);
    }
    QStringList places;
    for (const TimelineIndex::Entry& entry : m_index.places) {
        places.append(entry.id);
    }
    for (const TimelineEvent* event : timeline.eventsInOrder()) {
        for (const QString& character : event->characters) {
            if (!characters.contains(character)) {
                characters.append(character);
            }
        }
        if (!event->place.isEmpty() && !places.contains(event->place)) {
            places.append(event->place);
        }
    }
    for (const QString& character : characters) {
        //: Highlight in a timeline. In Polish: postać: %1
        m_highlight->addItem(tr("character: %1").arg(m_index.nameOf(character)),
                             QStringLiteral("character:") + character);
    }
    for (const QString& place : places) {
        //: Highlight in a timeline. In Polish: miejsce: %1
        m_highlight->addItem(tr("place: %1").arg(m_index.nameOf(place)),
                             QStringLiteral("place:") + place);
    }
    //: Highlight in a timeline: the events no chapter tells. In Polish: poza książką
    m_highlight->addItem(tr("not in the book"), QStringLiteral("unbooked"));
    m_highlight->setCurrentIndex(std::max(0, m_highlight->findData(current)));
}

void TimelineEditor::showMode(TimelineView::Mode mode, bool keep) {
    QWidget* shown = mode == TimelineView::Mode::List   ? static_cast<QWidget*>(m_list)
                     : mode == TimelineView::Mode::Book ? static_cast<QWidget*>(m_bookView)
                                                        : static_cast<QWidget*>(m_axis);
    m_stack->setCurrentWidget(shown);
    setFocusProxy(shown);
    if (keep && m_document->timeline().view().mode != mode) {
        m_document->changeView([mode](TimelineView& view) { view.mode = mode; });
    }
    updateBar();
    if (keep) {
        shown->setFocus(Qt::OtherFocusReason);
    }
}

void TimelineEditor::changeScale(TimelineView::Scale scale) {
    if (scale == m_document->timeline().view().scale) {
        return;
    }
    m_document->changeView([scale](TimelineView& view) { view.scale = scale; });
    if (scale == TimelineView::Scale::Order) {
        //: In Polish: Skala „Kolejność” pokazuje wydarzenia w kolejności opowieści. Ich czas
        //: zostaje w pliku i wraca po powrocie do dat albo dni.
        tell(tr("The scale “Order” shows the events in the order of the story. Their times "
                "stay in the file and come back with dates or days."),
             false);
        return;
    }
    const auto waiting = static_cast<int>(m_document->timeline().eventsWithoutTime().size());
    if (waiting > 0) {
        //: In Polish: %n wydarzenie czeka / wydarzenia czekają / wydarzeń czeka na pasku „%1” –
        //: przeciągnij je na oś albo wpisz czas.
        tell(tr("%n event(s) wait in “%1” – drag them onto the axis or give them a time.",
                nullptr, waiting)
                 .arg(TimelineWords::trayName()),
             false);
    }
}

void TimelineEditor::updateBar() {
    const Timeline& timeline = m_document->timeline();
    const TimelineView& view = timeline.view();
    const TimelineView::Mode mode = shownMode();
    m_axisButton->setChecked(mode == TimelineView::Mode::Axis);
    m_listButton->setChecked(mode == TimelineView::Mode::List);
    m_bookButton->setChecked(mode == TimelineView::Mode::Book);
    m_scale->setCurrentIndex(std::max(0, m_scale->findData(TimelineViewNames::of(view.scale))));
    m_lanes->setCurrentIndex(std::max(0, m_lanes->findData(TimelineViewNames::of(view.lanes))));
    m_cardsButton->setChecked(view.look == TimelineView::Look::Cards);
    m_labelsButton->setChecked(view.look == TimelineView::Look::Labels);
    m_compress->setChecked(view.compress);
    const bool ordered = view.scale == TimelineView::Scale::Order;
    m_lanesBox->setVisible(mode != TimelineView::Mode::Book);
    m_lookBox->setVisible(mode == TimelineView::Mode::Axis);
    m_compress->setVisible(mode != TimelineView::Mode::Book && !ordered);
    m_zoomBox->setVisible(mode == TimelineView::Mode::Axis && !ordered);
    m_zoomLabel->setText(
        QStringLiteral("%1%").arg(m_axis != nullptr ? m_axis->zoomPercent() : FULL_ZOOM));
    const auto events = static_cast<int>(timeline.events().size());
    const auto threads = static_cast<int>(timeline.threads().size());
    //: The numbers in the bar of a timeline. In Polish: %n wydarzenie / wydarzenia / wydarzeń
    const QString eventText = tr("%n event(s)", nullptr, events);
    //: In Polish: %n wątek / wątki / wątków
    const QString threadText = tr("%n thread(s)", nullptr, threads);
    m_counts->setText(eventText + QStringLiteral(" · ") + threadText);
    fillHighlight();
    updateCommands();
}

void TimelineEditor::applyLook() {
    const TimelineLook look = TimelineLook::current();
    m_axis->setLook(look);
    m_list->setLook(look);
    m_bookView->setLook(look);
}

void TimelineEditor::tell(const QString& message, bool undoable) {
    if (message.isEmpty()) {
        return;
    }
    m_info->setMessage(message);
    //: The button of the bar under a timeline. In Polish: Cofnij
    m_info->setActionText(undoable && m_document->canUndo() ? tr("Undo") : QString());
    m_info->show();
    m_infoTimer->start();
}

// =============================================================================
// The windows, the chapters
// =============================================================================

bool TimelineEditor::editEvent(const QString& id) {
    const TimelineEvent* event = m_document->timeline().event(id);
    if (event == nullptr) {
        return false;
    }
    TimelineEventDialog dialog(m_document->timeline(), *event, m_index, m_axis->look(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const TimelineEvent changed = dialog.result();
    const QString title = TimelineEdits::quoted(changed.title);
    //: What Undo undoes in a timeline. In Polish: zmiana wydarzenia „%1”
    const bool done = m_document->changeEvent(id, tr("changing the event %1").arg(title),
                                              [&changed](TimelineEvent& own) { own = changed; });
    if (!done) {
        return true;  // accepted as it was
    }
    const bool ordered = m_document->timeline().view().scale == TimelineView::Scale::Order;
    if (!ordered && !changed.time) {
        //: In Polish: Zapisano „%1”; bez czasu czeka na pasku „%2”.
        tell(tr("Saved %1; without a time it waits in “%2”.")
                 .arg(title, TimelineWords::trayName()),
             true);
    } else {
        //: In Polish: Zapisano wydarzenie „%1”.
        tell(tr("Saved the event %1.").arg(title), true);
    }
    return true;
}

void TimelineEditor::editTimeline() {
    TimelinePropertiesDialog dialog(m_document->timeline(), m_axis->look(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const qint64 dayOne = dialog.dayOne();
    const QList<TimelineThread> threads = dialog.threads();
    //: What Undo undoes in a timeline. In Polish: zmiana dnia 1 i wątków
    const bool done = m_document->change(tr("changing day one and the threads"),
                                         [dayOne, &threads](Timeline& timeline) {
        timeline.setDayOne(dayOne);
        QStringList kept;
        for (const TimelineThread& thread : threads) {
            if (!thread.id.isEmpty()) {
                kept.append(thread.id);
            }
        }
        QStringList gone;
        for (const TimelineThread& thread : timeline.threads()) {
            if (!kept.contains(thread.id)) {
                gone.append(thread.id);
            }
        }
        for (const QString& id : gone) {
            timeline.removeThread(id);
        }
        for (const TimelineThread& thread : threads) {
            if (thread.id.isEmpty()) {
                timeline.addThread(thread.name, thread.color);
            } else {
                timeline.setThread(thread);
            }
        }
        return true;
    });
    if (done) {
        //: In Polish: Zapisano dzień 1 i wątki.
        tell(tr("Saved day one and the threads."), true);
    }
}

void TimelineEditor::openChapter(const QString& id) {
    const TimelineEvent* event = m_document->timeline().event(id);
    if (event == nullptr) {
        return;
    }
    QString first;
    QStringList others;
    for (const QString& chapter : event->chapters) {
        const core::ProjectElement* element = m_book.findElement(chapter);
        if (element == nullptr) {
            continue;
        }
        if (first.isEmpty()) {
            first = chapter;
        } else {
            others.append(element->title);
        }
    }
    if (first.isEmpty()) {
        //: In Polish: To wydarzenie nie jest jeszcze w żadnym rozdziale. Wybierz rozdziały w
        //: oknie „Wydarzenie” (Enter).
        tell(tr("This event is not in any chapter yet. Choose its chapters in the window "
                "“Event” (Enter)."),
             false);
        return;
    }
    if (m_openElement) {
        m_openElement(first);
    }
    if (!others.isEmpty()) {
        //: In Polish: Wydarzenie jest też w: %1.
        tell(tr("The event is also in: %1.").arg(others.join(QStringLiteral(", "))), false);
    }
}

}  // namespace kalahari::timeline
