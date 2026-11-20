// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_export_book_dialog.cpp
/// @brief File > Export > DOCX, PDF, Markdown and EPUB: the window of the export, and the book it
///        exports as the program has it (the open texts with their unsaved changes)

#include <catch2/catch_test_macros.hpp>
#include "kalahari_tests/editor/editor_test_utils.h"
#include "test_support/dialog_checks.h"
#include "kalahari_tests/gui/document_window.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/extension_points.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/book_export.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/book_export_coordinator.h"
#include "kalahari/gui/dialogs/archive_dialogs.h"
#include "kalahari/gui/dialogs/export_book_dialog.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/utils/file_location.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFileInfo>
#include <QGuiApplication>
#include <QGroupBox>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTranslator>
#include <QTreeWidget>
#include <QUuid>

#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/// Qt's own way to press a key over the window's shortcuts, as the system delivers it (Qt
/// Test presses keys with it; the program does not link Qt Test)
Q_GUI_EXPORT bool qt_sendShortcutOverrideEvent(QObject* o, ulong timestamp, int k,
                                               Qt::KeyboardModifiers mods,
                                               const QString& text = QString(),
                                               bool autorep = false, ushort count = 1);

using namespace kalahari;
using gui::BookExportCoordinator;
using gui::dialogs::ExportBookDialog;
using test::AppFontOfPixels;
using test::checkFitsSmallScreen;
using test::onItsTab;
using test::OpenNovel;
using test::Polish;
using test::Window;

namespace {

/// The settings of a test, as they were before it
class SettingsKept {
public:
    SettingsKept() {
        auto& settings = core::SettingsManager::getInstance();
        for (const char* key : {"export.content", "export.font", "editor.page.size",
                                "ui.lengthUnit", "export.runningHead", "export.pdf.purpose",
                                "export.epub.look", "export.epub.cover"}) {
            m_strings.emplace_back(key, settings.get<std::string>(key));
        }
        for (const char* key : {"export.titles", "export.newPage", "export.pageNumbers",
                                "export.comments", "export.copyImages",
                                "editor.margins.mirrorEnabled", "export.pdf.comments",
                                "export.pdf.bookmarks", "export.pdf.facingPages",
                                "export.pdf.grayscale", "export.epub.embedFont"}) {
            m_flags.emplace_back(key, settings.get<bool>(key));
        }
        for (const char* key : {"editor.margins.pageInner", "editor.margins.pageOuter"}) {
            m_numbers.emplace_back(key, settings.get<double>(key));
        }
    }
    ~SettingsKept() {
        try {
            auto& settings = core::SettingsManager::getInstance();
            for (const auto& [key, value] : m_strings) {
                settings.set<std::string>(key, value);
            }
            for (const auto& [key, value] : m_flags) {
                settings.set<bool>(key, value);
            }
            for (const auto& [key, value] : m_numbers) {
                settings.set<double>(key, value);
            }
        } catch (...) {
            std::fputs("SettingsKept: the settings of the test stay\n", stderr);
        }
    }
    SettingsKept(const SettingsKept&) = delete;
    SettingsKept& operator=(const SettingsKept&) = delete;

private:
    std::vector<std::pair<std::string, std::string>> m_strings;
    std::vector<std::pair<std::string, bool>> m_flags;
    std::vector<std::pair<std::string, double>> m_numbers;
};

/// A book of the window: a dedication in the front, a part of two chapters, an epilogue
ExportBookDialog::Book sampleBook() {
    using Row = ExportBookDialog::Row;
    ExportBookDialog::Book book;
    book.title = QStringLiteral("Wind over the Oasis");
    book.rows = {
        Row{QString(), QStringLiteral("Front Matter"), QString(), QString(), 0, false},
        Row{QStringLiteral("d"), QStringLiteral("Dedication"), QStringLiteral("final"), QString(), 1, true},
        Row{QString(), QStringLiteral("Body"), QString(), QString(), 0, false},
        Row{QStringLiteral("p"), QStringLiteral("Part One"), QString(), QString(), 1, false},
        Row{QStringLiteral("c1"), QStringLiteral("The Caravan"), QStringLiteral("final"), QString(), 2, true},
        Row{QStringLiteral("c2"), QStringLiteral("The Well"), QStringLiteral("draft"), QString(), 2, true},
        Row{QStringLiteral("e"), QStringLiteral("Epilogue"), QStringLiteral("revision"), QString(), 1, true},
    };
    book.frontTitle = QStringLiteral("The Well");
    book.frontId = QStringLiteral("c2");
    return book;
}

/// The item of the list with title @p title
QTreeWidgetItem* itemOf(const QTreeWidget& list, const QString& title) {
    const QList<QTreeWidgetItem*> found =
        list.findItems(title, Qt::MatchExactly | Qt::MatchRecursive);
    REQUIRE(found.size() == 1);
    return found.first();
}

/// A program in place of the system's, which opens the file of the result
struct FileOpener {
    QStringList opened;

    FileOpener() {
        gui::setFileOpenerForTests([this](const QString& path) {
            opened << path;
            return true;
        });
    }
    ~FileOpener() { gui::setFileOpenerForTests({}); }
    FileOpener(const FileOpener&) = delete;
    FileOpener& operator=(const FileOpener&) = delete;
};

/// Type @p text as the name of the file, as the writer does
void typeName(ExportBookDialog& dialog, const QString& text) {
    dialog.nameField()->setText(text);
    emit dialog.nameField()->textEdited(text);
}

/// The choice of the whole book with the options of the settings
ExportBookDialog::Choice wholeBook() {
    ExportBookDialog::Choice choice;
    choice.content = ExportBookDialog::Content::Book;
    return choice;
}

/// An exporter of a plugin: an OpenDocument format with a tab of options, and a format without
/// one; the formats of the program's ids stay the program's
class OdtExporter : public core::IExporter {
public:
    [[nodiscard]] std::string getPluginId() const override { return "test.odt-exporter"; }
    [[nodiscard]] std::string getVersion() const override { return "1.0.0"; }
    void onInit() override {}
    void onActivate() override {}
    [[nodiscard]] std::vector<core::ExportFormatInfo> getFormats() const override {
        return {{"odt", "OpenDocument text", "odt"}, {"txt", "Plain text", "txt"},
                {"docx", "Another Word", "docx"}, {"pdf", "Another PDF", "pdf"},
                {"", "No id", "x"}};
    }
    QWidget* createOptionsPage(const std::string& format, QWidget* parent) override {
        if (format != "odt") {
            return nullptr;
        }
        auto* page = new QWidget(parent);
        m_tagged = new QCheckBox(QStringLiteral("Flat XML"), page);
        // The window deletes the page; the exporter forgets its check box with it
        QObject::connect(page, &QObject::destroyed, [this] { m_tagged = nullptr; });
        return page;
    }
    bool exportDocument(const std::string& format, const std::string& filepath) override {
        written.emplace_back(format, filepath);
        tagged = m_tagged != nullptr && m_tagged->isChecked();
        return format == "odt";
    }

    std::vector<std::pair<std::string, std::string>> written;
    bool tagged = false;

private:
    QCheckBox* m_tagged = nullptr;
};

/// The exporter registered while it lives
class RegisteredExporter {
public:
    RegisteredExporter() : exporter(std::make_shared<OdtExporter>()) {
        core::ExtensionPointRegistry::getInstance().registerPlugin(exporter);
    }
    ~RegisteredExporter() {
        core::ExtensionPointRegistry::getInstance().unregisterPlugin(exporter->getPluginId());
    }
    RegisteredExporter(const RegisteredExporter&) = delete;
    RegisteredExporter& operator=(const RegisteredExporter&) = delete;

    std::shared_ptr<OdtExporter> exporter;
};

}  // namespace

TEST_CASE("Export Book: the format, the name and the options of each format",
          "[gui][dialogs][export]") {
    SettingsKept kept;
    QTemporaryDir dir;
    ExportBookDialog dialog(sampleBook(), editor::ExportFormat::Docx, dir.path(), {});

    // The command of the menu chose the format, in the list of the program's formats in the
    // order of File > Export; the name is the title of the book
    REQUIRE(dialog.formatBox()->count() >= 4);
    CHECK(dialog.formatBox()->itemText(0) == QStringLiteral("Word document (.docx)"));
    CHECK(dialog.formatBox()->itemText(1) == QStringLiteral("PDF document (.pdf)"));
    CHECK(dialog.formatBox()->itemText(2) == QStringLiteral("Markdown (.md)"));
    CHECK(dialog.formatBox()->itemText(3) == QStringLiteral("EPUB e-book (.epub)"));
    CHECK(dialog.formatBox()->currentData().toString() == QStringLiteral("docx"));
    CHECK(dialog.choice().formatId == QStringLiteral("docx"));
    CHECK(dialog.nameField()->text() == QStringLiteral("Wind over the Oasis.docx"));
    CHECK(dialog.filePath() ==
          QDir(dir.path()).filePath(QStringLiteral("Wind over the Oasis.docx")));
    // Word: its pages and font, and its comments; the group of the images says that the
    // document has them
    CHECK(onItsTab(dialog.newPageBox()));
    CHECK(onItsTab(dialog.fontBox()));
    CHECK(onItsTab(dialog.commentsBox()));
    CHECK(onItsTab(dialog.imagesGroup()));
    CHECK_FALSE(onItsTab(dialog.copyImagesBox()));
    CHECK(dialog.acceptButton()->isEnabled());

    // Markdown: its extension, the titles as headings, its images and no comments
    dialog.setFormat(editor::ExportFormat::Markdown);
    CHECK(dialog.nameField()->text() == QStringLiteral("Wind over the Oasis.md"));
    CHECK(onItsTab(dialog.titlesBox()));
    CHECK_FALSE(onItsTab(dialog.newPageBox()));
    CHECK_FALSE(onItsTab(dialog.fontBox()));
    CHECK(onItsTab(dialog.notesGroup()));
    CHECK_FALSE(onItsTab(dialog.commentsBox()));
    CHECK(onItsTab(dialog.copyImagesBox()));
    CHECK(dialog.imagesFolderName() == QStringLiteral("Wind over the Oasis – images"));
    CHECK(dialog.choice().imagesFolder == QStringLiteral("Wind over the Oasis – images"));
    CHECK(dialog.copyImagesBox()->text() ==
          QStringLiteral("Copy the &images to the folder \"Wind over the Oasis – images\" next to "
                         "the file"));

    // A name typed without its extension gets it, and keeps it through a change of format
    typeName(dialog, QStringLiteral("Draft"));
    CHECK(dialog.nameNote() == QStringLiteral("The file gets the name \"Draft.md\"."));
    dialog.setFormat(editor::ExportFormat::Docx);
    CHECK(dialog.nameField()->text() == QStringLiteral("Draft.docx"));
    CHECK(dialog.nameNote().isEmpty());

    // A file there already: the export replaces it
    QFile existing(QDir(dir.path()).filePath(QStringLiteral("Draft.docx")));
    REQUIRE(existing.open(QIODevice::WriteOnly));
    existing.close();
    dialog.setFolder(dir.path());  // as Browse... does
    CHECK(dialog.nameNote() ==
          QStringLiteral("The folder already has the file \"Draft.docx\". The export replaces it."));
    CHECK(dialog.acceptButton()->isEnabled());

    // A name that cannot be the name of a file
    typeName(dialog, QStringLiteral("Draft: 2"));
    CHECK(dialog.nameNote() == QStringLiteral("A name cannot have the characters \\ / : * ? \" < > |."));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    typeName(dialog, QStringLiteral(".docx"));
    CHECK(dialog.nameNote() == QStringLiteral("Type the name of the file."));
    CHECK(dialog.filePath().isEmpty());
}

TEST_CASE("Export Book: the whole book, the chosen texts or the text in front",
          "[gui][dialogs][export]") {
    SettingsKept kept;
    core::SettingsManager::getInstance().set<std::string>("export.content", "book");
    QTemporaryDir dir;
    ExportBookDialog dialog(sampleBook(), editor::ExportFormat::Docx, dir.path(), {});

    CHECK(dialog.bookButton()->isChecked());
    CHECK(dialog.bookButton()->text() == QStringLiteral("Whole &book (4 texts)"));
    CHECK(dialog.frontButton()->text() == QStringLiteral("&Text in front: \"The Well\""));
    // The list stays in its place, and works with the chosen texts
    CHECK(onItsTab(dialog.textList()));
    CHECK_FALSE(dialog.textList()->isEnabled());
    CHECK(dialog.choice().content == ExportBookDialog::Content::Book);

    SECTION("The chosen texts: the list with their sections and parts") {
        dialog.setContent(ExportBookDialog::Content::Chosen);
        CHECK(dialog.textList()->isEnabled());
        CHECK(dialog.chosenNote() == QStringLiteral("4 of 4 texts chosen."));
        QTreeWidget& list = *dialog.textList();
        CHECK(list.topLevelItemCount() == 2);
        CHECK(itemOf(list, QStringLiteral("The Caravan"))->parent() ==
              itemOf(list, QStringLiteral("Part One")));
        CHECK(itemOf(list, QStringLiteral("The Well"))->text(1) == QStringLiteral("Draft"));

        // Only the final texts; a part follows its texts
        dialog.checkFinal();
        CHECK(dialog.chosenNote() == QStringLiteral("2 of 4 texts chosen."));
        CHECK(dialog.choice().chosen ==
              QStringList({QStringLiteral("d"), QStringLiteral("c1")}));
        CHECK(itemOf(list, QStringLiteral("Part One"))->checkState(0) == Qt::PartiallyChecked);

        // A part checked checks its texts
        itemOf(list, QStringLiteral("Part One"))->setCheckState(0, Qt::Checked);
        CHECK(dialog.choice().chosen == QStringList({QStringLiteral("d"), QStringLiteral("c1"),
                                                     QStringLiteral("c2")}));

        // Without a text the export waits
        dialog.checkNone();
        CHECK(dialog.chosenNote() == QStringLiteral("0 of 4 texts chosen."));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        dialog.checkAll();
        CHECK(dialog.acceptButton()->isEnabled());
        CHECK(dialog.choice().chosen.size() == 4);
    }

    SECTION("The text in front: its name, until the writer types one") {
        dialog.setContent(ExportBookDialog::Content::Front);
        CHECK(dialog.nameField()->text() == QStringLiteral("The Well.docx"));
        CHECK(dialog.choice().content == ExportBookDialog::Content::Front);
        dialog.setContent(ExportBookDialog::Content::Book);
        CHECK(dialog.nameField()->text() == QStringLiteral("Wind over the Oasis.docx"));

        typeName(dialog, QStringLiteral("Mine.docx"));
        dialog.setContent(ExportBookDialog::Content::Front);
        CHECK(dialog.nameField()->text() == QStringLiteral("Mine.docx"));
    }
}

TEST_CASE("Export Book: without a book only the text in front", "[gui][dialogs][export]") {
    SettingsKept kept;
    QTemporaryDir dir;
    ExportBookDialog::Book book;
    book.frontTitle = QStringLiteral("Notes");
    ExportBookDialog dialog(book, editor::ExportFormat::Markdown, dir.path(), {});
    CHECK(dialog.frontButton()->isChecked());
    CHECK_FALSE(onItsTab(dialog.bookButton()));
    CHECK_FALSE(onItsTab(dialog.chosenButton()));
    CHECK_FALSE(onItsTab(dialog.textList()));
    CHECK(dialog.nameField()->text() == QStringLiteral("Notes.md"));
}

TEST_CASE("Export Book: the set for a publisher, its fourth source", "[gui][dialogs][export]") {
    const SettingsKept kept;
    core::SettingsManager::getInstance().set<std::string>("export.content", "book");
    core::SettingsManager::getInstance().set<bool>("export.newPage", true);
    const QTemporaryDir dir;
    ExportBookDialog::Book book = sampleBook();
    {
        // A set without a text is not there
        const ExportBookDialog none(book, editor::ExportFormat::Docx, dir.path(), {});
        CHECK_FALSE(onItsTab(none.setButton()));
    }
    book.setTitle = QStringLiteral("Wind over the Oasis – set for the publisher");
    book.setParts = 3;
    QStringList written;
    ExportBookDialog::Choice used;
    ExportBookDialog dialog(book, editor::ExportFormat::Docx, dir.path(),
                            [&](const QString& path, const ExportBookDialog::Choice& choice,
                                const gui::dialogs::ArchiveDialog::Progress&,
                                editor::ExportResult* result, editor::ExportProblem*) {
                                written << QFileInfo(path).fileName();
                                used = choice;
                                result->file = path;
                                return true;
                            });
    CHECK(onItsTab(dialog.setButton()));
    CHECK(dialog.setButton()->text() == QStringLiteral("Set for the &publisher (3 parts)"));
    CHECK(dialog.bookButton()->isChecked());

    // The set: one text with its own name; its parts follow one another on the pages
    dialog.setContent(ExportBookDialog::Content::Set);
    CHECK(dialog.choice().content == ExportBookDialog::Content::Set);
    CHECK(dialog.nameField()->text() ==
          QStringLiteral("Wind over the Oasis – set for the publisher.docx"));
    CHECK(dialog.headingDescription() ==
          QStringLiteral("Saves the text \"Wind over the Oasis – set for the publisher\" as a "
                         "Word document."));
    CHECK_FALSE(dialog.textList()->isEnabled());
    CHECK_FALSE(dialog.newPageBox()->isEnabled());
    CHECK_FALSE(dialog.choice().newPage);
    dialog.setFormat(editor::ExportFormat::Markdown);
    CHECK(dialog.nameField()->text() ==
          QStringLiteral("Wind over the Oasis – set for the publisher.md"));

    // The next window opens with the book again, each text from a new page
    dialog.accept();
    CHECK(written == QStringList{QStringLiteral("Wind over the Oasis – set for the publisher.md")});
    CHECK(used.content == ExportBookDialog::Content::Set);
    const auto& settings = core::SettingsManager::getInstance();
    CHECK(settings.get<std::string>("export.content") == "book");
    CHECK(settings.get<bool>("export.newPage"));
    const ExportBookDialog next(book, editor::ExportFormat::Docx, dir.path(), {});
    CHECK(next.bookButton()->isChecked());
    CHECK(next.newPageBox()->isEnabled());
}

TEST_CASE("Export Book: the formats of the plugins in the list, with their own tabs",
          "[gui][dialogs][export]") {
    SettingsKept kept;
    QTemporaryDir dir;
    RegisteredExporter registered;

    // The program's formats first; a plugin's format needs an id and does not take the id of
    // another
    QStringList names;
    for (const ExportBookDialog::Format& format : ExportBookDialog::formats()) {
        names << format.name;
    }
    CHECK(names == QStringList({QStringLiteral("Word document (.docx)"),
                                QStringLiteral("PDF document (.pdf)"),
                                QStringLiteral("Markdown (.md)"),
                                QStringLiteral("EPUB e-book (.epub)"),
                                QStringLiteral("OpenDocument text (.odt)"),
                                QStringLiteral("Plain text (.txt)")}));
    CHECK(ExportBookDialog::exporterOf(QStringLiteral("odt")) == registered.exporter);
    CHECK(ExportBookDialog::exporterOf(QStringLiteral("docx")) == nullptr);
    CHECK(ExportBookDialog::exporterOf(QStringLiteral("pdf")) == nullptr);

    ExportBookDialog dialog(sampleBook(), editor::ExportFormat::Docx, dir.path(), {});
    QWidget* odtPage = dialog.formatPage(QStringLiteral("odt"));
    REQUIRE(odtPage != nullptr);
    CHECK(dialog.formatPage(QStringLiteral("txt")) == nullptr);
    const int odtTab = dialog.tabs()->indexOf(odtPage);
    const int extrasTab = dialog.tabs()->indexOf(dialog.notesGroup()->parentWidget());
    CHECK(dialog.tabs()->tabText(odtTab) == QStringLiteral("OpenDocument text"));
    CHECK_FALSE(dialog.tabs()->isTabVisible(odtTab));

    // OpenDocument: its extension, its tab, and the layout of all the formats; the options of
    // the program's formats go
    dialog.setFormatId(QStringLiteral("odt"));
    CHECK(dialog.choice().formatId == QStringLiteral("odt"));
    CHECK(dialog.nameField()->text() == QStringLiteral("Wind over the Oasis.odt"));
    CHECK(dialog.tabs()->isTabVisible(odtTab));
    CHECK_FALSE(dialog.tabs()->isTabVisible(extrasTab));
    CHECK_FALSE(dialog.tabs()->isTabVisible(dialog.tabs()->indexOf(dialog.pdfTab())));
    CHECK(onItsTab(dialog.titlesBox()));
    CHECK_FALSE(onItsTab(dialog.newPageBox()));
    CHECK_FALSE(onItsTab(dialog.embedFontBox()));

    // Plain text: no tab of its own
    dialog.setFormatId(QStringLiteral("txt"));
    CHECK(dialog.nameField()->text() == QStringLiteral("Wind over the Oasis.txt"));
    CHECK_FALSE(dialog.tabs()->isTabVisible(odtTab));

    // Back to Word: its options, and no tab of the plugin
    dialog.setFormat(editor::ExportFormat::Docx);
    CHECK(dialog.nameField()->text() == QStringLiteral("Wind over the Oasis.docx"));
    CHECK(dialog.tabs()->isTabVisible(extrasTab));
    CHECK_FALSE(dialog.tabs()->isTabVisible(odtTab));

    // The plugin writes the file with the options of its tab
    odtPage->findChild<QCheckBox*>()->setChecked(true);
    const QString path = QDir(dir.path()).filePath(QStringLiteral("book.odt"));
    int progress = 0;
    editor::ExportResult result;
    editor::ExportProblem problem;
    CHECK(BookExportCoordinator::exportWithPlugin(
        *registered.exporter, QStringLiteral("odt"), path,
        [&progress](int percent) { progress = percent; }, &result, &problem));
    CHECK(registered.exporter->written ==
          std::vector<std::pair<std::string, std::string>>{{"odt", path.toStdString()}});
    CHECK(registered.exporter->tagged);
    CHECK(progress == 100);
    CHECK(result.file == path);

    // A file the plugin does not write: what went wrong, for Copy
    CHECK_FALSE(BookExportCoordinator::exportWithPlugin(
        *registered.exporter, QStringLiteral("txt"), path, {}, &result, &problem));
    CHECK(problem.kind == editor::ExportProblem::Kind::Other);
    CHECK(problem.lines ==
          QStringList{QStringLiteral("test.odt-exporter: the plugin did not write %1 in the "
                                     "format txt")
                          .arg(path)});
}

TEST_CASE("Export Book: the tabs of each format, the window whole on a small screen, the access "
          "keys",
          "[gui][dialogs][export]") {
    SettingsKept kept;
    QTemporaryDir dir;
    // A book of more texts than the list shows, with a long title of the text in front
    ExportBookDialog::Book book = sampleBook();
    for (int i = 0; i < 30; ++i) {
        book.rows.append(ExportBookDialog::Row{QStringLiteral("x%1").arg(i),
                                               QStringLiteral("Chapter %1").arg(i + 3),
                                               QStringLiteral("draft"), QString(), 2, true});
    }
    book.frontTitle = QStringLiteral("The Well, where the caravan waited three days for the wind");
    // With the set for a publisher beside the whole book and the chosen texts
    book.setTitle = QStringLiteral("Wind over the Oasis – set for the publisher");
    book.setParts = 4;

    // The tabs shown, the keys of the window, and its size with each tab in front
    const auto check = [&](editor::ExportFormat format) {
        ExportBookDialog dialog(book, format, dir.path(), {});
        CHECK(dialog.tabs()->currentIndex() == 0);
        INFO(test::keyProblems(dialog, *dialog.tabs()).toStdString());
        CHECK(test::keyProblems(dialog, *dialog.tabs()).isEmpty());
        QStringList shown;
        for (int tab = 0; tab < dialog.tabs()->count(); ++tab) {
            if (!dialog.tabs()->isTabVisible(tab)) {
                continue;
            }
            shown << dialog.tabs()->tabText(tab);
            dialog.tabs()->setCurrentIndex(tab);
            INFO("The tab " << dialog.tabs()->tabText(tab).toStdString());
            checkFitsSmallScreen(dialog);
        }
        dialog.setContent(ExportBookDialog::Content::Chosen);
        checkFitsSmallScreen(dialog);
        return shown.join(QStringLiteral(" | "));
    };

    SECTION("In English") {
        CHECK(check(editor::ExportFormat::Docx) ==
              QStringLiteral("Fi&le | Text&s | La&yout | Annotations and Ima&ges"));
        CHECK(check(editor::ExportFormat::Pdf) ==
              QStringLiteral("Fi&le | Text&s | La&yout | Annotations and Ima&ges | P&DF"));
        check(editor::ExportFormat::Markdown);
        CHECK(
            check(editor::ExportFormat::Epub) ==
            QStringLiteral("Fi&le | Text&s | La&yout | Annotations and Ima&ges | Cover an&d Data"));
    }

    SECTION("In Polish") {
        Polish polish;
        REQUIRE(polish.loaded());
        CHECK(check(editor::ExportFormat::Docx) ==
              QStringLiteral("&Plik | T&eksty | U&kład | Uwagi i o&brazy"));
        CHECK(check(editor::ExportFormat::Pdf) ==
              QStringLiteral("&Plik | T&eksty | U&kład | Uwagi i o&brazy | P&DF"));
        check(editor::ExportFormat::Markdown);
        CHECK(check(editor::ExportFormat::Epub) ==
              QStringLiteral("&Plik | T&eksty | U&kład | Uwagi i o&brazy | Okła&dka i dane"));
    }

    // Alt and the key of a tab bring it to the front: of the two with the same key, the one of
    // the format chosen. The keys are shortcuts of the window: they need an active window,
    // which the offscreen platform has without a window on screen
    SECTION("The key of the tab of the format") {
        if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
            SKIP("needs an active window without a window on screen: run with "
                 "QT_QPA_PLATFORM=offscreen");
        }
        ExportBookDialog dialog(book, editor::ExportFormat::Pdf, dir.path(), {});
        dialog.show();
        dialog.activateWindow();
        REQUIRE(test::waitUntil([&dialog] { return dialog.isActiveWindow(); }));
        CHECK(qt_sendShortcutOverrideEvent(dialog.nameField(), 0, Qt::Key_D, Qt::AltModifier));
        CHECK(test::waitUntil(
            [&dialog] { return dialog.tabs()->currentWidget() == dialog.pdfTab(); }));
        dialog.setFormat(editor::ExportFormat::Epub);
        dialog.tabs()->setCurrentIndex(0);
        CHECK(qt_sendShortcutOverrideEvent(dialog.nameField(), 0, Qt::Key_D, Qt::AltModifier));
        CHECK(test::waitUntil(
            [&dialog] { return dialog.tabs()->currentWidget() == dialog.epubTab(); }));
    }

    // The result of the export shows under the tabs, in the place of the bar
    const auto checkResult = [&]() {
        ExportBookDialog dialog(
            book, editor::ExportFormat::Docx, dir.path(),
            [](const QString& path, const ExportBookDialog::Choice&,
               const gui::dialogs::ArchiveDialog::Progress&, editor::ExportResult* result,
               editor::ExportProblem*) {
                result->file = path;
                result->missingImages = 2;
                return true;
            });
        dialog.tabs()->setCurrentIndex(2);
        dialog.accept();
        REQUIRE(dialog.isDone());
        CHECK(dialog.openLink()->isVisibleTo(&dialog));
        CHECK_FALSE(dialog.progressBar()->isVisibleTo(&dialog));
        checkFitsSmallScreen(dialog);
    };

    SECTION("With the result of the export, in English") {
        checkResult();
    }

    SECTION("With the result of the export, in Polish") {
        Polish polish;
        REQUIRE(polish.loaded());
        checkResult();
    }

    // The width of the window follows the letters of the font, so its sentences take as many
    // lines in any font; a font of higher lines makes it only as much higher
    SECTION("In a font of 13 pixels, in Polish") {
        AppFontOfPixels font(13);
        Polish polish;
        REQUIRE(polish.loaded());
        check(editor::ExportFormat::Docx);
        check(editor::ExportFormat::Pdf);
        check(editor::ExportFormat::Epub);
        checkResult();
    }
}

TEST_CASE("Export Book: the page of the settings", "[gui][dialogs][export]") {
    SettingsKept kept;
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>("ui.lengthUnit", "cm");
    settings.set<std::string>("editor.page.size", "A5");
    settings.set<bool>("editor.margins.mirrorEnabled", true);
    settings.set<double>("editor.margins.pageInner", 30.0);
    settings.set<double>("editor.margins.pageOuter", 20.0);
    const auto cm = [](double millimeters) {
        return gui::lengthText(millimeters, gui::LengthUnit::Millimeters);
    };
    CHECK(ExportBookDialog::pageText() ==
          QStringLiteral("A5, mirror margins: inner %1, outer %2").arg(cm(30.0), cm(20.0)));

    // The window follows the settings the gear opens
    QTemporaryDir dir;
    ExportBookDialog dialog(sampleBook(), editor::ExportFormat::Docx, dir.path(), {});
    settings.set<bool>("editor.margins.mirrorEnabled", false);
    QApplication::processEvents();
    CHECK(dialog.shownPageText().startsWith(QStringLiteral("A5, margins: left ")));

    // The book of the export has the page of the settings
    const editor::ExportPage page = BookExportCoordinator::pageOfSettings();
    CHECK(page.width == 148.0);
    CHECK(page.height == 210.0);
    CHECK_FALSE(page.mirrored);
}

TEST_CASE("Export Book: the result, Open File, and the choices kept",
          "[gui][dialogs][export]") {
    SettingsKept kept;
    QTemporaryDir dir;
    const QString folder = QDir(dir.path()).filePath(QStringLiteral("Export"));
    bool succeeds = true;
    QStringList written;
    ExportBookDialog::Choice used;
    ExportBookDialog dialog(
        sampleBook(), editor::ExportFormat::Docx, folder,
        [&](const QString& path, const ExportBookDialog::Choice& choice,
            const gui::dialogs::ArchiveDialog::Progress& progress, editor::ExportResult* result,
            editor::ExportProblem* problem) {
            written << path;
            used = choice;
            progress(50);
            if (!succeeds) {
                problem->kind = editor::ExportProblem::Kind::CannotWrite;
                problem->lines << QStringLiteral("book.docx: Permission denied");
                return false;
            }
            QFile file(path);
            REQUIRE(file.open(QIODevice::WriteOnly));
            result->file = path;
            result->missingImages = 1;
            return true;
        });
    dialog.show();
    dialog.titlesBox()->setChecked(false);
    dialog.commentsBox()->setChecked(true);
    dialog.fontBox()->setCurrentIndex(dialog.fontBox()->findData(QStringLiteral("Georgia")));
    const QString path = QDir(folder).filePath(QStringLiteral("Wind over the Oasis.docx"));

    SECTION("Saved: the result with Open File and Show in Folder") {
        FileOpener opener;
        dialog.accept();
        CHECK(written == QStringList{path});
        CHECK(dialog.isDone());
        CHECK(dialog.resultText() ==
              QStringLiteral("Saved the file %1. The files of 1 images were not found: the file "
                             "does not have them.")
                  .arg(QDir::toNativeSeparators(path)));
        CHECK(dialog.openLink()->isVisibleTo(&dialog));
        dialog.openResult();
        CHECK(opener.opened == QStringList{path});

        // The choices go to the file, and to the next export
        CHECK_FALSE(used.titles);
        CHECK(used.comments);
        CHECK(used.fontFamily == QStringLiteral("Georgia"));
        const auto& settings = core::SettingsManager::getInstance();
        CHECK_FALSE(settings.get<bool>("export.titles"));
        CHECK(settings.get<bool>("export.comments"));
        CHECK(settings.get<std::string>("export.font") == "Georgia");
        ExportBookDialog next(sampleBook(), editor::ExportFormat::Docx, folder, {});
        CHECK_FALSE(next.titlesBox()->isChecked());
        CHECK(next.commentsBox()->isChecked());
        CHECK(next.fontBox()->currentData().toString() == QStringLiteral("Georgia"));
    }

    SECTION("Not saved: why, and another try") {
        succeeds = false;
        dialog.accept();
        CHECK_FALSE(dialog.isDone());
        CHECK(dialog.resultText() == QStringLiteral("The file could not be saved."));
        CHECK(dialog.reasonText() ==
              QStringLiteral("Kalahari cannot write the file in the folder %1. If the file is "
                             "open in another program, close it, or choose another folder.")
                  .arg(QDir::toNativeSeparators(folder)));
        CHECK(dialog.detailsText() == QStringLiteral("book.docx: Permission denied"));
        CHECK_FALSE(dialog.openLink()->isVisibleTo(&dialog));
        CHECK(dialog.acceptButton()->isEnabled());
    }
}

TEST_CASE("Export Book: a PDF, its purpose and its comments apart from Word's",
          "[gui][dialogs][export]") {
    SettingsKept kept;
    auto& settings = core::SettingsManager::getInstance();
    settings.set<bool>("export.comments", true);
    settings.set<bool>("export.pdf.comments", false);
    settings.set<std::string>("export.pdf.purpose", "reading");
    settings.set<bool>("export.pdf.facingPages", true);
    settings.set<std::string>("export.runningHead", "authorAndTitle");
    QTemporaryDir dir;
    ExportBookDialog dialog(
        sampleBook(), editor::ExportFormat::Pdf, dir.path(),
        [](const QString& path, const ExportBookDialog::Choice&,
           const gui::dialogs::ArchiveDialog::Progress&, editor::ExportResult* result,
           editor::ExportProblem*) {
            QFile file(path);
            REQUIRE(file.open(QIODevice::WriteOnly));
            result->file = path;
            return true;
        });
    CHECK(dialog.formatBox()->currentData().toString() == QStringLiteral("pdf"));
    CHECK(dialog.nameField()->text() == QStringLiteral("Wind over the Oasis.pdf"));

    // The layout of Word: the pages, the font and the running head
    CHECK(onItsTab(dialog.newPageBox()));
    CHECK(onItsTab(dialog.fontBox()));
    CHECK(onItsTab(dialog.runningHeadBox()));
    CHECK_FALSE(onItsTab(dialog.embedFontBox()));
    CHECK(dialog.runningHeadBox()->currentText() ==
          QStringLiteral("Author on the left, book title on the right"));

    // Its own comments, and what it does with the images
    CHECK(onItsTab(dialog.pdfCommentsBox()));
    CHECK_FALSE(onItsTab(dialog.commentsBox()));
    CHECK_FALSE(dialog.pdfCommentsBox()->isChecked());
    CHECK(dialog.pdfCommentsNote() ==
          QStringLiteral("The comments, to-dos and notes of the text, with their authors and "
                         "dates, as notes beside the text in a PDF reader. Without this option "
                         "the file has no annotations."));
    CHECK(dialog.notesNote().isEmpty());
    CHECK(dialog.imagesNote() ==
          QStringLiteral("A PDF document has the images in the file itself, with their cropping, "
                         "rotation, filter, border and shadow."));

    // Its tab: the purpose, the bookmarks, the pages in pairs
    CHECK(dialog.tabs()->isTabVisible(dialog.tabs()->indexOf(dialog.pdfTab())));
    CHECK_FALSE(dialog.tabs()->isTabVisible(dialog.tabs()->indexOf(dialog.epubTab())));
    CHECK(dialog.purposeButton(editor::PdfPurpose::Reading)->isChecked());
    CHECK(dialog.bookmarksBox()->isChecked());
    CHECK(dialog.facingPagesBox()->isChecked());

    dialog.pdfCommentsBox()->setChecked(true);
    dialog.facingPagesBox()->setChecked(false);
    dialog.runningHeadBox()->setCurrentIndex(
        dialog.runningHeadBox()->findData(static_cast<int>(editor::RunningHead::Chapter)));
    ExportBookDialog::Choice choice = dialog.choice();
    CHECK(choice.format == editor::ExportFormat::Pdf);
    CHECK(choice.comments);
    CHECK(choice.runningHead == editor::RunningHead::Chapter);
    CHECK(choice.pdf.purpose == editor::PdfPurpose::Reading);
    CHECK(choice.pdf.bookmarks);
    CHECK_FALSE(choice.pdf.facingPages);

    // A file for a print shop has no comments; a Qt that cannot write it does not offer it
    if (editor::pdfPurposeSupported(editor::PdfPurpose::PrintShop)) {
        dialog.purposeButton(editor::PdfPurpose::PrintShop)->setChecked(true);
        CHECK_FALSE(dialog.pdfCommentsBox()->isEnabled());
        CHECK(dialog.pdfCommentsNote() ==
              QStringLiteral("A file for a print shop (PDF/X-4) cannot have comments."));
        CHECK(dialog.choice().pdf.purpose == editor::PdfPurpose::PrintShop);
        CHECK_FALSE(dialog.choice().comments);
        dialog.purposeButton(editor::PdfPurpose::Reading)->setChecked(true);
        CHECK(dialog.pdfCommentsBox()->isEnabled());
        dialog.pdfCommentsBox()->setChecked(true);
    } else {
        CHECK_FALSE(dialog.purposeButton(editor::PdfPurpose::PrintShop)->isEnabled());
        CHECK_FALSE(dialog.grayscaleBox()->isEnabled());
        CHECK_FALSE(dialog.choice().pdf.grayscale);
    }

    // Word keeps its own comments
    dialog.setFormat(editor::ExportFormat::Docx);
    CHECK(dialog.commentsBox()->isChecked());
    dialog.commentsBox()->setChecked(false);
    CHECK_FALSE(dialog.choice().comments);

    // The next export: the choices of each format, the layout shared
    dialog.setFormat(editor::ExportFormat::Pdf);
    dialog.accept();
    REQUIRE(dialog.isDone());
    CHECK(settings.get<bool>("export.pdf.comments"));
    CHECK_FALSE(settings.get<bool>("export.comments"));
    CHECK_FALSE(settings.get<bool>("export.pdf.facingPages"));
    CHECK(settings.get<std::string>("export.runningHead") == "chapter");
    ExportBookDialog word(sampleBook(), editor::ExportFormat::Docx, dir.path(), {});
    CHECK_FALSE(word.commentsBox()->isChecked());
    CHECK(word.choice().runningHead == editor::RunningHead::Chapter);
    ExportBookDialog pdf(sampleBook(), editor::ExportFormat::Pdf, dir.path(), {});
    CHECK(pdf.pdfCommentsBox()->isChecked());
    CHECK_FALSE(pdf.facingPagesBox()->isChecked());
}

TEST_CASE("Export Book: an EPUB, its look, its cover and its data", "[gui][dialogs][export]") {
    SettingsKept kept;
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>("export.epub.look", "book");
    settings.set<std::string>("export.epub.cover", "design");
    settings.set<bool>("export.epub.embedFont", false);
    QTemporaryDir dir;
    QImage picture(60, 90, QImage::Format_RGB32);
    picture.fill(Qt::darkGreen);
    const QString design = QDir(dir.path()).filePath(QStringLiteral("design.png"));
    REQUIRE(picture.save(design));
    ExportBookDialog::Book book = sampleBook();
    book.author = QStringLiteral("Anna Nowak");
    book.language = QStringLiteral("Polish");
    book.description = QStringLiteral("Three days from the last well, the caravan waits.");
    book.coverTitle = QStringLiteral("Front, version 3");
    book.coverFile = design;
    book.isbn = QStringLiteral("978-3-16-148410-0");
    book.publisher = QStringLiteral("Dunes");
    ExportBookDialog dialog(
        book, editor::ExportFormat::Epub, dir.path(),
        [](const QString& path, const ExportBookDialog::Choice&,
           const gui::dialogs::ArchiveDialog::Progress&, editor::ExportResult* result,
           editor::ExportProblem*) {
            QFile file(path);
            REQUIRE(file.open(QIODevice::WriteOnly));
            result->file = path;
            result->coverMissing = true;
            result->fontNotEmbedded = true;
            return true;
        });
    CHECK(dialog.nameField()->text() == QStringLiteral("Wind over the Oasis.epub"));

    // The layout of an EPUB: its look and its font, no pages
    CHECK(onItsTab(dialog.bookStylesButton()));
    CHECK(dialog.bookStylesButton()->isChecked());
    CHECK(onItsTab(dialog.embedFontBox()));
    CHECK_FALSE(onItsTab(dialog.newPageBox()));
    CHECK_FALSE(onItsTab(dialog.fontBox()));
    CHECK_FALSE(onItsTab(dialog.runningHeadBox()));

    // No comments; the images in the file
    CHECK_FALSE(onItsTab(dialog.commentsBox()));
    CHECK_FALSE(onItsTab(dialog.pdfCommentsBox()));
    CHECK(dialog.notesNote() ==
          QStringLiteral("EPUB has no comments: the annotations stay only in the book."));
    CHECK(dialog.imagesNote() ==
          QStringLiteral("An EPUB e-book has the images in the file itself, with their "
                         "alternative text for screen readers."));

    // Its tab: the design of the materials and the data of the book; its notes show in front
    CHECK(dialog.tabs()->isTabVisible(dialog.tabs()->indexOf(dialog.epubTab())));
    CHECK_FALSE(dialog.tabs()->isTabVisible(dialog.tabs()->indexOf(dialog.pdfTab())));
    dialog.tabs()->setCurrentWidget(dialog.epubTab());
    CHECK(dialog.coverBox()->currentText() ==
          QStringLiteral("Front, version 3 (from the materials)"));
    CHECK(
        dialog.coverNote() ==
        QStringLiteral("The design of the front of the cover chosen in the materials \"Cover\"."));
    CHECK(dialog.isbnField()->text() == QStringLiteral("978-3-16-148410-0"));
    CHECK(dialog.publisherField()->text() == QStringLiteral("Dunes"));
    CHECK(dialog.bookDataNote().startsWith(QStringLiteral("Change the title, the author")));
    ExportBookDialog::Choice choice = dialog.choice();
    CHECK(choice.format == editor::ExportFormat::Epub);
    CHECK(choice.bookStyles);
    CHECK_FALSE(choice.embedFont);
    CHECK_FALSE(choice.comments);
    CHECK(choice.cover == ExportBookDialog::Cover::Design);
    CHECK(choice.coverImage == design);
    CHECK(choice.isbn == QStringLiteral("978-3-16-148410-0"));
    CHECK(choice.publisher == QStringLiteral("Dunes"));

    // No cover, then another picture
    dialog.coverBox()->setCurrentIndex(
        dialog.coverBox()->findData(static_cast<int>(ExportBookDialog::Cover::None)));
    CHECK(dialog.coverNote() == QStringLiteral("The e-book has no cover."));
    CHECK(dialog.choice().coverImage.isEmpty());
    const QString other = QDir(dir.path()).filePath(QStringLiteral("other.jpg"));
    REQUIRE(picture.save(other));
    dialog.setCoverFile(other);
    CHECK(dialog.coverBox()->currentText() == QStringLiteral("other.jpg"));
    CHECK(dialog.choice().cover == ExportBookDialog::Cover::File);
    CHECK(dialog.choice().coverImage == other);
    CHECK(dialog.coverNote() ==
          QStringLiteral("The picture %1.").arg(QDir::toNativeSeparators(other)));

    // An ISBN that is not one: the export waits, the other formats do not
    dialog.isbnField()->setText(QStringLiteral("978-3-16-148410-1"));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK(dialog.bookDataNote() ==
          QStringLiteral("This is not an ISBN: an ISBN has 13 digits (or 10) and the right check "
                         "digit, e.g. 978-3-16-148410-0."));
    dialog.setFormat(editor::ExportFormat::Docx);
    CHECK(dialog.acceptButton()->isEnabled());
    dialog.setFormat(editor::ExportFormat::Epub);
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    dialog.isbnField()->setText(QStringLiteral("0-306-40615-2"));
    CHECK(dialog.acceptButton()->isEnabled());
    dialog.isbnField()->clear();
    CHECK(dialog.acceptButton()->isEnabled());

    // The result says what the file lacks; the next export has the choices
    dialog.simpleLookButton()->setChecked(true);
    dialog.embedFontBox()->setChecked(true);
    dialog.accept();
    REQUIRE(dialog.isDone());
    CHECK(dialog.resultText().endsWith(
        QStringLiteral("The picture of the cover could not be read: the e-book has no cover. The "
                       "font of the text is not in the file: its licence does not allow it, or "
                       "the computer does not have it.")));
    CHECK(settings.get<std::string>("export.epub.look") == "simple");
    CHECK(settings.get<bool>("export.epub.embedFont"));
    // Another picture is not the book's: the next export has the design of the materials
    CHECK(settings.get<std::string>("export.epub.cover") == "design");
    ExportBookDialog next(book, editor::ExportFormat::Epub, dir.path(), {});
    CHECK(next.simpleLookButton()->isChecked());
    CHECK(next.embedFontBox()->isChecked());
    CHECK(next.choice().cover == ExportBookDialog::Cover::Design);

    // A book without a chosen design: no cover
    book.coverFile.clear();
    ExportBookDialog bare(book, editor::ExportFormat::Epub, dir.path(), {});
    CHECK(bare.choice().cover == ExportBookDialog::Cover::None);
    bare.tabs()->setCurrentWidget(bare.epubTab());
    CHECK(
        bare.coverNote() ==
        QStringLiteral("The materials \"Cover\" have no chosen design of the front of the cover."));
}

namespace {

/// The first volume of a series of two, with its set for a publisher
ExportBookDialog::Book seriesBook() {
    ExportBookDialog::Book book = sampleBook();
    book.author = QStringLiteral("Anna Nowak");
    book.isbn = QStringLiteral("978-3-16-148410-0");
    book.setTitle = QStringLiteral("Wind over the Oasis – set for the publisher");
    book.setParts = 3;
    book.volumes = {{QStringLiteral("v1"), QStringLiteral("Volume 1: Wind over the Oasis")},
                    {QStringLiteral("v2"), QStringLiteral("Volume 2: Dunes")}};
    book.volume = QStringLiteral("v1");
    return book;
}

/// The second volume of seriesBook(): two chapters, its own data, no set; the text in front
/// as the coordinator gives it
ExportBookDialog::Book dunesBook() {
    using Row = ExportBookDialog::Row;
    ExportBookDialog::Book book;
    book.title = QStringLiteral("Dunes");
    book.author = QStringLiteral("Ewa Las");
    book.language = QStringLiteral("Polish");
    book.publisher = QStringLiteral("Sand Press");
    book.rows = {Row{QStringLiteral("d1"), QStringLiteral("The Ridge"), QStringLiteral("draft"),
                     QString(), 0, true},
                 Row{QStringLiteral("d2"), QStringLiteral("The Hollow"), QStringLiteral("final"),
                     QString(), 0, true}};
    book.frontTitle = QStringLiteral("The Well");
    book.frontId = QStringLiteral("c2");
    book.volumes = seriesBook().volumes;
    book.volume = QStringLiteral("v2");
    return book;
}

/// Whether the window has a label of text @p text
bool hasLabel(const QWidget& window, const QString& text) {
    const QList<QLabel*> labels = window.findChildren<QLabel*>();
    return std::any_of(labels.cbegin(), labels.cend(),
                       [&text](const QLabel* label) { return label->text() == text; });
}

}  // namespace

TEST_CASE("Export Book: a series, the list Volume over the texts",
          "[gui][dialogs][export][series]") {
    const SettingsKept kept;
    core::SettingsManager::getInstance().set<std::string>("export.content", "book");
    const QTemporaryDir dir;

    // One book has no list Volume
    {
        const ExportBookDialog one(sampleBook(), editor::ExportFormat::Docx, dir.path(), {});
        CHECK(one.volumeBox() == nullptr);
        CHECK(one.layoutChoice().volume.isEmpty());
    }

    // A series: the list over the texts, the current volume chosen
    ExportBookDialog dialog(seriesBook(), editor::ExportFormat::Epub, dir.path(), {});
    QStringList asked;
    dialog.setVolumeBook([&asked](const QString& volumeId) {
        asked << volumeId;
        return dunesBook();
    });
    QComboBox* volumes = dialog.volumeBox();
    REQUIRE(volumes != nullptr);
    CHECK(onItsTab(volumes));
    REQUIRE(volumes->count() == 2);
    CHECK(volumes->itemText(0) == QStringLiteral("Volume 1: Wind over the Oasis"));
    CHECK(volumes->itemText(1) == QStringLiteral("Volume 2: Dunes"));
    CHECK(volumes->currentIndex() == 0);
    CHECK(dialog.choice().volume == QStringLiteral("v1"));
    CHECK(dialog.bookButton()->text() == QStringLiteral("Whole &book (4 texts)"));
    dialog.setContent(ExportBookDialog::Content::Set);
    CHECK(dialog.nameField()->text() ==
          QStringLiteral("Wind over the Oasis – set for the publisher.epub"));

    // Another volume: its texts, its counts, its name and its data; the text in front stays.
    // Without a set of its own, its whole book goes.
    volumes->setCurrentIndex(1);
    CHECK(asked == QStringList{QStringLiteral("v2")});
    CHECK(dialog.choice().volume == QStringLiteral("v2"));
    CHECK(dialog.textList()->topLevelItemCount() == 2);
    CHECK(dialog.textList()->topLevelItem(0)->text(0) == QStringLiteral("The Ridge"));
    CHECK(dialog.bookButton()->text() == QStringLiteral("Whole &book (2 texts)"));
    CHECK_FALSE(onItsTab(dialog.setButton()));
    CHECK(dialog.bookButton()->isChecked());
    CHECK(dialog.nameField()->text() == QStringLiteral("Dunes.epub"));
    CHECK(dialog.headingDescription() ==
          QStringLiteral("Saves the book \"Dunes\" as an e-book in the EPUB format."));
    CHECK(hasLabel(dialog, QStringLiteral("Dunes – Ewa Las")));
    CHECK(hasLabel(dialog, QStringLiteral("Polish")));
    CHECK(dialog.isbnField()->text().isEmpty());
    CHECK(dialog.publisherField()->text() == QStringLiteral("Sand Press"));
    CHECK(onItsTab(dialog.frontButton()));
    CHECK(dialog.frontButton()->text() == QStringLiteral("&Text in front: \"The Well\""));

    // Its texts chosen in the list go
    dialog.setContent(ExportBookDialog::Content::Chosen);
    dialog.checkFinal();
    CHECK(dialog.choice().chosen == QStringList{QStringLiteral("d2")});
    CHECK(dialog.chosenNote() == QStringLiteral("1 of 2 texts chosen."));

    // The text in front is of no volume: the list waits
    dialog.setContent(ExportBookDialog::Content::Front);
    CHECK_FALSE(volumes->isEnabledTo(&dialog));
    dialog.setContent(ExportBookDialog::Content::Book);
    CHECK(volumes->isEnabledTo(&dialog));

    // The name typed stays
    dialog.nameField()->setText(QStringLiteral("Mine.epub"));
    emit dialog.nameField()->textEdited(dialog.nameField()->text());
    volumes->setCurrentIndex(0);
    CHECK(dialog.nameField()->text() == QStringLiteral("Mine.epub"));
}

TEST_CASE("Export Book: a series, the keys and the window whole on a small screen",
          "[gui][dialogs][export][series]") {
    const SettingsKept kept;
    const QTemporaryDir dir;
    ExportBookDialog::Book book = seriesBook();
    for (int i = 0; i < 30; ++i) {
        book.rows.append(ExportBookDialog::Row{QStringLiteral("x%1").arg(i),
                                               QStringLiteral("Chapter %1").arg(i + 3),
                                               QStringLiteral("draft"), QString(), 2, true});
    }
    book.volumes.append({QStringLiteral("v3"),
                         QStringLiteral("Volume 3: The Long Road Through the Great Sand Sea "
                                        "and Back Again")});
    // The label of the list Volume
    const auto volumeLabel = [](const ExportBookDialog& dialog) {
        const QList<QLabel*> labels = dialog.findChildren<QLabel*>();
        const auto found =
            std::find_if(labels.cbegin(), labels.cend(), [&dialog](const QLabel* label) {
                return label->buddy() == dialog.volumeBox();
            });
        return found != labels.cend() ? (*found)->text() : QString();
    };
    const auto check = [&](editor::ExportFormat format) {
        ExportBookDialog dialog(book, format, dir.path(), {});
        INFO(test::keyProblems(dialog, *dialog.tabs()).toStdString());
        CHECK(test::keyProblems(dialog, *dialog.tabs()).isEmpty());
        for (int tab = 0; tab < dialog.tabs()->count(); ++tab) {
            if (dialog.tabs()->isTabVisible(tab)) {
                dialog.tabs()->setCurrentIndex(tab);
                INFO("The tab " << dialog.tabs()->tabText(tab).toStdString());
                checkFitsSmallScreen(dialog);
            }
        }
        dialog.setContent(ExportBookDialog::Content::Chosen);
        checkFitsSmallScreen(dialog);
        return volumeLabel(dialog);
    };

    SECTION("In English") {
        CHECK(check(editor::ExportFormat::Docx) == QStringLiteral("&Volume:"));
        check(editor::ExportFormat::Epub);
    }

    SECTION("In Polish") {
        const Polish polish;
        REQUIRE(polish.loaded());
        CHECK(check(editor::ExportFormat::Docx) == QStringLiteral("To&m:"));
        check(editor::ExportFormat::Pdf);
        check(editor::ExportFormat::Epub);
    }

    SECTION("In a font of 13 pixels, in Polish") {
        const AppFontOfPixels font(13);
        const Polish polish;
        REQUIRE(polish.loaded());
        check(editor::ExportFormat::Docx);
        check(editor::ExportFormat::Epub);
    }
}

TEST_CASE("Export Book: the book as the program has it, in reading order",
          "[gui][export]") {
    SettingsKept kept;
    OpenNovel novel(QStringLiteral("Wind"));
    core::ProjectManager& pm = novel.pm;
    // A dedication without a title, a part with the chapter and a second one
    const QString dedication = pm.addElement(
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("dedication")),
        QStringLiteral("Dedication"), core::BookPlace::Front);
    REQUIRE_FALSE(dedication.isEmpty());
    const QString part = pm.addElement(pm.partKind(), QStringLiteral("Part One"),
                                       core::BookPlace::Main, QString(), -1, {novel.chapterId});
    REQUIRE_FALSE(part.isEmpty());
    const QString second = pm.addElement(pm.chapterKindFor(core::BookPlace::Main, part),
                                         QStringLiteral("The Calm"), core::BookPlace::Main, part);
    REQUIRE_FALSE(second.isEmpty());
    pm.setChapterContent(second, QStringLiteral("<kml><p>Calm, saved.</p></kml>"));
    REQUIRE(pm.saveChapterContent(second));

    Window window;
    window.navigator.onElementSelected(novel.chapterId, QStringLiteral("The Storm"));
    gui::EditorPanel* storm = window.editor(window.tabs->currentIndex());
    // An unsaved change of the open text
    storm->setContent(QStringLiteral("<kml><p>Storm, not saved yet.</p></kml>"));
    BookExportCoordinator coordinator(
        pm, kalahari::test::commands(), window.tabs,
        [&window]() { return qobject_cast<gui::EditorPanel*>(window.tabs->currentWidget()); },
        nullptr,
        nullptr, &window.window);
    CHECK(coordinator.whyNotExport().isEmpty());

    SECTION("The whole book: the part with its texts, the open one as its editor has it") {
        const editor::BookExport book = coordinator.bookExport(wholeBook());
        CHECK(book.title == QStringLiteral("Wind"));
        CHECK(book.author == QStringLiteral("Author"));
        // A new novel starts with its title page and its first chapter
        REQUIRE(book.items.size() == 6);
        CHECK(book.items[0].title.isEmpty());  // the title page and the dedication have none
        CHECK(book.items[1].elementId == dedication);
        CHECK(book.items[1].title.isEmpty());
        CHECK(book.items[2].title == QStringLiteral("Chapter 1"));
        CHECK(book.items[3].kind == editor::ExportItemKind::Group);
        CHECK(book.items[3].title == QStringLiteral("Part One"));
        CHECK(book.items[3].titleStyle == QStringLiteral("part_title"));
        CHECK(book.items[4].elementId == novel.chapterId);
        CHECK(book.items[4].titleStyle == QStringLiteral("chapter_title"));
        CHECK(book.items[4].kml.contains(QStringLiteral("Storm, not saved yet.")));
        CHECK(book.items[5].kml.contains(QStringLiteral("Calm, saved.")));
    }

    SECTION("The chosen texts: a part goes with its chosen text") {
        ExportBookDialog::Choice choice = wholeBook();
        choice.content = ExportBookDialog::Content::Chosen;
        choice.chosen = {second};
        const editor::BookExport book = coordinator.bookExport(choice);
        REQUIRE(book.items.size() == 2);
        CHECK(book.items[0].elementId == part);
        CHECK(book.items[1].elementId == second);
    }

    SECTION("The text in front alone") {
        ExportBookDialog::Choice choice = wholeBook();
        choice.content = ExportBookDialog::Content::Front;
        const editor::BookExport book = coordinator.bookExport(choice);
        REQUIRE(book.items.size() == 1);
        CHECK(book.items[0].elementId == novel.chapterId);
        CHECK(book.items[0].title == QStringLiteral("The Storm"));
    }

    SECTION("The rows of the window: the sections, the part and the texts") {
        const ExportBookDialog::Book rows = coordinator.dialogBook();
        CHECK(rows.title == QStringLiteral("Wind"));
        CHECK(rows.volumes.isEmpty());  // one book: no list Volume
        CHECK(rows.volume.isEmpty());
        CHECK(rows.frontTitle == QStringLiteral("The Storm"));
        CHECK(rows.frontId == novel.chapterId);
        // The elements under their sections, when the Navigator shows the sections
        const int base = pm.book()->partsLayer ? 1 : 0;
        QStringList titles;
        for (const ExportBookDialog::Row& row : rows.rows) {
            if (row.id.isEmpty()) {
                CHECK(row.level == 0);
                CHECK_FALSE(row.leaf);
                continue;
            }
            titles << QString((row.level - base) * 2, QLatin1Char(' ')) + row.title +
                          (row.leaf ? QString() : QStringLiteral("/"));
        }
        CHECK(titles == QStringList({QStringLiteral("Title page"), QStringLiteral("Dedication"),
                                     QStringLiteral("Chapter 1"), QStringLiteral("Part One/"),
                                     QStringLiteral("  The Storm"), QStringLiteral("  The Calm")}));
    }
}

TEST_CASE("Export Book: a volume of a series, its texts, its data and its e-book",
          "[gui][export][series]") {
    const SettingsKept kept;
    OpenNovel novel(QStringLiteral("Wind"));
    core::ProjectManager& pm = novel.pm;
    core::NewBook request;
    request.title = QStringLiteral("Dunes");
    request.author = QStringLiteral("Ewa Las");
    const QString dunes = pm.addBook(request);
    REQUIRE_FALSE(dunes.isEmpty());
    const core::BookProject& project = *pm.project();
    const QString wind = project.books.at(0).id;
    const QList<core::ProjectElement>& second = project.elementsIn(core::BookPlace::Main, 1);
    REQUIRE(second.size() == 1);
    const QString ridge = second.constFirst().id;
    const QString ridgeTitle = second.constFirst().title;
    pm.setChapterContent(ridge, QStringLiteral("<kml><p>The ridge, saved.</p></kml>"));
    REQUIRE(pm.saveChapterContent(ridge));

    Window window;
    const BookExportCoordinator coordinator(
        pm, kalahari::test::commands(), window.tabs,
        [&window]() { return qobject_cast<gui::EditorPanel*>(window.tabs->currentWidget()); },
        nullptr, nullptr, &window.window);

    // The window: the volumes of the series in their order, the current one chosen
    const ExportBookDialog::Book current = coordinator.dialogBook();
    REQUIRE(current.volumes.size() == 2);
    CHECK(current.volumes.at(0).id == wind);
    CHECK(current.volumes.at(0).name == QStringLiteral("Volume 1: Wind"));
    CHECK(current.volumes.at(1).id == dunes);
    CHECK(current.volumes.at(1).name == QStringLiteral("Volume 2: Dunes"));
    CHECK(current.volume == wind);
    CHECK(current.title == QStringLiteral("Wind"));

    // Another volume: its title, its author and its texts; the set is the current book's
    const ExportBookDialog::Book other = coordinator.dialogBook(dunes);
    CHECK(other.title == QStringLiteral("Dunes"));
    CHECK(other.author == QStringLiteral("Ewa Las"));
    CHECK(other.volume == dunes);
    CHECK(other.volumes.size() == 2);
    CHECK(other.setTitle.isEmpty());
    QStringList texts;
    for (const ExportBookDialog::Row& row : other.rows) {
        if (row.leaf) {
            texts << row.title;
        }
    }
    CHECK(texts == QStringList{ridgeTitle});

    // Its export: its texts and its data; its e-book has an identifier of its own
    ExportBookDialog::Choice choice = wholeBook();
    choice.volume = dunes;
    const editor::BookExport book = coordinator.bookExport(choice);
    CHECK(book.title == QStringLiteral("Dunes"));
    CHECK(book.author == QStringLiteral("Ewa Las"));
    REQUIRE(book.items.size() == 1);
    CHECK(book.items[0].elementId == ridge);
    CHECK(book.items[0].kml.contains(QStringLiteral("The ridge, saved.")));
    const QString windIdentifier = coordinator.bookExport(wholeBook()).epub.identifier;
    CHECK_FALSE(book.epub.identifier.isEmpty());
    CHECK(book.epub.identifier != windIdentifier);

    // The ISBN and the publisher of the volume's e-book are its own
    REQUIRE(pm.setBookPublication(QStringLiteral("978-3-16-148410-0"), QStringLiteral("Sand Press"),
                                  dunes));
    CHECK(project.books.at(1).publisher == QStringLiteral("Sand Press"));
    CHECK(project.books.at(0).publisher.isEmpty());
    CHECK(coordinator.dialogBook(dunes).isbn == QStringLiteral("978-3-16-148410-0"));
    CHECK(coordinator.dialogBook().isbn.isEmpty());
    CHECK_FALSE(pm.setBookPublication(QString(), QString(), QStringLiteral("no-such-book")));
}

TEST_CASE("Export Book: a PDF and an EPUB of the book: the colors, the cover and the data",
          "[gui][export]") {
    using core::CoverMaterial;
    SettingsKept kept;
    OpenNovel novel(QStringLiteral("Wind"));
    core::ProjectManager& pm = novel.pm;
    // The chosen design of the front of the cover, and the chosen description of the back
    QTemporaryDir files;
    REQUIRE(files.isValid());
    QImage picture(60, 90, QImage::Format_RGB32);
    picture.fill(Qt::darkGreen);
    const QString pictureFile = QDir(files.path()).filePath(QStringLiteral("front.png"));
    REQUIRE(picture.save(pictureFile));
    const core::KindRef coverKind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("cover"));
    const auto addCover = [&](const QString& title, const char* part, const char* place,
                              const char* state, const QString& file, const QString& text) {
        core::NewMaterial material;
        material.title = title;
        material.filePath = file;
        material.description = text;
        material.cover.part = QString::fromLatin1(part);
        material.cover.place = QString::fromLatin1(place);
        material.cover.state = QString::fromLatin1(state);
        const QString id = pm.addMaterial(material, coverKind);
        REQUIRE_FALSE(id.isEmpty());
        return id;
    };
    addCover(QStringLiteral("Rejected design"), CoverMaterial::DESIGN, CoverMaterial::FRONT,
             CoverMaterial::REJECTED, pictureFile, QString());
    const QString front = addCover(QStringLiteral("Front, version 3"), CoverMaterial::DESIGN,
                                   CoverMaterial::FRONT, CoverMaterial::CHOSEN, pictureFile,
                                   QString());
    addCover(QStringLiteral("Flap"), CoverMaterial::BLURB, CoverMaterial::FRONT_FLAP,
             CoverMaterial::CHOSEN, QString(), QStringLiteral("On the flap."));
    addCover(QStringLiteral("Back"), CoverMaterial::BLURB, CoverMaterial::BACK,
             CoverMaterial::CHOSEN, QString(), QStringLiteral("Three days from the last well."));
    REQUIRE(pm.setBookPublication(QStringLiteral("978-3-16-148410-0"), QStringLiteral("Dunes")));

    BookExportCoordinator coordinator(pm, kalahari::test::commands(), nullptr,
                                      []() { return nullptr; }, nullptr, nullptr, nullptr);

    // The window: the data of the book, its cover and its description
    const ExportBookDialog::Book rows = coordinator.dialogBook();
    CHECK(rows.author == QStringLiteral("Author"));
    CHECK(rows.language == QStringLiteral("English"));
    CHECK(rows.coverTitle == QStringLiteral("Front, version 3"));
    const core::ProjectElement* frontElement = pm.findElement(front);
    REQUIRE(frontElement != nullptr);
    CHECK(rows.coverFile == pm.filePathOf(*frontElement));
    CHECK(QFileInfo::exists(rows.coverFile));
    CHECK(rows.description == QStringLiteral("Three days from the last well."));
    CHECK(rows.isbn == QStringLiteral("978-3-16-148410-0"));
    CHECK(rows.publisher == QStringLiteral("Dunes"));
    // The book keeps them in its .klh file
    QFile manifest(pm.getManifestPath());
    REQUIRE(manifest.open(QIODevice::ReadOnly));
    const QJsonObject saved = QJsonDocument::fromJson(manifest.readAll())
                                  .object()[QStringLiteral("books")][0]
                                  .toObject();
    CHECK(saved[QStringLiteral("isbn")].toString() == QStringLiteral("978-3-16-148410-0"));
    CHECK(saved[QStringLiteral("publisher")].toString() == QStringLiteral("Dunes"));

    // A PDF: the colors of the light paper, the running head and the options of the window
    ExportBookDialog::Choice choice = wholeBook();
    choice.format = editor::ExportFormat::Pdf;
    choice.formatId = QStringLiteral("pdf");
    choice.runningHead = editor::RunningHead::TitleAndChapter;
    choice.pdf.purpose = editor::PdfPurpose::Archive;
    choice.pdf.facingPages = true;
    choice.fontFamily = QStringLiteral("Georgia");
    const editor::BookExport pdf = coordinator.bookExport(choice);
    CHECK(pdf.runningHead == editor::RunningHead::TitleAndChapter);
    CHECK(pdf.pdf.purpose == editor::PdfPurpose::Archive);
    CHECK(pdf.pdf.facingPages);
    CHECK(pdf.fontFamily == QStringLiteral("Georgia"));
    CHECK(pdf.colors.continuous.backgroundLight ==
          gui::EditorPanel::themeColors().continuous.backgroundLight);

    // An EPUB: the editor's font, its cover, its data, and the book's lasting identifier
    choice.format = editor::ExportFormat::Epub;
    choice.formatId = QStringLiteral("epub");
    choice.bookStyles = false;
    choice.embedFont = true;
    choice.coverImage = rows.coverFile;
    choice.isbn = rows.isbn;
    choice.publisher = rows.publisher;
    const editor::BookExport epub = coordinator.bookExport(choice);
    CHECK(epub.fontFamily ==
          QString::fromStdString(core::SettingsManager::getInstance().get<std::string>(
              "editor.fontFamily")));
    CHECK_FALSE(epub.epub.bookStyles);
    CHECK(epub.epub.embedFont);
    CHECK(epub.epub.coverImage == rows.coverFile);
    CHECK(epub.epub.description == QStringLiteral("Three days from the last well."));
    CHECK(epub.epub.isbn == QStringLiteral("978-3-16-148410-0"));
    CHECK(epub.epub.publisher == QStringLiteral("Dunes"));
    CHECK_FALSE(QUuid(epub.epub.identifier).isNull());
    CHECK(coordinator.bookExport(choice).epub.identifier == epub.epub.identifier);
    // A part of the book is another e-book
    choice.content = ExportBookDialog::Content::Chosen;
    choice.chosen = {novel.chapterId};
    CHECK(coordinator.bookExport(choice).epub.identifier.isEmpty());
}

TEST_CASE("Export Book: the commands wait for a book or a text", "[gui][export]") {
    BookExportCoordinator coordinator(kalahari::test::book(), kalahari::test::commands(), nullptr,
                                      []() { return nullptr; }, nullptr, nullptr, nullptr);
    CHECK(coordinator.whyNotExport() == QStringLiteral("Open a book or a text to export"));
}
