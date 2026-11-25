// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file join_book_dialog.cpp
/// @brief Book > Join Book as Volume...: a Kalahari book copied into the series as a new
///        volume, with its Workshop joined to the series', in one window from the choice of
///        the book to the result

#include "kalahari/gui/dialogs/join_book_dialog.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/volume_copies.h"
#include "kalahari/gui/dialogs/archive_dialogs.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/utils/program_folders.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <initializer_list>
#include <utility>

namespace kalahari::gui::dialogs {

namespace {

/// The characters of the list of the places that the window shows at least
constexpr int PLACE_LENGTH = 24;

/// A value of the rows of the book: plain text that can be selected
QLabel* newValue(QWidget* parent) {
    auto* value = new QLabel(parent);
    value->setTextFormat(Qt::PlainText);
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return value;
}

/// A checked row of the list of the same cards: the title of the card of @p book in @p pair,
/// and its kind as @p series names it
void addCardRow(QTreeWidget* list, const core::ProjectManager& series,
                const core::BookProject& book, const core::CardPair& pair) {
    const core::ProjectElement* card = book.findElement(pair.card);
    auto* item = new QTreeWidgetItem(list);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    item->setCheckState(0, Qt::Checked);
    item->setText(0, card != nullptr ? card->title : pair.card);
    if (card != nullptr) {
        item->setText(1, wordsOf(series, *card).form(QStringLiteral("singular")));
    }
    item->setForeground(1, list->palette().brush(QPalette::PlaceholderText));
}

}  // namespace

JoinBookDialog::JoinBookDialog(const core::ProjectManager& book, Joiner joiner, QWidget* parent)
    : ArchiveDialog(parent),
      m_book(book),
      m_joiner(std::move(joiner)),
      m_titleValue(newValue(this)),
      m_authorValue(newValue(this)),
      m_placeBox(new QComboBox(this)) {
    //: The title of the window of Book > Join Book as Volume... In Polish: Dołącz książkę jako
    //: tom
    setHeading(tr("Join Book as Volume"),
               //: In Polish: Książka programu Kalahari jako nowy tom, a jej Warsztat połączony
               //: z Warsztatem serii.
               tr("A Kalahari book as a new volume, its Workshop joined to the series'."));
    setHeadingIcon(QStringLiteral("book.joinVolume"));
    setCompactHeading(true);

    // The book and what it is
    //: A group of the window: the book that joins. In Polish: Książka
    QGridLayout* source = addGroup(tr("Book"));
    QPushButton* browseButton = nullptr;
    //: In Polish: Plik:
    addRow(source, tr("File:"), newPathField(&m_fileField, &browseButton));
    //: In the empty field of the book. In Polish: Wybierz książkę przyciskiem „Przeglądaj...”
    m_fileField->setPlaceholderText(tr("Choose the book with Browse..."));
    //: In Polish: Wybierz książkę w oknie systemowym
    browseButton->setToolTip(tr("Choose the book in the system window"));
    m_fileNote = addNote(source);
    //: A row of the book. In Polish: Tytuł:
    addRow(source, tr("Title:"), m_titleValue);
    //: In Polish: Autor:
    addRow(source, tr("Author:"), m_authorValue);
    connect(browseButton, &QPushButton::clicked, this, &JoinBookDialog::browse);

    // Its place among the volumes, at first the last one
    //: A group of the window: where the volume goes. In Polish: Miejsce
    QGridLayout* place = addGroup(tr("Place"));
    m_placeBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_placeBox->setMinimumContentsLength(PLACE_LENGTH);
    QStringList titles;
    if (const core::BookProject* project = m_book.project()) {
        for (const core::ProjectBook& volume : project->books) {
            titles << volume.title;
        }
    }
    const auto count = static_cast<int>(titles.size());
    //: A place of the volume: %1 the title of the first volume.
    //: In Polish: przed tomem 1 „%1” – tom 1
    m_placeBox->addItem(tr("before volume 1 \"%1\" – volume 1").arg(titles.value(0)), 0);
    for (int at = 1; at <= count; ++at) {
        //: A place of the volume: %1 the number of the volume before it, %2 its title, %3 the
        //: number of the new one. In Polish: po tomie %1 „%2” – tom %3
        m_placeBox->addItem(tr("after volume %1 \"%2\" – volume %3")
                                .arg(QString::number(at), titles.value(at - 1),
                                     QString::number(at + 1)),
                            at);
    }
    m_placeBox->setCurrentIndex(count);
    //: In Polish: &Miejsce:
    addRow(place, tr("&Place:"), m_placeBox);

    // The cards of the book that are those of the series
    m_cardsGroup = new QGroupBox(this);
    auto* cardsLayout = new QVBoxLayout(m_cardsGroup);
    //: The list of the cards of the book that have the kind and the title of a card of the
    //: series. In Polish: Te same karty
    m_cardList = newRowList(m_cardsGroup, tr("Same cards"));
    cardsLayout->addWidget(m_cardList, 1);
    //: Under the list of the same cards. In Polish: Zaznaczona karta jest kartą serii, która
    //: zachowuje to, co ma, i dostaje powiązania karty książki; niezaznaczona będzie osobną
    //: kartą.
    cardsLayout->addWidget(newRemark(
        m_cardsGroup, tr("A checked card is the series' card, which keeps what it has and gains "
                         "the links of the book's card; an unchecked one is a card of its own.")));
    m_cardsGroup->hide();

    // In two columns, for a low screen: the book and its place at the left, the cards at the
    // right
    auto* columns = new QHBoxLayout();
    auto* left = new QVBoxLayout();
    for (const QGridLayout* group : {source, place}) {
        QWidget* box = group->parentWidget();
        fieldsLayout()->removeWidget(box);
        left->addWidget(box);
    }
    //: Under the place of the volume. In Polish: Książka zostaje bez zmian, z historią
    //: pisania: seria dostaje kopie jej plików.
    left->addWidget(newRemark(this, tr("The book stays as it is, with its record of writing: the "
                                       "series gets copies of its files.")));
    left->addStretch(1);
    columns->addLayout(left, 1);
    columns->addWidget(m_cardsGroup, 1);
    fieldsLayout()->addLayout(columns);
    fieldsLayout()->addStretch();

    //: In Polish: Zmiany książki otwartej w innym oknie zostaną najpierw zapisane.
    setHint(tr("The changes of a book open in another window are saved first."));

    //: The button that joins the book. In Polish: &Dołącz
    setAcceptText(tr("&Join"));
    setManifest(QString());
    browseButton->setFocus();
}

QTreeWidget* JoinBookDialog::cardList() const {
    return m_cardList;
}

void JoinBookDialog::setManifest(const QString& path) {
    m_manifest = path;
    showPath(m_fileField, path);
    m_project.reset();
    m_readProblems.clear();
    if (!path.isEmpty()) {
        m_project = core::BookProject::load(path, m_readProblems);
    }
    dropFailure();
    showBook();
    updateFields();
}

QString JoinBookDialog::bookText() const {
    if (!m_project) {
        return {};
    }
    //: In Polish: Tytuł: %1\nAutor: %2
    return tr("Title: %1\nAuthor: %2").arg(m_titleValue->text(), m_authorValue->text());
}

QList<core::CardPair> JoinBookDialog::sameCards() const {
    QList<core::CardPair> checked;
    for (int row = 0; row < m_cardList->topLevelItemCount(); ++row) {
        if (m_cardList->topLevelItem(row)->checkState(0) == Qt::Checked &&
            row < m_pairs.size()) {
            checked << m_pairs.at(row);
        }
    }
    return checked;
}

void JoinBookDialog::run() {
    if (!m_project || !problemText().isEmpty()) {
        return;
    }
    startWork();
    const int index = m_placeBox->currentData().toInt();
    const Outcome outcome = m_joiner(m_manifest, index, sameCards());
    if (outcome.bookId.isEmpty()) {
        //: In Polish: Nie udało się dołączyć książki.
        failWork(tr("The book could not be joined."), QString(), outcome.problems);
        updateFields();
        return;
    }
    m_bookId = outcome.bookId;
    //: %1: the title of the book, %2: its number in the series. In Polish: Książka „%1” jest
    //: teraz tomem %2 serii.
    finishWork(tr("The book \"%1\" is now volume %2 of the series.")
                   .arg(m_titleValue->text(), QString::number(index + 1)),
               QString());
}

void JoinBookDialog::browse() {
    const QString start =
        m_manifest.isEmpty() ? ProgramFolders::windowFolder(ProgramFolders::Operation::OpenBook)
                             : QFileInfo(m_manifest).absolutePath();
    //: The title of the system window that chooses the book. In Polish: Wybierz książkę
    const QString chosen = QFileDialog::getOpenFileName(this, tr("Choose the Book"), start,
                                                        //: In Polish: Książka Kalahari (*.klh)
                                                        tr("Kalahari Book (*.klh)"));
    if (!chosen.isEmpty()) {
        setManifest(chosen);
    }
}

void JoinBookDialog::showBook() {
    const core::BookProject* project = m_project.has_value() ? &m_project.value() : nullptr;
    const core::ProjectBook* volume =
        project != nullptr && !project->books.isEmpty() ? &project->books.first() : nullptr;
    m_titleValue->setText(volume != nullptr ? volume->title : QString());
    m_authorValue->setText(volume != nullptr ? volume->author : QString());
    setRowVisible(m_titleValue, volume != nullptr);
    setRowVisible(m_authorValue, volume != nullptr);

    // The cards of the same kind and title, each checked at first
    m_cardList->clear();
    m_pairs.clear();
    if (project != nullptr && m_book.joinProblemOf(*project) == core::JoinProblem::None) {
        m_pairs = m_book.sameCardsOf(*project);
        for (const core::CardPair& pair : std::as_const(m_pairs)) {
            addCardRow(m_cardList, m_book, *project, pair);
        }
    }
    m_cardList->setRows(static_cast<int>(m_pairs.size()));
    if (!m_pairs.isEmpty()) {
        m_cardList->setCurrentItem(m_cardList->topLevelItem(0));
    }
    //: A group of the window; %1: how many cards it lists. In Polish: Te same karty (%1)
    m_cardsGroup->setTitle(tr("Same Cards (%1)").arg(m_pairs.size()));
    m_cardsGroup->setVisible(!m_pairs.isEmpty());
}

void JoinBookDialog::updateFields() {
    const QString problem = problemText();
    showNote(m_fileNote, problem,
             problem.isEmpty() ? QString() : QStringLiteral("common.warning"));
    setFieldProblem(m_project ? QString() : problem, m_readProblems);
    acceptButton()->setEnabled(m_project.has_value() && problem.isEmpty());
}

QString JoinBookDialog::problemText() const {
    if (m_manifest.isEmpty()) {
        return {};
    }
    if (!m_project) {
        //: Under a book that cannot be read. In Polish: Kalahari nie może odczytać tej książki.
        QString text = tr("Kalahari cannot read this book.");
        if (!m_readProblems.isEmpty()) {
            text += QLatin1Char(' ') + copyHint();
        }
        return text;
    }
    switch (m_book.joinProblemOf(*m_project)) {
        case core::JoinProblem::None:
            return {};
        case core::JoinProblem::Series:
            //: In Polish: Ta książka jest serią: dołączyć można tylko książkę jednotomową.
            return tr("This book is a series: only a book of one volume can join.");
        case core::JoinProblem::OtherType:
            //: In Polish: Ta książka jest innego typu: dołączyć można tylko książkę tego
            //: samego typu.
            return tr("The book is of another type: only a book of the same type can join.");
        case core::JoinProblem::VolumeOfSeries:
            //: In Polish: Seria ma już tę książkę: to ta sama książka albo tom wydzielony z
            //: serii, który wciąż w niej jest.
            return tr("The series has this book already: it is the same book, or a volume taken "
                      "out of the series that is still in it.");
    }
    return {};
}

}  // namespace kalahari::gui::dialogs
