/// @file challenge_dialog.cpp
/// @brief The small window of Add Challenge

#include "kalahari/gui/dialogs/challenge_dialog.h"

#include "kalahari/gui/settings/settings_page.h"

#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// @brief Words of a challenge at most
constexpr int MAX_WORDS = 10'000'000;

/// @brief The targets a new challenge starts with
constexpr int DEFAULT_WORDS = 5000;
constexpr int DEFAULT_DAYS = 7;

/// @brief The choices of the last day
enum EndChoice { EndOfWeek, EndOfMonth, ChosenDay };

}  // namespace

ChallengeDialog::ChallengeDialog(const QDate& today, QWidget* parent)
    : KalahariDialog(parent)
    , m_today(today.isValid() ? today : QDate::currentDate())
{
    setHeading(
               //: In Polish: Dodaj wyzwanie
               tr("Add Challenge"),
               //: In Polish: Twój własny cel na określony czas, np. 50 000 słów w miesiąc.
               //: Program podaje na dole okna, gdy wyzwanie się uda.
               tr("A goal of your own for a time, e.g. 50,000 words in a month. The program "
                  "says so at the bottom of the window when it is done."));
    setHeadingIcon(QStringLiteral("tools.addChallenge"));

    m_kindBox = new QComboBox(this);
    m_kindBox->setObjectName(QStringLiteral("challengeKind"));
    //: In Polish: Napisać określoną liczbę słów
    m_kindBox->addItem(tr("Write a number of words"), int(core::ChallengeKind::Words));
    //: In Polish: Pisać określoną liczbę dni z rzędu
    m_kindBox->addItem(tr("Write a number of days in a row"), int(core::ChallengeKind::DaysInRow));
    //: In Polish: Pisać w określonej liczbie dni
    m_kindBox->addItem(tr("Write on a number of days"), int(core::ChallengeKind::WritingDays));
    //: In Polish: &Wyzwanie
    addField(tr("&Challenge"), m_kindBox);

    m_targetBox = new QSpinBox(this);
    m_targetBox->setObjectName(QStringLiteral("challengeTarget"));
    m_targetBox->setGroupSeparatorShown(true);
    m_targetBox->setRange(1, MAX_WORDS);
    m_targetBox->setValue(DEFAULT_WORDS);
    m_targetBox->setSingleStep(1000);
    //: In Polish: &Ile
    addField(tr("&How much"), m_targetBox);

    m_endBox = new QComboBox(this);
    m_endBox->setObjectName(QStringLiteral("challengeEnd"));
    const QLocale locale;
    const QDate sunday = m_today.addDays(7 - m_today.dayOfWeek());
    const QDate lastOfMonth(m_today.year(), m_today.month(), m_today.daysInMonth());
    //: %1: the day. In Polish: do końca tygodnia (%1)
    m_endBox->addItem(tr("by the end of this week (%1)")
                          .arg(locale.toString(sunday, QStringLiteral("dddd, d MMMM"))),
                      sunday);
    //: %1: the day. In Polish: do końca miesiąca (%1)
    m_endBox->addItem(
        tr("by the end of this month (%1)").arg(locale.toString(lastOfMonth, QStringLiteral("d MMMM"))),
        lastOfMonth);
    //: In Polish: do wybranego dnia
    m_endBox->addItem(tr("by a day of your choice"), QDate());
    //: In Polish: &Do kiedy
    addField(tr("&By when"), m_endBox);

    m_endEdit = new QDateEdit(this);
    m_endEdit->setObjectName(QStringLiteral("challengeEndDay"));
    m_endEdit->setCalendarPopup(true);
    m_endEdit->setDisplayFormat(locale.dateFormat(QLocale::ShortFormat));
    m_endEdit->setMinimumDate(m_today);
    //: In Polish: &Ostatni dzień
    addField(tr("&Last day"), m_endEdit);

    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("challengeNote"));
    m_note->setWordWrap(true);
    m_note->setStyleSheet(QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
    contentLayout()->addWidget(m_note);
    contentLayout()->addStretch(1);

    //: In Polish: &Dodaj
    setAcceptText(tr("&Add"));
    connect(m_kindBox, &QComboBox::currentIndexChanged, this, &ChallengeDialog::updateTarget);
    connect(m_endBox, &QComboBox::currentIndexChanged, this, &ChallengeDialog::updateEnd);
    connect(m_targetBox, &QSpinBox::valueChanged, this, &ChallengeDialog::updateAcceptButton);
    connect(m_endEdit, &QDateEdit::dateChanged, this, &ChallengeDialog::updateAcceptButton);
    m_endBox->setCurrentIndex(EndOfMonth);
    updateTarget();
    updateEnd();
    m_kindBox->setFocus();
}

core::WritingChallenge ChallengeDialog::challenge() const
{
    core::WritingChallenge challenge;
    challenge.kind = core::ChallengeKind(m_kindBox->currentData().toInt());
    challenge.target = m_targetBox->value();
    challenge.start = m_today;
    challenge.end = endDay();
    return challenge;
}

QDate ChallengeDialog::endDay() const
{
    const QDate chosen = m_endBox->currentData().toDate();
    return chosen.isValid() ? chosen : m_endEdit->date();
}

void ChallengeDialog::updateTarget()
{
    const auto kind = core::ChallengeKind(m_kindBox->currentData().toInt());
    const bool words = kind == core::ChallengeKind::Words;
    const bool wasWords = m_shownKind == core::ChallengeKind::Words;
    m_targetBox->setSingleStep(words ? 1000 : 1);
    //: In Polish: " słów" / " dni"
    m_targetBox->setSuffix(words ? tr(" words") : tr(" days"));
    if (words != wasWords) {
        m_targetBox->setValue(words ? DEFAULT_WORDS : DEFAULT_DAYS);
    }
    m_shownKind = kind;
    updateAcceptButton();
}

void ChallengeDialog::updateEnd()
{
    // The field shows the last day of the choice; it is open for a day of one's own, which
    // starts from the day shown before
    const QDate preset = m_endBox->currentData().toDate();
    if (preset.isValid()) {
        const QSignalBlocker blocker(m_endEdit);
        m_endEdit->setDate(preset);
    }
    m_endEdit->setEnabled(m_endBox->currentIndex() == ChosenDay);
    updateAcceptButton();
}

void ChallengeDialog::updateAcceptButton()
{
    const core::WritingChallenge challenge = this->challenge();
    const int days = int(challenge.start.daysTo(challenge.end)) + 1;
    const bool ofDays = challenge.kind != core::ChallengeKind::Words;
    const bool fits = challenge.target > 0 && days > 0 && (!ofDays || challenge.target <= days);
    acceptButton()->setEnabled(fits);
    if (ofDays && !fits) {
        m_note->setText(days == 1
                            //: In Polish: Do tego dnia jest tylko jeden dzień, dzisiejszy.
                            ? tr("There is only one day until then: today.")
                            //: In Polish: Do tego dnia jest tylko %n dni.
                            : tr("There are only %n days until then.", nullptr, std::max(0, days)));
    } else {
        m_note->setText(days == 1
                            //: In Polish: Wyzwanie trwa jeden dzień: dziś.
                            ? tr("The challenge lasts one day: today.")
                            //: In Polish: Wyzwanie trwa %n dni, od dziś do ostatniego dnia
                            //: włącznie.
                            : tr("The challenge lasts %n days, from today to the last day.",
                                 nullptr, std::max(0, days)));
    }
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
