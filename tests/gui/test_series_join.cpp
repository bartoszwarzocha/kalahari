// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_series_join.cpp
/// @brief A series in the window: a volume taken out as a book of its own (Take Out as a
/// Book... of a volume, the window Take Out Volume)

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/file_trash.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/gui/dialogs/take_out_volume_dialog.h>
#include <kalahari/gui/icon_registrar.h>
#include <kalahari/gui/navigator_coordinator.h>
#include <kalahari/gui/panels/editor_panel.h>
#include <kalahari/gui/panels/navigator_panel.h>

#include "kalahari_tests/gui/context_menu_reader.h"
#include "kalahari_tests/gui/dialog_checks.h"
#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/polish.h"
#include "kalahari_tests/test_support/window_parts.h"
#include "test_support/dialog_checks.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPushButton>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <functional>
#include <optional>

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using gui::dialogs::TakeOutVolumeDialog;

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

/// The item of the Navigator with the text @p text; nullptr: none
QTreeWidgetItem* itemWithText(const gui::NavigatorPanel& panel, const QString& text) {
    for (QTreeWidgetItemIterator it(panel.tree()); *it != nullptr; ++it) {
        if ((*it)->text(0) == text) {
            return *it;
        }
    }
    return nullptr;
}

/// Run @p command, handing the window Take Out Volume it shows to @p fill, then taking the
/// volume out, with its result in @p result, and closing the window
/// @return The number of the windows Take Out Volume shown
int takingOut(const std::function<void()>& command,
              const std::function<void(TakeOutVolumeDialog&)>& fill, QString* result = nullptr) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &fill, result]() {
        auto* dialog = qobject_cast<TakeOutVolumeDialog*>(test::shownDialog());
        if (dialog == nullptr || dialog->property("handled").toBool()) {
            return;
        }
        dialog->setProperty("handled", true);
        ++shown;
        fill(*dialog);
        // The work, then Close
        dialog->accept();
        if (result != nullptr) {
            *result = dialog->resultText();
        }
        if (dialog->isDone()) {
            QMetaObject::invokeMethod(dialog->acceptButton(), "click", Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(dialog, "reject", Qt::QueuedConnection);
        }
    });
    timer.start();
    command();
    return shown;
}

}  // namespace

TEST_CASE("Series: the window Take Out Volume says where the book goes", "[gui][series][join]") {
    gui::registerAllIcons();
    const QTemporaryDir out;
    REQUIRE(out.isValid());
    QStringList taken;
    TakeOutVolumeDialog dialog(
        {2, QStringLiteral("Dunes"), true}, out.path(),
        [&taken](const QString& folder, bool deleteVolume) {
            taken << folder << (deleteVolume ? QStringLiteral("delete") : QStringLiteral("keep"));
            return TakeOutVolumeDialog::Outcome{};
        });
    dialog.show();

    // A folder named after the title, in the folder chosen
    CHECK(dialog.bookFolder() == QDir(out.path()).filePath(QStringLiteral("Dunes")));
    CHECK(dialog.folderNote() == QStringLiteral("The book goes to the folder \"Dunes\"."));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.deleteBox()->isEnabled());
    CHECK_FALSE(dialog.deleteBox()->isChecked());
    CHECK(dialog.openBox()->isChecked());

    // A failure stays in the window, with Copy, and the fields can be changed
    dialog.deleteBox()->setChecked(true);
    dialog.accept();
    CHECK(taken == QStringList({out.path(), QStringLiteral("delete")}));
    CHECK_FALSE(dialog.isDone());
    CHECK(dialog.resultText() == QStringLiteral("The volume could not be taken out."));
    CHECK(dialog.manifest().isEmpty());
    CHECK(dialog.deleteBox()->isEnabled());

    // A folder of that name that is not empty takes no book
    REQUIRE(QDir(out.path()).mkpath(QStringLiteral("Dunes/book")));
    dialog.setFolder(out.path());
    CHECK(dialog.folderNote() ==
          QStringLiteral("The folder \"Dunes\" is already here and is not empty. Choose another "
                         "place for the book."));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    dialog.close();

    // A book open read-only keeps its volume
    const TakeOutVolumeDialog readOnly({1, QStringLiteral("Sand"), false}, out.path(),
                                       [](const QString&, bool) {
                                           return TakeOutVolumeDialog::Outcome{};
                                       });
    CHECK_FALSE(readOnly.deleteBox()->isEnabled());
    CHECK(readOnly.acceptButton()->isEnabled());
}

TEST_CASE("Series: the window Take Out Volume fits a small screen, in English and in Polish",
          "[gui][series][join]") {
    gui::registerAllIcons();
    const QTemporaryDir out;
    REQUIRE(out.isValid());
    const auto check = [&out](const char* language) {
        INFO("In " << language);
        TakeOutVolumeDialog dialog({2, QStringLiteral("A rather long title of the second volume"),
                                    true},
                                   out.path(), [](const QString&, bool) {
                                       return TakeOutVolumeDialog::Outcome{};
                                   });
        test::layOut(dialog);
        test::checkSmallScreen(dialog);
        test::checkKeys(dialog);
        dialog.close();
    };
    check("English");
    {
        // The window keeps its lines in a font of higher lines, as Windows has
        const test::AppFontOfPixels font(13);
        check("English, in a font of 13 pixels");
    }
    const test::Polish polish;
    REQUIRE(polish.loaded());
    check("Polish");
    const test::AppFontOfPixels font(13);
    check("Polish, in a font of 13 pixels");
}

TEST_CASE("Series: Take Out as a Book... writes the book with the texts as the writer sees "
          "them, deletes the volume when asked and opens the book in a new window",
          "[gui][series][join]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    const QString dunes = addVolume(novel.pm, QStringLiteral("Dunes"));
    test::Window window;
    window.navigator.refreshNavigator();

    // The command of the volume
    {
        QTreeWidgetItem* item =
            itemWithText(*window.navigatorPanel, QStringLiteral("Volume 2: Dunes"));
        REQUIRE(item != nullptr);
        QStringList asked;
        const QMetaObject::Connection connection =
            QObject::connect(window.navigatorPanel, &gui::NavigatorPanel::requestTakeOutVolume,
                             [&asked](const QString& bookId) { asked << bookId; });
        test::ContextMenuReader reader(window.navigatorPanel);
        reader.chooseItem(QStringLiteral("Take &Out as a Book..."));
        window.navigatorPanel->tree()->scrollToItem(item);
        emit window.navigatorPanel->tree()->customContextMenuRequested(
            window.navigatorPanel->tree()->visualItemRect(item).center());
        QObject::disconnect(connection);
        REQUIRE(reader.menusRead() == 1);
        CHECK(asked == QStringList{dunes});
    }

    // The first chapter of the volume open, with a change not saved yet
    const core::ProjectBook& volume = novel.pm.project()->books.at(1);
    REQUIRE_FALSE(volume.mainElements.isEmpty());
    const QString chapterId = volume.mainElements.constFirst().id;
    const QString file = novel.pm.filePathOf(volume.mainElements.constFirst());
    window.navigator.onElementSelected(chapterId, QStringLiteral("Chapter 1"));
    REQUIRE(window.tabs->count() == 1);
    auto* editor = qobject_cast<gui::EditorPanel*>(window.tabs->widget(0));
    REQUIRE(editor != nullptr);
    test::write(*editor, QStringLiteral("Wind over the dunes."));

    const QTemporaryDir out;
    REQUIRE(out.isValid());
    const TestTrash trash;
    QStringList opening;
    QObject::connect(&window.navigator, &gui::NavigatorCoordinator::requestOpenBookInNewWindow,
                     [&opening](const QString& manifest) { opening << manifest; });
    QString result;
    CHECK(takingOut([&window, &dunes]() { window.navigator.onRequestTakeOutVolume(dunes); },
                    [&out](TakeOutVolumeDialog& dialog) {
                        // At first beside the folder of the series
                        CHECK(QDir(dialog.folder()) ==
                              QDir(QFileInfo(test::book().getProjectPath()).absolutePath()));
                        dialog.setFolder(out.path());
                        dialog.deleteBox()->setChecked(true);
                    },
                    &result) == 1);
    const QString bookDir = QDir(out.path()).filePath(QStringLiteral("Dunes"));
    const QString manifest = QDir(bookDir).filePath(QStringLiteral("Dunes.klh"));
    CHECK(result == QStringLiteral("Volume \"Dunes\" is now a book of its own in the folder %1. "
                                   "The series no longer has the volume. The book opens in a "
                                   "new window when you close this one.")
                        .arg(QDir::toNativeSeparators(bookDir)));
    REQUIRE(QFileInfo::exists(manifest));

    // The new book has the text of the open tab
    QStringList problems;
    const std::optional<core::BookProject> book = core::BookProject::load(manifest, problems);
    REQUIRE(book.has_value());
    REQUIRE(book->books.size() == 1);
    REQUIRE_FALSE(book->books.first().mainElements.isEmpty());
    const std::optional<core::ChapterDocument> chapter = core::ChapterDocument::load(
        QDir(bookDir).filePath(book->books.first().mainElements.first().file));
    REQUIRE(chapter.has_value());
    CHECK(chapter->kml().contains(QStringLiteral("Wind over the dunes.")));

    // The series without the volume, its files in the Trash; the book on the Dashboard and in a
    // new window
    CHECK(novel.pm.project()->books.size() == 1);
    CHECK(window.tabs->count() == 0);
    CHECK(trash.taken.contains(file));
    CHECK(core::RecentBooksManager::getInstance().getRecentFiles().contains(manifest));
    CHECK(opening == QStringList{manifest});
}

TEST_CASE("Series: Take Out as a Book... keeps the volume unless asked, and opens no window "
          "when unchecked",
          "[gui][series][join]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    const QString dunes = addVolume(novel.pm, QStringLiteral("Dunes"));
    test::Window window;
    window.navigator.refreshNavigator();

    const QTemporaryDir out;
    REQUIRE(out.isValid());
    QStringList opening;
    QObject::connect(&window.navigator, &gui::NavigatorCoordinator::requestOpenBookInNewWindow,
                     [&opening](const QString& manifest) { opening << manifest; });
    QString result;
    CHECK(takingOut([&window, &dunes]() { window.navigator.onRequestTakeOutVolume(dunes); },
                    [&out](TakeOutVolumeDialog& dialog) {
                        dialog.setFolder(out.path());
                        dialog.openBox()->setChecked(false);
                    },
                    &result) == 1);
    CHECK(result.endsWith(
        QStringLiteral("The book is on the Dashboard and in File > Recent Books.")));
    CHECK(QFileInfo::exists(QDir(out.path()).filePath(QStringLiteral("Dunes/Dunes.klh"))));
    CHECK(novel.pm.project()->books.size() == 2);
    CHECK(opening.isEmpty());

    // A book without other volumes takes nothing out
    REQUIRE(novel.pm.removeBook(dunes).has_value());
    CHECK(takingOut([&window, &novel]() {
              window.navigator.onRequestTakeOutVolume(novel.pm.book()->id);
          },
                    [](TakeOutVolumeDialog&) {}) == 0);
}
