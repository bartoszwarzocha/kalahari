// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plan_chronology_view.cpp
/// @brief The chronology of the Book Plan: its options, its commands, its cards and its note

#include <kalahari/timeline/plan_chronology_view.h>

#include <kalahari/core/art_provider.h>
#include <kalahari/gui/widgets/card_bar.h>
#include <kalahari/gui/widgets/flow_layout.h>
#include <kalahari/timeline/timeline_index.h>

#include <QAction>
#include <QComboBox>
#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::timeline {

namespace {

constexpr int BAR_MARGIN = 6;
constexpr int BAR_SPACING = 6;
constexpr int GROUP_SPACING = 14;
constexpr int NOTE_MARGIN_X = 10;
constexpr int NOTE_MARGIN_Y = 4;
constexpr const char* PERIODS = "periods";

using Command = PlanChronologyView::Command;

QWidget* group(QWidget* parent, QHBoxLayout** row) {
    auto* box = new QWidget(parent);
    *row = new QHBoxLayout(box);
    (*row)->setContentsMargins(0, 0, 0, 0);
    (*row)->setSpacing(BAR_SPACING / 2);
    return box;
}

QKeyCombination keys(Qt::Key pressed, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    return QKeyCombination(modifiers, pressed);
}

}  // namespace

PlanChronologyView::PlanChronologyView(QWidget* parent)
    : gui::PlanChronology(parent), m_look(TimelineLook::current()) {
    setObjectName(QStringLiteral("planChronology"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* options = new QWidget(this);
    options->setObjectName(QStringLiteral("planChronologyOptions"));
    buildOptions(options);
    layout->addWidget(options);
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("planChronologyCommands"));
    buildCommands(bar);
    layout->addWidget(bar);
    m_grid = new PlanChronologyGrid(this);
    //: The line of the cards of the chronology without a time. In Polish: Bez czasu akcji:
    m_grid->setUntimedTitle(tr("Without a time of the action:"));
    layout->addWidget(m_grid, 1);
    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("planChronologyNote"));
    m_note->setWordWrap(true);
    m_note->setForegroundRole(QPalette::PlaceholderText);
    m_note->setContentsMargins(NOTE_MARGIN_X, NOTE_MARGIN_Y, NOTE_MARGIN_X, NOTE_MARGIN_Y);
    //: The note under the chronology. In Polish: Numer na karcie to jej miejsce w kolejności
    //: czytania: 3.2 – tekst 3, scena 2. Obwódka oznacza retrospekcję: scenę opowiedzianą po
    //: scenie, która dzieje się później. Przerywana ramka – karta w planie albo wydarzenie
    //: poza tekstem.
    m_note->setText(tr("The number on a card is its place in the order of reading: 3.2 – text "
                       "3, scene 2. A ring marks a flashback: a scene told after a scene that "
                       "happens later. A dashed frame – a card in the plan or an event outside "
                       "the text."));
    layout->addWidget(m_note);

    connect(m_grid, &PlanChronologyGrid::currentChanged, this, [this]() {
        const ChronologyItem* item = currentItem();
        m_wanted = item != nullptr ? item->id : QString();
        updateCommands();
        emit currentChanged();
    });
    connect(m_grid, &PlanChronologyGrid::activated, this, [this](int item) {
        const ChronologyItem* chosen = m_layout ? &m_layout->items().at(item) : nullptr;
        runCommand(chosen != nullptr && chosen->outside ? Command::Edit : Command::Open);
    });
    connect(m_grid, &PlanChronologyGrid::menuRequested, this, &PlanChronologyView::showMenu);
    m_grid->installEventFilter(this);

    // A plan of its own until the board gives it the book's
    TimelineView view;
    view.scale = TimelineView::Scale::Days;
    view.lanes = TimelineView::Lanes::Characters;
    m_plan.setView(view);
    m_plan.setDayOne(QDate::currentDate().toJulianDay());
    refresh();
}

QAction* PlanChronologyView::command(Command which) const {
    return m_commands.at(static_cast<std::size_t>(which));
}

// =============================================================================
// Building
// =============================================================================

void PlanChronologyView::buildOptions(QWidget* row) {
    auto* flow = new gui::FlowLayout(row, BAR_SPACING, BAR_SPACING / 2);
    flow->setContentsMargins(BAR_MARGIN, BAR_MARGIN / 2, BAR_MARGIN, 0);
    //: The scale of the chronology. In Polish: S&kala:
    auto* scaleLabel = new QLabel(tr("S&cale:"), row);
    m_scale = new QComboBox(row);
    m_scale->setObjectName(QStringLiteral("planChronologyScale"));
    scaleLabel->setBuddy(m_scale);
    //: In Polish: Dni opowieści
    m_scale->addItem(tr("Days of the story"), static_cast<int>(TimelineView::Scale::Days));
    //: In Polish: Daty
    m_scale->addItem(tr("Dates"), static_cast<int>(TimelineView::Scale::Dates));
    //: The order of the story, without lengths of time. In Polish: Kolejność
    m_scale->addItem(tr("Order"), static_cast<int>(TimelineView::Scale::Order));
    flow->addWidget(scaleLabel);
    flow->addWidget(m_scale);
    connect(m_scale, &QComboBox::activated, this, [this](int index) {
        const auto scale = static_cast<TimelineView::Scale>(m_scale->itemData(index).toInt());
        changeView([scale](TimelineView& view) { view.scale = scale; });
    });

    //: The bands of the chronology. In Polish: &Pasma:
    auto* lanesLabel = new QLabel(tr("&Bands:"), row);
    m_lanes = new QComboBox(row);
    m_lanes->setObjectName(QStringLiteral("planChronologyLanes"));
    lanesLabel->setBuddy(m_lanes);
    //: In Polish: Punkt widzenia
    m_lanes->addItem(tr("Point of view"), static_cast<int>(TimelineView::Lanes::Characters));
    //: In Polish: Miejsca
    m_lanes->addItem(tr("Places"), static_cast<int>(TimelineView::Lanes::Places));
    //: In Polish: Jedno pasmo
    m_lanes->addItem(tr("One band"), static_cast<int>(TimelineView::Lanes::One));
    flow->addWidget(lanesLabel);
    flow->addWidget(m_lanes);
    connect(m_lanes, &QComboBox::activated, this, [this](int index) {
        const auto lanes = static_cast<TimelineView::Lanes>(m_lanes->itemData(index).toInt());
        changeView([lanes](TimelineView& view) { view.lanes = lanes; });
    });

    m_compress = new QToolButton(row);
    m_compress->setObjectName(QStringLiteral("planChronologyCompress"));
    //: The switch of the chronology: a gap without cards is one narrow column. In Polish:
    //: Skracaj przerwy
    m_compress->setText(tr("Shorten Gaps"));
    //: In Polish: Przerwa bez kart to jedna wąska kolumna z jej długością; bez tego dni
    //: przerwy do miesiąca są pustymi kolumnami.
    m_compress->setToolTip(tr("A gap without cards is one narrow column with its length; "
                              "without it the days of a gap of up to a month are empty "
                              "columns."));
    m_compress->setCheckable(true);
    m_compress->setAutoRaise(true);
    m_compress->setToolButtonStyle(Qt::ToolButtonTextOnly);
    flow->addWidget(m_compress);
    connect(m_compress, &QToolButton::clicked, this, [this](bool checked) {
        changeView([checked](TimelineView& view) { view.compress = checked; });
    });

    m_periodsButton = new QToolButton(row);
    m_periodsButton->setObjectName(QStringLiteral("planChronologyPeriods"));
    m_periodsButton->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("plotBoard.periods")));
    //: Day one and the periods of the chronology. In Polish: &Okresy…
    m_periodsButton->setText(tr("&Periods…"));
    //: In Polish: Dzień pierwszy opowieści i własne nazwy okresów („Wyprawa” to „zima 1931”)
    m_periodsButton->setToolTip(tr("Day one of the story and names of your own for periods "
                                   "(“Expedition” is “winter 1931”)"));
    m_periodsButton->setAutoRaise(true);
    m_periodsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    flow->addWidget(m_periodsButton);
    connect(m_periodsButton, &QToolButton::clicked, this, &PlanChronologyView::editPeriods);

    m_totals = new QLabel(row);
    m_totals->setObjectName(QStringLiteral("planChronologyTotals"));
    m_totals->setForegroundRole(QPalette::PlaceholderText);
    flow->addWidget(m_totals);
}

void PlanChronologyView::buildCommands(QWidget* bar) {
    auto* flow = new gui::FlowLayout(bar, GROUP_SPACING, BAR_SPACING / 2);
    flow->setContentsMargins(BAR_MARGIN, BAR_MARGIN / 2, BAR_MARGIN, 0);
    const auto add = [this](Command which, const char* iconId, const QString& name,
                            QKeyCombination combination) {
        QAction* action =
            gui::CardBar::command(QString::fromLatin1(iconId), name, combination, this);
        m_commands.at(static_cast<std::size_t>(which)) = action;
        connect(action, &QAction::triggered, this, [this, which]() { runCommand(which); });
    };
    //: A command of the bar of the chronology. In Polish: Nowe wydarzenie poza tekstem
    add(Command::NewEvent, "plotBoard.outside", tr("New Event Outside the Text"),
        keys(Qt::Key_Insert));
    //: In Polish: Edytuj kartę
    add(Command::Edit, "edit.rename", tr("Edit the Card"), keys(Qt::Key_F2));
    //: In Polish: Otwórz tekst
    add(Command::Open, "template.chapter", tr("Open the Text"), keys(Qt::Key_Return));
    //: The time of the card a day (a month, a season...) earlier. In Polish: Dzień wcześniej
    add(Command::Earlier, "common.arrowBack", tr("A Day Earlier"),
        keys(Qt::Key_Left, Qt::ControlModifier));
    //: In Polish: Dzień później
    add(Command::Later, "common.arrowForward", tr("A Day Later"),
        keys(Qt::Key_Right, Qt::ControlModifier));
    //: In Polish: Usuń
    add(Command::Delete, "common.delete", tr("Delete"), keys(Qt::Key_Delete));
    //: In Polish: Cofnij
    add(Command::Undo, "edit.undo", tr("Undo"), keys(Qt::Key_Z, Qt::ControlModifier));
    //: In Polish: Ponów
    add(Command::Redo, "edit.redo", tr("Redo"), keys(Qt::Key_Y, Qt::ControlModifier));

    QHBoxLayout* line = nullptr;
    flow->addWidget(
        gui::CardBar::button(command(Command::NewEvent), "planChronologyNewEvent", true, bar));
    QWidget* card = group(bar, &line);
    line->addWidget(gui::CardBar::button(command(Command::Edit), "planChronologyEdit", false, card));
    line->addWidget(gui::CardBar::button(command(Command::Open), "planChronologyOpen", false, card));
    flow->addWidget(card);
    QWidget* time = group(bar, &line);
    line->addWidget(
        gui::CardBar::button(command(Command::Earlier), "planChronologyEarlier", false, time));
    line->addWidget(
        gui::CardBar::button(command(Command::Later), "planChronologyLater", false, time));
    flow->addWidget(time);
    flow->addWidget(
        gui::CardBar::button(command(Command::Delete), "planChronologyDelete", false, bar));
    QWidget* steps = group(bar, &line);
    line->addWidget(gui::CardBar::button(command(Command::Undo), "planChronologyUndo", false, steps));
    line->addWidget(gui::CardBar::button(command(Command::Redo), "planChronologyRedo", false, steps));
    flow->addWidget(steps);
}

// =============================================================================
// What the board gives it
// =============================================================================

void PlanChronologyView::setCards(const QList<gui::PlanChronologyCard>& cards) {
    m_cards = cards;
    refresh();
}

void PlanChronologyView::setPlan(const QJsonObject& plan) {
    if (!plan.isEmpty()) {
        Timeline read;
        if (read.fromJson(plan)) {
            m_plan = read;
        }
        m_periods = PlanPeriod::fromJson(plan.value(QLatin1String(PERIODS)).toArray());
    }
    refresh();
}

QJsonObject PlanChronologyView::plan() const {
    QJsonObject json = m_plan.toJson();
    json.insert(QLatin1String(PERIODS), PlanPeriod::toJson(m_periods));
    return json;
}

void PlanChronologyView::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
    m_periodsButton->setEnabled(!readOnly);
    updateCommands();
}

void PlanChronologyView::setUndoState(bool canUndo, bool canRedo) {
    m_canUndo = canUndo;
    m_canRedo = canRedo;
    updateCommands();
}

QString PlanChronologyView::currentCard() const {
    const ChronologyItem* item = currentItem();
    return item != nullptr ? item->id : QString();
}

bool PlanChronologyView::setCurrentCard(const QString& id) {
    const int index = m_layout ? m_layout->indexOf(id) : -1;
    if (index < 0) {
        return false;
    }
    m_grid->setCurrent(index);
    return true;
}

void PlanChronologyView::focusCards() {
    m_grid->setFocus(Qt::OtherFocusReason);
}

const ChronologyItem* PlanChronologyView::currentItem() const {
    if (!m_layout || m_grid->current() < 0) {
        return nullptr;
    }
    return &m_layout->items().at(m_grid->current());
}

QString PlanChronologyView::eventIdOf(const ChronologyItem& item) const {
    return item.outside ? item.id.mid(QLatin1String(EVENT_PREFIX).size()) : QString();
}

// =============================================================================
// The cards
// =============================================================================

void PlanChronologyView::refresh() {
    const QString wanted = m_wanted;
    const TimelineView& view = m_plan.view();
    ChronologyOptions options;
    options.scale = view.scale;
    options.lanes = view.lanes == TimelineView::Lanes::Threads ? TimelineView::Lanes::Characters
                                                               : view.lanes;
    options.compress = view.compress;
    options.dayOne = m_plan.dayOne();
    options.style = m_look.style;

    QList<ChronologyItem> items;
    for (const gui::PlanChronologyCard& card : m_cards) {
        ChronologyItem item;
        item.id = card.id;
        item.planned = card.planned;
        item.number = card.number;
        item.title = card.title;
        item.description = card.description;
        item.pov = card.pov;
        item.place = card.place;
        item.timeText = card.time;
        item.textTime = card.textTime;
        item.textTitle = card.textTitle;
        item.color = card.color;
        item.time = ChronologyLayout::timeOf(card.time, options.dayOne, m_periods);
        items.append(item);
    }
    for (const TimelineEvent* event : m_plan.eventsInOrder()) {
        ChronologyItem item;
        item.id = QString::fromLatin1(EVENT_PREFIX) + event->id;
        item.outside = true;
        item.title = event->title;
        item.description = event->description;
        item.pov = event->characters.value(0);
        item.place = event->place;
        if (event->time) {
            item.time = event->time;
            item.timeText = options.scale == TimelineView::Scale::Days &&
                                    event->time->precision == StoryTime::Precision::Day
                                ? event->time->dayText(options.dayOne, options.style)
                                : event->time->dateText(options.style);
        } else {
            item.timeText = event->timeText;
            item.time = ChronologyLayout::timeOf(event->timeText, options.dayOne, m_periods);
        }
        item.color = m_look.paletteColor(event->color);
        items.append(item);
    }
    m_layout = std::make_shared<ChronologyLayout>(items, options);
    //: The band of the cards without a point of view; of a place. In Polish: Bez punktu
    //: widzenia / Bez miejsca
    const QString none = options.lanes == TimelineView::Lanes::Places ? tr("Without a place")
                                                                       : tr("Without a point of view");
    //: The one band of the chronology. In Polish: Wszystkie karty
    m_grid->setChronology(m_layout, m_look, none, tr("All cards"));
    const int index = m_layout->indexOf(wanted);
    m_grid->setCurrent(index);
    m_wanted = wanted;

    m_scale->setCurrentIndex(std::max(0, m_scale->findData(static_cast<int>(options.scale))));
    m_lanes->setCurrentIndex(std::max(0, m_lanes->findData(static_cast<int>(options.lanes))));
    m_compress->setChecked(view.compress);
    m_compress->setEnabled(options.scale != TimelineView::Scale::Order);
    const auto cards = static_cast<int>(m_cards.size());
    const auto events = static_cast<int>(m_plan.events().size());
    //: The cards of the chronology; %1 "12 cards", %2 "2 events outside the text". In
    //: Polish: %1 · %2
    m_totals->setText(tr("%1 · %2").arg(tr("%n card(s) of the book", "", cards),
                                        tr("%n event(s) outside the text", "", events)));
    updateCommands();
}

void PlanChronologyView::updateCommands() {
    if (command(Command::Redo) == nullptr || m_grid == nullptr) {
        return;
    }
    const ChronologyItem* item = currentItem();
    const bool writable = !m_readOnly;
    command(Command::NewEvent)->setEnabled(writable);
    command(Command::Edit)->setEnabled(writable && item != nullptr);
    command(Command::Open)->setEnabled(item != nullptr && !item->outside && !item->planned);
    // A step as exactly as the time is known
    QString earlier = tr("A Day Earlier");
    QString later = tr("A Day Later");
    if (item != nullptr && item->time && item->time->precision != StoryTime::Precision::Day) {
        //: The time of the card a step earlier: a month, a season, a year. In Polish:
        //: Wcześniej
        earlier = tr("Earlier");
        //: In Polish: Później
        later = tr("Later");
    }
    command(Command::Earlier)->setText(earlier);
    command(Command::Earlier)->setToolTip(
        gui::CardBar::withKeys(earlier, keys(Qt::Key_Left, Qt::ControlModifier)));
    command(Command::Later)->setText(later);
    command(Command::Later)->setToolTip(
        gui::CardBar::withKeys(later, keys(Qt::Key_Right, Qt::ControlModifier)));
    const bool timed = writable && item != nullptr && item->time.has_value();
    command(Command::Earlier)->setEnabled(timed);
    command(Command::Later)->setEnabled(timed);
    //: The command of the chronology on a card of the book. In Polish: Usuń czas akcji
    QString remove = tr("Delete the Time of the Action");
    if (item != nullptr && item->outside) {
        //: In Polish: Usuń wydarzenie
        remove = tr("Delete the Event");
    }
    command(Command::Delete)->setText(remove);
    command(Command::Delete)->setToolTip(gui::CardBar::withKeys(remove, keys(Qt::Key_Delete)));
    command(Command::Delete)->setEnabled(
        writable && item != nullptr &&
        (item->outside || (!item->timeText.isEmpty() && !item->textTime)));
    command(Command::Undo)->setEnabled(writable && m_canUndo);
    command(Command::Redo)->setEnabled(writable && m_canRedo);
}

QString PlanChronologyView::hint() const {
    const ChronologyItem* item = currentItem();
    if (item == nullptr) {
        //: The line under the chronology without a card chosen. In Polish: Strzałki wybierają
        //: kartę, Insert dodaje wydarzenie poza tekstem, F2 edytuje kartę, Ctrl+← i Ctrl+→
        //: zmieniają jej czas.
        return tr("The arrows choose a card, Insert adds an event outside the text, F2 edits a "
                  "card, Ctrl+Left and Ctrl+Right change its time.");
    }
    if (item->outside) {
        //: The line under the chronology on an event outside the text; %1 its title. In
        //: Polish: „%1”: wydarzenie poza tekstem – dzieje się, ale książka go nie opowiada.
        return tr("“%1”: an event outside the text – it happens, but the book does not tell it.")
            .arg(item->title);
    }
    const QString name = QStringLiteral("%1 %2").arg(item->number, item->title);
    if (!item->time) {
        if (item->timeText.isEmpty()) {
            //: %1 "3.2 Burza". In Polish: %1: bez czasu akcji. F2 nada go, np. %2.
            return tr("%1: no time of the action. F2 gives it one, e.g. %2.")
                .arg(name, TimelineWords::examples());
        }
        //: %1 "3.2 Burza", %2 what the writer typed. In Polish: %1: program nie czyta czasu
        //: „%2”. F2 go zmieni; własne nazwy okresów: „Okresy…”.
        return tr("%1: the program does not read the time “%2”. F2 changes it; names of your "
                  "own: Periods….")
            .arg(name, item->timeText);
    }
    if (item->flashback) {
        //: %1 "3.2 Burza", %2 its time. In Polish: %1: %2 – retrospekcja: książka opowiada
        //: ją po scenie, która dzieje się później.
        return tr("%1: %2 – a flashback: the book tells it after a scene that happens later.")
            .arg(name, item->timeText);
    }
    if (item->textTime) {
        //: A scene without a time of its own; %1 "3.2 Burza", %2 the time of its text, %3 the
        //: title of the text. In Polish: %1: %2 – czas tekstu „%3”. F2 albo Ctrl+← i Ctrl+→
        //: dadzą scenie własny.
        return tr("%1: %2 – the time of the text “%3”. F2 or Ctrl+Left and Ctrl+Right give the "
                  "scene its own.")
            .arg(name, item->timeText, item->textTitle);
    }
    if (item->planned) {
        //: In Polish: %1: %2, w planie – jeszcze bez tekstu.
        return tr("%1: %2, in the plan – no text yet.").arg(name, item->timeText);
    }
    //: In Polish: %1: %2.
    return tr("%1: %2.").arg(name, item->timeText);
}

// =============================================================================
// The commands
// =============================================================================

void PlanChronologyView::runCommand(Command which) {
    const ChronologyItem* item = currentItem();
    switch (which) {
    case Command::NewEvent:
        addEvent();
        break;
    case Command::Edit:
        if (item != nullptr) {
            editCard(m_grid->current());
        }
        break;
    case Command::Open:
        if (item != nullptr && !item->outside) {
            if (item->planned) {
                //: Under the chronology after Enter on a card in the plan. In Polish: „%1” jest
                //: w planie: tekst daje „Utwórz plik rozdziału” albo „Napisz scenę” na
                //: tablicy.
                emit told(tr("“%1” is in the plan: Create the Chapter File or Write the Scene "
                             "on the board gives it its text.")
                              .arg(item->title));
            } else {
                emit openRequested(item->id);
            }
        }
        break;
    case Command::Earlier:
        shift(-1);
        break;
    case Command::Later:
        shift(1);
        break;
    case Command::Delete:
        removeCurrent();
        break;
    case Command::Undo:
        emit undoRequested();
        break;
    case Command::Redo:
        emit redoRequested();
        break;
    case Command::Count:
        break;
    }
    if (which != Command::Open || item == nullptr || item->outside || item->planned) {
        focusCards();
    }
}

bool PlanChronologyView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_grid &&
        (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)) {
        auto* key = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers modifiers =
            key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
        const QKeyCombination pressed(modifiers, static_cast<Qt::Key>(key->key()));
        static const std::array<std::pair<QKeyCombination, Command>, 7> table{{
            {keys(Qt::Key_Insert), Command::NewEvent},
            {keys(Qt::Key_F2), Command::Edit},
            {keys(Qt::Key_Left, Qt::ControlModifier), Command::Earlier},
            {keys(Qt::Key_Right, Qt::ControlModifier), Command::Later},
            {keys(Qt::Key_Delete), Command::Delete},
            {keys(Qt::Key_Z, Qt::ControlModifier), Command::Undo},
            {keys(Qt::Key_Y, Qt::ControlModifier), Command::Redo},
        }};
        for (const auto& [combination, which] : table) {
            if (combination != pressed) {
                continue;
            }
            // The keys of the chronology go to it, not to the commands of the window
            if (event->type() == QEvent::ShortcutOverride) {
                key->accept();
                return true;
            }
            if (command(which)->isEnabled() || which == Command::Undo || which == Command::Redo) {
                runCommand(which);
            }
            return true;
        }
    }
    return gui::PlanChronology::eventFilter(watched, event);
}

void PlanChronologyView::showMenu(int item, const QPoint& globalPos) {
    if (item >= 0) {
        m_grid->setCurrent(item);
    }
    QMenu menu(this);
    for (const Command which : {Command::Open, Command::Edit, Command::Earlier, Command::Later,
                                Command::Delete, Command::NewEvent}) {
        if (which == Command::NewEvent || which == Command::Delete) {
            menu.addSeparator();
        }
        menu.addAction(command(which));
    }
    menu.exec(globalPos);
}

QStringList PlanChronologyView::suggestions(bool places) const {
    QStringList names;
    for (const gui::PlanChronologyCard& card : m_cards) {
        const QString name = (places ? card.place : card.pov).simplified();
        if (!name.isEmpty() && !names.contains(name, Qt::CaseInsensitive)) {
            names.append(name);
        }
    }
    for (const TimelineEvent& event : m_plan.events()) {
        const QString name = (places ? event.place : event.characters.value(0)).simplified();
        if (!name.isEmpty() && !names.contains(name, Qt::CaseInsensitive)) {
            names.append(name);
        }
    }
    return names;
}

void PlanChronologyView::editCard(int index) {
    if (m_readOnly || !m_layout || index < 0) {
        return;
    }
    const ChronologyItem item = m_layout->items().at(index);
    gui::PlanChronologyEdit edit{item.title, item.description, item.pov, item.place,
                                 item.timeText};
    PlanCardDialog dialog(edit, item.outside, m_plan.dayOne(), m_periods, suggestions(false),
                          suggestions(true), m_look, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const gui::PlanChronologyEdit changed = dialog.result();
    if (changed == edit) {
        return;
    }
    if (!item.outside) {
        emit cardEdited(item.id, changed);
        return;
    }
    const QString id = eventIdOf(item);
    changePlan(
        [&id, &changed](Timeline& plan, QList<PlanPeriod>& /*periods*/) {
            const TimelineEvent* found = plan.event(id);
            if (found == nullptr) {
                return;
            }
            TimelineEvent event = *found;
            if (!changed.title.isEmpty()) {
                event.title = changed.title;
            }
            event.description = changed.description;
            event.characters = changed.pov.isEmpty() ? QStringList() : QStringList{changed.pov};
            event.place = changed.place;
            plan.setTimeOf(event, changed.time);
            plan.setEvent(event);
        },
        //: What Undo undoes in the chronology, after "Undone:"; %1 the title of the event. In
        //: Polish: zmiana wydarzenia „%1”
        tr("change of the event “%1”").arg(item.title),
        //: Under the chronology. In Polish: Zmieniono wydarzenie „%1”.
        tr("The event “%1” changed.").arg(changed.title.isEmpty() ? item.title : changed.title));
}

void PlanChronologyView::addEvent() {
    if (m_readOnly) {
        return;
    }
    // At the time and in the band of the card chosen
    gui::PlanChronologyEdit edit;
    //: The title of a new event outside the text. In Polish: Nowe wydarzenie
    edit.title = tr("New event");
    if (const ChronologyItem* item = currentItem(); item != nullptr) {
        edit.time = item->time ? item->timeText : QString();
        if (m_plan.view().lanes == TimelineView::Lanes::Places) {
            edit.place = item->place;
        } else {
            edit.pov = item->pov;
        }
    }
    PlanCardDialog dialog(edit, true, m_plan.dayOne(), m_periods, suggestions(false),
                          suggestions(true), m_look, this);
    //: In Polish: Dodaj
    dialog.setAcceptText(tr("Add"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const gui::PlanChronologyEdit added = dialog.result();
    QString newId;
    const QString title = added.title.isEmpty() ? edit.title : added.title;
    changePlan(
        [&added, &title, &newId](Timeline& plan, QList<PlanPeriod>& /*periods*/) {
            TimelineEvent event;
            event.title = title;
            event.description = added.description;
            event.characters = added.pov.isEmpty() ? QStringList() : QStringList{added.pov};
            event.place = added.place;
            plan.setTimeOf(event, added.time);
            newId = plan.addEvent(event);
        },
        //: What Undo undoes, after "Undone:". In Polish: dodanie wydarzenia „%1”
        tr("adding the event “%1”").arg(title),
        //: Under the chronology. In Polish: Dodano wydarzenie poza tekstem „%1”.
        tr("Added the event outside the text “%1”.").arg(title));
    setCurrentCard(QString::fromLatin1(EVENT_PREFIX) + newId);
}

std::optional<QString> PlanChronologyView::shiftedTime(const QString& text, int steps,
                                                       qint64 dayOne,
                                                       const QList<PlanPeriod>& periods,
                                                       const StoryTimeStyle& style) {
    const std::optional<StoryTime> time = ChronologyLayout::timeOf(text, dayOne, periods);
    if (!time || steps == 0) {
        return std::nullopt;
    }
    constexpr int YEAR = 12;
    StoryTime moved;
    switch (time->precision) {
    case StoryTime::Precision::Day:
        moved = time->movedByDays(steps);
        break;
    case StoryTime::Precision::Month:
        moved = time->movedByMonths(steps * time->months());
        break;
    case StoryTime::Precision::Year:
        moved = time->movedByMonths(steps * YEAR);
        break;
    }
    // Days of the story stay days of the story: the text says another day for another day one
    const bool storyDays = time->precision == StoryTime::Precision::Day &&
                           StoryTime::parse(text, dayOne + 1) != StoryTime::parse(text, dayOne);
    StoryTimeStyle words = style;
    words.dates = DateStyle::Words;
    return storyDays ? moved.dayText(dayOne, words) : moved.dateText(words);
}

void PlanChronologyView::shift(int steps) {
    const ChronologyItem* item = currentItem();
    if (m_readOnly || item == nullptr || !item->time) {
        return;
    }
    if (!item->outside) {
        const std::optional<QString> time =
            shiftedTime(item->timeText, steps, m_plan.dayOne(), m_periods, m_look.style);
        if (time) {
            emit cardEdited(item->id, gui::PlanChronologyEdit{item->title, item->description,
                                                               item->pov, item->place, *time});
        }
        return;
    }
    const QString id = eventIdOf(*item);
    const TimelineEvent* event = m_plan.event(id);
    if (event == nullptr) {
        return;
    }
    // A period of the writer or a time in words: written as the time it is
    const QString text = event->time ? event->time->dateText(m_look.style) : event->timeText;
    const std::optional<QString> time =
        shiftedTime(text, steps, m_plan.dayOne(), m_periods, m_look.style);
    if (!time) {
        return;
    }
    const QString title = item->title;
    changePlan(
        [&id, &time](Timeline& plan, QList<PlanPeriod>& /*periods*/) {
            const TimelineEvent* found = plan.event(id);
            if (found != nullptr) {
                TimelineEvent changed = *found;
                plan.setTimeOf(changed, *time);
                plan.setEvent(changed);
            }
        },
        //: What Undo undoes, after "Undone:". In Polish: przesunięcie wydarzenia „%1”
        tr("moving the event “%1”").arg(title),
        //: Under the chronology; %1 the event, %2 its new time. In Polish: „%1”: teraz %2.
        tr("“%1”: now %2.").arg(title, *time));
}

void PlanChronologyView::removeCurrent() {
    const ChronologyItem* item = currentItem();
    if (m_readOnly || item == nullptr) {
        return;
    }
    if (!item->outside) {
        if (!item->timeText.isEmpty()) {
            emit cardEdited(item->id, gui::PlanChronologyEdit{item->title, item->description,
                                                               item->pov, item->place,
                                                               QString()});
        }
        return;
    }
    const QString id = eventIdOf(*item);
    const QString title = item->title;
    changePlan([&id](Timeline& plan, QList<PlanPeriod>& /*periods*/) { plan.removeEvent(id); },
               //: What Undo undoes, after "Undone:". In Polish: usunięcie wydarzenia „%1”
               tr("deleting the event “%1”").arg(title),
               //: Under the chronology. In Polish: Usunięto wydarzenie „%1”.
               tr("Deleted the event “%1”.").arg(title));
}

void PlanChronologyView::editPeriods() {
    if (m_readOnly) {
        return;
    }
    PlanPeriodsDialog dialog(m_plan.dayOne(), m_periods, m_look, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const qint64 dayOne = dialog.dayOne();
    const QList<PlanPeriod> periods = dialog.periods();
    if (dayOne == m_plan.dayOne() && periods == m_periods) {
        return;
    }
    changePlan(
        [dayOne, &periods](Timeline& plan, QList<PlanPeriod>& kept) {
            plan.setDayOne(dayOne);
            kept = periods;
        },
        //: What Undo undoes, after "Undone:". In Polish: zmiana okresów
        tr("change of the periods"),
        //: Under the chronology. In Polish: Zmieniono dzień pierwszy i okresy.
        tr("Day one and the periods changed."));
}

void PlanChronologyView::changePlan(
    const std::function<void(Timeline&, QList<PlanPeriod>&)>& change, const QString& undoText,
    const QString& message) {
    const QJsonObject before = plan();
    change(m_plan, m_periods);
    const QJsonObject after = plan();
    if (after == before) {
        return;
    }
    refresh();
    emit planEdited(after, undoText, message);
}

void PlanChronologyView::changeView(const std::function<void(TimelineView&)>& change) {
    TimelineView view = m_plan.view();
    change(view);
    if (view == m_plan.view()) {
        return;
    }
    m_plan.setView(view);
    refresh();
    emit planViewChanged(plan());
    focusCards();
}

void PlanChronologyView::changeEvent(QEvent* event) {
    gui::PlanChronology::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        m_look = TimelineLook::current();
        if (m_grid != nullptr) {
            refresh();
        }
    }
}

}  // namespace kalahari::timeline
