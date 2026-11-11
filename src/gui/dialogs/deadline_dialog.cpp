/// @file deadline_dialog.cpp
/// @brief The small window of Add Deadline and Change Deadline

#include "kalahari/gui/dialogs/deadline_dialog.h"

#include <QComboBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// @brief Characters of a name at most: a line of the list of the deadlines
constexpr int MAX_NAME_LENGTH = 120;

/// @brief Words of a part at most
constexpr int MAX_WORDS = 10'000'000;

/// @brief The spaces before a part in the list, for its depth
QString indented(const QString& title, int depth)
{
    return QString(qsizetype(std::max(0, depth)) * 4, QChar(0x2002)) + title;
}

}  // namespace

DeadlineDialog::DeadlineDialog(const core::WritingDeadline& deadline,
                               const QList<BookPart>& parts, QWidget* parent)
    : KalahariDialog(parent)
{
    const bool adding = deadline.name.isEmpty();
    //: In Polish: Dodaj termin / Zmień termin
    const QString heading = adding ? tr("Add Deadline") : tr("Change Deadline");
    //: In Polish: Część książki albo cała książka na określony dzień, np. rozdziały dla
    //: redaktorki. Program przypomina o terminie na dole okna.
    setHeading(heading, tr("A part of the book, or all of it, due by a day, e.g. the chapters "
                           "for the editor. The program reminds you of it at the bottom of "
                           "the window."));
    setHeadingIcon(QStringLiteral("tools.writingGoals"));

    m_nameEdit = new QLineEdit(deadline.name, this);
    m_nameEdit->setObjectName(QStringLiteral("deadlineName"));
    m_nameEdit->setMaxLength(MAX_NAME_LENGTH);
    //: In Polish: np. Rozdziały 1–10 dla redaktorki
    m_nameEdit->setPlaceholderText(tr("e.g. Chapters 1-10 for the editor"));
    //: In Polish: &Nazwa
    addField(tr("&Name"), m_nameEdit);

    m_dateEdit = new QDateEdit(this);
    m_dateEdit->setObjectName(QStringLiteral("deadlineDate"));
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
    m_dateEdit->setDate(deadline.date.isValid() ? deadline.date
                                                : QDate::currentDate().addMonths(1));
    //: In Polish: &Data
    addField(tr("&Date"), m_dateEdit);

    m_partBox = new QComboBox(this);
    m_partBox->setObjectName(QStringLiteral("deadlinePart"));
    //: In Polish: cała książka
    m_partBox->addItem(tr("the whole book"), QString());
    for (const BookPart& part : parts) {
        m_partBox->addItem(indented(part.title, part.depth), part.id);
    }
    int current = m_partBox->findData(deadline.elementId);
    if (current < 0) {
        // A part the book has no more: kept, so that Change does not lose it
        //: In Polish: część, której nie ma już w książce
        m_partBox->addItem(tr("a part no longer in the book"), deadline.elementId);
        current = m_partBox->count() - 1;
    }
    m_partBox->setCurrentIndex(current);
    //: In Polish: &Czego dotyczy
    addField(tr("&What it is about"), m_partBox);

    m_wordsBox = new QSpinBox(this);
    m_wordsBox->setObjectName(QStringLiteral("deadlineWords"));
    m_wordsBox->setRange(0, MAX_WORDS);
    m_wordsBox->setSingleStep(1000);
    m_wordsBox->setGroupSeparatorShown(true);
    //: The words a part should have by the deadline: none. In Polish: brak
    m_wordsBox->setSpecialValueText(tr("none"));
    //: In Polish: " słów"
    m_wordsBox->setSuffix(tr(" words"));
    m_wordsBox->setValue(std::clamp(deadline.targetWords, 0, MAX_WORDS));
    //: In Polish: &Cel: słowa w tej części do terminu
    addField(tr("&Goal: the words of the part by then"), m_wordsBox);
    contentLayout()->addStretch(1);

    //: In Polish: &Dodaj / &Zmień
    setAcceptText(adding ? tr("&Add") : tr("&Change"));
    connect(m_nameEdit, &QLineEdit::textChanged, this, &DeadlineDialog::updateAcceptButton);
    updateAcceptButton();
    m_nameEdit->selectAll();
    m_nameEdit->setFocus();
}

core::WritingDeadline DeadlineDialog::deadline() const
{
    core::WritingDeadline deadline;
    deadline.name = m_nameEdit->text().trimmed();
    deadline.date = m_dateEdit->date();
    deadline.elementId = m_partBox->currentData().toString();
    deadline.targetWords = m_wordsBox->value();
    return deadline;
}

void DeadlineDialog::updateAcceptButton()
{
    acceptButton()->setEnabled(!m_nameEdit->text().trimmed().isEmpty());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
