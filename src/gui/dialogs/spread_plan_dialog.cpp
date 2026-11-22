// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file spread_plan_dialog.cpp
/// @brief The small window Plan the Texts of the tab Writing Schedule

#include "kalahari/gui/dialogs/spread_plan_dialog.h"

#include "kalahari/gui/widgets/stats_painter.h"

#include <QButtonGroup>
#include <QDateEdit>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace kalahari::gui::dialogs {

namespace {

/// @brief Words of a text at most
constexpr int MAX_WORDS = 10'000'000;

/// @brief The length of a text without its own, without a length of the book
constexpr int DEFAULT_TEXT_WORDS = 3000;

/// @brief The last day without a deadline of the book: four weeks from the first
constexpr int DAYS_WITHOUT_DEADLINE = 27;

/// @brief The text is written still: not in Revision, not Final
bool isBeingWritten(const ScheduleRow& row)
{
    return row.kind == ScheduleRow::Kind::Text && row.status != QLatin1String("revision")
           && row.status != QLatin1String("final");
}

QDateEdit* dateField(const QDate& day, const QString& name, QWidget* parent)
{
    auto* field = new QDateEdit(parent);
    field->setObjectName(name);
    field->setCalendarPopup(true);
    field->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
    field->setDate(day);
    return field;
}

/// @brief The words @p row still needs: its length (or @p defaultLength) less its words
int wordsLeftOf(const ScheduleRow& row, const core::WritingPlan& plan, int defaultLength)
{
    const core::TextPlan* found = plan.find(row.id);
    const int target = found != nullptr && found->targetWords > 0 ? found->targetWords
                                                                  : defaultLength;
    return std::max(0, target - row.words);
}

}  // namespace

SpreadPlanDialog::SpreadPlanDialog(const QList<ScheduleRow>& rows, const core::WritingPlan& plan,
                                   const core::BookGoals& goals, const core::WritingRules& rules,
                                   const QDate& today, QWidget* parent)
    : KalahariDialog(parent)
    , m_rows(rows)
    , m_plan(plan)
    , m_rules(rules)
{
    const QDate first = today.isValid() ? today : QDate::currentDate();
    //: The window that plans the writing of many texts at once. In Polish: Rozplanuj teksty
    const QString heading = tr("Plan the Texts");
    //: In Polish: Rozkłada pisanie tekstów jeden po drugim, w kolejności książki, na dni
    //: pisania – każdy według słów, których mu brakuje. Zaplanowane poprawki zostają.
    setHeading(heading, tr("Lays the writing of the texts one after another, in the order of "
                           "the book, on the writing days, each by the words it still needs. "
                           "The revisions planned stay."));
    setHeadingIcon(QStringLiteral("tools.writingSchedule"));

    //: In Polish: Teksty &bez planu
    m_withoutPlan = new QRadioButton(tr("Texts &without a plan"), this);
    m_withoutPlan->setObjectName(QStringLiteral("spreadWithoutPlan"));
    //: Draft is the state of a text, as in the Navigator. In Polish: &Wszystkie teksty w stanie
    //: „Szkic”
    m_beingWritten = new QRadioButton(tr("&All the texts in Draft"), this);
    m_beingWritten->setObjectName(QStringLiteral("spreadBeingWritten"));
    auto* choice = new QButtonGroup(this);
    choice->addButton(m_withoutPlan);
    choice->addButton(m_beingWritten);
    // The texts without a plan when there are any, otherwise all of them again
    const bool anyWithout = !textsToPlan(rows, plan, Texts::WithoutPlan).isEmpty();
    m_withoutPlan->setChecked(anyWithout);
    m_beingWritten->setChecked(!anyWithout);
    contentLayout()->addWidget(m_withoutPlan);
    contentLayout()->addWidget(m_beingWritten);

    const bool deadline = goals.deadline.isValid() && goals.deadline >= first;
    m_from = dateField(first, QStringLiteral("spreadFrom"), this);
    //: The first day of the writing planned. In Polish: &Od:
    addField(tr("&From:"), m_from);
    m_to = dateField(deadline ? goals.deadline : first.addDays(DAYS_WITHOUT_DEADLINE),
                     QStringLiteral("spreadTo"), this);
    //: The last day of the writing planned. In Polish: &Do:
    addField(tr("&To:"), m_to);
    if (!deadline) {
        auto* hint = new QLabel(this);
        hint->setObjectName(QStringLiteral("spreadNoDeadline"));
        hint->setWordWrap(true);
        hint->setForegroundRole(QPalette::PlaceholderText);
        //: In Polish: Książka nie ma terminu (Cele i terminy pisania): ostatni dzień do wyboru.
        hint->setText(tr("The book has no deadline (Writing Goals and Deadlines): the last day "
                         "is yours to choose."));
        contentLayout()->addWidget(hint);
    }

    m_length = new QSpinBox(this);
    m_length->setObjectName(QStringLiteral("spreadLength"));
    m_length->setRange(0, MAX_WORDS);
    m_length->setSingleStep(500);
    m_length->setGroupSeparatorShown(true);
    //: The length of a text: none. In Polish: brak
    m_length->setSpecialValueText(tr("none"));
    //: In Polish: " słów"
    m_length->setSuffix(tr(" words"));
    m_length->setValue(defaultLengthOf(rows, goals));
    //: The length a text without its own takes, and keeps in its plan. In Polish: Dłu&gość
    //: tekstu bez własnej:
    addField(tr("&Length of a text without its own:"), m_length);

    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("spreadNote"));
    m_note->setWordWrap(true);
    m_note->setTextFormat(Qt::PlainText);
    contentLayout()->addWidget(m_note);
    contentLayout()->addStretch(1);

    //: In Polish: &Rozplanuj
    setAcceptText(tr("&Plan"));

    for (QRadioButton* button : {m_withoutPlan, m_beingWritten}) {
        connect(button, &QRadioButton::toggled, this, &SpreadPlanDialog::updateFields);
    }
    for (QDateEdit* field : {m_from, m_to}) {
        connect(field, &QDateEdit::dateChanged, this, &SpreadPlanDialog::updateFields);
    }
    connect(m_length, &QSpinBox::valueChanged, this, &SpreadPlanDialog::updateFields);
    updateFields();
    (m_withoutPlan->isChecked() ? m_withoutPlan : m_beingWritten)->setFocus();
}

SpreadPlanDialog::Texts SpreadPlanDialog::chosenTexts() const
{
    return m_beingWritten->isChecked() ? Texts::BeingWritten : Texts::WithoutPlan;
}

core::WritingPlan SpreadPlanDialog::plan() const
{
    return spread(m_rows, m_plan, chosenTexts(), m_from->date(),
                  std::max(m_from->date(), m_to->date()), m_length->value(), m_rules);
}

QStringList SpreadPlanDialog::textsToPlan(const QList<ScheduleRow>& rows,
                                          const core::WritingPlan& plan, Texts texts)
{
    QStringList ids;
    for (const ScheduleRow& row : rows) {
        if (!isBeingWritten(row) || row.id.isEmpty()) {
            continue;
        }
        const core::TextPlan* found = plan.find(row.id);
        if (texts == Texts::BeingWritten || found == nullptr || !found->hasWriting()) {
            ids << row.id;
        }
    }
    return ids;
}

core::WritingPlan SpreadPlanDialog::spread(const QList<ScheduleRow>& rows,
                                           const core::WritingPlan& plan, Texts texts,
                                           const QDate& from, const QDate& to, int defaultLength,
                                           const core::WritingRules& rules)
{
    // The texts to plan and their words, both in the order of the rows
    const QStringList chosen = textsToPlan(rows, plan, texts);
    QStringList ids;
    QList<int> wordsLeft;
    for (const ScheduleRow& row : rows) {
        if (chosen.contains(row.id) && !ids.contains(row.id)) {
            ids << row.id;
            wordsLeft << wordsLeftOf(row, plan, defaultLength);
        }
    }
    const QList<std::pair<QDate, QDate>> bars =
        core::spreadOverWritingDays(rules, wordsLeft, from, to);
    core::WritingPlan spreadPlan = plan;
    for (qsizetype i = 0; i < bars.size() && i < ids.size(); ++i) {
        core::TextPlan text;
        if (const core::TextPlan* found = plan.find(ids.at(i))) {
            text = *found;
        }
        text.elementId = ids.at(i);
        text.start = bars.at(i).first;
        text.end = bars.at(i).second;
        if (text.targetWords <= 0) {
            text.targetWords = std::max(0, defaultLength);
        }
        spreadPlan.set(text);
    }
    return spreadPlan;
}

int SpreadPlanDialog::defaultLengthOf(const QList<ScheduleRow>& rows,
                                      const core::BookGoals& goals)
{
    const auto texts = std::count_if(rows.cbegin(), rows.cend(), [](const ScheduleRow& row) {
        return row.kind == ScheduleRow::Kind::Text;
    });
    if (goals.targetWords <= 0 || texts == 0) {
        return DEFAULT_TEXT_WORDS;
    }
    const double share = double(goals.targetWords) / double(texts);
    return std::clamp(int(std::lround(share / 100.0)) * 100, 100, MAX_WORDS);
}

void SpreadPlanDialog::updateFields()
{
    m_to->setMinimumDate(m_from->date());
    const QDate from = m_from->date();
    const QDate to = std::max(from, m_to->date());
    const QStringList ids = textsToPlan(m_rows, m_plan, chosenTexts());
    const int days = core::writingDaysBetween(m_rules, from, to);
    QStringList lines;
    if (ids.isEmpty()) {
        //: In Polish: Nie ma tekstów do rozplanowania.
        lines << tr("There is no text to plan.");
    } else if (days == 0) {
        //: In Polish: Między tymi dniami nie ma dnia pisania.
        lines << tr("There is no writing day between these days.");
    } else {
        qint64 left = 0;
        int replaced = 0;
        for (const ScheduleRow& row : std::as_const(m_rows)) {
            if (ids.contains(row.id)) {
                left += wordsLeftOf(row, m_plan, m_length->value());
                const core::TextPlan* found = m_plan.find(row.id);
                replaced += found != nullptr && found->hasWriting() ? 1 : 0;
            }
        }
        const int leftWords = int(std::min<qint64>(left, MAX_WORDS));
        //: In Polish: %n tekst / %n teksty / %n tekstów
        const QString textsText = tr("%n texts", nullptr, int(ids.size()));
        //: In Polish: %n dzień pisania / %n dni pisania
        const QString daysText = tr("%n writing days", nullptr, days);
        //: %1: the texts, %2: the words they still need, %3: the writing days, %4: the words
        //: of a writing day. In Polish: %1, brakuje im %2; %3 – około %4 na dzień pisania.
        lines << tr("%1 lack %2, on %3: about %4 a writing day.")
                     .arg(textsText, stats_painter::words(leftWords), daysText,
                          stats_painter::words(core::wordsPerWritingDay(leftWords, days)));
        if (replaced > 0) {
            //: In Polish: Pisanie %n tekstu / tekstów zostanie zaplanowane od nowa.
            lines << tr("The writing of %n texts is planned anew.", nullptr, replaced);
        }
    }
    m_note->setText(lines.join(QLatin1Char('\n')));
    acceptButton()->setEnabled(!ids.isEmpty() && days > 0);
}

}  // namespace kalahari::gui::dialogs
