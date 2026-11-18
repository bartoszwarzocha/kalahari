// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file new_volume_dialog.cpp
/// @brief Book > New Volume...: a new book of a series, with the same Workshop

#include "kalahari/gui/dialogs/new_volume_dialog.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari::gui::dialogs {

namespace {

/// The characters of the list of the places that the window shows at least
constexpr int PLACE_LENGTH = 24;

/// A label of a field in a grid: its text, its buddy
QLabel* fieldLabel(const QString& text, QWidget* field, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setBuddy(field);
    return label;
}

/// A note under a field or an option: grey, wrapped
QLabel* noteLabel(const QString& text, QWidget* parent) {
    auto* note = new QLabel(text, parent);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    return note;
}

/// The languages of the text a volume can have, as the book's properties offer them
QList<std::pair<QString, QString>> languages() {
    return {{QStringLiteral("en"), NewVolumeDialog::tr("English")},
            {QStringLiteral("pl"), NewVolumeDialog::tr("Polish")},
            {QStringLiteral("de"), NewVolumeDialog::tr("German")},
            {QStringLiteral("fr"), NewVolumeDialog::tr("French")},
            {QStringLiteral("es"), NewVolumeDialog::tr("Spanish")},
            {QStringLiteral("it"), NewVolumeDialog::tr("Italian")},
            {QStringLiteral("pt"), NewVolumeDialog::tr("Portuguese")},
            {QStringLiteral("ru"), NewVolumeDialog::tr("Russian")},
            {QStringLiteral("zh"), NewVolumeDialog::tr("Chinese")},
            {QStringLiteral("ja"), NewVolumeDialog::tr("Japanese")}};
}

}  // namespace

NewVolumeDialog::NewVolumeDialog(const core::ProjectManager& book, QWidget* parent)
    : KalahariDialog(parent) {
    if (const core::BookProject* project = book.project()) {
        for (const core::ProjectBook& volume : project->books) {
            m_titles << volume.title;
        }
    }
    //: Book > New Volume... In Polish: Nowy tom / Nowa książka serii, z tym samym Warsztatem.
    setHeading(tr("New Volume"), tr("A new book of the series, with the same Workshop."));
    setHeadingIcon(QStringLiteral("project.book"));
    setCompactHeading(true);
    //: The button of the window New Volume. In Polish: &Dodaj tom
    setAcceptText(tr("&Add Volume"));

    auto* columns = new QHBoxLayout();
    auto* left = new QVBoxLayout();
    if (!book.isSeries()) {
        left->addWidget(seriesGroup(m_titles.value(0)));
    }
    left->addWidget(volumeGroup(book));
    left->addStretch(1);
    auto* right = new QVBoxLayout();
    right->addWidget(startGroup());
    //: In Polish: Wspólne dla wszystkich tomów: karty postaci, miejsc i przedmiotów, słownik
    //: książki, mapy myśli, osie czasu, notatki robocze, wycinki i materiały źródłowe. Każdy
    //: tom ma własną okładkę, teksty dla wydawnictwa, przypisy, podpisy i bibliografię.
    right->addWidget(noteLabel(
        tr("Shared by all the volumes: the cards of the characters, locations and items, the "
           "book dictionary, the mind maps, the timelines, the working notes, the clippings and "
           "the sources. Each volume has its own cover, texts for publishers, footnotes, "
           "captions and bibliography."),
        this));
    right->addStretch(1);
    columns->addLayout(left, 1);
    columns->addLayout(right, 1);
    contentLayout()->addLayout(columns);

    connect(m_titleEdit, &QLineEdit::textChanged, this,
            &NewVolumeDialog::updateAcceptButton);
    connect(m_placeBox, &QComboBox::currentIndexChanged, this, &NewVolumeDialog::updatePlace);
    updatePlace();
    updateAcceptButton();
    m_titleEdit->setFocus();
}

QWidget* NewVolumeDialog::seriesGroup(const QString& title) {
    //: The group of the window New Volume in a project with one book. In Polish: Seria
    auto* group = new QGroupBox(tr("Series"), this);
    auto* grid = new QGridLayout(group);
    m_seriesEdit = new QLineEdit(title, group);
    //: In Polish: &Nazwa serii
    grid->addWidget(fieldLabel(tr("&Name of the series"), m_seriesEdit, group), 0, 0);
    grid->addWidget(m_seriesEdit, 0, 1);
    m_seriesNote = noteLabel(QString(), group);  // updatePlace()
    grid->addWidget(m_seriesNote, 1, 0, 1, 2);
    grid->setColumnStretch(1, 1);
    return group;
}

QWidget* NewVolumeDialog::volumeGroup(const core::ProjectManager& book) {
    const core::ProjectBook* current = book.book();
    //: The group of the window New Volume. In Polish: Tom
    auto* group = new QGroupBox(tr("Volume"), this);
    auto* grid = new QGridLayout(group);
    m_titleEdit = new QLineEdit(group);
    m_authorEdit = new QLineEdit(current != nullptr ? current->author : QString(), group);
    m_languageBox = new QComboBox(group);
    for (const auto& [code, name] : languages()) {
        m_languageBox->addItem(name, code);
    }
    const QString language = current != nullptr ? current->language : QString();
    if (!language.isEmpty() && m_languageBox->findData(language) < 0) {
        m_languageBox->addItem(language, language);
    }
    m_languageBox->setCurrentIndex(std::max(0, m_languageBox->findData(language)));

    // The places: before the first volume, or after each of them. Long titles do not widen the
    // window: the list shows them whole when it opens
    m_placeBox = new QComboBox(group);
    m_placeBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_placeBox->setMinimumContentsLength(PLACE_LENGTH);
    const auto count = static_cast<int>(m_titles.size());
    for (int at = 0; at <= count; ++at) {
        if (at == 0) {
            //: A place of a new volume: %1 the title of the first volume. In Polish: przed
            //: tomem 1 „%1” – tom 1
            m_placeBox->addItem(tr("before volume 1 \"%1\" – volume 1").arg(m_titles.value(0)),
                                at);
        } else {
            //: A place of a new volume: %1 the number of the volume before it, %2 its title,
            //: %3 the number of the new one. In Polish: po tomie %1 „%2” – tom %3
            m_placeBox->addItem(tr("after volume %1 \"%2\" – volume %3")
                                    .arg(QString::number(at), m_titles.value(at - 1),
                                         QString::number(at + 1)),
                                at);
        }
    }
    const auto after = static_cast<int>(book.currentBookIndex()) + 1;
    m_placeBox->setCurrentIndex(std::clamp(after, 0, count));

    //: In Polish: &Tytuł
    grid->addWidget(fieldLabel(tr("&Title"), m_titleEdit, group), 0, 0);
    grid->addWidget(m_titleEdit, 0, 1);
    //: In Polish: &Autor
    grid->addWidget(fieldLabel(tr("A&uthor"), m_authorEdit, group), 1, 0);
    grid->addWidget(m_authorEdit, 1, 1);
    //: In Polish: &Język tekstu
    grid->addWidget(fieldLabel(tr("&Language of the text"), m_languageBox, group), 2, 0);
    grid->addWidget(m_languageBox, 2, 1);
    //: The place of the new volume among the volumes. In Polish: &Miejsce
    grid->addWidget(fieldLabel(tr("&Place"), m_placeBox, group), 3, 0);
    grid->addWidget(m_placeBox, 3, 1);
    grid->setColumnStretch(1, 1);
    return group;
}

QWidget* NewVolumeDialog::startGroup() {
    //: The group of the window New Volume: what the volume starts with. In Polish: Początek
    //: tomu
    auto* group = new QGroupBox(tr("Start of the Volume"), this);
    auto* layout = new QVBoxLayout(group);
    //: In Polish: &Puste sekcje
    m_emptyButton = new QRadioButton(tr("&Empty sections"), group);
    layout->addWidget(m_emptyButton);
    //: In Polish: Tylko pierwszy rozdział, jak w nowej książce.
    layout->addWidget(noteLabel(tr("Only the first chapter, as in a new book."), group));
    m_pagesButton = new QRadioButton(group);
    m_pagesButton->setChecked(true);
    layout->addWidget(m_pagesButton);
    m_pagesNote = noteLabel(QString(), group);
    layout->addWidget(m_pagesNote);
    return group;
}

void NewVolumeDialog::updatePlace() {
    const int place = m_placeBox->currentData().toInt();
    if (m_seriesNote != nullptr) {
        // The book is volume 1, or volume 2 after a new volume 1
        //: %1: the title of the book, %2: its number in the series. In Polish: Książka „%1”
        //: stanie się tomem %2 serii. Plik projektu zostaje ten sam, a Nawigator pokaże tomy
        //: i jeden wspólny Warsztat.
        m_seriesNote->setText(tr("The book \"%1\" becomes volume %2 of the series. The file "
                                 "of the project stays the same, and the Navigator shows the "
                                 "volumes and one Workshop.")
                                  .arg(m_titles.value(0), QString::number(place == 0 ? 2 : 1)));
    }

    // The pages come from the volume before the new one, or from the first volume
    const int source = std::max(place - 1, 0) + 1;
    //: %1: the number of the volume the pages come from. In Polish: &Strony z tomu %1, bez
    //: rozdziałów
    m_pagesButton->setText(tr("Pa&ges of volume %1, without chapters").arg(source));
    //: %1: the number of the volume. In Polish: Te same strony sekcji początkowej i końcowej
    //: co w tomie %1: strona tytułowa z tytułem nowego tomu, pozostałe (np. prawa autorskie,
    //: o autorze) z tekstem z tomu %1, a do tego pierwszy rozdział.
    m_pagesNote->setText(
        tr("The same pages of the front and back sections as in volume %1: the title page "
           "with the title of the new volume, the others (such as the copyright page or About "
           "the Author) with the text of volume %1, and the first chapter.")
            .arg(source));
}

void NewVolumeDialog::updateAcceptButton() {
    acceptButton()->setEnabled(!m_titleEdit->text().trimmed().isEmpty());
}

core::NewBook NewVolumeDialog::request() const {
    core::NewBook request;
    request.title = m_titleEdit->text().trimmed();
    request.author = m_authorEdit->text().trimmed();
    request.language = m_languageBox->currentData().toString();
    request.index = m_placeBox->currentData().toInt();
    request.pagesOfPrevious = m_pagesButton->isChecked();
    return request;
}

QString NewVolumeDialog::seriesName() const {
    return m_seriesEdit != nullptr ? m_seriesEdit->text().trimmed() : QString();
}

}  // namespace kalahari::gui::dialogs
