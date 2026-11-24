// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plan_chronology_dialogs.cpp
/// @brief The windows of the chronology of the Book Plan: a card, and the periods with day one

#include <kalahari/timeline/plan_chronology_view.h>

#include <kalahari/timeline/timeline_index.h>

#include <QCompleter>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>

namespace kalahari::timeline {

namespace {

constexpr int DESCRIPTION_LINES = 4;
constexpr int PERIOD_LINES = 6;
constexpr const char* PAGE = "timeline";

QLabel* noteLabel(QWidget* parent) {
    auto* note = new QLabel(parent);
    note->setWordWrap(true);
    note->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return note;
}

/// A note under a field: muted, or in the color of a problem
void showNote(QLabel* note, const QString& text, bool ok, const TimelineLook& look) {
    note->setText(text);
    QPalette palette = note->palette();
    palette.setColor(QPalette::WindowText, ok ? look.mutedText : look.flashback);
    note->setPalette(palette);
}

/// A field with the names the book has already
QLineEdit* suggestingField(const QString& text, const QStringList& names, QWidget* parent) {
    auto* field = new QLineEdit(text, parent);
    auto* completer = new QCompleter(names, field);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    field->setCompleter(completer);
    return field;
}

/// What the program reads in @p text: "Recognized: wiosna 1931 – Dzień 40"; @p ok whether it
/// is a time
QString recognizedText(const std::optional<StoryTime>& time, const QString& text, qint64 dayOne,
                       const TimelineLook& look, bool* ok) {
    *ok = time.has_value() || text.trimmed().isEmpty();
    if (text.trimmed().isEmpty()) {
        //: The note under "When" of a card of the chronology without a time. In Polish: Bez
        //: czasu – karta czeka pod chronologią, w „Bez czasu akcji”.
        return PlanCardDialog::tr("No time – the card waits under the chronology, in “Without "
                                  "a time of the action”.");
    }
    if (!time) {
        //: In Polish: Nie rozpoznano – karta czeka w „Bez czasu akcji”. Przykłady: %1. Własne
        //: nazwy okresów: „Okresy…”.
        return PlanCardDialog::tr("Not recognized – the card waits in “Without a time of the "
                                  "action”. Examples: %1. Names of your own: Periods….")
            .arg(TimelineWords::examples());
    }
    StoryTimeStyle style = look.style;
    style.dates = DateStyle::Words;
    const QString dates = time->dateText(style);
    if (time->precision != StoryTime::Precision::Day) {
        //: The note under "When"; %1 the time. In Polish: Rozpoznano: %1.
        return PlanCardDialog::tr("Recognized: %1.").arg(dates);
    }
    //: The note under "When"; %1 the time in dates, %2 in days of the story. In Polish:
    //: Rozpoznano: %1 – %2.
    return PlanCardDialog::tr("Recognized: %1 – %2.").arg(dates, time->dayText(dayOne, style));
}

}  // namespace

// =============================================================================
// The window of a card
// =============================================================================

PlanCardDialog::PlanCardDialog(const gui::PlanChronologyEdit& card, bool outside, qint64 dayOne,
                               const QList<PlanPeriod>& periods, const QStringList& povs,
                               const QStringList& places, const TimelineLook& look,
                               QWidget* parent)
    : gui::dialogs::KalahariDialog(parent), m_dayOne(dayOne), m_periods(periods), m_look(look) {
    setObjectName(QStringLiteral("planCardDialog"));
    if (outside) {
        //: The window of an event of the chronology outside the text. In Polish: Wydarzenie
        //: poza tekstem / Dzieje się w opowieści, ale książka go nie opowiada.
        setHeading(tr("Event Outside the Text"),
                   tr("It happens in the story, but the book does not tell it."));
    } else {
        //: The window of a card of the chronology. In Polish: Karta / Tytuł, czas akcji,
        //: punkt widzenia, miejsce i opis, jak na tablicy.
        setHeading(tr("Card"),
                   tr("The title, the time of the action, the point of view, the place and the "
                      "description, as on the board."));
    }
    setHeadingIcon(QStringLiteral("template.timeline"));
    //: The gear of the window. In Polish: Ustawienia osi czasu
    addSettingsButton(QString::fromLatin1(PAGE), tr("Timeline Settings"));

    m_title = new QLineEdit(card.title, this);
    m_title->setObjectName(QStringLiteral("planCardTitle"));
    //: In Polish: Tytuł
    addField(tr("Title"), m_title);
    m_when = new QLineEdit(card.time, this);
    m_when->setObjectName(QStringLiteral("planCardTime"));
    //: Examples of a time of the action. In Polish: np. Dzień 3, noc / wiosna 1931
    m_when->setPlaceholderText(tr("e.g. Day 3, night / spring 1931"));
    //: In Polish: Czas akcji
    addField(tr("Time of the action"), m_when);
    m_note = noteLabel(this);
    contentLayout()->addWidget(m_note);
    connect(m_when, &QLineEdit::textChanged, this, &PlanCardDialog::updateNote);
    m_pov = suggestingField(card.pov, povs, this);
    m_pov->setObjectName(QStringLiteral("planCardPov"));
    //: In Polish: Punkt widzenia
    addField(tr("Point of view"), m_pov);
    m_place = suggestingField(card.place, places, this);
    m_place->setObjectName(QStringLiteral("planCardPlace"));
    //: In Polish: Miejsce
    addField(tr("Place"), m_place);
    m_description = new QPlainTextEdit(card.description, this);
    m_description->setObjectName(QStringLiteral("planCardDescription"));
    m_description->setTabChangesFocus(true);
    m_description->setMinimumHeight(m_description->fontMetrics().lineSpacing() *
                                    DESCRIPTION_LINES);
    //: In Polish: Opis
    addField(tr("Description"), m_description);
    //: In Polish: Zapisz
    setAcceptText(tr("Save"));
    updateNote();
    m_title->setFocus();
    m_title->selectAll();
}

void PlanCardDialog::updateNote() {
    bool ok = false;
    const QString text = m_when->text();
    const QString note = recognizedText(ChronologyLayout::timeOf(text, m_dayOne, m_periods), text,
                                        m_dayOne, m_look, &ok);
    showNote(m_note, note, ok, m_look);
}

gui::PlanChronologyEdit PlanCardDialog::result() const {
    gui::PlanChronologyEdit edit;
    edit.title = m_title->text().trimmed();
    edit.time = m_when->text().simplified();
    edit.pov = m_pov->text().simplified();
    edit.place = m_place->text().simplified();
    edit.description = m_description->toPlainText().trimmed();
    return edit;
}

// =============================================================================
// The window "Periods"
// =============================================================================

PlanPeriodsDialog::PlanPeriodsDialog(qint64 dayOne, const QList<PlanPeriod>& periods,
                                     const TimelineLook& look, QWidget* parent)
    : gui::dialogs::KalahariDialog(parent), m_look(look), m_dayOne(dayOne) {
    setObjectName(QStringLiteral("planPeriodsDialog"));
    //: The window of day one and the periods of the chronology. In Polish: Dzień pierwszy i
    //: okresy / Od dnia pierwszego liczą się dni opowieści („Dzień 3”, „dzień drugi”); okres
    //: to własna nazwa czasu, którą może podać czas akcji.
    setHeading(tr("Day One and Periods"),
               tr("The days of the story count from day one (“Day 3”, “day two”); a period is a "
                  "name of your own for a time, which a time of the action may say."));
    setHeadingIcon(QStringLiteral("template.timeline"));
    addSettingsButton(QString::fromLatin1(PAGE), tr("Timeline Settings"));

    StoryTimeStyle style = look.style;
    style.dates = DateStyle::Words;
    m_day = new QLineEdit(StoryTime::day(dayOne).dateText(style), this);
    m_day->setObjectName(QStringLiteral("planDayOne"));
    //: In Polish: np. 14 kwietnia 1931
    m_day->setPlaceholderText(tr("e.g. 14 April 1931"));
    //: In Polish: Dzień pierwszy opowieści
    addField(tr("Day one of the story"), m_day);
    m_note = noteLabel(this);
    contentLayout()->addWidget(m_note);
    connect(m_day, &QLineEdit::textChanged, this, &PlanPeriodsDialog::updateNote);

    m_periods = new QTableWidget(0, 2, this);
    m_periods->setObjectName(QStringLiteral("planPeriods"));
    //: The columns of the periods. In Polish: Nazwa / Czas
    m_periods->setHorizontalHeaderLabels({tr("Name"), tr("Time")});
    m_periods->horizontalHeader()->setStretchLastSection(true);
    m_periods->verticalHeader()->hide();
    m_periods->setMinimumHeight(m_periods->fontMetrics().height() * PERIOD_LINES);
    m_periods->setTabKeyNavigation(false);
    //: In Polish: Okresy
    addField(tr("Periods"), m_periods);
    for (const PlanPeriod& period : periods) {
        addRow(period.name, period.time.dateText(style));
    }
    connect(m_periods, &QTableWidget::itemChanged, this, &PlanPeriodsDialog::updateNote);

    auto* buttons = new QWidget(this);
    auto* row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 0, 0, 0);
    //: In Polish: &Dodaj okres
    auto* add = new QPushButton(tr("&Add Period"), buttons);
    //: In Polish: &Usuń okres
    auto* remove = new QPushButton(tr("&Remove Period"), buttons);
    row->addWidget(add);
    row->addWidget(remove);
    row->addStretch();
    contentLayout()->addWidget(buttons);
    connect(add, &QPushButton::clicked, this, [this]() {
        //: A new period. In Polish: Nowy okres / lato 1931
        addRow(tr("New period"), tr("summer 1931"));
        m_periods->setCurrentCell(m_periods->rowCount() - 1, 0);
        m_periods->editItem(m_periods->item(m_periods->rowCount() - 1, 0));
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        if (const int current = m_periods->currentRow(); current >= 0) {
            m_periods->removeRow(current);
            updateNote();
        }
    });
    m_periodsNote = noteLabel(this);
    contentLayout()->addWidget(m_periodsNote);
    //: In Polish: Zapisz
    setAcceptText(tr("Save"));
    updateNote();
}

void PlanPeriodsDialog::addRow(const QString& name, const QString& when) {
    const int row = m_periods->rowCount();
    m_periods->insertRow(row);
    m_periods->setItem(row, 0, new QTableWidgetItem(name));
    m_periods->setItem(row, 1, new QTableWidgetItem(when));
}

void PlanPeriodsDialog::updateNote() {
    const std::optional<StoryTime> day = StoryTime::parse(m_day->text(), m_dayOne);
    if (day && day->precision == StoryTime::Precision::Day && !day->isSpan()) {
        m_dayOne = day->first;
        StoryTimeStyle style = m_look.style;
        style.dates = DateStyle::Words;
        //: The note under day one. In Polish: Rozpoznano: %1.
        showNote(m_note, tr("Recognized: %1.").arg(day->dateText(style)), true, m_look);
    } else {
        //: In Polish: Dzień pierwszy to jeden dzień kalendarza, np. 14 kwietnia 1931.
        showNote(m_note, tr("Day one is one day of the calendar, e.g. 14 April 1931."), false,
                 m_look);
    }
    // The periods the program does not read
    QStringList wrong;
    for (int row = 0; row < m_periods->rowCount(); ++row) {
        const QTableWidgetItem* name = m_periods->item(row, 0);
        const QTableWidgetItem* when = m_periods->item(row, 1);
        if (name != nullptr && when != nullptr && !name->text().trimmed().isEmpty() &&
            !StoryTime::parse(when->text(), m_dayOne)) {
            wrong << name->text().trimmed();
        }
    }
    if (wrong.isEmpty()) {
        //: The note under the periods. In Polish: Czas okresu to dowolny czas, który program
        //: czyta: „zima 1931”, „pierwsza połowa 1932”, „Dni 1–30”, „1931”. Karta z czasem
        //: akcji „Wyprawa” albo „Wyprawa, wieczorem” stanie w jego czasie.
        showNote(m_periodsNote,
                 tr("The time of a period is any time the program reads: “winter 1931”, “first "
                    "half of 1932”, “Days 1–30”, “1931”. A card whose time of the action is "
                    "“Expedition” or “Expedition, in the evening” stands at its time."),
                 true, m_look);
    } else {
        //: %1 the names of the periods. In Polish: Program nie czyta czasu okresów: %1. Te
        //: okresy nie zostaną zapisane.
        showNote(m_periodsNote,
                 tr("The program does not read the time of the periods: %1. They will not be "
                    "kept.")
                     .arg(wrong.join(QStringLiteral(", "))),
                 false, m_look);
    }
}

QList<PlanPeriod> PlanPeriodsDialog::periods() const {
    QList<PlanPeriod> periods;
    for (int row = 0; row < m_periods->rowCount(); ++row) {
        const QTableWidgetItem* name = m_periods->item(row, 0);
        const QTableWidgetItem* when = m_periods->item(row, 1);
        if (name == nullptr || when == nullptr || name->text().simplified().isEmpty()) {
            continue;
        }
        if (const std::optional<StoryTime> time = StoryTime::parse(when->text(), m_dayOne)) {
            periods.append(PlanPeriod{name->text().simplified(), *time});
        }
    }
    return periods;
}

}  // namespace kalahari::timeline
