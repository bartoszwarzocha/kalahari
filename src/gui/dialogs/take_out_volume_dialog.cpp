// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file take_out_volume_dialog.cpp
/// @brief Take Out as a Book... of a volume: the volume copied out of the series as a book of
///        its own, in one window from the choice of its place to the result

#include "kalahari/gui/dialogs/take_out_volume_dialog.h"

#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/archive_dialogs.h"
#include "kalahari/gui/dialogs/delete_dialogs.h"
#include "kalahari/gui/utils/program_folders.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <initializer_list>
#include <utility>

namespace kalahari::gui::dialogs {

namespace {

/// A value of the rows of the new book: plain text that can be selected
QLabel* newValue(const QString& text, QWidget* parent) {
    auto* value = new QLabel(text, parent);
    value->setTextFormat(Qt::PlainText);
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return value;
}

/// The row under the last row of @p grid; a grid without widgets has none
int nextRow(const QGridLayout* grid) {
    return grid->count() == 0 ? 0 : grid->rowCount();
}

}  // namespace

TakeOutVolumeDialog::TakeOutVolumeDialog(Volume volume, const QString& folder, Taker taker,
                                         QWidget* parent)
    : ArchiveDialog(parent), m_volume(std::move(volume)), m_taker(std::move(taker)) {
    //: The title of the window of Take Out as a Book... of a volume. In Polish: Wydziel tom
    setHeading(tr("Take Out Volume"),
               //: %1: the number of the volume, %2: its title. In Polish: Tom %1 „%2” jako
               //: osobna książka.
               tr("Volume %1 \"%2\" as a book of its own.")
                   .arg(QString::number(m_volume.number), m_volume.title));
    setHeadingIcon(QStringLiteral("book.takeOutVolume"));
    setCompactHeading(true);

    // The folder in which the folder of the book is made
    //: A group of the window: where the new book goes. In Polish: Miejsce
    QGridLayout* place = addGroup(tr("Place"));
    QPushButton* browseButton = nullptr;
    //: In Polish: Folder:
    addRow(place, tr("Folder:"), newPathField(&m_folderField, &browseButton));
    //: In Polish: Wybierz folder w oknie systemowym
    browseButton->setToolTip(tr("Choose the folder in the system window"));
    m_folderNote = addNote(place);
    connect(browseButton, &QPushButton::clicked, this, &TakeOutVolumeDialog::browseFolder);

    // What the book gets
    //: A group of the window: what the new book gets. In Polish: Nowa książka
    QGridLayout* book = addGroup(tr("New Book"));
    //: A row of the new book. In Polish: Tom:
    addRow(book, tr("Volume:"),
           //: In Polish: Teksty tomu z ich wersjami i obrazami, jego okładka, teksty dla
           //: wydawnictwa i cele.
           newValue(tr("The texts of the volume with their versions and images, its cover, its "
                       "texts for publishers and its goals."),
                    this));
    //: A row of the new book. In Polish: Warsztat:
    addRow(book, tr("Workshop:"),
           //: In Polish: Warsztat serii bez okładek i tekstów dla wydawnictwa innych tomów i bez
           //: powiązań z ich tekstami.
           newValue(tr("The Workshop of the series, without the covers and the texts for "
                       "publishers of the other volumes and without the links to their texts."),
                    this));
    const FieldNote record = addNote(book);
    //: Under the rows of the new book. In Polish: Historia pisania zostaje w serii, a nowa
    //: książka zaczyna własną.
    showNote(record, tr("The record of writing stays with the series, and the new book starts "
                        "its own."));

    // The series without the volume, and the new book in a new window
    //: A group of the window. In Polish: Po wydzieleniu
    QGridLayout* after = addGroup(tr("After Taking Out"));
    //: In Polish: &Usuń tom z serii
    m_deleteBox = new QCheckBox(tr("&Delete the volume from the series"), after->parentWidget());
    m_deleteBox->setEnabled(m_volume.canDelete);
    connect(m_deleteBox, &QCheckBox::toggled, this, &TakeOutVolumeDialog::dropFailure);
    after->addWidget(m_deleteBox, nextRow(after), 1);
    m_deleteNote = addNote(after);
    if (m_volume.canDelete) {
        const QHash<QString, core::KindWords> nouns{{QStringLiteral("bin"), trashWords()}};
        //: Under the check box that deletes the volume. In Polish: Jego pliki trafią do Kosza,
        //: jak przy „Usuń tom...”; nowa książka ma ich kopie.
        showNote(m_deleteNote,
                 core::fillWords(tr("Its files go to the {bin}, as with Delete Volume...; the new "
                                    "book has copies of them."),
                                 nouns));
    } else {
        //: Under the check box that deletes the volume, in a book open read-only. In Polish:
        //: Książka jest otwarta tylko do odczytu: tom zostaje w serii.
        showNote(m_deleteNote, tr("The book is open read-only: the volume stays in the series."));
    }
    //: In Polish: &Otwórz nową książkę w nowym oknie
    m_openBox = new QCheckBox(tr("&Open the new book in a new window"), after->parentWidget());
    m_openBox->setChecked(true);
    after->addWidget(m_openBox, nextRow(after), 1);

    // In two columns, for a low screen: the place and what comes after it at the left, the new
    // book at the right
    auto* columns = new QHBoxLayout();
    auto* left = new QVBoxLayout();
    for (const QGridLayout* group : {place, after}) {
        QWidget* box = group->parentWidget();
        fieldsLayout()->removeWidget(box);
        left->addWidget(box);
    }
    left->addStretch(1);
    QWidget* bookBox = book->parentWidget();
    fieldsLayout()->removeWidget(bookBox);
    columns->addLayout(left, 1);
    columns->addWidget(bookBox, 1, Qt::AlignTop);
    fieldsLayout()->addLayout(columns);
    fieldsLayout()->addStretch();

    //: In Polish: Postęp i wynik pokażą się tutaj.
    setHint(tr("The progress and the result will show here."));

    //: The button that takes the volume out. In Polish: &Wydziel
    setAcceptText(tr("&Take Out"));
    setFolder(folder);
    acceptButton()->setFocus();
}

void TakeOutVolumeDialog::setFolder(const QString& folder) {
    m_folder = ProgramFolders::cleanPath(folder);
    showPath(m_folderField, m_folder);
    dropFailure();
    updateFields();
}

QString TakeOutVolumeDialog::bookFolder() const {
    return core::ProjectManager::newProjectFolder(m_folder, m_volume.title, true);
}

void TakeOutVolumeDialog::run() {
    startWork();
    const bool deleteVolume = m_deleteBox->isChecked();
    const Outcome outcome = m_taker(m_folder, deleteVolume);
    if (outcome.manifest.isEmpty()) {
        //: In Polish: Nie udało się wydzielić tomu.
        failWork(tr("The volume could not be taken out."), QString(), outcome.problems);
        updateFields();
        return;
    }
    m_manifest = outcome.manifest;
    const QString folder = QFileInfo(m_manifest).absolutePath();
    //: %1: the title of the volume, %2: the folder of the new book. In Polish: Tom „%1” jest
    //: teraz osobną książką w folderze %2.
    QString text = tr("Volume \"%1\" is now a book of its own in the folder %2.")
                       .arg(m_volume.title, QDir::toNativeSeparators(folder));
    if (outcome.volumeDeleted) {
        //: After the result. In Polish: Seria nie ma już tego tomu.
        text += QLatin1Char(' ') + tr("The series no longer has the volume.");
    }
    text += QLatin1Char(' ');
    if (m_openBox->isChecked()) {
        //: After the result. In Polish: Książka otworzy się w nowym oknie, gdy zamkniesz to
        //: okno.
        text += tr("The book opens in a new window when you close this one.");
    } else {
        //: After the result. In Polish: Książkę znajdziesz na Pulpicie programu i w menu Plik
        //: > Ostatnie książki.
        text += tr("The book is on the Dashboard and in File > Recent Books.");
    }
    finishWork(text, folder);
    if (deleteVolume && !outcome.volumeDeleted) {
        //: Under the result, when the volume could not be deleted from the series. In Polish:
        //: Tom zostaje w serii: nie udało się zapisać projektu.
        setResultNotes({tr("The volume stays in the series: the project could not be saved.")});
    }
}

void TakeOutVolumeDialog::browseFolder() {
    // A folder that does not exist yet opens the window in the nearest one that does
    QString start = ProgramFolders::existingFolderAt(m_folder);
    if (start.isEmpty()) {
        start = ProgramFolders::documentsFolder();
    }
    //: The title of the system window that chooses where the new book goes. In Polish:
    //: Miejsce książki
    const QString chosen = QFileDialog::getExistingDirectory(
        this, tr("Place of the Book"), start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!chosen.isEmpty()) {
        setFolder(chosen);
    }
}

void TakeOutVolumeDialog::updateFields() {
    // Where the book goes, or why it cannot go there
    bool usable = false;
    const QString problem = ProgramFolders::problemOf(m_folder);
    const QString target = bookFolder();
    if (!problem.isEmpty()) {
        showNote(m_folderNote, problem, QStringLiteral("common.warning"));
    } else if (target.isEmpty()) {
        //: Under the folder, when the title of the volume cannot name a folder. In Polish:
        //: Z tytułu tomu nie powstanie nazwa folderu. Zmień tytuł we właściwościach tomu.
        showNote(m_folderNote,
                 tr("The title of the volume gives no name of a folder. Change the title in the "
                    "properties of the volume."),
                 QStringLiteral("common.warning"));
    } else if (!core::ProjectManager::canHoldNewProject(target)) {
        //: %1: the name of the folder of the book. In Polish: W tym miejscu jest już folder
        //: „%1” i nie jest pusty. Wybierz dla książki inne miejsce.
        showNote(m_folderNote,
                 tr("The folder \"%1\" is already here and is not empty. Choose another place "
                    "for the book.")
                     .arg(QFileInfo(target).fileName()),
                 QStringLiteral("common.warning"));
    } else {
        usable = true;
        //: %1: the name of the folder of the book. In Polish: Książka powstanie w folderze
        //: „%1”.
        showNote(m_folderNote,
                 tr("The book goes to the folder \"%1\".").arg(QFileInfo(target).fileName()));
    }
    acceptButton()->setEnabled(usable);
}

}  // namespace kalahari::gui::dialogs
