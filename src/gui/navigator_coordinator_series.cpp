// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file navigator_coordinator_series.cpp
/// @brief NavigatorCoordinator: the volumes of a series – a new one, their order, their removal

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/file_trash.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/delete_dialogs.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/new_volume_dialog.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"

#include <QDialog>
#include <QDir>
#include <QHash>
#include <QLocale>
#include <QStatusBar>
#include <QStringList>
#include <QWidget>

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <ranges>

namespace kalahari::gui {

namespace {

/// The time a message about a volume stays in the status bar, in milliseconds
constexpr int MESSAGE_TIME = 2000;

/// The elements of the text of @p book and its files outside the text, at the tops of their
/// lists
QList<const core::ProjectElement*> topElementsOf(const core::ProjectBook& book) {
    QList<const core::ProjectElement*> elements;
    for (const QList<core::ProjectElement>* list :
         {&book.frontElements, &book.mainElements, &book.backElements, &book.files}) {
        for (const core::ProjectElement& element : *list) {
            elements << &element;
        }
    }
    return elements;
}

/// The first text of @p elements, inside parts too; nullptr: none
const core::ProjectElement* firstTextOf(const QList<core::ProjectElement>& elements,
                                        const core::ProjectManager& book) {
    // Depth first, in reading order: the elements still to look at, the next one last
    QList<const core::ProjectElement*> waiting;
    const auto wait = [&waiting](const QList<core::ProjectElement>& list) {
        for (const core::ProjectElement& element : std::views::reverse(list)) {
            waiting << &element;
        }
    };
    wait(elements);
    while (!waiting.isEmpty()) {
        const core::ProjectElement* element = waiting.takeLast();
        if (book.formOf(*element) == core::ElementForm::Text) {
            return element;
        }
        wait(element->elements);
    }
    return nullptr;
}

/// The files of the elements of @p volume, listed before it goes
QStringList filesOfVolume(const core::ProjectManager& pm, const core::ProjectBook& volume) {
    QStringList files;
    for (const core::ProjectElement* element : topElementsOf(volume)) {
        files << pm.filesOf(*element);
    }
    return files;
}

/// The question of Delete Volume...: what goes with volume @p index, @p title, and where its
/// @p files go: to the Trash, or for good where the Trash cannot take them
QString deleteVolumeQuestion(qsizetype index, const QString& title, const QStringList& files) {
    const QString number = QLocale().toString(index + 1);
    //: The question of Delete Volume... in the Navigator: %1 the number of the volume, %2 its
    //: title. In Polish: Usunąć tom %1 „%2” z serii? Warsztat zostaje bez zmian, ze wszystkimi
    //: kartami.
    QString question = NavigatorCoordinator::tr("Delete volume %1 \"%2\" from the series? The "
                                                "Workshop stays as it is, with all its cards.")
                           .arg(number, title);
    if (files.isEmpty()) {
        return question;
    }
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("bin"), dialogs::trashWords()}};
    if (std::all_of(files.cbegin(), files.cend(),
                    [](const QString& file) { return core::canMoveToTrash(file); })) {
        //: After the question of Delete Volume...; %n: the number of the files of the volume.
        //: In Polish: Pliki tomu (%n) trafią do Kosza.
        question += QStringLiteral("\n\n") +
                    core::fillWords(NavigatorCoordinator::tr(
                                        "The files of the volume (%n) go to the {bin}.", nullptr,
                                        static_cast<int>(files.size())),
                                    nouns);
    } else {
        //: After the question of Delete Volume..., when the files are in a folder without the
        //: system's Trash. In Polish: Kosz nie przyjmie plików tomu: program zapyta, czy
        //: usunąć je na zawsze.
        question += QStringLiteral("\n\n") +
                    core::fillWords(NavigatorCoordinator::tr(
                                        "The {bin} cannot take the files of the volume: the "
                                        "program asks whether to delete them for good."),
                                    nouns);
    }
    return question;
}

}  // namespace

void NavigatorCoordinator::onRequestNewVolume() {
    auto& pm = m_book;
    auto* window = qobject_cast<QWidget*>(parent());
    if (!pm.isProjectOpen() || pm.isReadOnly()) {
        core::Logger::getInstance().warn(
            "NavigatorCoordinator: New volume requested without a book that can be changed");
        return;
    }

    dialogs::NewVolumeDialog dialog(pm, window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const core::NewBook request = dialog.request();
    const QString bookId = pm.addBook(request);
    if (bookId.isEmpty()) {
        core::Logger::getInstance().error("NavigatorCoordinator: Failed to add a volume");
        //: In Polish: Nie udało się dodać tomu: nie można zapisać projektu.
        dialogs::MessageDialog::warning(window, tr("New Volume"),
                                        tr("Could not add the volume: the project could not "
                                           "be saved."));
        return;
    }
    // The name the window gave the series stays, also when the new volume goes first; the
    // name of the first volume is the name of a series without its own
    const QString series = dialog.seriesName();
    if (!series.isEmpty() && series != pm.seriesName()) {
        pm.setSeriesName(series);
    }
    pm.setCurrentBook(bookId);
    core::Logger::getInstance().info("NavigatorCoordinator: Added volume: {}",
                                     bookId.toStdString());

    // The new volume is shown, with its first chapter open for writing
    const core::ProjectBook* volume = pm.book();
    const core::ProjectElement* first =
        volume != nullptr ? firstTextOf(volume->mainElements, pm) : nullptr;
    if (first != nullptr) {
        showAdded(first->id, first->title);
    } else {
        refreshNavigator();
        m_navigatorPanel->revealBook(bookId);
    }
    m_propertiesPanel->refresh();
    //: In Polish: Dodano tom do serii: %1
    m_statusBar->showMessage(tr("Volume added to the series: %1").arg(request.title),
                             MESSAGE_TIME);
    emit documentModified();
}

void NavigatorCoordinator::onRequestMoveVolume(const QString& bookId, int step) {
    auto& pm = m_book;
    const core::BookProject* project = pm.project();
    if (project == nullptr || pm.isReadOnly()) {
        return;
    }
    const qsizetype index = project->bookIndex(bookId);
    const qsizetype to = index + step;
    if (index < 0 || to < 0 || to >= project->books.size()) {
        return;
    }
    if (!pm.moveBook(bookId, to)) {
        core::Logger::getInstance().error("NavigatorCoordinator: Failed to move a volume");
        //: In Polish: Nie udało się przenieść tomu: nie można zapisać projektu.
        dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()), tr("Move Volume"),
                                        tr("Could not move the volume: the project could not "
                                           "be saved."));
        return;
    }
    refreshNavigator();
    m_navigatorPanel->revealBook(bookId);
    m_propertiesPanel->refresh();
    emit documentModified();
}

void NavigatorCoordinator::onRequestOpenVolume(const QString& bookId) {
    auto& pm = m_book;
    const core::BookProject* project = pm.project();
    const qsizetype index = project != nullptr ? project->bookIndex(bookId) : -1;
    if (index < 0) {
        return;
    }
    pm.setCurrentBook(bookId);
    const core::ProjectElement* first = firstTextOf(project->books.at(index).mainElements, pm);
    if (first == nullptr) {
        m_navigatorPanel->revealBook(bookId);
        return;
    }
    m_navigatorPanel->revealElement(first->id);
    onElementSelected(first->id, first->title);
}

bool NavigatorCoordinator::saveTextsOfVolume(const core::ProjectBook& book) {
    QStringList unsaved;
    for (const core::ProjectElement* element : topElementsOf(book)) {
        for (const core::ProjectElement* text : saveTextsOf(*element)) {
            unsaved << QStringLiteral("  ") + text->title;
        }
    }
    if (unsaved.isEmpty()) {
        return true;
    }
    //: In Polish: Zmian w tych tekstach tomu nie udało się zapisać w ich plikach:\n%1\n\nMimo
    //: to usunąć tom? Pliki zachowają tekst zapisany wcześniej.
    const QString question = tr("The changes in these texts of the volume could not be saved "
                                "to their files:\n%1\n\nDelete the volume anyway? Their files "
                                "keep the text saved before.")
                                 .arg(unsaved.join(QLatin1Char('\n')));
    //: The title of the questions of Delete Volume... In Polish: Usuń tom
    const QString title = tr("Delete Volume");
    //: In Polish: Usuń &mimo to
    const QString button = tr("Delete &Anyway");
    return dialogs::MessageDialog::confirm(qobject_cast<QWidget*>(parent()), title, question,
                                           button, dialogs::MessageDialog::Kind::Warning,
                                           dialogs::MessageDialog::DefaultButton::Cancel);
}

void NavigatorCoordinator::onRequestDeleteVolume(const QString& bookId) {
    auto& pm = m_book;
    auto* window = qobject_cast<QWidget*>(parent());
    const core::BookProject* project = pm.project();
    if (project == nullptr || pm.isReadOnly()) {
        return;
    }
    const qsizetype index = project->bookIndex(bookId);
    if (index < 0 || project->books.size() < 2) {
        return;
    }
    // A copy: removing it changes the project. Its files, listed before it goes, go after it
    const core::ProjectBook volume = project->books.at(index);
    const QStringList files = filesOfVolume(pm, volume);
    //: The title of the questions of Delete Volume... In Polish: Usuń tom
    const QString title = tr("Delete Volume");
    //: The button of the question of Delete Volume... In Polish: &Usuń tom
    const QString button = tr("&Delete Volume");
    if (!dialogs::MessageDialog::confirm(window, title,
                                         deleteVolumeQuestion(index, volume.title, files),
                                         button, dialogs::MessageDialog::Kind::Warning,
                                         dialogs::MessageDialog::DefaultButton::Cancel) ||
        !saveTextsOfVolume(volume)) {
        return;
    }

    const std::optional<core::ProjectBook> removed = pm.removeBook(bookId);
    if (!removed) {
        core::Logger::getInstance().error("NavigatorCoordinator: Failed to delete a volume");
        //: In Polish: Nie udało się usunąć tomu: nie można zapisać projektu.
        dialogs::MessageDialog::warning(window, title,
                                        tr("Could not delete the volume: the project could not "
                                           "be saved."));
        return;
    }
    const core::ProjectBook& gone = *removed;
    core::Logger::getInstance().info("NavigatorCoordinator: Deleted volume: {}",
                                     bookId.toStdString());
    for (const core::ProjectElement* element : topElementsOf(gone)) {
        closeTabsOf(*element);
    }
    // Its files go to the Trash, or for good when the writer says so; its folder goes when
    // nothing is left in it
    deleteFiles(files, true);
    if (!gone.folder.isEmpty()) {
        QDir(pm.getProjectPath()).rmdir(gone.folder);
    }
    refreshNavigator();
    m_propertiesPanel->refresh();
    //: In Polish: Usunięto tom z serii: %1
    m_statusBar->showMessage(tr("Volume deleted from the series: %1").arg(gone.title),
                             MESSAGE_TIME);
    emit documentModified();
}

}  // namespace kalahari::gui
