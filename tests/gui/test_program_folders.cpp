/// @file test_program_folders.cpp
/// @brief The folders of the program: the folders of the books, the archives and the backups,
/// the folders the windows choosing files start in, and the Folders page of the Settings

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/backup_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/utils/program_folders.h"
#include "kalahari/gui/widgets/folder_field.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <functional>
#include <string>

using namespace kalahari;
using gui::ProgramFolders;
using Operation = gui::ProgramFolders::Operation;

namespace {

core::SettingsManager& settings() {
    return core::SettingsManager::getInstance();
}

std::string setting(const std::string& key) {
    return settings().get<std::string>(key);
}

/// @brief A path the way the settings keep it
std::string stored(const QString& path) {
    return ProgramFolders::cleanPath(path).toStdString();
}

void writeFile(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write("text");
}

/// @brief Wait until @p done, at most two seconds
bool waitUntil(const std::function<bool()>& done) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 2000) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return done();
}

/// @brief Open page @p title of category @p category of the Settings
void openPage(gui::SettingsDialog& dialog, const QString& category, const QString& title) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        QTreeWidgetItem* parent = (*it)->parent();
        if ((*it)->text(0) == title && (parent ? parent->text(0) : QString()) == category) {
            tree->setCurrentItem(*it);
            return;
        }
    }
    FAIL("No page " << title.toStdString());
}

/// @brief The page shown in the tree of the Settings
QString shownPage(gui::SettingsDialog& dialog) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    return tree->currentItem() ? tree->currentItem()->text(0) : QString();
}

/// @brief Run @p command and close the message it shows
/// @return The title and the text of the message; empty when it shows none
QString messageOf(const std::function<void()>& command) {
    QString text;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&text]() {
        auto* message =
            qobject_cast<gui::dialogs::MessageDialog*>(QApplication::activeModalWidget());
        if (message) {
            text = message->clipboardText();
            message->reject();
        }
    });
    timer.start();
    command();
    return text;
}

/// @brief The button of a field of a folder with text @p text
QPushButton* buttonOf(const gui::FolderField& field, const QString& text) {
    for (QPushButton* button : field.findChildren<QPushButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

/// @brief The folder field of the New Book window
QLineEdit* locationOf(const gui::dialogs::NewItemDialog& dialog) {
    for (QLineEdit* field : dialog.findChildren<QLineEdit*>()) {
        if (field->placeholderText() == QStringLiteral("Select book folder...")) {
            return field;
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("Folders: the default folders are in the Kalahari folder of Documents",
          "[gui][folders]") {
    // The tests keep away from the Documents folder of the user
    const QString documents = ProgramFolders::documentsFolder();
    CHECK(documents == ProgramFolders::cleanPath(QDir(QDir::tempPath()).filePath("Documents")));

    CHECK(ProgramFolders::defaultBooksFolder() == documents + "/Kalahari");
    CHECK(ProgramFolders::defaultArchivesFolder() == documents + "/Kalahari/Archives");
    CHECK(ProgramFolders::defaultBackupsFolder() == documents + "/Kalahari/Backups");
    CHECK(ProgramFolders::booksFolder() == ProgramFolders::defaultBooksFolder());
    CHECK(ProgramFolders::archivesFolder() == ProgramFolders::defaultArchivesFolder());

    // A folder of the settings, typed with the separators of the system and spaces
    QTemporaryDir dir;
    settings().set<std::string>(
        ProgramFolders::BOOKS_SETTING,
        (QDir::toNativeSeparators(dir.path() + "/Books/./Mine/") + "  ").toStdString());
    CHECK(ProgramFolders::booksFolder() == QDir(dir.path()).filePath("Books/Mine"));
}

TEST_CASE("Folders: a window starts in the folder it used last, else in the folder of the "
          "settings",
          "[gui][folders]") {
    QTemporaryDir dir;
    const QDir root(dir.path());
    const QString books = root.filePath("Books");
    const QString archives = root.filePath("Archives");
    settings().set<std::string>(ProgramFolders::BOOKS_SETTING, books.toStdString());
    settings().set<std::string>(ProgramFolders::ARCHIVES_SETTING, archives.toStdString());

    // At first the folders of the settings; files can be anywhere, so Open File starts in
    // the Documents folder
    CHECK(ProgramFolders::startFolder(Operation::NewBook) == books);
    CHECK(ProgramFolders::startFolder(Operation::OpenBook) == books);
    CHECK(ProgramFolders::startFolder(Operation::ImportDestination) == books);
    CHECK(ProgramFolders::startFolder(Operation::ExportArchive) == archives);
    CHECK(ProgramFolders::startFolder(Operation::ImportArchive) == archives);
    CHECK(ProgramFolders::startFolder(Operation::OpenFile) == ProgramFolders::documentsFolder());

    // A window creates the folder of the settings when it is first used
    CHECK_FALSE(QFileInfo::exists(books));
    CHECK(ProgramFolders::windowFolder(Operation::OpenBook) == books);
    CHECK(QFileInfo(books).isDir());

    // Each operation remembers its own folder, in the settings, so also after a restart
    REQUIRE(root.mkpath("Elsewhere/Books"));
    const QString elsewhere = root.filePath("Elsewhere/Books");
    ProgramFolders::remember(Operation::OpenBook, QDir::toNativeSeparators(elsewhere));
    CHECK(setting("project.lastFolders.openBook") == stored(elsewhere));
    CHECK(ProgramFolders::startFolder(Operation::OpenBook) == elsewhere);
    CHECK(ProgramFolders::windowFolder(Operation::OpenBook) == elsewhere);
    CHECK(ProgramFolders::startFolder(Operation::NewBook) == books);
    CHECK(ProgramFolders::startFolder(Operation::ImportDestination) == books);

    // A folder that is gone (a drive that is not connected) gives way to the settings
    REQUIRE(QDir(root.filePath("Elsewhere")).removeRecursively());
    CHECK(ProgramFolders::startFolder(Operation::OpenBook) == books);

    // A folder of the settings that cannot be created opens the window in the nearest one
    writeFile(root.filePath("file.txt"));
    settings().set<std::string>(ProgramFolders::ARCHIVES_SETTING,
                                root.filePath("file.txt/Archives").toStdString());
    CHECK(ProgramFolders::windowFolder(Operation::ExportArchive) == root.path());
}

TEST_CASE("Folders: a changed folder of the settings is where its windows start again",
          "[gui][folders]") {
    QTemporaryDir dir;
    const QString last = QDir(dir.path()).filePath("Last");
    REQUIRE(QDir().mkpath(last));
    for (Operation operation : {Operation::NewBook, Operation::OpenBook, Operation::OpenFile,
                                Operation::ExportArchive, Operation::ImportArchive,
                                Operation::ImportDestination}) {
        ProgramFolders::remember(operation, last);
    }

    ProgramFolders::forgetFolders(ProgramFolders::BOOKS_SETTING);
    CHECK(setting("project.lastFolders.newBook").empty());
    CHECK(setting("project.lastFolders.openBook").empty());
    CHECK(setting("project.lastFolders.importDestination").empty());
    CHECK(setting("project.lastFolders.exportArchive") == stored(last));
    CHECK(setting("project.lastFolders.importArchive") == stored(last));
    CHECK(setting("project.lastFolders.openFile") == stored(last));

    ProgramFolders::forgetFolders(ProgramFolders::ARCHIVES_SETTING);
    CHECK(setting("project.lastFolders.exportArchive").empty());
    CHECK(setting("project.lastFolders.importArchive").empty());
    CHECK(setting("project.lastFolders.openFile") == stored(last));
}

TEST_CASE("Folders: why the program cannot use a folder", "[gui][folders]") {
    QTemporaryDir dir;
    const QDir root(dir.path());

    CHECK(ProgramFolders::problemOf(root.path()).isEmpty());
    // A folder that does not exist yet is created when it is first needed
    CHECK(ProgramFolders::problemOf(root.filePath("New/Deeper")).isEmpty());
    CHECK(ProgramFolders::problemOf("Books") == "Give the full path of the folder.");
    CHECK(ProgramFolders::problemOf("  ") == "Give the full path of the folder.");

    writeFile(root.filePath("file.txt"));
    CHECK(ProgramFolders::problemOf(root.filePath("file.txt")) == "This is a file, not a folder.");
    CHECK(ProgramFolders::problemOf(root.filePath("file.txt/Books")) ==
          "Kalahari cannot create this folder.");

    // A folder without the right to write; the administrator may write anywhere, so there
    // the folder is fine
    REQUIRE(root.mkpath("Locked"));
    const QString locked = root.filePath("Locked");
    QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner);
    bool writable = false;
    {
        QTemporaryFile probe(QDir(locked).filePath("probe_XXXXXX"));
        writable = probe.open();
    }
    if (!writable) {
        CHECK(ProgramFolders::problemOf(locked) == "Kalahari cannot save files in this folder.");
        CHECK(ProgramFolders::problemOf(QDir(locked).filePath("New")) ==
              "Kalahari cannot create this folder.");
    } else {
        CHECK(ProgramFolders::problemOf(locked).isEmpty());
    }
    QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    // The nearest existing folder, and the same folder written another way
    CHECK(ProgramFolders::existingFolderAt(root.filePath("New/Deeper")) == root.path());
    CHECK(ProgramFolders::existingFolderAt(root.filePath("file.txt/Books")) == root.path());
    CHECK(ProgramFolders::existingFolderAt("Books").isEmpty());
    CHECK(ProgramFolders::samePath(root.path() + "/", QDir::toNativeSeparators(root.path())));
    CHECK(ProgramFolders::samePath(root.filePath("New/../Books"), root.filePath("Books")));
#ifdef Q_OS_WIN
    CHECK(ProgramFolders::samePath("C:/Books", "c:/books"));
#else
    CHECK_FALSE(ProgramFolders::samePath("/Books", "/books"));
#endif
}

TEST_CASE("Folders: the field of a folder", "[gui][folders]") {
    QTemporaryDir dir;
    const QDir root(dir.path());
    gui::FolderField field;
    field.setDefaultFolder(root.filePath("Default"));

    // An empty field means the default folder, which it shows
    CHECK(field.lineEdit()->placeholderText() ==
          QDir::toNativeSeparators(root.filePath("Default")));
    CHECK(field.folder() == root.filePath("Default"));

    // A folder is shown with the separators of the system and kept the way the settings
    // keep it; one that does not exist yet is created when it is first needed
    field.setFolder(root.filePath("Mine") + "/");
    CHECK(field.lineEdit()->text() == QDir::toNativeSeparators(root.filePath("Mine")));
    CHECK(field.folder() == root.filePath("Mine"));
    CHECK(field.stateText() == "Kalahari will create this folder when it is first needed.");
    REQUIRE(root.mkpath("Mine"));
    field.setFolder(root.filePath("Mine"));
    CHECK(field.stateText().isEmpty());
    CHECK(field.problem().isEmpty());

    // The line under the field follows typing
    writeFile(root.filePath("file.txt"));
    field.lineEdit()->setText(QDir::toNativeSeparators(root.filePath("file.txt")));
    CHECK(waitUntil([&field]() { return !field.stateText().isEmpty(); }));
    CHECK(field.stateText() == "This is a file, not a folder.");
    CHECK(field.problem() == "This is a file, not a folder.");

    // Restore Default, available while the field holds another folder
    QPushButton* restore = buttonOf(field, QStringLiteral("Restore Default"));
    REQUIRE(restore != nullptr);
    CHECK(restore->isEnabled());
    restore->click();
    CHECK(field.lineEdit()->text() == QDir::toNativeSeparators(root.filePath("Default")));
    CHECK(field.folder() == root.filePath("Default"));
    CHECK_FALSE(restore->isEnabled());
    field.lineEdit()->clear();
    CHECK_FALSE(restore->isEnabled());
    CHECK(buttonOf(field, QStringLiteral("Choose...")) != nullptr);

    // A field that is not used says nothing about its folder
    field.setFolder(root.filePath("file.txt"));
    CHECK_FALSE(field.stateText().isEmpty());
    field.setEnabled(false);
    CHECK(field.stateText().isEmpty());
}

TEST_CASE("Folders page: the folders of the books and the archives", "[gui][settings][folders]") {
    QTemporaryDir dir;
    const QDir root(dir.path());
    const QString last = root.filePath("Last");
    REQUIRE(QDir().mkpath(last));
    for (Operation operation : {Operation::NewBook, Operation::OpenBook, Operation::OpenFile,
                                Operation::ExportArchive, Operation::ImportArchive,
                                Operation::ImportDestination}) {
        ProgramFolders::remember(operation, last);
    }

    gui::SettingsDialog dialog(nullptr);
    openPage(dialog, QStringLiteral("Files"), QStringLiteral("Folders"));
    auto* books = dialog.findChild<gui::FolderField*>(ProgramFolders::BOOKS_SETTING);
    auto* archives = dialog.findChild<gui::FolderField*>(ProgramFolders::ARCHIVES_SETTING);
    REQUIRE(books != nullptr);
    REQUIRE(archives != nullptr);

    // The default folders, kept as empty settings
    CHECK(books->lineEdit()->text() ==
          QDir::toNativeSeparators(ProgramFolders::defaultBooksFolder()));
    CHECK(archives->folder() == ProgramFolders::defaultArchivesFolder());
    CHECK_FALSE(dialog.hasChanges());

    // Another folder of the books: the windows that start in it start there again, the others
    // keep the folders they used last
    books->setFolder(root.filePath("Books"));
    CHECK(dialog.hasChanges());
    CHECK(dialog.applyChanges() == QStringList{QStringLiteral("project.defaultLocation")});
    CHECK(setting(ProgramFolders::BOOKS_SETTING) == stored(root.filePath("Books")));
    CHECK(setting("project.lastFolders.newBook").empty());
    CHECK(setting("project.lastFolders.openBook").empty());
    CHECK(setting("project.lastFolders.importDestination").empty());
    CHECK(setting("project.lastFolders.exportArchive") == stored(last));
    CHECK(setting("project.lastFolders.openFile") == stored(last));

    archives->setFolder(root.filePath("Archives"));
    CHECK(dialog.applyChanges() == QStringList{QStringLiteral("project.archiveLocation")});
    CHECK(setting(ProgramFolders::ARCHIVES_SETTING) == stored(root.filePath("Archives")));
    CHECK(setting("project.lastFolders.exportArchive").empty());
    CHECK(setting("project.lastFolders.importArchive").empty());
    CHECK(setting("project.lastFolders.openFile") == stored(last));

    // Restore Default empties the setting again
    QPushButton* restore = buttonOf(*books, QStringLiteral("Restore Default"));
    REQUIRE(restore != nullptr);
    restore->click();
    CHECK(books->folder() == ProgramFolders::defaultBooksFolder());
    CHECK(dialog.applyChanges() == QStringList{QStringLiteral("project.defaultLocation")});
    CHECK(setting(ProgramFolders::BOOKS_SETTING).empty());
}

TEST_CASE("Folders page: where and how many backups of the databases are kept",
          "[gui][settings][folders]") {
    QTemporaryDir dir;
    const QDir root(dir.path());
    gui::SettingsDialog dialog(nullptr);
    openPage(dialog, QStringLiteral("Files"), QStringLiteral("Folders"));
    auto* inEachBook = dialog.findChild<QRadioButton*>(QStringLiteral("backupsInEachBook"));
    auto* inOneFolder = dialog.findChild<QRadioButton*>(QStringLiteral("backupsInOneFolder"));
    auto* folder = dialog.findChild<gui::FolderField*>(QStringLiteral("backupFolder"));
    auto* count = dialog.findChild<QSpinBox*>(QStringLiteral("backupCount"));
    REQUIRE(inEachBook != nullptr);
    REQUIRE(inOneFolder != nullptr);
    REQUIRE(folder != nullptr);
    REQUIRE(count != nullptr);

    // At first in the folder of each book, five copies
    CHECK(inEachBook->isChecked());
    CHECK_FALSE(folder->isEnabled());
    CHECK(folder->folder() == ProgramFolders::defaultBackupsFolder());
    CHECK(count->value() == 5);

    // One folder for all books: the suggested folder goes to the settings as it is
    inOneFolder->setChecked(true);
    CHECK(folder->isEnabled());
    count->setValue(3);
    const QStringList written = dialog.applyChanges();
    CHECK(written.contains(QStringLiteral("project.backupFolder")));
    CHECK(written.contains(QStringLiteral("project.backupCount")));
    CHECK(setting(core::BackupManager::FOLDER_SETTING) ==
          stored(ProgramFolders::defaultBackupsFolder()));
    CHECK(settings().get<int>(core::BackupManager::COUNT_SETTING) == 3);

    // Another folder, then the folder of each book again
    folder->setFolder(root.filePath("Backups"));
    CHECK(dialog.applyChanges() == QStringList{QStringLiteral("project.backupFolder")});
    CHECK(setting(core::BackupManager::FOLDER_SETTING) == stored(root.filePath("Backups")));
    inEachBook->setChecked(true);
    CHECK_FALSE(folder->isEnabled());
    CHECK(dialog.applyChanges() == QStringList{QStringLiteral("project.backupFolder")});
    CHECK(setting(core::BackupManager::FOLDER_SETTING).empty());

    // The page shows the common folder of the settings
    settings().set<std::string>(core::BackupManager::FOLDER_SETTING,
                                root.filePath("Backups").toStdString());
    gui::SettingsDialog again(nullptr);
    openPage(again, QStringLiteral("Files"), QStringLiteral("Folders"));
    auto* shownOneFolder = again.findChild<QRadioButton*>(QStringLiteral("backupsInOneFolder"));
    auto* shownFolder = again.findChild<gui::FolderField*>(QStringLiteral("backupFolder"));
    REQUIRE(shownOneFolder != nullptr);
    REQUIRE(shownFolder != nullptr);
    CHECK(shownOneFolder->isChecked());
    CHECK(shownFolder->isEnabled());
    CHECK(shownFolder->folder() == root.filePath("Backups"));
    CHECK_FALSE(again.hasChanges());
}

TEST_CASE("Folders page: a folder the program cannot use is not saved",
          "[gui][settings][folders]") {
    QTemporaryDir dir;
    const QDir root(dir.path());
    writeFile(root.filePath("file.txt"));
    const QString blocked = root.filePath("file.txt/Books");

    gui::SettingsDialog dialog(nullptr);
    dialog.show();
    openPage(dialog, QStringLiteral("Files"), QStringLiteral("Folders"));
    auto* books = dialog.findChild<gui::FolderField*>(ProgramFolders::BOOKS_SETTING);
    REQUIRE(books != nullptr);
    books->setFolder(blocked);
    CHECK(books->stateText() == "Kalahari cannot create this folder.");

    // Apply says why and saves nothing
    QString message = messageOf([&dialog]() { CHECK(dialog.applyChanges().isEmpty()); });
    CHECK(message.startsWith(QStringLiteral("Folders")));
    CHECK(message.contains(QStringLiteral("The folder '%1' cannot be used. Kalahari cannot "
                                          "create this folder.")
                               .arg(QDir::toNativeSeparators(blocked))));
    CHECK(setting(ProgramFolders::BOOKS_SETTING).empty());
    CHECK(dialog.hasChanges());

    // OK keeps the dialog open, on the page with the folder
    openPage(dialog, QStringLiteral("Editor"), QStringLiteral("General"));
    message = messageOf([&dialog]() { dialog.accept(); });
    CHECK_FALSE(message.isEmpty());
    CHECK(dialog.isVisible());
    CHECK(shownPage(dialog) == QStringLiteral("Folders"));
    CHECK(setting(ProgramFolders::BOOKS_SETTING).empty());

    // A folder the program can use is saved
    books->setFolder(root.filePath("Books"));
    message = messageOf([&dialog]() { dialog.accept(); });
    CHECK(message.isEmpty());
    CHECK_FALSE(dialog.isVisible());
    CHECK(setting(ProgramFolders::BOOKS_SETTING) == stored(root.filePath("Books")));
}

TEST_CASE("Folders page: a folder the program cannot use now stays until it is changed",
          "[gui][settings][folders]") {
    // A drive that is not connected now does not stop other changes
    QTemporaryDir dir;
    const QDir root(dir.path());
    writeFile(root.filePath("file.txt"));
    const QString blocked = root.filePath("file.txt/Books");
    settings().set<std::string>(ProgramFolders::BOOKS_SETTING, blocked.toStdString());

    gui::SettingsDialog dialog(nullptr);
    openPage(dialog, QStringLiteral("Files"), QStringLiteral("Folders"));
    auto* books = dialog.findChild<gui::FolderField*>(ProgramFolders::BOOKS_SETTING);
    auto* count = dialog.findChild<QSpinBox*>(QStringLiteral("backupCount"));
    REQUIRE(books != nullptr);
    REQUIRE(count != nullptr);
    CHECK(books->folder() == blocked);
    CHECK(books->stateText() == "Kalahari cannot create this folder.");

    count->setValue(7);
    QStringList written;
    CHECK(messageOf([&dialog, &written]() { written = dialog.applyChanges(); }).isEmpty());
    CHECK(written == QStringList{QStringLiteral("project.backupCount")});
    CHECK(setting(ProgramFolders::BOOKS_SETTING) == blocked.toStdString());
}

TEST_CASE("New Book: the book starts in the folder the last book was created in",
          "[gui][dialogs][folders]") {
    QTemporaryDir dir;
    const QDir root(dir.path());
    settings().set<std::string>(ProgramFolders::BOOKS_SETTING,
                                root.filePath("Books").toStdString());
    {
        gui::dialogs::NewItemDialog dialog(gui::dialogs::NewItemMode::Project);
        QLineEdit* location = locationOf(dialog);
        REQUIRE(location != nullptr);
        CHECK(location->text() == QDir::toNativeSeparators(root.filePath("Books")));
    }

    REQUIRE(root.mkpath("Last"));
    ProgramFolders::remember(Operation::NewBook, root.filePath("Last"));
    {
        gui::dialogs::NewItemDialog dialog(gui::dialogs::NewItemMode::Project);
        QLineEdit* location = locationOf(dialog);
        REQUIRE(location != nullptr);
        CHECK(location->text() == QDir::toNativeSeparators(root.filePath("Last")));
    }

    // Not when that folder is gone
    REQUIRE(QDir(root.filePath("Last")).removeRecursively());
    gui::dialogs::NewItemDialog dialog(gui::dialogs::NewItemMode::Project);
    QLineEdit* location = locationOf(dialog);
    REQUIRE(location != nullptr);
    CHECK(location->text() == QDir::toNativeSeparators(root.filePath("Books")));
}
