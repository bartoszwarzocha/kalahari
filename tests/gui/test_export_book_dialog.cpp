/// @file test_export_book_dialog.cpp
/// @brief File > Export > DOCX and Markdown: the window of the export, and the book it exports
///        as the program has it (the open texts with their unsaved changes)

#include <catch2/catch_test_macros.hpp>
#include "document_window.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/extension_points.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/book_export_coordinator.h"
#include "kalahari/gui/dialogs/export_book_dialog.h"
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
#include <QGroupBox>
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

#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace kalahari;
using gui::BookExportCoordinator;
using gui::dialogs::ExportBookDialog;
using test::OpenNovel;
using test::Window;

namespace {

/// The settings of a test, as they were before it
class SettingsKept {
public:
    SettingsKept() {
        auto& settings = core::SettingsManager::getInstance();
        for (const char* key : {"export.content", "export.font", "editor.page.size",
                                "ui.lengthUnit"}) {
            m_strings.emplace_back(key, settings.get<std::string>(key));
        }
        for (const char* key : {"export.titles", "export.newPage", "export.pageNumbers",
                                "export.comments", "export.copyImages",
                                "editor.margins.mirrorEnabled"}) {
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

/// The page of the tabs that has @p widget; null for a widget outside the tabs
const QWidget* tabPageOf(const QWidget* widget) {
    for (const QWidget* each = widget; each != nullptr; each = each->parentWidget()) {
        if (qobject_cast<const QStackedWidget*>(each->parentWidget()) != nullptr &&
            qobject_cast<const QTabWidget*>(each->parentWidget()->parentWidget()) != nullptr) {
            return each;
        }
    }
    return nullptr;
}

/// Whether @p widget shows on its tab, whichever tab is in front
bool onItsTab(const QWidget* widget) {
    const QWidget* page = tabPageOf(widget);
    REQUIRE(page != nullptr);
    return widget == page || widget->isVisibleTo(page);
}

/// The access key of a text: the letter after a single "&"
QChar accessKey(const QString& text) {
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&')) {
            if (text.at(i + 1) == QLatin1Char('&')) {
                ++i;
                continue;
            }
            return text.at(i + 1).toLower();
        }
    }
    return QChar();
}

/// The texts with an access key while tab @p tab is in front: the titles of the tabs, and the
/// labels of fields, the buttons and the groups that show on that tab or outside the tabs
QStringList keyTexts(const ExportBookDialog& dialog, int tab) {
    QStringList texts;
    for (int i = 0; i < dialog.tabs()->count(); ++i) {
        texts << dialog.tabs()->tabText(i);
    }
    for (const QWidget* widget : dialog.findChildren<QWidget*>()) {
        const QWidget* page = tabPageOf(widget);
        const bool shown = page == nullptr ? widget->isVisibleTo(&dialog)
                                           : page == dialog.tabs()->widget(tab) && onItsTab(widget);
        if (!shown) {
            continue;
        }
        if (const auto* label = qobject_cast<const QLabel*>(widget); label && label->buddy()) {
            texts << label->text();
        } else if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
            texts << button->text();
        } else if (const auto* group = qobject_cast<const QGroupBox*>(widget)) {
            texts << group->title();
        }
    }
    return texts;
}

/// The texts with the same access key, or with a key that is not a Latin letter or digit (Alt
/// does not reach "ś"), on any tab in front; empty when each key is the only one
QString keyProblems(const ExportBookDialog& dialog) {
    QStringList problems;
    for (int tab = 0; tab < dialog.tabs()->count(); ++tab) {
        std::map<QChar, QStringList> texts;
        for (const QString& text : keyTexts(dialog, tab)) {
            const QChar key = accessKey(text);
            if (key.isNull()) {
                continue;
            }
            texts[key] << text;
            if (key.unicode() >= 0x80 || !key.isLetterOrNumber()) {
                problems << QStringLiteral("not a Latin key: ") + text;
            }
        }
        for (const auto& [key, each] : texts) {
            if (each.size() > 1) {
                problems << each.join(QStringLiteral(" / "));
            }
        }
    }
    problems.removeDuplicates();
    return problems.join(QStringLiteral("; "));
}

/// An exporter of a plugin: a PDF format with a tab of options, and a format without one
class PdfExporter : public core::IExporter {
public:
    std::string getPluginId() const override { return "test.pdf-exporter"; }
    std::string getVersion() const override { return "1.0.0"; }
    void onInit() override {}
    void onActivate() override {}
    std::vector<core::ExportFormatInfo> getFormats() const override {
        return {{"pdf", "PDF document", "pdf"}, {"txt", "Plain text", "txt"},
                {"docx", "Another Word", "docx"}, {"", "No id", "x"}};
    }
    QWidget* createOptionsPage(const std::string& format, QWidget* parent) override {
        if (format != "pdf") {
            return nullptr;
        }
        auto* page = new QWidget(parent);
        m_tagged = new QCheckBox(QStringLiteral("Tagged PDF"), page);
        // The window deletes the page; the exporter forgets its check box with it
        QObject::connect(page, &QObject::destroyed, [this] { m_tagged = nullptr; });
        return page;
    }
    bool exportDocument(const std::string& format, const std::string& filepath) override {
        written.emplace_back(format, filepath);
        tagged = m_tagged != nullptr && m_tagged->isChecked();
        return format == "pdf";
    }

    std::vector<std::pair<std::string, std::string>> written;
    bool tagged = false;

private:
    QCheckBox* m_tagged = nullptr;
};

/// The exporter registered while it lives
class RegisteredExporter {
public:
    RegisteredExporter() : exporter(std::make_shared<PdfExporter>()) {
        core::ExtensionPointRegistry::getInstance().registerPlugin(exporter);
    }
    ~RegisteredExporter() {
        core::ExtensionPointRegistry::getInstance().unregisterPlugin(exporter->getPluginId());
    }
    RegisteredExporter(const RegisteredExporter&) = delete;
    RegisteredExporter& operator=(const RegisteredExporter&) = delete;

    std::shared_ptr<PdfExporter> exporter;
};

/// The font of the program of @p pixels while it lives: the window fits with the font of
/// another system, whose letters are as high (Windows' Segoe UI has the lines of a 13 pixel
/// font, macOS has 13 pixels)
class AppFontOfPixels {
public:
    explicit AppFontOfPixels(int pixels) : m_font(QApplication::font()) {
        QFont font = m_font;
        font.setPixelSize(pixels);
        QApplication::setFont(font);
    }
    ~AppFontOfPixels() { QApplication::setFont(m_font); }
    AppFontOfPixels(const AppFontOfPixels&) = delete;
    AppFontOfPixels& operator=(const AppFontOfPixels&) = delete;

private:
    QFont m_font;
};

/// The windows in Polish while it lives: the translation the program ships
class Polish {
public:
    Polish() {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
    }
    ~Polish() { QCoreApplication::removeTranslator(&m_translator); }
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    bool m_loaded = false;
};

/// The window fits a small screen whole, so nothing in it scrolls: on Windows and Linux
/// 1366x768 at 150%, 911 pixels wide and 460 high. A Mac has no such scaling: its smallest
/// screen is 1366x768 at 100% (the 11-inch MacBook Air), 640 pixels high under the menu bar,
/// the title bar and the Dock, and its controls and spacing are larger
void checkFitsSmallScreen(const ExportBookDialog& dialog) {
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 640;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    CHECK(dialog.sizeHint().width() <= 911);
    CHECK(dialog.sizeHint().height() <= MAX_HEIGHT);
}

}  // namespace

TEST_CASE("Export Book: the format, the name and the options of each format",
          "[gui][dialogs][export]") {
    SettingsKept kept;
    QTemporaryDir dir;
    ExportBookDialog dialog(sampleBook(), editor::ExportFormat::Docx, dir.path(), {});

    // The command of the menu chose the format, in the list of the program's formats; the
    // name is the title of the book
    REQUIRE(dialog.formatBox()->count() >= 2);
    CHECK(dialog.formatBox()->itemText(0) == QStringLiteral("Word document (.docx)"));
    CHECK(dialog.formatBox()->itemText(1) == QStringLiteral("Markdown (.md)"));
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
                                QStringLiteral("Markdown (.md)"),
                                QStringLiteral("PDF document (.pdf)"),
                                QStringLiteral("Plain text (.txt)")}));
    CHECK(ExportBookDialog::exporterOf(QStringLiteral("pdf")) == registered.exporter);
    CHECK(ExportBookDialog::exporterOf(QStringLiteral("docx")) == nullptr);

    ExportBookDialog dialog(sampleBook(), editor::ExportFormat::Docx, dir.path(), {});
    QWidget* pdfPage = dialog.formatPage(QStringLiteral("pdf"));
    REQUIRE(pdfPage != nullptr);
    CHECK(dialog.formatPage(QStringLiteral("txt")) == nullptr);
    const int pdfTab = dialog.tabs()->indexOf(pdfPage);
    const int extrasTab = dialog.tabs()->indexOf(dialog.notesGroup()->parentWidget());
    CHECK(dialog.tabs()->tabText(pdfTab) == QStringLiteral("PDF document"));
    CHECK_FALSE(dialog.tabs()->isTabVisible(pdfTab));

    // PDF: its extension, its tab, and the layout of all the formats; the options of Word and
    // Markdown go
    dialog.setFormatId(QStringLiteral("pdf"));
    CHECK(dialog.choice().formatId == QStringLiteral("pdf"));
    CHECK(dialog.nameField()->text() == QStringLiteral("Wind over the Oasis.pdf"));
    CHECK(dialog.tabs()->isTabVisible(pdfTab));
    CHECK_FALSE(dialog.tabs()->isTabVisible(extrasTab));
    CHECK(onItsTab(dialog.titlesBox()));
    CHECK_FALSE(onItsTab(dialog.newPageBox()));

    // Plain text: no tab of its own
    dialog.setFormatId(QStringLiteral("txt"));
    CHECK(dialog.nameField()->text() == QStringLiteral("Wind over the Oasis.txt"));
    CHECK_FALSE(dialog.tabs()->isTabVisible(pdfTab));

    // Back to Word: its options, and no tab of the plugin
    dialog.setFormat(editor::ExportFormat::Docx);
    CHECK(dialog.nameField()->text() == QStringLiteral("Wind over the Oasis.docx"));
    CHECK(dialog.tabs()->isTabVisible(extrasTab));
    CHECK_FALSE(dialog.tabs()->isTabVisible(pdfTab));

    // The plugin writes the file with the options of its tab
    pdfPage->findChild<QCheckBox*>()->setChecked(true);
    const QString path = QDir(dir.path()).filePath(QStringLiteral("book.pdf"));
    int progress = 0;
    editor::ExportResult result;
    editor::ExportProblem problem;
    CHECK(BookExportCoordinator::exportWithPlugin(
        *registered.exporter, QStringLiteral("pdf"), path,
        [&progress](int percent) { progress = percent; }, &result, &problem));
    CHECK(registered.exporter->written ==
          std::vector<std::pair<std::string, std::string>>{{"pdf", path.toStdString()}});
    CHECK(registered.exporter->tagged);
    CHECK(progress == 100);
    CHECK(result.file == path);

    // A file the plugin does not write: what went wrong, for Copy
    CHECK_FALSE(BookExportCoordinator::exportWithPlugin(
        *registered.exporter, QStringLiteral("txt"), path, {}, &result, &problem));
    CHECK(problem.kind == editor::ExportProblem::Kind::Other);
    CHECK(problem.lines ==
          QStringList{QStringLiteral("test.pdf-exporter: the plugin did not write %1 in the "
                                     "format txt")
                          .arg(path)});
}

TEST_CASE("Export Book: four tabs, the window whole on a small screen, the access keys",
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

    const auto check = [&](editor::ExportFormat format) {
        ExportBookDialog dialog(book, format, dir.path(), {});
        REQUIRE(dialog.tabs()->count() == 4);
        CHECK(dialog.tabs()->currentIndex() == 0);
        CHECK(keyProblems(dialog).isEmpty());
        for (int tab = 0; tab < dialog.tabs()->count(); ++tab) {
            dialog.tabs()->setCurrentIndex(tab);
            checkFitsSmallScreen(dialog);
        }
        dialog.setContent(ExportBookDialog::Content::Chosen);
        checkFitsSmallScreen(dialog);
        return dialog.tabs()->tabText(0) + QStringLiteral(" | ") + dialog.tabs()->tabText(1) +
               QStringLiteral(" | ") + dialog.tabs()->tabText(2) + QStringLiteral(" | ") +
               dialog.tabs()->tabText(3);
    };

    SECTION("In English") {
        CHECK(check(editor::ExportFormat::Docx) ==
              QStringLiteral("Fi&le | Text&s | La&yout | Annotations and Ima&ges"));
        check(editor::ExportFormat::Markdown);
    }

    SECTION("In Polish") {
        Polish polish;
        REQUIRE(polish.loaded());
        CHECK(check(editor::ExportFormat::Docx) ==
              QStringLiteral("&Plik | T&eksty | U&kład | Uwagi i o&brazy"));
        check(editor::ExportFormat::Markdown);
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

TEST_CASE("Export Book: the commands wait for a book or a text", "[gui][export]") {
    BookExportCoordinator coordinator(kalahari::test::book(), kalahari::test::commands(), nullptr,
                                      []() { return nullptr; }, nullptr, nullptr, nullptr);
    CHECK(coordinator.whyNotExport() == QStringLiteral("Open a book or a text to export"));
}
