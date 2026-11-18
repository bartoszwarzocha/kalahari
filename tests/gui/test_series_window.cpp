// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_series_window.cpp
/// @brief A series in the window: the volumes in the Navigator and the current one, the window
/// New Volume, the commands of the volumes and the page of the series in the Properties panel

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/file_trash.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/gui/add_target.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/dialogs/move_element_dialog.h>
#include <kalahari/gui/dialogs/new_file_dialog.h>
#include <kalahari/gui/dialogs/new_volume_dialog.h>
#include <kalahari/gui/icon_registrar.h>
#include <kalahari/gui/navigator_coordinator.h>
#include <kalahari/gui/panels/editor_panel.h>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/gui/panels/properties_panel.h>
#include <kalahari/gui/panels/series_page.h>
#include <kalahari/gui/widgets/element_place_picker.h>

#include "kalahari_tests/gui/dialog_checks.h"
#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/polish.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStringList>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>

#include <functional>
#include <optional>

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using gui::dialogs::MessageDialog;
using gui::dialogs::NewVolumeDialog;

namespace {

/// The Trash of a test in place of the system's until the test ends: it takes the files and
/// keeps their paths
struct TestTrash {
    QStringList taken;  ///< The files it took

    TestTrash() {
        core::setTrashForTests([](const QString&) { return true; },
                               [this](const QString& path) {
                                   if (QFileInfo(path).isDir()) {
                                       return QDir(path).removeRecursively();
                                   }
                                   taken << path;
                                   return QFile::remove(path);
                               });
    }
    ~TestTrash() { core::setTrashForTests({}, {}); }
    TestTrash(const TestTrash&) = delete;
    TestTrash& operator=(const TestTrash&) = delete;
    TestTrash(TestTrash&&) = delete;
    TestTrash& operator=(TestTrash&&) = delete;
};

/// A new volume @p title of the open book, after the current one
QString addVolume(core::ProjectManager& pm, const QString& title) {
    core::NewBook request;
    request.title = title;
    const QString bookId = pm.addBook(request);
    REQUIRE_FALSE(bookId.isEmpty());
    return bookId;
}

/// The texts of the nodes under the root of the Navigator, in their order
QStringList rootNodes(const gui::NavigatorPanel& panel) {
    QStringList texts;
    const QTreeWidgetItem* root = panel.tree()->topLevelItem(0);
    REQUIRE(root != nullptr);
    for (int i = 0; i < root->childCount(); ++i) {
        texts << root->child(i)->text(0);
    }
    return texts;
}

/// Run @p command, filling the window New Volume it shows with @p fill and pressing Add Volume
/// @return The number of the windows New Volume shown
int addingVolume(const std::function<void()>& command,
                 const std::function<void(NewVolumeDialog&)>& fill) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &fill]() {
        auto* dialog = qobject_cast<NewVolumeDialog*>(test::shownDialog());
        if (dialog == nullptr || dialog->property("handled").toBool()) {
            return;
        }
        dialog->setProperty("handled", true);
        ++shown;
        fill(*dialog);
        QMetaObject::invokeMethod(dialog->acceptButton(), "click", Qt::QueuedConnection);
    });
    timer.start();
    command();
    return shown;
}

/// The titles of the elements of the main section among @p sections
QStringList mainTitles(const QList<gui::PickerSection>& sections) {
    QStringList titles;
    for (const gui::PickerSection& section : sections) {
        if (section.place == core::BookPlace::Main) {
            for (const core::ProjectElement& element : section.elements) {
                titles << element.title;
            }
        }
    }
    return titles;
}

/// Press @p key with @p modifiers on @p widget
void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent down(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(widget, &down);
}

/// The texts of the column @p column of @p list, in their order
QStringList column(const QTreeWidget& list, int column) {
    QStringList texts;
    for (int i = 0; i < list.topLevelItemCount(); ++i) {
        texts << list.topLevelItem(i)->text(column);
    }
    return texts;
}

}  // namespace

TEST_CASE("Series: the window New Volume of a book names the series and starts the volume",
          "[gui][series][dialogs]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    novel.pm.book()->language = QStringLiteral("pl");

    NewVolumeDialog dialog(novel.pm);
    // A book asks for the name of the series, at first its title; the book is volume 1
    REQUIRE(dialog.seriesEdit() != nullptr);
    CHECK(dialog.seriesEdit()->text() == QStringLiteral("Sand"));
    CHECK(dialog.authorEdit()->text() == QStringLiteral("Author"));
    CHECK(dialog.languageBox()->currentData().toString() == QStringLiteral("pl"));
    REQUIRE(dialog.placeBox()->count() == 2);
    CHECK(dialog.placeBox()->currentData().toInt() == 1);
    CHECK(dialog.placeBox()->itemText(1) == QStringLiteral("after volume 1 \"Sand\" – volume 2"));
    CHECK(dialog.pagesButton()->isChecked());
    CHECK(dialog.pagesButton()->text() == QStringLiteral("Pa&ges of volume 1, without chapters"));
    const auto notes = [&dialog]() {
        QStringList texts;
        for (const QLabel* label : dialog.findChildren<QLabel*>()) {
            texts << label->text();
        }
        return texts.join(QLatin1Char('\n'));
    };
    CHECK(notes().contains(QStringLiteral("The book \"Sand\" becomes volume 1 of the series.")));
    // Before the book, the book is volume 2
    dialog.placeBox()->setCurrentIndex(0);
    CHECK(notes().contains(QStringLiteral("The book \"Sand\" becomes volume 2 of the series.")));

    // Only with a title
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    dialog.titleEdit()->setText(QStringLiteral("  Dunes "));
    CHECK(dialog.acceptButton()->isEnabled());
    dialog.seriesEdit()->setText(QStringLiteral(" Sands "));
    dialog.emptyButton()->setChecked(true);
    const core::NewBook request = dialog.request();
    CHECK(request.title == QStringLiteral("Dunes"));
    CHECK(request.author == QStringLiteral("Author"));
    CHECK(request.language == QStringLiteral("pl"));
    CHECK(request.index == 0);
    CHECK_FALSE(request.pagesOfPrevious);
    CHECK(dialog.seriesName() == QStringLiteral("Sands"));
}

TEST_CASE("Series: the window New Volume of a series places the volume after the current one",
          "[gui][series][dialogs]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    // Each after the current one, the first: Sand, Oasis, Dunes
    addVolume(novel.pm, QStringLiteral("Dunes"));
    addVolume(novel.pm, QStringLiteral("Oasis"));
    REQUIRE(novel.pm.setCurrentBook(novel.pm.project()->books.at(1).id));

    const NewVolumeDialog dialog(novel.pm);
    CHECK(dialog.seriesEdit() == nullptr);
    CHECK(dialog.seriesName().isEmpty());
    REQUIRE(dialog.placeBox()->count() == 4);
    CHECK(dialog.placeBox()->currentIndex() == 2);
    CHECK(dialog.placeBox()->currentText() ==
          QStringLiteral("after volume 2 \"Oasis\" – volume 3"));
    CHECK(dialog.pagesButton()->text() == QStringLiteral("Pa&ges of volume 2, without chapters"));
    dialog.placeBox()->setCurrentIndex(0);
    CHECK(dialog.placeBox()->currentText() ==
          QStringLiteral("before volume 1 \"Sand\" – volume 1"));
    CHECK(dialog.pagesButton()->text() == QStringLiteral("Pa&ges of volume 1, without chapters"));
}

TEST_CASE("Series: the window New Volume fits a small screen, in English and in Polish",
          "[gui][series][dialogs]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("A rather long title of the first book"));
    const auto check = [&novel](const char* language) {
        INFO("In " << language);
        NewVolumeDialog dialog(novel.pm);
        test::layOut(dialog);
        test::checkSmallScreen(dialog);
        test::checkKeys(dialog);
        dialog.close();
    };
    check("English");
    const test::Polish polish;
    REQUIRE(polish.loaded());
    check("Polish");
}

TEST_CASE("Series: the Navigator shows the volumes under the name of the series, the current "
          "one in bold",
          "[gui][series][navigator]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    test::Window window;
    const gui::NavigatorPanel& panel = *window.navigatorPanel;
    // A book: its node, as before
    CHECK(panel.tree()->headerItem()->text(0) == QStringLiteral("Book Structure"));
    CHECK(panel.tree()->topLevelItem(0)->text(0) == QStringLiteral("Sand"));
    CHECK(rootNodes(panel).first() == QStringLiteral("Book"));

    const QString dunes = addVolume(novel.pm, QStringLiteral("Dunes"));
    const QString sand = novel.pm.project()->books.constFirst().id;
    window.navigator.refreshNavigator();
    CHECK(panel.tree()->headerItem()->text(0) == QStringLiteral("Series Structure"));
    CHECK(panel.tree()->topLevelItem(0)->text(0) == QStringLiteral("Sand"));
    REQUIRE(rootNodes(panel).mid(0, 2) ==
            QStringList{QStringLiteral("Volume 1: Sand"), QStringLiteral("Volume 2: Dunes")});
    CHECK(panel.bookItem(sand)->font(0).bold());
    CHECK_FALSE(panel.bookItem(dunes)->font(0).bold());

    // A node of a volume makes it the current one
    panel.tree()->setCurrentItem(panel.bookItem(dunes)->child(1));
    CHECK(novel.pm.book()->id == dunes);
    CHECK(panel.bookItem(dunes)->font(0).bold());
    CHECK_FALSE(panel.bookItem(sand)->font(0).bold());
    CHECK(gui::NavigatorPanel::bookIdOf(panel.bookItem(dunes)->child(1)) == dunes);

    // The name of the series
    REQUIRE(novel.pm.setSeriesName(QStringLiteral("Sands")));
    window.navigator.refreshNavigator();
    CHECK(panel.tree()->topLevelItem(0)->text(0) == QStringLiteral("Sands"));
    // The current volume stays bold in the tree loaded again
    CHECK(panel.bookItem(dunes)->font(0).bold());
}

TEST_CASE("Series: New Volume adds the volume with its first chapter open and names the series",
          "[gui][series][navigator]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    test::Window window;

    const int shown = addingVolume([&window]() { window.navigator.onRequestNewVolume(); },
                                   [](NewVolumeDialog& dialog) {
                                       dialog.seriesEdit()->setText(QStringLiteral("Sands"));
                                       dialog.titleEdit()->setText(QStringLiteral("Dunes"));
                                   });
    CHECK(shown == 1);
    const core::BookProject* project = novel.pm.project();
    REQUIRE(project->books.size() == 2);
    const core::ProjectBook& dunes = project->books.at(1);
    CHECK(dunes.title == QStringLiteral("Dunes"));
    CHECK(novel.pm.seriesName() == QStringLiteral("Sands"));
    CHECK(novel.pm.book()->id == dunes.id);
    // Its first chapter is open, and the Navigator shows the volumes
    REQUIRE(window.tabs->count() == 1);
    REQUIRE_FALSE(dunes.mainElements.isEmpty());
    CHECK(window.editor(0)->property("elementId").toString() ==
          dunes.mainElements.constFirst().id);
    CHECK(rootNodes(*window.navigatorPanel).mid(0, 2) ==
          QStringList{QStringLiteral("Volume 1: Sand"), QStringLiteral("Volume 2: Dunes")});

    // The name of the series is left as the title of the first volume
    const int again = addingVolume([&window]() { window.navigator.onRequestNewVolume(); },
                                   [](NewVolumeDialog& dialog) {
                                       dialog.titleEdit()->setText(QStringLiteral("Oasis"));
                                   });
    CHECK(again == 1);
    CHECK(novel.pm.project()->books.size() == 3);
    CHECK(novel.pm.seriesName() == QStringLiteral("Sands"));
}

TEST_CASE("Series: a volume goes earlier and later, and is deleted with its files in the Trash",
          "[gui][series][navigator]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    const QString dunes = addVolume(novel.pm, QStringLiteral("Dunes"));
    test::Window window;
    window.navigator.refreshNavigator();

    window.navigator.onRequestMoveVolume(dunes, -1);
    CHECK(novel.pm.project()->books.constFirst().id == dunes);
    CHECK(rootNodes(*window.navigatorPanel).first() == QStringLiteral("Volume 1: Dunes"));
    window.navigator.onRequestMoveVolume(dunes, -1);  // the first one stays first
    CHECK(novel.pm.project()->books.constFirst().id == dunes);
    window.navigator.onRequestMoveVolume(dunes, 1);
    CHECK(novel.pm.project()->books.at(1).id == dunes);

    // Its first chapter is open; the question, then its tab closes and its files go to the
    // Trash, with its folder
    const core::ProjectBook& volume = novel.pm.project()->books.at(1);
    REQUIRE_FALSE(volume.mainElements.isEmpty());
    const QString chapterId = volume.mainElements.constFirst().id;
    const QString file = novel.pm.filePathOf(volume.mainElements.constFirst());
    const QString folder = QDir(novel.pm.getProjectPath()).filePath(volume.folder);
    REQUIRE(QFileInfo::exists(file));
    window.navigator.onElementSelected(chapterId, QStringLiteral("Chapter 1"));
    REQUIRE(window.tabs->count() == 1);

    const TestTrash trash;
    QStringList said;
    // Cancel keeps the volume
    CHECK(test::answering([&window, &dunes]() { window.navigator.onRequestDeleteVolume(dunes); },
                          MessageDialog::Answer::Cancel, said) == 1);
    CHECK(novel.pm.project()->books.size() == 2);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(
        QStringLiteral("Delete volume 2 \"Dunes\" from the series? The Workshop stays as it is")));
    said.clear();
    CHECK(test::answering([&window, &dunes]() { window.navigator.onRequestDeleteVolume(dunes); },
                          MessageDialog::Answer::Accept, said) == 1);
    REQUIRE(novel.pm.project()->books.size() == 1);
    CHECK(novel.pm.book()->title == QStringLiteral("Sand"));
    CHECK(window.tabs->count() == 0);
    CHECK(trash.taken.contains(file));
    CHECK_FALSE(QFileInfo::exists(file));
    CHECK_FALSE(QFileInfo::exists(folder));
    // A book again
    CHECK(rootNodes(*window.navigatorPanel).first() == QStringLiteral("Book"));
    // The last book is never deleted
    window.navigator.onRequestDeleteVolume(novel.pm.book()->id);
    CHECK(novel.pm.project()->books.size() == 1);
}

TEST_CASE("Series: the page of the series in the Properties panel – its name and its volumes",
          "[gui][series][properties]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    const QString dunes = addVolume(novel.pm, QStringLiteral("Dunes"));
    test::Window window;
    window.navigator.refreshNavigator();
    const gui::PropertiesPanel& properties = *window.properties;
    const gui::SeriesPage& page = *properties.seriesPage();
    // As the panels of the main window are connected (DockCoordinator, MainWindow)
    QObject::connect(window.navigatorPanel, &gui::NavigatorPanel::requestSeriesProperties,
                     &properties, &gui::PropertiesPanel::showSeriesProperties);
    QObject::connect(&properties, &gui::PropertiesPanel::bookChanged, &window.navigator,
                     &gui::NavigatorCoordinator::refreshNavigator);
    QObject::connect(&properties, &gui::PropertiesPanel::requestOpenVolume, &window.navigator,
                     &gui::NavigatorCoordinator::onRequestOpenVolume);

    // The node of the series shows the page
    window.navigatorPanel->tree()->setCurrentItem(window.navigatorPanel->tree()->topLevelItem(0));
    REQUIRE(properties.currentPage() == gui::PropertiesPanel::Page::Series);
    CHECK(page.nameEdit()->text().isEmpty());
    CHECK(page.nameEdit()->placeholderText() == QStringLiteral("Sand"));
    CHECK(column(*page.volumeList(), 0) == QStringList{QStringLiteral("1"), QStringLiteral("2")});
    CHECK(column(*page.volumeList(), 1) ==
          QStringList{QStringLiteral("Sand"), QStringLiteral("Dunes")});
    CHECK(page.volumeList()->headerItem()->text(3) == QStringLiteral("Chapters"));
    CHECK(page.summaryLabel()->text().startsWith(QStringLiteral("In all: ")));
    CHECK(page.summaryLabel()->text().contains(QStringLiteral("words in 2 volumes")));

    // The current volume is chosen: Earlier, not Later
    CHECK(page.chosenVolume() == novel.pm.project()->books.constFirst().id);
    CHECK_FALSE(page.earlierButton()->isEnabled());
    CHECK(page.laterButton()->isEnabled());
    CHECK(page.openButton()->isEnabled());

    // Alt+Down in the table moves the volume chosen, saved with the book
    press(page.volumeList(), Qt::Key_Down, Qt::AltModifier);
    CHECK(novel.pm.project()->books.at(1).title == QStringLiteral("Sand"));
    CHECK(column(*page.volumeList(), 1) ==
          QStringList{QStringLiteral("Dunes"), QStringLiteral("Sand")});
    CHECK(page.volumeList()->currentItem()->text(1) == QStringLiteral("Sand"));
    CHECK(rootNodes(*window.navigatorPanel).first() == QStringLiteral("Volume 1: Dunes"));
    page.earlierButton()->click();
    CHECK(novel.pm.project()->books.constFirst().title == QStringLiteral("Sand"));

    // The name, saved when the field is left
    page.nameEdit()->setText(QStringLiteral(" Sands "));
    emit page.nameEdit()->editingFinished();
    CHECK(novel.pm.project()->series == QStringLiteral("Sands"));
    CHECK(window.navigatorPanel->tree()->topLevelItem(0)->text(0) == QStringLiteral("Sands"));

    // Open: the volume becomes the current one, with its first chapter open
    const QTreeWidgetItem* second = page.volumeList()->topLevelItem(1);
    page.volumeList()->setCurrentItem(page.volumeList()->topLevelItem(1));
    REQUIRE(second->text(1) == QStringLiteral("Dunes"));
    press(page.volumeList(), Qt::Key_Return);
    CHECK(novel.pm.book()->id == dunes);
    REQUIRE(window.tabs->count() == 1);
    CHECK(window.editor(0)->property("elementId").toString() ==
          novel.pm.book()->mainElements.constFirst().id);
}

TEST_CASE("Series: the page of a volume says its number", "[gui][series][properties]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    const test::Window window;
    gui::PropertiesPanel& properties = *window.properties;
    properties.showProjectProperties();
    auto* volume = properties.findChild<QLabel*>(QStringLiteral("projectVolumeLabel"));
    REQUIRE(volume != nullptr);
    CHECK_FALSE(volume->isVisibleTo(&properties));

    const QString dunes = addVolume(novel.pm, QStringLiteral("Dunes"));
    REQUIRE(novel.pm.setCurrentBook(dunes));
    properties.showProjectProperties();
    CHECK(volume->isVisibleTo(&properties));
    CHECK(volume->text() == QStringLiteral("2 of 2"));
    CHECK(properties.findChild<QLineEdit*>(QStringLiteral("projectTitleEdit"))->text() ==
          QStringLiteral("Dunes"));
}

TEST_CASE("Series: Add Element, Move and the drops of the Navigator work in the volume of the "
          "element",
          "[gui][series][navigator]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    const QString sand = novel.pm.book()->id;
    const QStringList sandTitles = [&novel]() {
        QStringList titles;
        for (const core::ProjectElement& element : novel.pm.book()->mainElements) {
            titles << element.title;
        }
        return titles;
    }();
    REQUIRE(sandTitles.contains(QStringLiteral("The Storm")));
    const QString dunes = addVolume(novel.pm, QStringLiteral("Dunes"));
    REQUIRE(novel.pm.setCurrentBook(dunes));
    const core::ProjectBook& volume = novel.pm.project()->books.at(1);
    REQUIRE(volume.id == dunes);
    REQUIRE(volume.mainElements.size() == 1);
    const QString dunesChapter = volume.mainElements.constFirst().title;

    // Add Element: the places of the current volume
    CHECK(mainTitles(gui::dialogs::NewFileDialog::openBook(novel.pm).sections) ==
          QStringList{dunesChapter});
    const gui::AddTarget whole;
    CHECK(mainTitles(gui::dialogs::NewFileDialog::forAdding(novel.pm, whole,
                                                            gui::AddChooser::Book)
                         .sections) == QStringList{dunesChapter});

    // Move: the places of the volume of the element, also when another one is current
    const gui::dialogs::MoveElementDialog::Book move =
        gui::dialogs::MoveElementDialog::openBook(novel.pm, novel.chapterId);
    QStringList without = sandTitles;
    without.removeOne(QStringLiteral("The Storm"));
    CHECK(mainTitles(move.sections) == without);
    REQUIRE(novel.pm.setCurrentBook(sand));
    const gui::dialogs::MoveElementDialog::Book moveDunes =
        gui::dialogs::MoveElementDialog::openBook(
            novel.pm, novel.pm.project()->books.at(1).mainElements.constFirst().id);
    CHECK(mainTitles(moveDunes.sections).isEmpty());
    REQUIRE(novel.pm.setCurrentBook(dunes));

    // A drop: in the volume of the element, never in another one
    const test::Window window;
    const gui::NavigatorPanel& panel = *window.navigatorPanel;
    const QTreeWidgetItem* storm = nullptr;
    for (QTreeWidgetItemIterator it(panel.tree()); *it != nullptr; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == novel.chapterId) {
            storm = *it;
        }
    }
    REQUIRE(storm != nullptr);
    const QTreeWidgetItem* sandMain = panel.bookItem(sand)->child(1);
    const QTreeWidgetItem* dunesMain = panel.bookItem(dunes)->child(1);
    REQUIRE(sandMain->text(0) == dunesMain->text(0));
    const core::BookProject& project = *novel.pm.project();
    using Spot = gui::NavigatorPanel::DropSpot;
    CHECK_FALSE(gui::NavigatorPanel::dropPlaceOf(project, novel.pm.bookTypes(), storm, dunesMain,
                                                 Spot::On));
    CHECK_FALSE(gui::NavigatorPanel::dropPlaceOf(project, novel.pm.bookTypes(), storm,
                                                 dunesMain->child(0), Spot::Above));
    // Above the first chapter of its own volume
    REQUIRE(sandTitles.indexOf(QStringLiteral("The Storm")) > 0);
    const std::optional<core::ElementPlace> first = gui::NavigatorPanel::dropPlaceOf(
        project, novel.pm.bookTypes(), storm, sandMain->child(0), Spot::Above);
    REQUIRE(first.has_value());
    CHECK(first->place == core::BookPlace::Main);
    CHECK(first->index == 0);
}
