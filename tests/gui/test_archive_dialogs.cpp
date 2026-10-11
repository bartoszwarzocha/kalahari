/// @file test_archive_dialogs.cpp
/// @brief Export Project Archive and Import Project Archive: the fields and the lines under
/// them, and the progress and the result of the work in the window

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/archive_dialogs.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/utils/file_location.h"
#include "kalahari/gui/widgets/fitting_scroll_area.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QJsonDocument>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QTemporaryDir>
#include <QTranslator>
#include <QTreeWidget>

#include <zip.h>

#include <utility>

using namespace kalahari;
using gui::dialogs::ArchiveDialog;
using gui::dialogs::ExportArchiveDialog;
using gui::dialogs::ImportArchiveDialog;

namespace {

/// @brief The book types of the resources folder
const core::BookTypeRegistry& builtInTypes() {
    static const core::BookTypeRegistry registry = [] {
        core::BookTypeRegistry loaded;
        loaded.load({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        return loaded;
    }();
    return registry;
}

/// @brief Whether a label of @p dialog shows @p text, also while the dialog is hidden
bool showsText(const QDialog& dialog, const QString& text) {
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->isVisibleTo(&dialog) && label->text() == text) {
            return true;
        }
    }
    return false;
}

/// @brief Let the layouts follow a change: each level of widgets tells the one above it with an
/// event
void settle() {
    for (int i = 0; i < 6; ++i) {
        QApplication::processEvents();
    }
}

/// @brief The label of @p dialog that shows @p text; nullptr when none does
QLabel* labelWith(const QDialog& dialog, const QString& text) {
    for (QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->text() == text) {
            return label;
        }
    }
    return nullptr;
}

/// @brief Write file @p path with @p data
void writeFile(const QString& path, const QByteArray& data) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(data) == data.size());
}

/// @brief Write archive @p path with @p entries: the name and the data of each file
void writeArchive(const QString& path, const QList<std::pair<QByteArray, QByteArray>>& entries) {
    int error = 0;
    zip_t* archive =
        zip_open(QFile::encodeName(path).constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    REQUIRE(archive != nullptr);
    for (const auto& [name, data] : entries) {
        zip_source_t* source = zip_source_buffer(archive, data.constData(),
                                                 static_cast<zip_uint64_t>(data.size()), 0);
        REQUIRE(source != nullptr);
        REQUIRE(zip_file_add(archive, name.constData(), source, ZIP_FL_ENC_UTF_8) >= 0);
    }
    REQUIRE(zip_close(archive) == 0);
}

/// @brief Write archive @p path with the .klh file @p name of @p project
void writeArchive(const QString& path, const QByteArray& name, const QByteArray& data) {
    writeArchive(path, {{name, data}});
}

/// @brief An archive of the new novel "Travels" by Anna, in @p dir
QString novelArchive(const QTemporaryDir& dir) {
    auto& pm = core::ProjectManager::getInstance();
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.createProject(dir.path(), "Travels", "Anna", "en", true, "kalahari.novel"));
    const QString archive = QDir(dir.path()).filePath(QStringLiteral("Travels.klh.zip"));
    REQUIRE(pm.exportArchive(archive));
    REQUIRE(pm.closeProject(false));
    return archive;
}

/// @brief An element of kind @p kindId of package @p packageId
core::ProjectElement element(const QString& packageId, const QString& kindId, int number) {
    core::ProjectElement made;
    made.id = QStringLiteral("%1_%2").arg(kindId).arg(number);
    made.kind = {packageId, kindId};
    made.title = QStringLiteral("%1 %2").arg(kindId).arg(number);
    return made;
}

/// @brief @p count elements of kind @p kindId of package @p packageId, numbered from @p first
QList<core::ProjectElement> elements(const QString& packageId, const QString& kindId,
                                     int count, int first = 1) {
    QList<core::ProjectElement> made;
    for (int i = first; i < first + count; ++i) {
        made.append(element(packageId, kindId, i));
    }
    return made;
}

/// @brief Pointers to @p list, as ImportArchiveDialog::contentsOf() takes them
QList<const core::ProjectElement*> pointers(const QList<core::ProjectElement>& list) {
    QList<const core::ProjectElement*> all;
    for (const core::ProjectElement& each : list) {
        all.append(&each);
    }
    return all;
}

/// @brief The file manager of the tests: it remembers what the window shows
struct FileManager {
    QStringList shown;

    FileManager() {
        gui::setFileManagerForTests([this](const QString& path) {
            shown << path;
            return true;
        });
    }
    ~FileManager() { gui::setFileManagerForTests({}); }
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;
};

/// @brief The Polish translation of the sentences of contentsOf(), with the Polish forms of
/// the numbers: one, a few, many
class ContentsTranslator : public QTranslator {
public:
    QString translate(const char* context, const char* sourceText, const char* /*disambiguation*/,
                      int n) const override {
        if (QString::fromLatin1(context) !=
            QLatin1String("kalahari::gui::dialogs::ImportArchiveDialog")) {
            return {};
        }
        const QString source = QString::fromUtf8(sourceText);
        const int form = n == 1 ? 0
                         : (n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % 100 > 14)) ? 1
                                                                                            : 2;
        if (source == QLatin1String("%n {kind:plural}")) {
            const char* forms[] = {"%n {kind}", "%n {kind:plural}", "%n {kind:genitivePlural}"};
            return QString::fromLatin1(forms[form]);
        }
        if (source == QLatin1String("%n elements: %1")) {
            const char* forms[] = {"%n element: %1", "%n elementy: %1", "%n elementów: %1"};
            return QString::fromUtf8(forms[form]);
        }
        if (source == QLatin1String("{kind:indefinite}")) {
            return QStringLiteral("{kind}");
        }
        if (source == QLatin1String("no elements")) {
            return QStringLiteral("brak elementów");
        }
        return {};
    }
    bool isEmpty() const override { return false; }
};

} // namespace

// =============================================================================
// Export Project Archive
// =============================================================================

TEST_CASE("Export Project Archive: the folder and the name of the archive",
          "[gui][dialogs][archive]") {
    QTemporaryDir dir;
    const QString folder = QDir(dir.path()).filePath(QStringLiteral("Archives"));
    ExportArchiveDialog dialog({QStringLiteral("Travels"), {}, false, 0}, folder,
                               [](const QString&, bool, const ArchiveDialog::Progress&,
                                  QStringList*) { return true; });

    CHECK(dialog.windowTitle() == QStringLiteral("Export Project Archive"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Export"));
    CHECK(dialog.nameField()->text() == QStringLiteral("Travels.klh.zip"));
    CHECK(dialog.nameField()->selectedText() == QStringLiteral("Travels"));
    CHECK(dialog.folder() == folder);
    CHECK_FALSE(dialog.unreadyGroup()->isVisibleTo(&dialog));
    CHECK(showsText(dialog, QStringLiteral("The progress and the result will show here.")));
    CHECK_FALSE(dialog.progressBar()->isVisibleTo(&dialog));

    SECTION("A folder that does not exist yet is created") {
        CHECK(dialog.folderNote() ==
              QStringLiteral("Kalahari creates this folder during the export."));
        CHECK(dialog.nameNote().isEmpty());
        CHECK(dialog.archivePath() ==
              QDir(folder).filePath(QStringLiteral("Travels.klh.zip")));
        CHECK(dialog.acceptButton()->isEnabled());
    }

    SECTION("A file or a relative path is not a folder") {
        const QString file = QDir(dir.path()).filePath(QStringLiteral("notes.txt"));
        writeFile(file, "notes");
        dialog.setFolder(file);
        CHECK(dialog.folderNote() == QStringLiteral("This is a file, not a folder."));
        CHECK(dialog.nameNote().isEmpty());
        CHECK(dialog.archivePath().isEmpty());
        CHECK_FALSE(dialog.acceptButton()->isEnabled());

        dialog.setFolder(QStringLiteral("Archives"));
        CHECK(dialog.folderNote() == QStringLiteral("Give the full path of the folder."));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
    }

    SECTION("The name") {
        dialog.setFolder(dir.path());
        CHECK(dialog.folderNote().isEmpty());

        dialog.nameField()->setText(QString());
        CHECK(dialog.nameNote() == QStringLiteral("Type the name of the archive."));
        CHECK(dialog.archivePath().isEmpty());
        CHECK_FALSE(dialog.acceptButton()->isEnabled());

        dialog.nameField()->setText(QStringLiteral("Tra/vels"));
        CHECK(dialog.nameNote() ==
              QStringLiteral("A name cannot have the characters \\ / : * ? \" < > |."));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());

        // The extension is added, and the window says so
        dialog.nameField()->setText(QStringLiteral(" Holiday "));
        CHECK(dialog.nameNote() ==
              QStringLiteral("The archive gets the name \"Holiday.klh.zip\"."));
        CHECK(dialog.archivePath() ==
              QDir(dir.path()).filePath(QStringLiteral("Holiday.klh.zip")));
        CHECK(dialog.acceptButton()->isEnabled());
        dialog.nameField()->setText(QStringLiteral("Holiday.KLH.ZIP"));
        CHECK(dialog.nameNote().isEmpty());

        // An archive of that name is replaced; a folder of that name is in the way
        writeFile(QDir(dir.path()).filePath(QStringLiteral("Holiday.klh.zip")), "old");
        dialog.nameField()->setText(QStringLiteral("Holiday"));
        CHECK(dialog.nameNote() == QStringLiteral("The folder already has the archive "
                                                  "\"Holiday.klh.zip\". The export replaces it."));
        CHECK(dialog.acceptButton()->isEnabled());
        REQUIRE(QDir(dir.path()).mkdir(QStringLiteral("Box.klh.zip")));
        dialog.nameField()->setText(QStringLiteral("Box"));
        CHECK(dialog.nameNote() == QStringLiteral("The folder has a folder named \"Box.klh.zip\". "
                                                  "Choose another name."));
        CHECK(dialog.archivePath().isEmpty());
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
    }
}

TEST_CASE("Export Project Archive: the files that are not ready and the unsaved changes",
          "[gui][dialogs][archive]") {
    QTemporaryDir dir;
    const ExportArchiveDialog::Book book{
        QStringLiteral("Travels"),
        {{QStringLiteral("Chapter 2"), QStringLiteral("draft"), QStringLiteral("template.chapter")},
         {QStringLiteral("Epilogue"), QStringLiteral("revision"),
          QStringLiteral("template.chapter")}},
        true,
        0};
    ExportArchiveDialog dialog(book, dir.path(),
                               [](const QString&, bool, const ArchiveDialog::Progress&,
                                  QStringList*) { return true; });

    REQUIRE(dialog.unreadyGroup()->isVisibleTo(&dialog));
    CHECK(dialog.unreadyGroup()->title() == QStringLiteral("Files Not Ready (2)"));
    QTreeWidget* list = dialog.unreadyList();
    REQUIRE(list->topLevelItemCount() == 2);
    CHECK(list->topLevelItem(0)->text(0) == QStringLiteral("Chapter 2"));
    CHECK(list->topLevelItem(0)->text(1) == QStringLiteral("Draft"));
    CHECK(list->topLevelItem(1)->text(0) == QStringLiteral("Epilogue"));
    CHECK(list->topLevelItem(1)->text(1) == QStringLiteral("Revision"));
    CHECK(showsText(dialog, QStringLiteral("These files go to the archive as they are. You "
                                           "change their status in the Navigator.")));
    CHECK(showsText(dialog, QStringLiteral("Kalahari saves the unsaved changes before the "
                                           "export. The progress and the result will show "
                                           "here.")));
}

TEST_CASE("Export Project Archive: the progress and the result in the window",
          "[gui][dialogs][archive]") {
    QTemporaryDir dir;
    const QString folder = QDir(dir.path()).filePath(QStringLiteral("Archives"));
    const QString path = QDir(folder).filePath(QStringLiteral("Travels.klh.zip"));
    QStringList written;
    bool succeeds = true;
    ExportArchiveDialog* shown = nullptr;
    ExportArchiveDialog dialog(
        {QStringLiteral("Travels"), {}, false, 0}, folder,
        [&](const QString& archive, bool, const ArchiveDialog::Progress& progress,
            QStringList* problems) {
            written << archive;
            // While the work lasts, the window waits and cannot be closed
            progress(40);
            CHECK(shown->progressBar()->isVisibleTo(shown));
            CHECK(shown->progressBar()->value() == 40);
            CHECK_FALSE(shown->acceptButton()->isEnabled());
            CHECK_FALSE(shown->cancelButton()->isEnabled());
            CHECK_FALSE(shown->nameField()->isEnabled());
            shown->reject();
            CHECK(shown->isVisible());
            if (!succeeds) {
                problems->append(QStringLiteral("Travels.klh.zip: the disk is full"));
                return false;
            }
            writeFile(archive, "archive");
            return true;
        });
    shown = &dialog;
    dialog.show();

    SECTION("Saved: the result, Show in Folder and Close") {
        FileManager fileManager;
        dialog.accept();
        CHECK(written == QStringList{path});
        CHECK(QFileInfo(folder).isDir());
        CHECK(dialog.isDone());
        CHECK(dialog.isVisible());
        CHECK(dialog.resultText() ==
              QStringLiteral("Saved the archive %1.").arg(QDir::toNativeSeparators(path)));
        CHECK(dialog.detailsText().isEmpty());
        CHECK(dialog.progressBar()->value() == 100);
        CHECK(dialog.acceptButton()->text() == QStringLiteral("Close"));
        CHECK(dialog.acceptButton()->isEnabled());
        CHECK_FALSE(dialog.cancelButton()->isVisibleTo(&dialog));
        CHECK_FALSE(dialog.copyButton()->isVisibleTo(&dialog));
        CHECK_FALSE(showsText(dialog, QStringLiteral("The progress and the result will show "
                                                     "here.")));

        dialog.showResult();
        CHECK(fileManager.shown == QStringList{path});

        // Close closes the window
        dialog.accept();
        CHECK_FALSE(dialog.isVisible());
        CHECK(dialog.result() == QDialog::Accepted);
    }

    SECTION("Not saved: what went wrong, Copy, and another try") {
        succeeds = false;
        dialog.accept();
        CHECK_FALSE(dialog.isDone());
        CHECK(dialog.isVisible());
        CHECK(dialog.resultText() == QStringLiteral("The archive could not be saved."));
        CHECK(dialog.detailsText() == QStringLiteral("Travels.klh.zip: the disk is full"));
        CHECK(dialog.copyButton()->isVisibleTo(&dialog));
        CHECK(dialog.clipboardText() ==
              QStringLiteral("Export Project Archive\n\nThe archive could not be saved.\n\n"
                             "Travels.klh.zip: the disk is full"));
        CHECK_FALSE(dialog.progressBar()->isVisibleTo(&dialog));
        CHECK(dialog.acceptButton()->text() == QStringLiteral("Export"));
        CHECK(dialog.acceptButton()->isEnabled());
        CHECK(dialog.cancelButton()->isEnabled());
        CHECK(dialog.nameField()->isEnabled());

        // The second try is saved
        succeeds = true;
        dialog.accept();
        CHECK(dialog.isDone());
        CHECK(dialog.detailsText().isEmpty());
        CHECK_FALSE(dialog.copyButton()->isVisibleTo(&dialog));
        CHECK(written == QStringList{path, path});
    }
}

TEST_CASE("Export Project Archive: the window grows for the result", "[gui][dialogs][archive]") {
    QTemporaryDir dir;
    // A long path: the sentence of the result takes more than one line
    const QString folder = QDir(dir.path()).filePath(
        QStringLiteral("The archives of the books of the writer/The archives of this year"));
    ExportArchiveDialog::Book book{QStringLiteral("Travels"), {}, false, 0};
    for (int i = 1; i <= 4; ++i) {
        book.unready.append({QStringLiteral("Chapter %1").arg(i), QStringLiteral("draft"),
                             QStringLiteral("template.chapter")});
    }
    ExportArchiveDialog dialog(book, folder,
                               [](const QString& archive, bool, const ArchiveDialog::Progress&,
                                  QStringList*) {
                                   writeFile(archive, "archive");
                                   return true;
                               });
    dialog.show();
    settle();
    QTreeWidget* list = dialog.unreadyList();
    const int rowsHeight = list->height();
    CHECK(rowsHeight >= list->sizeHint().height());

    dialog.accept();
    settle();
    REQUIRE(dialog.isDone());
    // The list keeps its rows, and the result has room for its lines: the window grows for
    // them as far as the screen allows, and on a lower screen the content scrolls
    CHECK(list->height() == rowsHeight);
    const auto* area = dialog.findChild<gui::FittingScrollArea*>();
    REQUIRE(area != nullptr);
    const int needed = area->contentHeightForWidth(area->viewport()->width());
    const int screenHeight = dialog.screen()->availableGeometry().height();
    const int windowHeight = dialog.frameGeometry().height();
    INFO("The window is " << windowHeight << " high on a screen " << screenHeight
                          << " high; the content needs " << needed << " and has "
                          << area->viewport()->height());
    const bool fits = area->viewport()->height() >= needed;
    const bool asHighAsTheScreen = windowHeight >= screenHeight;
    CHECK((fits || asHighAsTheScreen));
}

TEST_CASE("Export Project Archive: a line under a field is as high as its sentence",
          "[gui][dialogs][archive]") {
    QTemporaryDir dir;
    writeFile(QDir(dir.path()).filePath(QStringLiteral("Travels.klh.zip")), "old");
    ExportArchiveDialog dialog({QStringLiteral("Travels"), {}, false, 0}, dir.path(),
                               [](const QString&, bool, const ArchiveDialog::Progress&,
                                  QStringList*) { return true; });
    dialog.show();
    settle();

    // One line or two, by the font
    const QLabel* replaced = labelWith(
        dialog, QStringLiteral("The folder already has the archive \"Travels.klh.zip\". The "
                               "export replaces it."));
    REQUIRE(replaced != nullptr);
    // The line: the icon and the sentence beside it
    const QWidget* line = replaced->parentWidget();
    CHECK(line->height() == line->heightForWidth(line->width()));

    // More lines
    const QString name = QStringLiteral("Travels of Anna to the north of the country, with the "
                                        "notes of the journey and the letters to her family");
    dialog.nameField()->setText(name);
    settle();
    const QLabel* named = labelWith(
        dialog, QStringLiteral("The archive gets the name \"%1.klh.zip\".").arg(name));
    REQUIRE(named != nullptr);
    line = named->parentWidget();
    CHECK(line->height() == line->heightForWidth(line->width()));
    CHECK(named->height() >= 2 * named->fontMetrics().lineSpacing());

    // One line in any font, after more lines
    dialog.nameField()->clear();
    settle();
    const QLabel* empty = labelWith(dialog, QStringLiteral("Type the name of the archive."));
    REQUIRE(empty != nullptr);
    line = empty->parentWidget();
    CHECK(line->height() == line->heightForWidth(line->width()));
    CHECK(empty->height() < 2 * empty->fontMetrics().lineSpacing());
}

TEST_CASE("Export Project Archive: the backups of the database go when the writer wants them",
          "[gui][dialogs][archive]") {
    QTemporaryDir dir;
    QList<bool> withBackups;
    const auto exporter = [&withBackups](const QString& archive, bool backups,
                                         const ArchiveDialog::Progress&, QStringList*) {
        withBackups << backups;
        writeFile(archive, "archive");
        return true;
    };

    SECTION("A book with backups") {
        ExportArchiveDialog dialog({QStringLiteral("Travels"), {}, false, 3}, dir.path(),
                                   exporter);
        CHECK(dialog.backupsBox()->text() == QStringLiteral("Include the database backups (3)"));
        CHECK(dialog.backupsBox()->isEnabled());
        CHECK_FALSE(dialog.backupsBox()->isChecked());
        CHECK(dialog.backupsNote() ==
              QStringLiteral("The archive takes the copies of the database made when the book "
                             "was closed, so after an import on another computer the book has "
                             "its backups."));

        SECTION("Not checked") {
            dialog.accept();
            CHECK(withBackups == QList<bool>{false});
        }

        SECTION("Checked") {
            dialog.backupsBox()->setChecked(true);
            dialog.accept();
            CHECK(withBackups == QList<bool>{true});
        }
    }

    SECTION("A book without backups") {
        ExportArchiveDialog dialog({QStringLiteral("Travels"), {}, false, 0}, dir.path(),
                                   exporter);
        CHECK(dialog.backupsBox()->text() == QStringLiteral("Include the database backups (0)"));
        CHECK_FALSE(dialog.backupsBox()->isEnabled());
        CHECK(dialog.backupsNote() == QStringLiteral("The book has no backups of its database "
                                                     "yet: Kalahari makes them when the book is "
                                                     "closed."));
        dialog.accept();
        CHECK(withBackups == QList<bool>{false});
    }
}

// =============================================================================
// Import Project Archive
// =============================================================================

TEST_CASE("Import Project Archive: the book of the archive before the import",
          "[gui][dialogs][archive]") {
    QTemporaryDir dir;
    const QString archive = novelArchive(dir);
    const QString place = QDir(dir.path()).filePath(QStringLiteral("Imported"));
    ImportArchiveDialog dialog(builtInTypes(), place,
                               [](const QString&, const QString&, bool,
                                  const ArchiveDialog::Progress&, QStringList*) { return true; });

    // Without an archive, there is nothing to import
    CHECK(dialog.windowTitle() == QStringLiteral("Import Project Archive"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Import"));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK(dialog.archive().isEmpty());
    CHECK(dialog.bookText().isEmpty());
    CHECK(dialog.archiveNote().isEmpty());
    CHECK(dialog.folderNote().isEmpty());
    CHECK(showsText(dialog, QStringLiteral("The progress of the import will show here. When "
                                           "something goes wrong, the description of the error "
                                           "stays in this window, and Copy puts it on the "
                                           "clipboard.")));

    SECTION("The book, read from the archive, and the folder it goes to") {
        dialog.setArchive(archive);
        CHECK(dialog.bookText() == QStringLiteral("Title: Travels\nAuthor: Anna\nType: Novel\n"
                                                  "Contents: 2 elements: a chapter and a title "
                                                  "page"));
        CHECK(dialog.archiveNote().isEmpty());
        CHECK(dialog.folderNote() == QStringLiteral("The book goes to the folder \"Travels\"."));
        CHECK(dialog.bookFolder() == QDir(place).filePath(QStringLiteral("Travels")));
        CHECK(dialog.acceptButton()->isEnabled());
        CHECK_FALSE(QFileInfo::exists(place));  // nothing is unpacked before the import

        // Not where a folder of that name is
        REQUIRE(QDir().mkpath(QDir(place).filePath(QStringLiteral("Travels"))));
        dialog.setFolder(place);
        CHECK(dialog.folderNote() == QStringLiteral("The folder \"Travels\" is already here. "
                                                    "Choose another place for the book."));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        dialog.setFolder(QStringLiteral("Imported"));
        CHECK(dialog.folderNote() == QStringLiteral("Give the full path of the folder."));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
    }

    SECTION("An archive without a book") {
        const QString notes = QDir(dir.path()).filePath(QStringLiteral("Notes.klh.zip"));
        writeArchive(notes, "notes.txt", "notes");
        dialog.setArchive(notes);
        CHECK(dialog.archiveNote() ==
              QStringLiteral("Kalahari cannot read the book of this archive.\n"
                             "Notes.klh.zip: the archive has no .klh file"));
        CHECK(dialog.bookText().isEmpty());
        // Without a book, the window does not say where it goes
        CHECK(dialog.folderNote().isEmpty());
        CHECK_FALSE(dialog.acceptButton()->isEnabled());

        // Another archive replaces it
        dialog.setArchive(archive);
        CHECK(dialog.archiveNote().isEmpty());
        CHECK(dialog.bookText().startsWith(QStringLiteral("Title: Travels\n")));
        CHECK(dialog.folderNote() == QStringLiteral("The book goes to the folder \"Travels\"."));
        CHECK(dialog.acceptButton()->isEnabled());
    }

    SECTION("A book without an author, with a Workshop, of a type that is not installed") {
        QStringList problems;
        std::optional<core::BookProject> project =
            core::ProjectManager::readArchive(archive, &problems);
        REQUIRE(project.has_value());
        project->books.first().author.clear();
        project->type->id = QStringLiteral("acme.saga");
        project->workshop.name = QStringLiteral("Research");
        project->workshop.elements = elements(QStringLiteral("kalahari.base"),
                                              QStringLiteral("character"), 3);
        project->workshop.elements.append(
            element(QStringLiteral("kalahari.base"), QStringLiteral("location"), 1));
        const QString other = QDir(dir.path()).filePath(QStringLiteral("Saga.klh.zip"));
        writeArchive(other, "Saga.klh", QJsonDocument(project->toJson()).toJson());

        dialog.setArchive(other);
        CHECK(dialog.bookText() ==
              QStringLiteral("Title: Travels\nType: acme.saga (not installed)\n"
                             "Contents: 2 elements: a chapter and a title page\n"
                             "Research: 4 elements: 3 characters and a location"));
        CHECK(dialog.folderNote() == QStringLiteral("The book goes to the folder \"Saga\"."));
    }
}

TEST_CASE("Import Project Archive: each row of the book is as high as its sentence",
          "[gui][dialogs][archive]") {
    QTemporaryDir dir;
    const QString archive = novelArchive(dir);
    std::optional<core::BookProject> project = core::ProjectManager::readArchive(archive);
    REQUIRE(project.has_value());
    // A title and an author of more lines, between rows of one line
    const QString title = QStringLiteral("Travels of Anna to the north of the country, with the "
                                         "notes of the journey and the letters to her family");
    const QString author = QStringLiteral("Anna Maria Nowak-Kowalska, with her brother, her "
                                          "sister and the friends she made on the way north");
    project->books.first().title = title;
    project->books.first().author = author;
    const QString other = QDir(dir.path()).filePath(QStringLiteral("Long.klh.zip"));
    writeArchive(other, "Long.klh", QJsonDocument(project->toJson()).toJson());

    ImportArchiveDialog dialog(builtInTypes(), QDir(dir.path()).filePath(QStringLiteral("Books")),
                               [](const QString&, const QString&, bool,
                                  const ArchiveDialog::Progress&, QStringList*) { return true; });
    dialog.show();
    settle();
    dialog.setArchive(other);
    settle();

    for (const QString& text : {title, author, QStringLiteral("Novel"),
                                QStringLiteral("2 elements: a chapter and a title page")}) {
        CAPTURE(text.toStdString());
        const QLabel* value = labelWith(dialog, text);
        REQUIRE(value != nullptr);
        CHECK(value->height() == value->heightForWidth(value->width()));
    }
    const QLabel* titleValue = labelWith(dialog, title);
    CHECK(titleValue->height() >= 2 * titleValue->fontMetrics().lineSpacing());
}

TEST_CASE("Import Project Archive: the backups of the database in the archive",
          "[gui][dialogs][archive]") {
    QTemporaryDir dir;
    const QString archive = novelArchive(dir);
    const std::optional<core::BookProject> project = core::ProjectManager::readArchive(archive);
    REQUIRE(project.has_value());
    const QByteArray manifest = QJsonDocument(project->toJson()).toJson();
    ImportArchiveDialog dialog(builtInTypes(), QDir(dir.path()).filePath(QStringLiteral("Books")),
                               [](const QString&, const QString&, bool,
                                  const ArchiveDialog::Progress&, QStringList*) { return true; });

    const QString two = QDir(dir.path()).filePath(QStringLiteral("Two.klh.zip"));
    writeArchive(two, {{"Two.klh", manifest},
                       {".backups/project_20000101_000000.db", "first"},
                       {".backups/project_20000102_000000.db", "second"}});
    dialog.setArchive(two);
    CHECK(dialog.bookText().endsWith(QStringLiteral("\nBackups: 2 database backups")));
    const QLabel* backups = labelWith(dialog, QStringLiteral("2 database backups"));
    REQUIRE(backups != nullptr);
    CHECK(backups->toolTip() == QStringLiteral("After the import the backups are where "
                                               "Kalahari keeps backups (Settings > Files > "
                                               "Folders)."));

    const QString one = QDir(dir.path()).filePath(QStringLiteral("One.klh.zip"));
    writeArchive(one, {{"One.klh", manifest}, {".backups/project_20000101_000000.db", "first"}});
    dialog.setArchive(one);
    CHECK(dialog.bookText().endsWith(QStringLiteral("\nBackups: 1 database backup")));

    // An archive without backups has no row of them
    dialog.setArchive(archive);
    CHECK_FALSE(dialog.bookText().contains(QStringLiteral("Backups:")));
}

TEST_CASE("Import Project Archive: the contents of a book in a sentence",
          "[gui][dialogs][archive]") {
    const auto contents = [](const QList<core::ProjectElement>& list) {
        return ImportArchiveDialog::contentsOf(builtInTypes(), pointers(list)).toStdString();
    };
    const QString base = QStringLiteral("kalahari.base");
    const QString novel = QStringLiteral("kalahari.novel");

    CHECK(contents({}) == "no elements");
    CHECK(contents(elements(base, QStringLiteral("chapter"), 1)) == "1 chapter");
    CHECK(contents(elements(base, QStringLiteral("chapter"), 5)) == "5 chapters");

    // The kinds of more elements first; of as many, in the order of their first elements
    QList<core::ProjectElement> book{element(novel, QStringLiteral("prologue"), 1)};
    book.append(elements(base, QStringLiteral("chapter"), 5));
    book.append(element(novel, QStringLiteral("epilogue"), 1));
    book.append(element(base, QStringLiteral("title_page"), 1));
    CHECK(contents(book) == "8 elements: 5 chapters, a prologue, an epilogue, and a title page");

    SECTION("In Polish, with the forms of the numbers") {
        ContentsTranslator translator;
        QCoreApplication::installTranslator(&translator);
        gui::setProgramLanguage(QStringLiteral("pl"));
        struct Restore {
            QTranslator* translator;
            ~Restore() {
                QCoreApplication::removeTranslator(translator);
                gui::setProgramLanguage(QStringLiteral("en"));
            }
        } restore{&translator};

        CHECK(contents({}) == "brak elementów");
        CHECK(contents(elements(base, QStringLiteral("chapter"), 1)) == "1 rozdział");
        CHECK(contents(elements(base, QStringLiteral("chapter"), 3)) == "3 rozdziały");
        CHECK(contents(elements(base, QStringLiteral("chapter"), 12)) == "12 rozdziałów");
        CHECK(contents(elements(base, QStringLiteral("chapter"), 22)) == "22 rozdziały");
        CHECK(contents(book) == "8 elementów: 5 rozdziałów, prolog, epilog i strona tytułowa");
        QList<core::ProjectElement> poems = elements(base, QStringLiteral("chapter"), 2);
        poems.append(element(base, QStringLiteral("title_page"), 1));
        CHECK(contents(poems) == "3 elementy: 2 rozdziały i strona tytułowa");
    }
}

TEST_CASE("Import Project Archive: the open book, the progress and the result",
          "[gui][dialogs][archive]") {
    QTemporaryDir dir;
    const QString archive = novelArchive(dir);
    const QString place = QDir(dir.path()).filePath(QStringLiteral("Imported"));
    const QString bookFolder = QDir(place).filePath(QStringLiteral("Travels"));
    struct Call {
        QString archive;
        QString folder;
        bool open = false;
    };
    QList<Call> calls;
    bool succeeds = true;
    ImportArchiveDialog* shown = nullptr;
    ImportArchiveDialog dialog(
        builtInTypes(), place,
        [&](const QString& file, const QString& folder, bool open,
            const ArchiveDialog::Progress& progress, QStringList* problems) {
            calls.append({file, folder, open});
            progress(50);
            CHECK(shown->progressBar()->value() == 50);
            CHECK_FALSE(shown->acceptButton()->isEnabled());
            shown->reject();
            CHECK(shown->isVisible());
            if (!succeeds) {
                // A folder made before the failure stays in the way of the next try
                CHECK(QDir().mkpath(bookFolder));
                problems->append(QStringLiteral("Travels.klh: not valid JSON"));
                return false;
            }
            CHECK(QDir().mkpath(bookFolder));
            return true;
        });
    shown = &dialog;
    dialog.setArchive(archive);
    dialog.show();

    QStringList asked;
    ImportArchiveDialog::Opening answer = ImportArchiveDialog::Opening::Open;
    dialog.setOpeningQuestion([&asked, &answer](const QString& title) {
        asked << title;
        return answer;
    });

    SECTION("Cancel returns to the window as it was") {
        answer = ImportArchiveDialog::Opening::Cancel;
        dialog.accept();
        CHECK(asked == QStringList{QStringLiteral("Travels")});
        CHECK(calls.isEmpty());
        CHECK_FALSE(dialog.isDone());
        CHECK(dialog.resultText().isEmpty());
        CHECK(dialog.acceptButton()->isEnabled());
        CHECK(dialog.cancelButton()->isEnabled());
        CHECK_FALSE(dialog.progressBar()->isVisibleTo(&dialog));
    }

    SECTION("The imported book opens in the place of the open one") {
        FileManager fileManager;
        dialog.accept();
        REQUIRE(calls.size() == 1);
        CHECK(calls.first().archive == archive);
        CHECK(calls.first().folder == place);
        CHECK(calls.first().open);
        CHECK(dialog.isDone());
        CHECK(dialog.resultText() ==
              QStringLiteral("Imported \"Travels\" to the folder %1. The book is open.")
                  .arg(QDir::toNativeSeparators(bookFolder)));
        CHECK(dialog.acceptButton()->text() == QStringLiteral("Close"));
        dialog.showResult();
        CHECK(fileManager.shown == QStringList{bookFolder});
    }

    SECTION("The imported book does not open") {
        answer = ImportArchiveDialog::Opening::DontOpen;
        dialog.accept();
        REQUIRE(calls.size() == 1);
        CHECK_FALSE(calls.first().open);
        CHECK(dialog.resultText() ==
              QStringLiteral("Imported \"Travels\" to the folder %1. The book is on the "
                             "Dashboard and in File > Recent Books.")
                  .arg(QDir::toNativeSeparators(bookFolder)));
    }

    SECTION("Not imported: what went wrong, and the fields for another try") {
        succeeds = false;
        dialog.accept();
        CHECK_FALSE(dialog.isDone());
        CHECK(dialog.resultText() == QStringLiteral("The archive could not be imported."));
        CHECK(dialog.detailsText() == QStringLiteral("Travels.klh: not valid JSON"));
        CHECK(dialog.copyButton()->isVisibleTo(&dialog));
        CHECK(dialog.clipboardText() ==
              QStringLiteral("Import Project Archive\n\nThe archive could not be imported.\n\n"
                             "Travels.klh: not valid JSON"));
        CHECK(dialog.folderNote() == QStringLiteral("The folder \"Travels\" is already here. "
                                                    "Choose another place for the book."));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        CHECK(dialog.cancelButton()->isEnabled());
    }
}
