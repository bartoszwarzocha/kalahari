// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_series_join.cpp
/// @brief A series in the window: a volume taken out as a book of its own (Take Out as a
/// Book... of a volume, the window Take Out Volume) and a book joined as a volume (Book > Join
/// Book as Volume..., the window Join Book as Volume)

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/file_trash.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/core/volume_copies.h>
#include <kalahari/gui/command.h>
#include <kalahari/gui/command_registrar.h>
#include <kalahari/gui/command_registry.h>
#include <kalahari/gui/dialogs/join_book_dialog.h>
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
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QPushButton>
#include <QStatusBar>
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
using gui::dialogs::JoinBookDialog;
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

/// Run @p command, handing the window @p Dialog it shows to @p fill, then doing its work, with
/// its result in @p result, and closing the window
/// @return The number of the windows @p Dialog shown
template <typename Dialog>
int working(const std::function<void()>& command, const std::function<void(Dialog&)>& fill,
            QString* result = nullptr) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &fill, result]() {
        auto* dialog = qobject_cast<Dialog*>(test::shownDialog());
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

/// Take a volume out, as working() does
int takingOut(const std::function<void()>& command,
              const std::function<void(TakeOutVolumeDialog&)>& fill, QString* result = nullptr) {
    return working<TakeOutVolumeDialog>(command, fill, result);
}

/// Join a book, as working() does
int joining(const std::function<void()>& command,
            const std::function<void(JoinBookDialog&)>& fill, QString* result = nullptr) {
    return working<JoinBookDialog>(command, fill, result);
}

/// The kind of the characters of @p pm
core::KindRef character(const core::ProjectManager& pm) {
    return pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("character"));
}

/// A book @p title of type @p type in the folder @p folder, with the characters @p cards and
/// the volumes @p volumes after the first one, written by a manager of its own
/// @return Its .klh file
QString writeBook(const QString& folder, const QString& title, const QStringList& cards = {},
                  const QStringList& volumes = {},
                  const QString& type = QStringLiteral("kalahari.novel")) {
    core::ProjectManager other;
    core::ProjectManager::loadBookTypes(
        {QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(other.createProject(folder, title, QStringLiteral("Author"), QStringLiteral("en"),
                                false, type));
    for (const QString& card : cards) {
        REQUIRE_FALSE(
            other.addElement(character(other), card, core::BookPlace::Workshop).isEmpty());
    }
    for (const QString& volume : volumes) {
        addVolume(other, volume);
    }
    const QString manifest = other.getManifestPath();
    REQUIRE(other.closeProject(false));
    return manifest;
}

/// The work of the window Join Book as Volume that joins nothing: it puts in @p calls what it
/// gets, and gives the id of @p outcomes of the call, empty: a failure
JoinBookDialog::Joiner recording(QList<QStringList>& calls, const QStringList& outcomes) {
    return [&calls, outcomes](const QString& manifest, qsizetype index,
                              const QList<core::CardPair>& pairs) {
        QStringList call{manifest, QString::number(index)};
        for (const core::CardPair& pair : pairs) {
            call << pair.card + QLatin1Char('=') + pair.seriesCard;
        }
        calls << call;
        return JoinBookDialog::Outcome{outcomes.value(calls.size() - 1),
                                       {QStringLiteral("a problem")}};
    };
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

TEST_CASE("Series: the window Join Book as Volume says why a book cannot join, and lists the "
          "same cards",
          "[gui][series][join]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    const QString anna =
        novel.pm.addElement(character(novel.pm), QStringLiteral("Anna"), core::BookPlace::Workshop);
    REQUIRE_FALSE(anna.isEmpty());
    const QTemporaryDir books;
    REQUIRE(books.isValid());
    const QDir folder(books.path());
    REQUIRE(folder.mkpath(QStringLiteral("dunes")));
    const QString dunes = writeBook(folder.filePath(QStringLiteral("dunes")),
                                    QStringLiteral("Dunes"),
                                    {QStringLiteral("anna"), QStringLiteral("Ola")});

    QList<QStringList> calls;
    JoinBookDialog dialog(novel.pm, recording(calls, {QString(), QStringLiteral("dunes-id")}));
    dialog.show();

    // Without a book: nothing to join, the places among the volumes, at first the last one
    CHECK(dialog.manifest().isEmpty());
    CHECK(dialog.fileNote().isEmpty());
    CHECK(dialog.bookText().isEmpty());
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK_FALSE(dialog.cardList()->isVisibleTo(&dialog));
    REQUIRE(dialog.placeBox()->count() == 2);
    CHECK(dialog.placeBox()->itemText(0) == QStringLiteral("before volume 1 \"Sand\" – volume 1"));
    CHECK(dialog.placeBox()->currentText() ==
          QStringLiteral("after volume 1 \"Sand\" – volume 2"));

    // The book: what it is, and its card of the series' kind and title, checked
    dialog.setManifest(dunes);
    CHECK(dialog.fileNote().isEmpty());
    CHECK(dialog.bookText() == QStringLiteral("Title: Dunes\nAuthor: Author"));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.cardList()->isVisibleTo(&dialog));
    REQUIRE(dialog.cardList()->topLevelItemCount() == 1);
    QTreeWidgetItem* pair = dialog.cardList()->topLevelItem(0);
    CHECK(pair->text(0) == QStringLiteral("anna"));
    CHECK(pair->text(1) == QStringLiteral("character"));
    REQUIRE(dialog.sameCards().size() == 1);
    const QString bookAnna = dialog.sameCards().constFirst().card;
    CHECK(dialog.sameCards().constFirst().seriesCard == anna);
    pair->setCheckState(0, Qt::Unchecked);
    CHECK(dialog.sameCards().isEmpty());
    pair->setCheckState(0, Qt::Checked);

    // A failure stays in the window, and the book can be joined again
    dialog.placeBox()->setCurrentIndex(0);
    dialog.accept();
    REQUIRE(calls.size() == 1);
    CHECK(calls.constFirst() ==
          QStringList({dunes, QStringLiteral("0"), bookAnna + QLatin1Char('=') + anna}));
    CHECK_FALSE(dialog.isDone());
    CHECK(dialog.resultText() == QStringLiteral("The book could not be joined."));
    CHECK(dialog.bookId().isEmpty());
    CHECK(dialog.acceptButton()->isEnabled());
    dialog.accept();
    REQUIRE(calls.size() == 2);
    CHECK(dialog.isDone());
    CHECK(dialog.resultText() ==
          QStringLiteral("The book \"Dunes\" is now volume 1 of the series."));
    CHECK(dialog.bookId() == QStringLiteral("dunes-id"));
    dialog.close();

    // Books that cannot join: a series, one of another type, the series' own book, a file that
    // is no book
    REQUIRE(folder.mkpath(QStringLiteral("series")));
    REQUIRE(folder.mkpath(QStringLiteral("poems")));
    const QString series =
        writeBook(folder.filePath(QStringLiteral("series")), QStringLiteral("Winds"), {},
                  {QStringLiteral("Winds 2")});
    const QString poems = writeBook(folder.filePath(QStringLiteral("poems")),
                                    QStringLiteral("Poems"), {}, {},
                                    QStringLiteral("kalahari.poetry"));
    const QString broken = folder.filePath(QStringLiteral("broken.klh"));
    {
        QFile file(broken);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("no book");
    }
    JoinBookDialog refusing(novel.pm, recording(calls, {}));
    const auto refused = [&refusing](const QString& manifest) {
        refusing.setManifest(manifest);
        CHECK_FALSE(refusing.acceptButton()->isEnabled());
        CHECK_FALSE(refusing.cardList()->isVisibleTo(&refusing));
        return refusing.fileNote();
    };
    CHECK(refused(series) ==
          QStringLiteral("This book is a series: only a book of one volume can join."));
    CHECK(refused(poems) ==
          QStringLiteral("The book is of another type: only a book of the same type can join."));
    CHECK(refused(novel.pm.getManifestPath()) ==
          QStringLiteral("The series has this book already: it is the same book, or a volume "
                         "taken out of the series that is still in it."));
    CHECK(refused(broken).startsWith(QStringLiteral("Kalahari cannot read this book.")));
    CHECK(refusing.bookText().isEmpty());
    CHECK(calls.size() == 2);
}

TEST_CASE("Series: the window Join Book as Volume fits a small screen, in English and in Polish",
          "[gui][series][join]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("A rather long title of the first volume"));
    const QStringList names{QStringLiteral("Anna Maria Kowalska-Wiśniewska"),
                            QStringLiteral("Ola"), QStringLiteral("Piotr"),
                            QStringLiteral("Zofia"), QStringLiteral("Jan"),
                            QStringLiteral("Ewa"), QStringLiteral("Marek")};
    for (const QString& name : names) {
        REQUIRE_FALSE(
            novel.pm.addElement(character(novel.pm), name, core::BookPlace::Workshop).isEmpty());
    }
    addVolume(novel.pm, QStringLiteral("A rather long title of the second volume"));
    const QTemporaryDir books;
    REQUIRE(books.isValid());
    const QString dunes = writeBook(books.path(), QStringLiteral("Dunes"), names);
    const auto check = [&novel, &dunes](const char* language) {
        INFO("In " << language);
        QList<QStringList> calls;
        JoinBookDialog dialog(novel.pm, recording(calls, {}));
        dialog.setManifest(dunes);
        REQUIRE(dialog.cardList()->topLevelItemCount() == 7);
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

TEST_CASE("Series: Book > Join Book as Volume... joins the book chosen, saving its window first, "
          "and the Navigator shows the volume",
          "[gui][series][join]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Sand"));
    test::Window window;
    const QTemporaryDir books;
    REQUIRE(books.isValid());
    const QString dunes = writeBook(books.path(), QStringLiteral("Dunes"));

    // The command of the Book menu
    {
        gui::CommandRegistry& registry = test::commands();
        registry.clear();
        gui::CommandCallbacks callbacks;
        int asked = 0;
        callbacks.onJoinVolume = [&asked]() { ++asked; };
        gui::registerAllCommands(registry, callbacks);
        const gui::Command* command = registry.getCommand("book.joinVolume");
        REQUIRE(command != nullptr);
        CHECK(command->menuText == "&Join Book as Volume...");
        CHECK(command->menuPath == "BOOK/Join Book as Volume...");
        registry.executeCommand("book.joinVolume");
        CHECK(asked == 1);
        registry.clear();
    }

    // The window of the book cannot save its changes: the book does not join
    QStringList saved;
    bool saves = false;
    const gui::BookSaver saver = [&saved, &saves](const QString& manifest) {
        saved << manifest;
        return saves;
    };
    QString result;
    const auto join = [&window, &saver]() { window.navigator.onRequestJoinVolume(saver); };
    const auto choose = [&dunes](JoinBookDialog& dialog) { dialog.setManifest(dunes); };
    CHECK(joining(join, choose, &result) == 1);
    CHECK(saved == QStringList{dunes});
    CHECK(result == QStringLiteral("The book could not be joined."));
    CHECK(novel.pm.project()->books.size() == 1);

    // It can: the volume at the end, shown in the Navigator, with its first chapter
    saves = true;
    int modified = 0;
    QObject::connect(&window.navigator, &gui::NavigatorCoordinator::documentModified,
                     [&modified]() { ++modified; });
    CHECK(joining(join, choose, &result) == 1);
    CHECK(saved == QStringList({dunes, dunes}));
    CHECK(result == QStringLiteral("The book \"Dunes\" is now volume 2 of the series."));
    REQUIRE(novel.pm.project()->books.size() == 2);
    const core::ProjectBook& volume = novel.pm.project()->books.at(1);
    CHECK(volume.title == QStringLiteral("Dunes"));
    CHECK(itemWithText(*window.navigatorPanel, QStringLiteral("Volume 2: Dunes")) != nullptr);
    CHECK(window.statusBar.currentMessage() == QStringLiteral("Book joined as volume 2: Dunes"));
    CHECK(modified == 1);
    REQUIRE_FALSE(volume.mainElements.isEmpty());
    CHECK(QFileInfo::exists(novel.pm.filePathOf(volume.mainElements.constFirst())));
}
