// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file file_associations_page.cpp
/// @brief Settings page: Files > File Associations
///
/// Which program the system opens the books (.klh) and the documents (.kchapter) with, and a
/// button that makes it Kalahari, as far as the system lets the program do it
/// (gui/utils/file_associations.h). On Windows a second button opens the Default Apps page of
/// Windows Settings, where only the writer chooses the default program.

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/utils/file_associations.h"

#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>

namespace kalahari::gui {

namespace {

/// What the page says of @p association
QString stateText(const FileAssociation& association)
{
    switch (association.state) {
    case FileAssociation::State::Kalahari:
        //: Which program opens the files of a row. In Polish: Otwiera je Kalahari
        return FileAssociationsPage::tr("Kalahari opens them");
    case FileAssociation::State::OtherProgram:
        //: %1 is the name of another program. In Polish: Otwiera je %1
        return FileAssociationsPage::tr("%1 opens them").arg(association.program);
    case FileAssociation::State::NoProgram:
        //: In Polish: Nie otwiera ich żaden program: system pyta, którego użyć
        return FileAssociationsPage::tr("No program opens them: the system asks which one to use");
    case FileAssociation::State::Unknown:
        break;
    }
    return association.program.isEmpty()
               //: In Polish: System tego nie podaje
               ? FileAssociationsPage::tr("The system does not tell")
               //: %1 is why. In Polish: System tego nie podaje: %1
               : FileAssociationsPage::tr("The system does not tell: %1").arg(association.program);
}

}  // namespace

FileAssociationsPage::FileAssociationsPage(QWidget* parent) : SettingsPage(parent)
{
    //: The group of Settings > Files > File Associations. In Polish: Pliki programu Kalahari
    QFormLayout* files = addGroup(tr("Files of Kalahari"));
    //: In Polish: System otwiera te pliki w programie, który im przypisano. Instalator
    //: przypisuje im program Kalahari; tutaj widać, czy tak jest nadal.
    addNote(files, tr("The system opens these files with the program it gives them. The "
                      "installer gives them Kalahari; here you can see whether it still does."));
    const QStringList labels{
        //: In Polish: Książki (.klh):
        tr("Books (.klh):"),
        //: In Polish: Dokumenty (.kchapter):
        tr("Documents (.kchapter):")};
    const auto& types = kalahariFileTypes();
    for (std::size_t index = 0; index < types.size(); ++index) {
        auto* state = new QLabel();
        state->setObjectName(QStringLiteral("fileAssociation_") + types[index].extension);
        state->setWordWrap(true);
        files->addRow(labels.value(static_cast<qsizetype>(index)), state);
        m_states.push_back(state);
    }

    auto* buttons = new QHBoxLayout();
    //: In Polish: &Powiąż z programem Kalahari
    m_associate = new QPushButton(tr("&Associate with Kalahari"));
    m_associate->setObjectName(QStringLiteral("associateFiles"));
    connect(m_associate, &QPushButton::clicked, this, &FileAssociationsPage::associate);
    buttons->addWidget(m_associate);
    if (fileAssociationSystem().hasSettingsWindow()) {
        //: Opens Settings > Apps > Default apps of Windows. In Polish: &Aplikacje domyślne w
        //: Ustawieniach systemu Windows…
        m_systemSettings = new QPushButton(tr("&Default Apps in Windows Settings…"));
        m_systemSettings->setObjectName(QStringLiteral("fileAssociationSettings"));
        connect(m_systemSettings, &QPushButton::clicked, this, [this]() {
            if (!fileAssociationSystem().openSettingsWindow()) {
                //: In Polish: Ustawienia systemu Windows się nie otworzyły.
                m_result->setText(tr("Windows Settings did not open."));
                m_result->show();
            }
        });
        buttons->addWidget(m_systemSettings);
    }
    buttons->addStretch();
    files->addRow(buttons);

    m_result = new QLabel();
    m_result->setObjectName(QStringLiteral("fileAssociationResult"));
    m_result->setWordWrap(true);
    m_result->hide();
    files->addRow(m_result);

#if defined(Q_OS_WIN)
    //: In Polish: W systemie Windows domyślny program danego rodzaju plików możesz wybrać tylko
    //: Ty. Przycisk rejestruje Kalahari jako program, który je otwiera; jeśli nadal otwiera je
    //: inny program, wybierz Kalahari na stronie Aplikacje domyślne w Ustawieniach systemu
    //: Windows.
    addNote(files, tr("Windows lets only you choose the default program of a kind of files. The "
                      "button registers Kalahari as a program that opens them; when another "
                      "program still opens them, choose Kalahari on the Default Apps page of "
                      "Windows Settings."));
#elif defined(Q_OS_MACOS)
    //: In Polish: System macOS może poprosić o potwierdzenie zmiany.
    addNote(files, tr("macOS may ask you to confirm the change."));
#else
    //: In Polish: Przycisk ustawia Kalahari jako domyślny program tych plików za pomocą
    //: xdg-utils; program w pliku AppImage najpierw dodaje dla Ciebie rodzaje swoich plików
    //: i swój skrót w menu.
    addNote(files, tr("The button makes Kalahari the default program of these files with "
                      "xdg-utils; the AppImage first adds the kinds of its files and its menu "
                      "entry for you."));
#endif
}

bool FileAssociationsPage::refresh()
{
    bool kalahari = true;
    const auto& types = kalahariFileTypes();
    for (std::size_t index = 0; index < types.size() && index < m_states.size(); ++index) {
        const FileAssociation association = fileAssociationSystem().current(types[index]);
        m_states[index]->setText(stateText(association));
        kalahari = kalahari && association.state == FileAssociation::State::Kalahari;
    }
    return kalahari;
}

void FileAssociationsPage::showEvent(QShowEvent* event)
{
    SettingsPage::showEvent(event);
    refresh();
}

void FileAssociationsPage::changeEvent(QEvent* event)
{
    SettingsPage::changeEvent(event);
    // The writer comes back from the settings of the system
    if (event->type() == QEvent::ActivationChange && isVisible() && isActiveWindow()) {
        refresh();
    }
}

void FileAssociationsPage::associate()
{
    m_associate->setEnabled(false);
    const QPointer<FileAssociationsPage> page(this);
    fileAssociationSystem().associate([page](const QString& problem) {
        if (!page) {
            return;
        }
        page->m_associate->setEnabled(true);
        const bool kalahari = page->refresh();
        if (!problem.isEmpty()) {
            //: %1 is what went wrong. In Polish: Nie udało się powiązać plików z programem
            //: Kalahari: %1
            page->m_result->setText(
                tr("Kalahari could not become the program of its files: %1").arg(problem));
        } else if (kalahari) {
            //: In Polish: Kalahari otwiera teraz swoje książki i dokumenty.
            page->m_result->setText(tr("Kalahari opens its books and documents now."));
        } else if (fileAssociationSystem().hasSettingsWindow()) {
            //: In Polish: Program Kalahari jest zarejestrowany, ale Windows nadal otwiera część
            //: tych plików innym programem: wybierz Kalahari na stronie Aplikacje domyślne w
            //: Ustawieniach systemu Windows.
            page->m_result->setText(tr("Kalahari is registered, yet Windows still opens some of "
                                       "the files with another program: choose Kalahari on the "
                                       "Default Apps page of Windows Settings."));
        } else {
            //: In Polish: System nadal otwiera część tych plików innym programem.
            page->m_result->setText(
                tr("The system still opens some of the files with another program."));
        }
        page->m_result->show();
    });
}

}  // namespace kalahari::gui
