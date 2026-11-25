// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file text_plan_dialog.cpp
/// @brief The small window of the plan of a text

#include "kalahari/gui/dialogs/text_plan_dialog.h"

#include "kalahari/core/writing_goals.h"
#include "kalahari/gui/widgets/stats_painter.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QGridLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::gui::dialogs {

namespace {

/// @brief Words of a text at most
constexpr int MAX_WORDS = 10'000'000;

/// @brief The days of writing of a new plan
constexpr int NEW_PLAN_DAYS = 13;

QDateEdit* dateField(const QDate& day, const QString& name, QWidget* parent)
{
    auto* field = new QDateEdit(parent);
    field->setObjectName(name);
    field->setCalendarPopup(true);
    field->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
    field->setDate(day);
    return field;
}

}  // namespace

TextPlanDialog::TextPlanDialog(const QString& title, const core::TextPlan& plan, int words,
                               const core::WritingRules& rules, const QDate& today,
                               QWidget* parent)
    : KalahariDialog(parent)
    , m_elementId(plan.elementId)
    , m_words(words)
    , m_rules(rules)
    , m_today(today.isValid() ? today : QDate::currentDate())
{
    //: In Polish: Plan tekstu
    const QString heading = tr("Plan of the Text");
    //: %1: the title of the text. In Polish: %1: kiedy powstaje i kiedy jest poprawiany.
    setHeading(heading, tr("%1: when it is written and when it is revised.").arg(title));
    setHeadingIcon(QStringLiteral("tools.writingSchedule"));

    const bool planned = plan.hasWriting() || plan.hasRevision();
    const QDate start = plan.hasWriting() ? plan.start : m_today;
    const QDate end = plan.hasWriting() ? plan.end : m_today.addDays(NEW_PLAN_DAYS);
    const QDate revisionStart = plan.hasRevision() ? plan.revisionStart : end.addDays(1);
    const QDate revisionEnd = plan.hasRevision() ? plan.revisionEnd : end.addDays(3);

    auto* stages = new QWidget(this);
    auto* grid = new QGridLayout(stages);
    grid->setContentsMargins(0, 0, 0, 0);
    //: The first day of a stage of the plan. In Polish: od
    const QString fromText = tr("from");
    //: The last day of a stage of the plan. In Polish: do
    const QString toText = tr("to");

    //: In Polish: &Pisanie
    m_writing = new QCheckBox(tr("&Writing"), stages);
    m_writing->setObjectName(QStringLiteral("planWriting"));
    m_writing->setChecked(plan.hasWriting() || !planned);
    m_start = dateField(start, QStringLiteral("planStart"), stages);
    //: In Polish: Pisanie: pierwszy dzień
    m_start->setAccessibleName(tr("Writing: the first day"));
    m_end = dateField(end, QStringLiteral("planEnd"), stages);
    //: In Polish: Pisanie: ostatni dzień
    m_end->setAccessibleName(tr("Writing: the last day"));
    grid->addWidget(m_writing, 0, 0);
    grid->addWidget(new QLabel(fromText, stages), 0, 1);
    grid->addWidget(m_start, 0, 2);
    grid->addWidget(new QLabel(toText, stages), 0, 3);
    grid->addWidget(m_end, 0, 4);

    //: In Polish: P&oprawki
    m_revision = new QCheckBox(tr("&Revisions"), stages);
    m_revision->setObjectName(QStringLiteral("planRevision"));
    m_revision->setChecked(plan.hasRevision());
    m_revisionStart = dateField(revisionStart, QStringLiteral("planRevisionStart"), stages);
    //: In Polish: Poprawki: pierwszy dzień
    m_revisionStart->setAccessibleName(tr("Revisions: the first day"));
    m_revisionEnd = dateField(revisionEnd, QStringLiteral("planRevisionEnd"), stages);
    //: In Polish: Poprawki: ostatni dzień
    m_revisionEnd->setAccessibleName(tr("Revisions: the last day"));
    grid->addWidget(m_revision, 1, 0);
    grid->addWidget(new QLabel(fromText, stages), 1, 1);
    grid->addWidget(m_revisionStart, 1, 2);
    grid->addWidget(new QLabel(toText, stages), 1, 3);
    grid->addWidget(m_revisionEnd, 1, 4);
    grid->setColumnStretch(5, 1);
    contentLayout()->addWidget(stages);

    m_length = new QSpinBox(this);
    m_length->setObjectName(QStringLiteral("planLength"));
    m_length->setRange(0, MAX_WORDS);
    m_length->setSingleStep(500);
    m_length->setGroupSeparatorShown(true);
    //: The length of a text: none. In Polish: brak
    m_length->setSpecialValueText(tr("none"));
    //: In Polish: " słów"
    m_length->setSuffix(tr(" words"));
    m_length->setValue(std::clamp(plan.targetWords, 0, MAX_WORDS));
    //: In Polish: &Długość: słowa gotowego tekstu
    addField(tr("&Length: the words of the written text"), m_length);

    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("planNote"));
    m_note->setWordWrap(true);
    m_note->setTextFormat(Qt::PlainText);
    contentLayout()->addWidget(m_note);
    contentLayout()->addStretch(1);

    //: In Polish: &Zapisz
    setAcceptText(tr("&Save"));
    if (planned) {
        //: In Polish: &Usuń plan
        QPushButton* remove = addSideButton(tr("Re&move Plan"));
        remove->setObjectName(QStringLiteral("planRemove"));
        connect(remove, &QPushButton::clicked, this, [this]() {
            m_removed = true;
            accept();
        });
    }

    for (QCheckBox* box : {m_writing, m_revision}) {
        connect(box, &QCheckBox::toggled, this, &TextPlanDialog::updateFields);
    }
    for (QDateEdit* field : {m_start, m_end, m_revisionStart, m_revisionEnd}) {
        connect(field, &QDateEdit::dateChanged, this, &TextPlanDialog::updateFields);
    }
    connect(m_length, &QSpinBox::valueChanged, this, &TextPlanDialog::updateNote);
    updateFields();
    m_writing->setFocus();
}

core::TextPlan TextPlanDialog::plan() const
{
    core::TextPlan plan;
    plan.elementId = m_elementId;
    if (m_removed) {
        return plan;
    }
    if (m_writing->isChecked()) {
        plan.start = m_start->date();
        plan.end = std::max(m_start->date(), m_end->date());
    }
    if (m_revision->isChecked()) {
        plan.revisionStart = m_revisionStart->date();
        plan.revisionEnd = std::max(m_revisionStart->date(), m_revisionEnd->date());
    }
    plan.targetWords = m_length->value();
    return plan;
}

void TextPlanDialog::updateFields()
{
    m_start->setEnabled(m_writing->isChecked());
    m_end->setEnabled(m_writing->isChecked());
    m_revisionStart->setEnabled(m_revision->isChecked());
    m_revisionEnd->setEnabled(m_revision->isChecked());
    // The last day not before the first
    m_end->setMinimumDate(m_start->date());
    m_revisionEnd->setMinimumDate(m_revisionStart->date());
    updateNote();
}

void TextPlanDialog::updateNote()
{
    QStringList lines;
    if (m_writing->isChecked()) {
        const QDate start = m_start->date();
        const QDate end = std::max(start, m_end->date());
        const int days = core::writingDaysBetween(m_rules, start, end);
        //: In Polish: Pisanie: %n dzień pisania / dni pisania.
        QString line = tr("Writing: %n writing days.", nullptr, days);
        const int left = std::max(0, m_length->value() - m_words);
        if (left > 0 && end >= m_today) {
            const int daysLeft = core::writingDaysBetween(m_rules, std::max(start, m_today), end);
            //: %1: the words of the text now, %2: the words left, %3: the words of a writing
            //: day. In Polish: Tekst ma %1, zostało %2, czyli %3 na dzień pisania.
            line += QLatin1Char(' ')
                    + tr("The text has %1, %2 are left, that is %3 a writing day.")
                          .arg(stats_painter::words(m_words), stats_painter::words(left),
                               stats_painter::words(core::wordsPerWritingDay(left, daysLeft)));
        }
        lines << line;
    }
    if (m_revision->isChecked()) {
        const QDate start = m_revisionStart->date();
        const int days =
            core::writingDaysBetween(m_rules, start, std::max(start, m_revisionEnd->date()));
        //: In Polish: Poprawki: %n dzień pisania / dni pisania.
        lines << tr("Revisions: %n writing days.", nullptr, days);
        if (m_writing->isChecked() && start <= std::max(m_start->date(), m_end->date())) {
            //: In Polish: Poprawki zaczynają się, zanim skończy się pisanie.
            lines << tr("The revisions start before the writing ends.");
        }
    }
    if (lines.isEmpty()) {
        //: In Polish: Bez pisania i poprawek tekst nie ma planu.
        lines << tr("Without writing and revisions the text has no plan.");
    }
    m_note->setText(lines.join(QLatin1Char('\n')));
}

} // namespace kalahari::gui::dialogs
