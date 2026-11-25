// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_dialogs.cpp
/// @brief The windows "Event" and "Timeline" of a timeline

#include <kalahari/timeline/timeline_dialogs.h>

#include <kalahari/timeline/timeline_edits.h>

#include <QComboBox>
#include <QCompleter>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringListModel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::timeline {

namespace {

constexpr int DESCRIPTION_LINES = 4;
constexpr int CHAPTER_LINES = 6;
constexpr int THREAD_LINES = 6;
constexpr int ID_ROLE = Qt::UserRole + 1;
constexpr const char* PAGE = "timeline";

/// A note under a field: muted, or in the color of a problem
void showNote(QLabel* note, const QString& text, bool ok, const TimelineLook& look) {
    note->setText(text);
    QPalette palette = note->palette();
    palette.setColor(QPalette::WindowText, ok ? look.mutedText : look.flashback);
    note->setPalette(palette);
}

/// The colors of the palette in @p box: @p none first (empty), then "1"..."6"
void fillColors(QComboBox* box, const QString& none, const TimelineLook& look) {
    if (!none.isEmpty()) {
        box->addItem(none, QString());
    }
    for (std::size_t i = 0; i < look.palette.size(); ++i) {
        const QString key = QString::number(i + 1);
        //: A color of the palette of the threads. In Polish: Kolor %1
        box->addItem(TimelineEdits::swatch(look.palette.at(i), look.mid),
                     TimelineEventDialog::tr("Color %1").arg(key), key);
    }
}

QLabel* noteLabel(QWidget* parent) {
    auto* note = new QLabel(parent);
    note->setWordWrap(true);
    note->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return note;
}

}  // namespace

// =============================================================================
// The window "Event"
// =============================================================================

TimelineEventDialog::TimelineEventDialog(const Timeline& timeline, const TimelineEvent& event,
                                         const TimelineIndex& index, const TimelineLook& look,
                                         QWidget* parent)
    : gui::dialogs::KalahariDialog(parent),
      m_timeline(timeline),
      m_event(event),
      m_index(index),
      m_look(look) {
    setObjectName(QStringLiteral("timelineEventDialog"));
    //: The window of an event of a timeline. In Polish: Wydarzenie / Wszystkie pola wydarzenia
    //: w jednym miejscu.
    setHeading(tr("Event"), tr("All the fields of the event in one place."));
    setHeadingIcon(QStringLiteral("template.timeline"));
    //: The gear of the window "Event". In Polish: Ustawienia osi czasu
    addSettingsButton(QString::fromLatin1(PAGE), tr("Timeline Settings"));

    m_title = new QLineEdit(event.title, this);
    //: In Polish: Tytuł
    addField(tr("Title"), m_title);

    const bool ordered = timeline.view().scale == TimelineView::Scale::Order;
    m_when = new QLineEdit(TimelineWords::whenToEdit(timeline, event, look), this);
    //: Examples of a time, in the field "When". In Polish: np. Tej samej nocy / np. 14 kwietnia
    //: 1931, rano
    m_when->setPlaceholderText(ordered ? tr("e.g. That same night")
                                       : tr("e.g. 14 April 1931, in the morning"));
    //: In Polish: Podpis czasu / Kiedy
    addField(ordered ? tr("Label of the time") : tr("When"), m_when);
    m_note = noteLabel(this);
    contentLayout()->addWidget(m_note);
    connect(m_when, &QLineEdit::textChanged, this, &TimelineEventDialog::updateNote);

    m_thread = new QComboBox(this);
    for (const TimelineThread& thread : timeline.threads()) {
        const QColor color = look.paletteColor(thread.color);
        if (color.isValid()) {
            m_thread->addItem(TimelineEdits::swatch(color, look.mid), thread.name, thread.id);
        } else {
            m_thread->addItem(thread.name, thread.id);
        }
    }
    m_thread->addItem(TimelineWords::withoutLane(TimelineView::Lanes::Threads), QString());
    m_thread->setCurrentIndex(std::max(0, m_thread->findData(event.thread)));
    //: In Polish: Wątek
    addField(tr("Thread"), m_thread);

    m_characters = new QLineEdit(index.namesOf(event.characters), this);
    //: In Polish: po przecinku, np. Amara, Jusuf
    m_characters->setPlaceholderText(tr("with commas, e.g. Amara, Jusuf"));
    //: In Polish: Postacie
    addField(tr("Characters"), m_characters);

    m_place = new QLineEdit(event.place.isEmpty() ? QString() : index.nameOf(event.place), this);
    QStringList places;
    for (const TimelineIndex::Entry& entry : index.places) {
        places.append(entry.name);
    }
    auto* completer = new QCompleter(places, m_place);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    m_place->setCompleter(completer);
    //: In Polish: Miejsce
    addField(tr("Place"), m_place);
    QLabel* cards = noteLabel(this);
    //: In Polish: Program podpowiada postacie i miejsca z kart Warsztatu; nazwa bez karty
    //: zostaje zwykłym tekstem.
    showNote(cards,
             tr("Characters and places come from the cards of the Workshop; a name without a card "
                "stays plain text."),
             true, look);
    contentLayout()->addWidget(cards);

    m_chapters = new QListWidget(this);
    for (const TimelineIndex::Entry& chapter : index.chapters) {
        auto* item = new QListWidgetItem(chapter.name, m_chapters);
        item->setData(ID_ROLE, chapter.id);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        item->setCheckState(event.chapters.contains(chapter.id) ? Qt::Checked : Qt::Unchecked);
    }
    m_chapters->setMinimumHeight(m_chapters->fontMetrics().height() * CHAPTER_LINES);
    //: The chapters that tell an event. In Polish: W książce
    addField(tr("In the book"), m_chapters);

    m_description = new QPlainTextEdit(event.description, this);
    m_description->setTabChangesFocus(true);
    m_description->setMinimumHeight(m_description->fontMetrics().lineSpacing() *
                                    DESCRIPTION_LINES);
    //: In Polish: Opis
    addField(tr("Description"), m_description);

    m_color = new QComboBox(this);
    //: The color of an event: that of its thread. In Polish: kolor wątku
    fillColors(m_color, tr("the color of its thread"), look);
    m_color->setCurrentIndex(std::max(0, m_color->findData(event.color)));
    //: In Polish: Kolor
    addField(tr("Color"), m_color);

    //: In Polish: Zapisz
    setAcceptText(tr("Save"));
    updateNote();
    m_title->setFocus();
    m_title->selectAll();
}

void TimelineEventDialog::updateNote() {
    TimelineEvent changed = m_event;
    TimelineWords::setWhen(m_timeline, changed, m_when->text());
    bool ok = false;
    const QString text = TimelineWords::recognized(m_timeline, changed, m_look, &ok);
    showNote(m_note, text, ok, m_look);
}

bool TimelineEventDialog::whenRecognized() const {
    TimelineEvent changed = m_event;
    return TimelineWords::setWhen(m_timeline, changed, m_when->text());
}

TimelineEvent TimelineEventDialog::result() const {
    TimelineEvent changed = m_event;
    const QString title = m_title->text().trimmed();
    if (!title.isEmpty()) {
        changed.title = title;
    }
    if (const QString shown = TimelineWords::whenToEdit(m_timeline, m_event, m_look);
        m_when->text().trimmed() != shown.trimmed()) {
        TimelineWords::setWhen(m_timeline, changed, m_when->text());
    }
    changed.thread = m_thread->currentData().toString();
    changed.characters = m_index.charactersOf(m_characters->text());
    const QString place = m_place->text().trimmed();
    changed.place = place.isEmpty() ? QString() : m_index.placeOf(place);
    changed.chapters.clear();
    for (int row = 0; row < m_chapters->count(); ++row) {
        const QListWidgetItem* item = m_chapters->item(row);
        if (item->checkState() == Qt::Checked) {
            changed.chapters.append(item->data(ID_ROLE).toString());
        }
    }
    changed.description = m_description->toPlainText().trimmed();
    changed.color = m_color->currentData().toString();
    return changed;
}

// =============================================================================
// The window "Timeline"
// =============================================================================

TimelinePropertiesDialog::TimelinePropertiesDialog(const Timeline& timeline,
                                                   const TimelineLook& look, QWidget* parent)
    : gui::dialogs::KalahariDialog(parent),
      m_timeline(timeline),
      m_look(look),
      m_dayOne(timeline.dayOne()) {
    setObjectName(QStringLiteral("timelinePropertiesDialog"));
    //: The window of a timeline: day one and the threads. In Polish: Dzień 1 i wątki / Od dnia
    //: 1 liczą się dni opowieści; wątki to linie fabuły, do których należą wydarzenia.
    setHeading(tr("Day One and Threads"),
               tr("The days of the story count from day one; the threads are the lines of the "
                  "plot that the events belong to."));
    setHeadingIcon(QStringLiteral("template.timeline"));
    addSettingsButton(QString::fromLatin1(PAGE), tr("Timeline Settings"));

    StoryTimeStyle style = look.style;
    style.dates = DateStyle::Words;
    m_day = new QLineEdit(StoryTime::day(timeline.dayOne()).dateText(style), this);
    //: In Polish: Dzień 1 opowieści
    addField(tr("Day one of the story"), m_day);
    m_note = noteLabel(this);
    contentLayout()->addWidget(m_note);
    connect(m_day, &QLineEdit::textChanged, this, &TimelinePropertiesDialog::updateNote);

    m_threads = new QTableWidget(0, 2, this);
    m_threads->setObjectName(QStringLiteral("timelineThreads"));
    //: The columns of the threads of a timeline. In Polish: Nazwa / Kolor
    m_threads->setHorizontalHeaderLabels({tr("Name"), tr("Color")});
    m_threads->verticalHeader()->hide();
    m_threads->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_threads->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_threads->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_threads->setSelectionMode(QAbstractItemView::SingleSelection);
    m_threads->setMinimumHeight(m_threads->fontMetrics().height() * 2 * THREAD_LINES);
    for (const TimelineThread& thread : timeline.threads()) {
        addRow(thread);
    }
    //: In Polish: Wątki
    addField(tr("Threads"), m_threads);

    auto* buttons = new QHBoxLayout();
    //: In Polish: Dodaj wątek
    auto* add = new QPushButton(tr("Add a Thread"), this);
    //: In Polish: Usuń wątek
    auto* remove = new QPushButton(tr("Delete the Thread"), this);
    add->setAutoDefault(false);
    remove->setAutoDefault(false);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch(1);
    contentLayout()->addLayout(buttons);
    QLabel* threadNote = noteLabel(this);
    //: In Polish: Wydarzenia usuniętego wątku zostają, bez wątku.
    showNote(threadNote, tr("The events of a deleted thread stay, without a thread."), true,
             look);
    contentLayout()->addWidget(threadNote);
    connect(add, &QPushButton::clicked, this, [this]() {
        TimelineThread thread;
        //: The name of a new thread of a timeline. In Polish: Nowy wątek
        thread.name = tr("New thread");
        thread.color = QString::number(m_threads->rowCount() % 6 + 1);
        addRow(thread);
        const int row = m_threads->rowCount() - 1;
        m_threads->setCurrentCell(row, 0);
        m_threads->editItem(m_threads->item(row, 0));
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        const int row = m_threads->currentRow();
        if (row >= 0) {
            m_threads->removeRow(row);
        }
    });

    //: In Polish: Zapisz
    setAcceptText(tr("Save"));
    updateNote();
}

void TimelinePropertiesDialog::addRow(const TimelineThread& thread) {
    const int row = m_threads->rowCount();
    m_threads->insertRow(row);
    auto* name = new QTableWidgetItem(thread.name);
    name->setData(ID_ROLE, thread.id);
    m_threads->setItem(row, 0, name);
    auto* color = new QComboBox(m_threads);
    fillColors(color, QString(), m_look);
    color->setCurrentIndex(std::max(0, color->findData(thread.color)));
    m_threads->setCellWidget(row, 1, color);
}

void TimelinePropertiesDialog::updateNote() {
    const std::optional<StoryTime> day = StoryTime::parse(m_day->text(), m_timeline.dayOne());
    const bool ok = day.has_value() && day->precision == StoryTime::Precision::Day;
    if (ok) {
        m_dayOne = day->first;
        StoryTimeStyle style = m_look.style;
        style.dates = DateStyle::Words;
        //: In Polish: Rozpoznano: %1. Zmiana dnia 1 niczego nie przesuwa: wydarzenia mają swoje
        //: daty.
        showNote(m_note,
                 tr("Recognized: %1. Changing day one moves nothing: the events keep their "
                    "dates.")
                     .arg(StoryTime::day(m_dayOne).dateText(style)),
                 true, m_look);
    } else {
        m_dayOne = m_timeline.dayOne();
        //: In Polish: Nie rozpoznano – wpisz dzień, np. 14 kwietnia 1931.
        showNote(m_note, tr("Not recognized – type a day, e.g. 14 April 1931."), false, m_look);
    }
}

QList<TimelineThread> TimelinePropertiesDialog::threads() const {
    QList<TimelineThread> threads;
    for (int row = 0; row < m_threads->rowCount(); ++row) {
        const QTableWidgetItem* name = m_threads->item(row, 0);
        const auto* color = qobject_cast<QComboBox*>(m_threads->cellWidget(row, 1));
        if (name == nullptr || name->text().trimmed().isEmpty()) {
            continue;
        }
        const QString id = name->data(ID_ROLE).toString();
        TimelineThread thread;
        if (const TimelineThread* own = m_timeline.thread(id); own != nullptr) {
            thread = *own;
        }
        thread.id = id;
        thread.name = name->text().trimmed();
        thread.color = color != nullptr ? color->currentData().toString() : QString();
        threads.append(thread);
    }
    return threads;
}

}  // namespace kalahari::timeline
