// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_import_book_dialog.cpp
/// @brief File > Import > DOCX, Markdown and Plain Text: the window of the import, its tabs on
///        a small screen and their access keys, the files and their order, the texts and
///        their kinds, plain text, the import into the open book and Undo Import, a new book,
///        the images of Internet addresses, and the choices kept for the next import

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/book_project.h"
#include "kalahari/core/extension_points.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_export.h"
#include "kalahari/editor/book_import.h"
#include "kalahari/gui/book_import_coordinator.h"
#include "kalahari/gui/book_styles.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/import_book_dialog.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari_tests/test_support/window_parts.h"

#include <QAbstractButton>
#include <QApplication>
#include <QScreen>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTranslator>
#include <QTreeWidget>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace kalahari;
using gui::BookImportCoordinator;
using gui::ImportFormat;
using gui::dialogs::ImportBookDialog;

namespace {

/// The settings of the import, as they were before the test
class SettingsKept {
   public:
    SettingsKept() {
        auto& settings = core::SettingsManager::getInstance();
        m_strings.emplace_back("import.target", settings.get<std::string>("import.target"));
        m_strings.emplace_back("project.lastFolders.newBook",
                               settings.get<std::string>("project.lastFolders.newBook"));
        m_strings.emplace_back("project.lastFolders.importText",
                               settings.get<std::string>("project.lastFolders.importText"));
        m_flags.emplace_back("import.wholeFiles", settings.get<bool>("import.wholeFiles"));
        m_flags.emplace_back("import.webImages", settings.get<bool>("import.webImages"));
        for (const std::string format : {"docx", "markdown", "text"}) {
            const std::string prefix = "import." + format + ".";
            for (const char* key : {"headingAsTitle", "dialogueStyle", "typography", "doubleSpaces",
                                    "leadingSpaces", "sceneMarkers"}) {
                m_flags.emplace_back(prefix + key, settings.get<bool>(prefix + key));
            }
            m_strings.emplace_back(prefix + "emptyParagraphs",
                                   settings.get<std::string>(prefix + "emptyParagraphs"));
        }
        m_strings.emplace_back("import.docx.formatting",
                               settings.get<std::string>("import.docx.formatting"));
        m_flags.emplace_back("import.docx.comments", settings.get<bool>("import.docx.comments"));
        m_flags.emplace_back("import.docx.rejectChanges",
                             settings.get<bool>("import.docx.rejectChanges"));
        for (const char* key : {"import.text.split", "import.text.titles", "import.text.prefix",
                                "import.text.parts", "import.text.style"}) {
            m_strings.emplace_back(key, settings.get<std::string>(key));
        }
        m_numbers.emplace_back("import.text.emptyLines",
                               settings.get<int>("import.text.emptyLines"));
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
                settings.set<int>(key, value);
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
    std::vector<std::pair<std::string, int>> m_numbers;
};

/// A novel "Szkło" open in the book of the tests: a title page and a chapter
class Novel {
   public:
    Novel() {
        REQUIRE(m_folder.isValid());
        core::ProjectManager::loadBookTypes(
            {QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        REQUIRE(test::book().createProject(m_folder.path(), QStringLiteral("Szkło"),
                                           QStringLiteral("Anna"), QStringLiteral("pl"), true,
                                           QStringLiteral("kalahari.novel")));
    }
    ~Novel() {
        try {
            test::book().closeProject(false);
        } catch (...) {
            std::fputs("Novel: the book of the test is still open\n", stderr);
        }
    }
    Novel(const Novel&) = delete;
    Novel& operator=(const Novel&) = delete;

    core::ProjectManager& book() { return test::book(); }

    /// What the window says about the book
    static ImportBookDialog::Book dialogBook() {
        const BookImportCoordinator coordinator(test::book(), test::commands(), {}, nullptr);
        return coordinator.dialogBook();
    }

    /// The titles of the elements of the main text
    QStringList mainTitles() {
        QStringList titles;
        for (const core::ProjectElement& element : book().book()->mainElements) {
            titles << element.title;
        }
        return titles;
    }

   private:
    QTemporaryDir m_folder;
};

/// Files of the test in a folder of their own
class Files {
   public:
    Files() { REQUIRE(m_folder.isValid()); }

    /// A file of @p text named @p name; its path
    QString write(const QString& name, const QByteArray& text) const {
        const QString path = QDir(m_folder.path()).filePath(name);
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write(text);
        return path;
    }

    /// A Word document of @p chapters, each with a paragraph; its path
    QString writeDocx(const QString& name, const QStringList& chapters) const {
        editor::BookExport book;
        book.title = QStringLiteral("Szklane miasto");
        book.author = QStringLiteral("Anna");
        book.language = QStringLiteral("pl");
        book.styles = gui::newBookParagraphStyles(QStringLiteral("kalahari.novel"));
        for (const QString& chapter : chapters) {
            editor::ExportItem item;
            item.title = chapter;
            item.titleStyle = QStringLiteral("chapter_title");
            item.kml = QStringLiteral("<kml><p>Treść: %1.</p></kml>").arg(chapter);
            book.items.push_back(item);
        }
        const QString path = QDir(m_folder.path()).filePath(name);
        REQUIRE(editor::exportDocx(book, path, {}, nullptr, nullptr));
        return path;
    }

    QString path() const { return m_folder.path(); }

   private:
    QTemporaryDir m_folder;
};

/// The Markdown of chapters @p from to @p to, a paragraph each
QByteArray chapters(int from, int to) {
    QByteArray text;
    for (int i = from; i <= to; ++i) {
        text += "# Rozdział " + QByteArray::number(i) + "\n\nNoc " + QByteArray::number(i) +
                " była długa.\n\n";
    }
    return text;
}

/// The window's import into the open book, and Undo Import
ImportBookDialog::Actions intoBook() {
    ImportBookDialog::Actions actions;
    actions.import = [](const ImportBookDialog::Choice& choice,
                        const std::vector<gui::ImportFile>& files,
                        const std::vector<gui::ImportRow>& rows, const gui::ImportKinds& kinds,
                        const gui::dialogs::ArchiveDialog::Progress& progress,
                        ImportBookDialog::Outcome* outcome, QString* problem,
                        QStringList* details) {
        if (choice.target == ImportBookDialog::Target::NewBook) {
            return BookImportCoordinator::importIntoNewBook(choice, files, rows, kinds, progress,
                                                            outcome, problem, details);
        }
        return BookImportCoordinator::importInto(test::book(), choice, files, rows, kinds, progress,
                                                 outcome, problem, details);
    };
    actions.undo = [](const ImportBookDialog::Outcome& outcome) {
        return gui::removeImported(test::book(), outcome.added);
    };
    actions.showInNavigator = [](const ImportBookDialog::Outcome&) {
    };
    actions.openFirstText = [](const ImportBookDialog::Outcome&) {
    };
    return actions;
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

/// The texts with an access key while tab @p tab is in front: the titles of the visible tabs,
/// and the labels of fields, the buttons and the groups on that tab or outside the tabs
QStringList keyTexts(const ImportBookDialog& dialog, int tab) {
    QStringList texts;
    for (int i = 0; i < dialog.tabs()->count(); ++i) {
        if (dialog.tabs()->isTabVisible(i)) {
            texts << dialog.tabs()->tabText(i);
        }
    }
    for (const QWidget* widget : dialog.findChildren<QWidget*>()) {
        const QWidget* page = tabPageOf(widget);
        // The pages of the place that are not in front (a QStackedWidget inside the tab)
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

/// The texts with the same access key, or with a key that is not a Latin letter or digit, on
/// any tab in front; empty when each key is the only one
QString keyProblems(const ImportBookDialog& dialog) {
    QStringList problems;
    for (int tab = 0; tab < dialog.tabs()->count(); ++tab) {
        if (!dialog.tabs()->isTabVisible(tab)) {
            continue;
        }
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

/// The font of the program of @p pixels while it lives
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

/// The window fits a small screen whole: 1366x768 at 150% on Windows and Linux, 911 pixels
/// wide and 460 high; a Mac's smallest screen leaves 640 pixels under its bars
void checkFitsSmallScreen(const ImportBookDialog& dialog) {
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 640;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    CHECK(dialog.sizeHint().width() <= 911);
    CHECK(dialog.sizeHint().height() <= MAX_HEIGHT);
}

/// The titles of the list of the texts, a part's texts after "  "
QStringList listTitles(const ImportBookDialog& dialog) {
    QStringList titles;
    const QTreeWidget* list = dialog.textList();
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* item = list->topLevelItem(i);
        titles << item->text(0);
        for (int j = 0; j < item->childCount(); ++j) {
            titles << QStringLiteral("  ") + item->child(j)->text(0);
        }
    }
    return titles;
}

/// The names of the files of the list
QStringList fileNames(const ImportBookDialog& dialog) {
    QStringList names;
    for (int i = 0; i < dialog.fileList()->topLevelItemCount(); ++i) {
        names << dialog.fileList()->topLevelItem(i)->text(0);
    }
    return names;
}

/// The texts of the options of @p page in front with the same access key, without one, or
/// with a key that is not a Latin letter or digit; empty when each option has its own key
QString pageKeyProblems(const QWidget& page) {
    QStringList problems;
    std::map<QChar, QStringList> texts;
    for (const QWidget* widget : page.findChildren<QWidget*>()) {
        if (!widget->isVisibleTo(&page)) {
            continue;
        }
        QString text;
        if (const auto* label = qobject_cast<const QLabel*>(widget); label && label->buddy()) {
            text = label->text();
        } else if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
            text = button->text();
        } else {
            continue;
        }
        const QChar key = accessKey(text);
        if (key.isNull()) {
            problems << QStringLiteral("no key: ") + text;
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
    return problems.join(QStringLiteral("; "));
}

/// The opener of the Settings of the gears while it lives
class SettingsOpener {
   public:
    explicit SettingsOpener(std::function<void(QWidget*, const QString&)> opener) {
        gui::dialogs::KalahariDialog::setSettingsOpener(std::move(opener));
    }
    ~SettingsOpener() { gui::dialogs::KalahariDialog::setSettingsOpener(nullptr); }
    SettingsOpener(const SettingsOpener&) = delete;
    SettingsOpener& operator=(const SettingsOpener&) = delete;
};

/// A web server of the test on this computer: a picture at /map.png, nothing anywhere else
class PictureServer {
   public:
    PictureServer() {
        QImage image(4, 3, QImage::Format_RGB32);
        image.fill(Qt::darkGreen);
        QBuffer buffer(&m_picture);
        REQUIRE(buffer.open(QIODevice::WriteOnly));
        REQUIRE(image.save(&buffer, "PNG"));
        REQUIRE(m_server.listen(QHostAddress::LocalHost));
        QObject::connect(&m_server, &QTcpServer::newConnection, [this]() {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                auto request = std::make_shared<QByteArray>();
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, request]() {
                    *request += socket->readAll();
                    if (request->contains("\r\n\r\n")) {
                        answer(socket, request->split(' ').value(1));
                    }
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket,
                                 &QObject::deleteLater);
            }
        });
    }
    PictureServer(const PictureServer&) = delete;
    PictureServer& operator=(const PictureServer&) = delete;

    /// The address of @p path on the server
    QString url(const QString& path) const {
        return QStringLiteral("http://127.0.0.1:%1/%2").arg(m_server.serverPort()).arg(path);
    }

    /// The paths asked, in the order they came
    const QStringList& asked() const { return m_asked; }

    /// The picture at /map.png
    const QByteArray& picture() const { return m_picture; }

   private:
    void answer(QTcpSocket* socket, const QByteArray& path) {
        m_asked << QString::fromLatin1(path);
        if (path == "/map.png") {
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " +
                          QByteArray::number(m_picture.size()) +
                          "\r\nConnection: close\r\n\r\n" + m_picture);
        } else {
            socket->write(
                "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        }
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QByteArray m_picture;
    QStringList m_asked;
};

/// The text of element @p id at the top of the main text of the open book, as its file has it
QString mainText(const QString& id) {
    for (const core::ProjectElement& element : test::book().book()->mainElements) {
        if (element.id == id) {
            QFile file(QDir(test::book().getProjectPath()).filePath(element.file));
            REQUIRE(file.open(QIODevice::ReadOnly));
            return QString::fromUtf8(file.readAll());
        }
    }
    return QString();
}

/// An importer of a plugin: screenplays with a tab of options, each scene a text, and formats
/// the program leaves out
class ScreenplayImporter : public core::IImporter {
   public:
    std::string getPluginId() const override { return "test.screenplay-importer"; }
    std::string getVersion() const override { return "1.0.0"; }
    void onInit() override {}
    void onActivate() override {}
    std::vector<core::ImportFormatInfo> getFormats() const override {
        return {{"screenplay", "Screenplay", {"fountain", ".SPMD"}},
                {"markdown", "Another Markdown", {"md"}},  // the program's own
                {"bare", "No extension", {}},
                {"", "No id", {"x"}}};
    }
    QWidget* createOptionsPage(const std::string& format, QWidget* parent) override {
        if (format != "screenplay") {
            return nullptr;
        }
        auto* page = new QWidget(parent);
        m_scenes = new QCheckBox(QStringLiteral("Each &scene as a text"), page);
        m_scenes->setChecked(true);
        // The window deletes the page; the importer forgets its check box with it
        QObject::connect(page, &QObject::destroyed, [this] { m_scenes = nullptr; });
        return page;
    }
    std::vector<core::ImportTextInfo> importDocument(const std::string& /*format*/,
                                                     const std::string& filepath) override {
        ++reads;
        if (QString::fromStdString(filepath).endsWith(QLatin1String("broken.fountain"))) {
            throw std::runtime_error("line 3: no scene heading");
        }
        if (m_scenes != nullptr && !m_scenes->isChecked()) {
            return {{"", "Szklane miasto"},
                    {"Cały scenariusz", "Anna czyta list.\n\nWiatr nad pustynią."}};
        }
        return {{"", "Szklane miasto"},
                {"INT. DOM – NOC", "Anna *czyta* list."},
                {"EXT. PUSTYNIA – DZIEŃ", "Wiatr nad pustynią."}};
    }

    int reads = 0;  ///< The files read

   private:
    QCheckBox* m_scenes = nullptr;
};

/// The importer of a plugin registered while it lives
class RegisteredImporter {
   public:
    RegisteredImporter() : importer(std::make_shared<ScreenplayImporter>()) {
        core::ExtensionPointRegistry::getInstance().registerPlugin(importer);
    }
    ~RegisteredImporter() {
        core::ExtensionPointRegistry::getInstance().unregisterPlugin(importer->getPluginId());
    }
    RegisteredImporter(const RegisteredImporter&) = delete;
    RegisteredImporter& operator=(const RegisteredImporter&) = delete;

    std::shared_ptr<ScreenplayImporter> importer;
};

}  // namespace

TEST_CASE("Import Text: six tabs, the window whole on a small screen, the access keys",
          "[gui][dialogs][import]") {
    SettingsKept kept;
    Novel novel;
    Files files;
    // More files and texts than the lists show, a long title of the book
    QStringList markdown;
    for (int i = 1; i <= 6; ++i) {
        markdown << files.write(QStringLiteral("Część %1.md").arg(i), chapters(i * 10, i * 10 + 5));
    }
    // An image of an Internet address: the option of its download shows too, off
    core::SettingsManager::getInstance().set<bool>("import.webImages", false);
    markdown << files.write(QStringLiteral("Część 7.md"),
                            chapters(70, 71) + "![Mapa](https://example.com/mapa.png)\n");
    QStringList titles;
    for (int i = 1; i <= 30; ++i) {
        titles << QStringLiteral("Rozdział %1").arg(i);
    }
    const QString docx = files.writeDocx(QStringLiteral("Szklane miasto.docx"), titles);
    const QString text = files.write(QStringLiteral("Szkic.txt"),
                                     "Rozdział 1\n\nPiasek.\n\nRozdział 2\n\nWiatr.\n");
    ImportBookDialog::Book book = Novel::dialogBook();
    book.title = QStringLiteral(
        "Szklane miasto, w którym karawana czekała trzy dni na wiatr znad pustyni");

    const auto check = [&](ImportFormat format, const QStringList& paths) {
        ImportBookDialog dialog(book, core::ProjectManager::installedBookTypes(), format, paths, {});
        REQUIRE(dialog.tabs()->count() == 6);
        CHECK(dialog.tabs()->currentIndex() == 0);
        CHECK(dialog.tabs()->isTabVisible(5) == (format != ImportFormat::PlainText));
        for (const ImportBookDialog::Target target :
             {ImportBookDialog::Target::Book, ImportBookDialog::Target::NewBook,
              ImportBookDialog::Target::OtherFiles}) {
            dialog.setTarget(target);
            INFO("Target " << static_cast<int>(target) << ", format " << static_cast<int>(format));
            CHECK(keyProblems(dialog).toStdString().empty());
            for (int tab = 0; tab < dialog.tabs()->count(); ++tab) {
                dialog.tabs()->setCurrentIndex(tab);
                checkFitsSmallScreen(dialog);
            }
        }
        QStringList tabs;
        for (int i = 0; i < dialog.tabs()->count(); ++i) {
            tabs << dialog.tabs()->tabText(i);
        }
        return tabs.join(QStringLiteral(" | "));
    };
    const auto checkAll = [&]() {
        const QString tabs = check(ImportFormat::Markdown, markdown);
        check(ImportFormat::Docx, {docx});
        check(ImportFormat::PlainText, {text});
        return tabs;
    };

    SECTION("In English") {
        CHECK(checkAll() ==
              QStringLiteral(
                  "&File | &Place | &Division | &Styles | &Cleanup | Annotations and Ima&ges"));
    }

    SECTION("In Polish") {
        Polish polish;
        REQUIRE(polish.loaded());
        CHECK(checkAll() ==
              QStringLiteral("&Plik | &Miejsce | Po&dział | &Style | Por&ządki | Uwagi i o&brazy"));
        // "No parts" in one line: with the notes for the translators it made the window too
        // wide on macOS
        ImportBookDialog headings(book, core::ProjectManager::installedBookTypes(),
                                  ImportFormat::Markdown, markdown, {});
        CHECK(headings.partLevelBox()->itemText(0) == QStringLiteral("– bez części –"));
        ImportBookDialog plain(book, core::ProjectManager::installedBookTypes(),
                               ImportFormat::PlainText, {text}, {});
        CHECK(plain.partPatternBox()->itemText(2) == QStringLiteral("– bez części –"));
    }

    // The result of the import shows under the tabs, with its links and what it left out
    const auto checkResult = [&]() {
        ImportBookDialog::Actions actions = intoBook();
        ImportBookDialog dialog(book, core::ProjectManager::installedBookTypes(),
                                ImportFormat::Markdown, markdown, std::move(actions));
        dialog.tabs()->setCurrentIndex(2);
        dialog.accept();
        REQUIRE(dialog.isDone());
        CHECK(dialog.undoLink()->isVisibleTo(&dialog));
        CHECK(dialog.openTextLink()->isVisibleTo(&dialog));
        CHECK_FALSE(dialog.progressBar()->isVisibleTo(&dialog));
        checkFitsSmallScreen(dialog);
        dialog.undoImport();
    };

    SECTION("With the result of the import, in English") {
        checkResult();
    }

    SECTION("With the result of the import, in Polish") {
        Polish polish;
        REQUIRE(polish.loaded());
        checkResult();
    }

    SECTION("In a font of 13 pixels, in Polish") {
        AppFontOfPixels font(13);
        Polish polish;
        REQUIRE(polish.loaded());
        checkAll();
        checkResult();
    }
}

TEST_CASE("Import Text: the window opens higher than its content, the lists take the height",
          "[gui][dialogs][import]") {
    SettingsKept kept;
    Novel novel;
    Files files;
    QStringList titles;
    for (int i = 1; i <= 30; ++i) {
        titles << QStringLiteral("Rozdział %1").arg(i);
    }
    const QString docx = files.writeDocx(QStringLiteral("Szklane miasto.docx"), titles);
    ImportBookDialog dialog(Novel::dialogBook(), core::ProjectManager::installedBookTypes(),
                            ImportFormat::Docx, {docx}, {});
    dialog.setTarget(ImportBookDialog::Target::Book);
    dialog.show();
    QApplication::processEvents();

    // About 100 px higher than the content asks for, within the screen
    const QRect screen = dialog.screen()->availableGeometry();
    CHECK(dialog.height() > dialog.sizeHint().height());
    CHECK((dialog.height() >= dialog.sizeHint().height() + 100 ||
           dialog.frameGeometry().height() >= screen.height() - 40));

    // Each list higher than its rows ask for: the files, the place, the texts, the styles
    const auto* place = dialog.picker()->findChild<QTreeWidget*>();
    REQUIRE(place != nullptr);
    const QList<std::pair<int, const QTreeWidget*>> lists{{0, dialog.fileList()},
                                                          {1, place},
                                                          {2, dialog.textList()},
                                                          {3, dialog.styleList()}};
    for (const auto& [tab, list] : lists) {
        dialog.tabs()->setCurrentIndex(tab);
        QApplication::processEvents();
        INFO("Tab " << tab);
        CHECK(list->isVisible());
        CHECK(list->height() > list->sizeHint().height());
    }
}

TEST_CASE("Import Text: the files in the order of their names, moved, added, and what is in them",
          "[gui][dialogs][import]") {
    SettingsKept kept;
    Novel novel;
    Files files;
    const QString ten = files.write(QStringLiteral("Rozdział 10.md"), chapters(10, 10));
    const QString two = files.write(QStringLiteral("Rozdział 2.md"),
                                    "---\ntitle: Szkło\n---\n" + chapters(2, 3));
    ImportBookDialog dialog(Novel::dialogBook(), core::ProjectManager::installedBookTypes(),
                            ImportFormat::Markdown, {ten, two}, intoBook());

    // The command of the menu chose the format; the files in the order of the numbers
    CHECK(dialog.formatBox()->currentData().toInt() == static_cast<int>(ImportFormat::Markdown));
    CHECK(fileNames(dialog) ==
          QStringList({QStringLiteral("Rozdział 2.md"), QStringLiteral("Rozdział 10.md")}));
    CHECK(dialog.contentsText() ==
          QStringLiteral("3 paragraphs, 3 headings 1. From its properties: the title \"Szkło\"."));
    // More files: each one text at first, its heading its title
    CHECK(dialog.wholeFilesBox()->isChecked());
    CHECK(listTitles(dialog) ==
          QStringList({QStringLiteral("Rozdział 2"), QStringLiteral("Rozdział 10")}));
    dialog.wholeFilesBox()->setChecked(false);
    CHECK(listTitles(dialog) ==
          QStringList({QStringLiteral("Rozdział 2"), QStringLiteral("Rozdział 3"),
                       QStringLiteral("Rozdział 10")}));
    CHECK(dialog.encodingBox()->itemText(0) == QStringLiteral("Detect (found: UTF-8)"));
    CHECK(dialog.previewText().startsWith(QStringLiteral("---\ntitle: Szkło")));
    CHECK(dialog.acceptButton()->isEnabled());

    // The second file first: its texts first
    dialog.fileList()->setCurrentItem(dialog.fileList()->topLevelItem(1));
    dialog.moveFile(-1);
    CHECK(fileNames(dialog) ==
          QStringList({QStringLiteral("Rozdział 10.md"), QStringLiteral("Rozdział 2.md")}));
    CHECK(listTitles(dialog).front() == QStringLiteral("Rozdział 10"));
    CHECK(dialog.choice().files.front() == QFileInfo(ten).absoluteFilePath());

    // Added: each file once, the new ones after
    const QString one = files.write(QStringLiteral("Rozdział 1.md"), chapters(1, 1));
    dialog.addFiles({two, one});
    CHECK(fileNames(dialog) ==
          QStringList({QStringLiteral("Rozdział 10.md"), QStringLiteral("Rozdział 2.md"),
                       QStringLiteral("Rozdział 1.md")}));
    CHECK(dialog.wholeFilesBox()->isVisibleTo(dialog.tabs()->widget(2)));

    // Each file as one text again
    dialog.wholeFilesBox()->setChecked(true);
    CHECK(listTitles(dialog) ==
          QStringList({QStringLiteral("Rozdział 10"), QStringLiteral("Rozdział 2"),
                       QStringLiteral("Rozdział 1")}));

    // A file of another format cannot be read: the others go
    dialog.setFormat(ImportFormat::Docx);
    CHECK(dialog.filesNote().contains(
        QStringLiteral("The file \"Rozdział 10.md\" is not a Word document (DOCX).")));
    CHECK(dialog.fileList()->topLevelItem(0)->text(1) == QStringLiteral("cannot be read"));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    dialog.setFormat(ImportFormat::Markdown);
    CHECK(dialog.filesNote().isEmpty());

    // Taken out of the list
    dialog.fileList()->setCurrentItem(dialog.fileList()->topLevelItem(0));
    dialog.removeFile();
    CHECK(fileNames(dialog) ==
          QStringList({QStringLiteral("Rozdział 2.md"), QStringLiteral("Rozdział 1.md")}));
    dialog.removeFile();
    dialog.removeFile();
    CHECK(fileNames(dialog).isEmpty());
    CHECK(dialog.contentsText() == QStringLiteral("No files."));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
}

TEST_CASE("Import Text: the kinds of the texts, and those the book has no room for",
          "[gui][dialogs][import]") {
    SettingsKept kept;
    Novel novel;
    Files files;
    const QString path = files.write(
        QStringLiteral("Szkło.md"),
        "# Prolog\n\nNoc.\n\n# Rozdział 1\n\nRano.\n\n# Prolog\n\nDrugi.\n\n"
        "# Podziękowania\n\nDziękuję.\n");
    ImportBookDialog dialog(Novel::dialogBook(), core::ProjectManager::installedBookTypes(),
                            ImportFormat::Markdown, {path}, intoBook());
    REQUIRE(dialog.rows().size() == 4);
    dialog.tabs()->setCurrentIndex(2);  // the notes of the list show on its tab
    const auto kindOf = [&dialog](size_t row) {
        return dialog.rows()[row].kind.section(QLatin1Char(':'), -1);
    };
    CHECK(kindOf(0) == QStringLiteral("prologue"));
    CHECK(kindOf(1) == QStringLiteral("chapter"));
    CHECK(kindOf(3) == QStringLiteral("acknowledgments"));
    // The book can have one prologue: the second one is unchecked
    CHECK_FALSE(dialog.rows()[2].checked);
    const QTreeWidgetItem* second = dialog.textList()->topLevelItem(2);
    CHECK(second->checkState(0) == Qt::Unchecked);
    CHECK(dialog.textsNote() == QStringLiteral("3 of 4 texts go to the book (3 words).\nThe texts "
                                               "of a kind the book cannot have more of "
                                               "are unchecked; change their kind to add them."));
    CHECK(dialog.acceptButton()->isEnabled());

    // Checked by the writer: the window says why it cannot import
    dialog.textList()->topLevelItem(2)->setCheckState(0, Qt::Checked);
    const QString prologue = dialog.kinds().find(dialog.rows()[2].kind)->name;
    CHECK(dialog.textsNote() ==
          QStringLiteral(
              "4 of 4 texts go to the book (4 words).\nThe book cannot have more texts of the kind "
              "\"%1\": uncheck them or change their kind.")
              .arg(prologue));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());

    // Its kind changed to a chapter, as the list of the kinds does
    dialog.textList()->setCurrentItem(dialog.textList()->topLevelItem(2));
    CHECK(dialog.kindBox()->isEnabled());
    const int chapter = dialog.kindBox()->findData(dialog.kinds().main.reference());
    REQUIRE(chapter >= 0);
    dialog.kindBox()->setCurrentIndex(chapter);
    emit dialog.kindBox()->activated(chapter);
    CHECK(kindOf(2) == QStringLiteral("chapter"));
    CHECK(dialog.textList()->topLevelItem(2)->text(1) ==
          dialog.kinds().find(dialog.kinds().main.reference())->name);
    CHECK(dialog.acceptButton()->isEnabled());

    // The title changed in the list
    dialog.textList()->topLevelItem(2)->setText(0, QStringLiteral("Drugi poranek"));
    CHECK(dialog.rows()[2].title == QStringLiteral("Drugi poranek"));

    // Into the book: the prologue at the start, the acknowledgments in the back matter
    dialog.accept();
    REQUIRE(dialog.isDone());
    CHECK(dialog.resultText() == QStringLiteral("Added 4 texts to the book \"Szkło\" (4 words)."));
    const QStringList main = novel.mainTitles();
    CHECK(main.front() == QStringLiteral("Prolog"));
    CHECK(main.contains(QStringLiteral("Drugi poranek")));
    QStringList back;
    for (const core::ProjectElement& element : novel.book().book()->backElements) {
        back << element.title;
    }
    CHECK(back.contains(QStringLiteral("Podziękowania")));
    CHECK_FALSE(dialog.outcome().firstText.isEmpty());

    // Undo Import: the book as it was
    dialog.undoImport();
    CHECK(dialog.resultText() ==
          QStringLiteral("The import is undone: the book is as it was before it."));
    CHECK_FALSE(novel.mainTitles().contains(QStringLiteral("Prolog")));
    CHECK_FALSE(dialog.undoLink()->isVisibleTo(&dialog));
}

TEST_CASE("Import Text: plain text: its encoding, its titles, the empty lines between the texts",
          "[gui][dialogs][import]") {
    SettingsKept kept;
    Novel novel;
    Files files;
    // Windows-1250: "Rozdział" and "Żółć", the paragraphs separated by empty lines
    const QString path = files.write(
        QStringLiteral("rękopis.txt"),
        "Rozdzia\xB3 1\n\nPierwsza linia\nakapitu.\n\n\xAF\xF3\xB3\xE6.\n\n\n\n"
        "Rozdzia\xB3 2\n\nWiatr.\n");
    ImportBookDialog dialog(Novel::dialogBook(), core::ProjectManager::installedBookTypes(),
                            ImportFormat::PlainText, {path}, intoBook());
    CHECK(dialog.encodingBox()->itemText(0) ==
          QStringLiteral("Detect (found: Windows-1250 (Central European))"));
    CHECK(dialog.previewText() == QStringLiteral("Rozdział 1\nPierwsza linia"));
    CHECK(dialog.fileList()->topLevelItem(0)->text(1) ==
          QStringLiteral("Windows-1250 (Central European)"));
    // The book is Polish: the titles "Rozdział 1"
    CHECK(dialog.titlePatternBox()->currentData().toInt() ==
          static_cast<int>(editor::TitlePattern::Polish));
    CHECK(dialog.headingsButton()->isChecked());
    CHECK(dialog.headingsButton()->text() == QStringLiteral("At the &titles"));
    CHECK(dialog.emptyLineButton()->isChecked() == false);
    CHECK(listTitles(dialog) ==
          QStringList({QStringLiteral("Rozdział 1"), QStringLiteral("Rozdział 2")}));
    CHECK_FALSE(dialog.tabs()->isTabVisible(5));

    SECTION("The paragraphs separated by empty lines: their lines joined") {
        dialog.emptyLineButton()->setChecked(true);
        REQUIRE(dialog.files().front().isRead());
        const editor::ImportDocument& document = *dialog.files().front().document;
        bool joined = false;
        for (const editor::ImportBlock& block : document.blocks) {
            joined = joined ||
                     editor::importPlainText(block) == QStringLiteral("Pierwsza linia akapitu.");
        }
        CHECK(joined);
    }

    SECTION("The texts after three empty lines") {
        dialog.pagesButton()->setChecked(true);
        CHECK(dialog.emptyLinesBox()->value() == 3);
        REQUIRE(dialog.rows().size() == 2);
        // A text without a title: the start of its first paragraph
        CHECK(dialog.rows()[0].title == QStringLiteral("Rozdział 1"));
        CHECK(dialog.rows()[1].title == QStringLiteral("Rozdział 2"));
    }

    SECTION("Another encoding: the letters as it reads them") {
        dialog.encodingBox()->setCurrentIndex(
            dialog.encodingBox()->findData(static_cast<int>(editor::TextEncoding::Utf8)));
        CHECK(dialog.fileList()->topLevelItem(0)->text(1) == QStringLiteral("UTF-8"));
        CHECK(dialog.previewText() != QStringLiteral("Rozdział 1\nPierwsza linia"));
        // Its titles are not found: the whole file one text
        REQUIRE(dialog.rows().size() == 1);
        CHECK(dialog.rows()[0].kind == dialog.kinds().main.reference());
        CHECK(dialog.rows()[0].title == QStringLiteral("rękopis"));
        dialog.encodingBox()->setCurrentIndex(0);
    }

    // The paragraphs in the style chosen, the choices kept for the next plain text
    dialog.textStyleBox()->setCurrentIndex(
        dialog.textStyleBox()->findData(QStringLiteral("normal")));
    dialog.doubleSpacesBox()->setChecked(false);
    CHECK(dialog.acceptButton()->isEnabled());
    dialog.accept();
    REQUIRE(dialog.isDone());
    const auto& settings = core::SettingsManager::getInstance();
    CHECK_FALSE(settings.get<bool>("import.text.doubleSpaces"));
    CHECK(settings.get<bool>("import.markdown.doubleSpaces"));
    CHECK(settings.get<std::string>("import.text.titles") == "polish");
    ImportBookDialog next(Novel::dialogBook(), core::ProjectManager::installedBookTypes(),
                          ImportFormat::PlainText, {path}, {});
    CHECK_FALSE(next.doubleSpacesBox()->isChecked());
    next.setFormat(ImportFormat::Markdown);
    CHECK(next.doubleSpacesBox()->isChecked());
    dialog.undoImport();
}

TEST_CASE("Import Text: the styles of a Word document, a new style of the book, the comments",
          "[gui][dialogs][import]") {
    SettingsKept kept;
    Novel novel;
    Files files;
    const QString docx = files.writeDocx(QStringLiteral("Szklane miasto.docx"),
                                         {QStringLiteral("Huta"), QStringLiteral("Wiatr")});
    ImportBookDialog dialog(Novel::dialogBook(), core::ProjectManager::installedBookTypes(),
                            ImportFormat::Docx, {docx}, intoBook());
    REQUIRE(dialog.files().front().isRead());
    CHECK(dialog.formattingBox()->currentData().toInt() ==
          static_cast<int>(editor::ImportFormatting::Emphasis));
    CHECK(dialog.typographyBox()->isChecked());
    CHECK(dialog.doubleSpacesBox()->isChecked());
    CHECK(dialog.leadingSpacesBox()->isChecked());
    CHECK(dialog.sceneMarkersBox()->isChecked());
    CHECK(dialog.emptyBox()->currentData().toInt() ==
          static_cast<int>(editor::EmptyParagraphs::Remove));
    CHECK(dialog.imagesText() == QStringLiteral("The file has no images."));
    CHECK_FALSE(dialog.commentsBox()->isEnabled());
    CHECK(listTitles(dialog) == QStringList({QStringLiteral("Huta"), QStringLiteral("Wiatr")}));

    // The styles of the file, each with the book's style the program suggests
    const QTreeWidget* styles = dialog.styleList();
    REQUIRE(styles->topLevelItemCount() > 0);
    QComboBox* normal = nullptr;
    QString normalName;
    for (const gui::ImportFileStyle& style : gui::importFileStyles(dialog.files())) {
        QComboBox* box = dialog.styleChoice(style.name);
        REQUIRE(box != nullptr);
        CHECK_FALSE(box->currentData().toString().isEmpty());
        if (box->currentData().toString() == QStringLiteral("normal")) {
            normal = box;
            normalName = style.name;
        }
    }
    REQUIRE(normal != nullptr);

    // A new style of the book for the paragraphs of the file: Undo Import takes it out
    const int made = normal->findData(QLatin1String(gui::NEW_STYLE_PREFIX) + normalName);
    REQUIRE(made >= 0);
    normal->setCurrentIndex(made);
    dialog.accept();
    REQUIRE(dialog.isDone());
    CHECK(dialog.outcome().styles == 1);
    CHECK(dialog.resultText().endsWith(QStringLiteral("The book has new styles (1).")));
    const QString id = dialog.outcome().added.styles.value(0);
    CHECK(gui::bookParagraphStyles(novel.book()).contains(id));
    dialog.undoImport();
    CHECK_FALSE(gui::bookParagraphStyles(novel.book()).contains(id));
}

TEST_CASE("Import Text: a new book, and the group Other Files", "[gui][dialogs][import]") {
    SettingsKept kept;
    Files files;
    const QString path = files.write(QStringLiteral("Szklane miasto.md"), chapters(1, 2));
    const QString parent = QDir(files.path()).filePath(QStringLiteral("Książki"));
    REQUIRE(QDir().mkpath(parent));

    SECTION("Without a book: a new book, which opens when the window closes") {
        ImportBookDialog dialog(ImportBookDialog::Book(),
                                core::ProjectManager::installedBookTypes(), ImportFormat::Markdown,
                                {path}, intoBook());
        CHECK(dialog.newBookButton()->isChecked());
        CHECK_FALSE(dialog.bookButton()->isVisibleTo(&dialog));
        CHECK(dialog.titleField()->text() == QStringLiteral("Szklane miasto"));
        dialog.tabs()->setCurrentIndex(1);  // the note of the folder shows on its tab
        dialog.setNewBookFolder(parent);
        const QString folder = QDir(parent).filePath(QStringLiteral("Szklane miasto"));
        CHECK(dialog.newBookNote() == QStringLiteral("The book is made in the folder %1.")
                                          .arg(QDir::toNativeSeparators(folder)));
        dialog.titleField()->clear();
        CHECK(dialog.newBookNote() == QStringLiteral("Type the title of the book."));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        dialog.titleField()->setText(QStringLiteral("Szklane miasto"));
        CHECK(dialog.acceptButton()->isEnabled());

        dialog.accept();
        REQUIRE(dialog.isDone());
        const QString manifest = dialog.outcome().bookFile;
        REQUIRE(QFileInfo::exists(manifest));
        CHECK(dialog.resultText() ==
              QStringLiteral("Made the book \"Szklane miasto\" of 2 texts in the folder %1. "
                             "It opens when this window closes.")
                  .arg(QDir::toNativeSeparators(folder)));
        CHECK_FALSE(dialog.undoLink()->isVisibleTo(&dialog));
        CHECK_FALSE(dialog.openTextLink()->isVisibleTo(&dialog));

        // The new book has the texts in place of its empty first chapter
        core::ProjectManager made;
        REQUIRE(made.openProject(manifest));
        QStringList main;
        for (const core::ProjectElement& element : made.book()->mainElements) {
            main << element.title;
        }
        CHECK(main == QStringList({QStringLiteral("Rozdział 1"), QStringLiteral("Rozdział 2")}));
        CHECK(made.book()->frontElements.size() == 1);  // its title page
        made.closeProject(false);
    }

    SECTION("Into the group Other Files: no kinds") {
        Novel novel;
        ImportBookDialog dialog(Novel::dialogBook(), core::ProjectManager::installedBookTypes(),
                                ImportFormat::Markdown, {path}, intoBook());
        dialog.setTarget(ImportBookDialog::Target::OtherFiles);
        CHECK(dialog.textList()->isColumnHidden(1));
        CHECK_FALSE(dialog.kindBox()->isVisibleTo(&dialog));
        const QStringList before = novel.mainTitles();
        dialog.accept();
        REQUIRE(dialog.isDone());
        CHECK(dialog.resultText() == QStringLiteral("Added 2 files to the group \"Other Files\"."));
        CHECK(novel.mainTitles() == before);
        CHECK(core::SettingsManager::getInstance().get<std::string>("import.target") ==
              "otherFiles");
        dialog.undoImport();
    }
}

TEST_CASE("Import Text: the commands of the menu and the window of the book", "[gui][import]") {
    Novel novel;
    const ImportBookDialog::Book book = Novel::dialogBook();
    CHECK(book.title == QStringLiteral("Szkło"));
    CHECK(book.language == QStringLiteral("pl"));
    CHECK_FALSE(book.readOnly);
    REQUIRE(book.kinds.main);
    CHECK(book.kinds.main.kind->id == QStringLiteral("chapter"));
    CHECK(book.styles.contains(QStringLiteral("normal")));

    // Without a book: nothing of a book
    novel.book().closeProject(false);
    CHECK(Novel::dialogBook().title.isEmpty());
}

TEST_CASE("Import Text: Settings > Files > Import, the choices the window starts with",
          "[gui][settings][import]") {
    SettingsKept kept;
    Novel novel;
    Files files;
    const QString path = files.write(QStringLiteral("Szkic.md"), chapters(1, 2));
    auto& settings = core::SettingsManager::getInstance();
    settings.set<bool>("import.wholeFiles", false);
    settings.set<bool>("import.webImages", false);
    settings.set<std::string>("import.docx.formatting", "styles");
    settings.set<bool>("import.docx.rejectChanges", true);
    settings.set<bool>("import.markdown.typography", false);
    settings.set<std::string>("import.text.emptyParagraphs", "sceneBreak");

    // The gear of the window of the import opens the page
    QString openedPage;
    const SettingsOpener opener(
        [&openedPage](QWidget*, const QString& page) { openedPage = page; });
    {
        ImportBookDialog window(Novel::dialogBook(), core::ProjectManager::installedBookTypes(),
                                ImportFormat::Markdown, {path}, {});
        QToolButton* gear = nullptr;
        for (QToolButton* each : window.findChildren<QToolButton*>()) {
            if (each->toolTip() == QStringLiteral("Import Settings")) {
                gear = each;
            }
        }
        REQUIRE(gear != nullptr);
        gear->click();
        CHECK(openedPage == QString::fromLatin1(gui::SettingsDialog::IMPORT_PAGE));
    }

    gui::SettingsDialog dialog(test::book(), test::commands(), nullptr);
    REQUIRE(dialog.openPage(QString::fromLatin1(gui::SettingsDialog::IMPORT_PAGE)));
    const auto box = [&dialog](const char* name) {
        auto* found = dialog.findChild<QCheckBox*>(QLatin1String(name));
        REQUIRE(found != nullptr);
        return found;
    };
    const auto choice = [&dialog](const char* name) {
        auto* found = dialog.findChild<QComboBox*>(QLatin1String(name));
        REQUIRE(found != nullptr);
        return found;
    };

    // The page shows the settings
    CHECK_FALSE(box("importWholeFiles")->isChecked());
    CHECK_FALSE(box("importWebImages")->isChecked());
    CHECK(choice("importFormatting")->currentData().toString() == QStringLiteral("styles"));
    CHECK(choice("importChanges")->currentIndex() == 1);
    CHECK(box("importComments")->isChecked());
    CHECK(box("import_docx_typography")->isChecked());
    CHECK_FALSE(box("import_markdown_typography")->isChecked());
    CHECK(choice("import_text_emptyParagraphs")->currentData().toString() ==
          QStringLiteral("sceneBreak"));

    // One format at a time
    auto* page = dialog.findChild<gui::ImportPage*>();
    auto* stack = dialog.findChild<QStackedWidget*>(QStringLiteral("importFormats"));
    REQUIRE(page != nullptr);
    REQUIRE(stack != nullptr);
    CHECK(stack->currentWidget()->isAncestorOf(box("import_docx_sceneMarkers")));
    CHECK(pageKeyProblems(*page).toStdString().empty());
    choice("importFormat")->setCurrentIndex(2);
    CHECK(stack->currentWidget()->isAncestorOf(choice("import_text_emptyParagraphs")));
    CHECK(pageKeyProblems(*page).toStdString().empty());
    choice("importFormat")->setCurrentIndex(1);
    CHECK(stack->currentWidget()->isAncestorOf(box("import_markdown_typography")));

    // Changed and applied: only the changed choices are written
    box("importWholeFiles")->setChecked(true);
    box("importWebImages")->setChecked(true);
    choice("importChanges")->setCurrentIndex(0);
    box("import_markdown_typography")->setChecked(true);
    box("import_markdown_sceneMarkers")->setChecked(false);
    QComboBox* empty = choice("import_text_emptyParagraphs");
    empty->setCurrentIndex(empty->findData(QStringLiteral("keep")));
    QStringList written = dialog.applyChanges();
    written.sort();
    CHECK(written == QStringList({QStringLiteral("import.docx.rejectChanges"),
                                  QStringLiteral("import.markdown.sceneMarkers"),
                                  QStringLiteral("import.markdown.typography"),
                                  QStringLiteral("import.text.emptyParagraphs"),
                                  QStringLiteral("import.webImages"),
                                  QStringLiteral("import.wholeFiles")}));
    CHECK(settings.get<bool>("import.wholeFiles"));
    CHECK_FALSE(settings.get<bool>("import.docx.rejectChanges"));
    CHECK(settings.get<std::string>("import.text.emptyParagraphs") == "keep");

    // The window of the import starts with them
    ImportBookDialog next(Novel::dialogBook(), core::ProjectManager::installedBookTypes(),
                          ImportFormat::Markdown, {path}, {});
    CHECK(next.wholeFilesBox()->isChecked());
    CHECK(next.webImagesBox()->isChecked());
    CHECK(next.typographyBox()->isChecked());
    CHECK_FALSE(next.sceneMarkersBox()->isChecked());
    next.setFormat(ImportFormat::PlainText);
    CHECK(next.emptyBox()->currentData().toInt() ==
          static_cast<int>(editor::EmptyParagraphs::Keep));
    CHECK(next.sceneMarkersBox()->isChecked());

    // In Polish, each option of the page in front has its own access key too
    Polish polish;
    REQUIRE(polish.loaded());
    gui::SettingsDialog polishDialog(test::book(), test::commands(), nullptr);
    REQUIRE(polishDialog.openPage(QString::fromLatin1(gui::SettingsDialog::IMPORT_PAGE)));
    auto* polishPage = polishDialog.findChild<gui::ImportPage*>();
    REQUIRE(polishPage != nullptr);
    CHECK(pageKeyProblems(*polishPage).toStdString().empty());
    auto* polishFormat = polishDialog.findChild<QComboBox*>(QStringLiteral("importFormat"));
    REQUIRE(polishFormat != nullptr);
    polishFormat->setCurrentIndex(2);
    CHECK(pageKeyProblems(*polishPage).toStdString().empty());
}

TEST_CASE("Import Text: the images of Internet addresses, downloaded only on the writer's wish",
          "[gui][dialogs][import]") {
    SettingsKept kept;
    Novel novel;
    Files files;
    PictureServer server;
    auto& settings = core::SettingsManager::getInstance();
    settings.set<bool>("import.webImages", false);
    const QByteArray map = server.url(QStringLiteral("map.png")).toUtf8();
    const QByteArray well = server.url(QStringLiteral("well.png")).toUtf8();
    const QString path = files.write(QStringLiteral("Oaza.md"),
                                     "# Rozdział 1\n\nKarawana.\n\n![Mapa oazy](" + map +
                                         ")\n\n![Studnia](" + well + ")\n\nJeszcze raz ![Mapa](" +
                                         map + ").\n");
    ImportBookDialog dialog(Novel::dialogBook(), core::ProjectManager::installedBookTypes(),
                            ImportFormat::Markdown, {path}, intoBook());

    // The option shows with the images of Internet addresses, off as the setting says
    QCheckBox* box = dialog.webImagesBox();
    CHECK(box->isVisibleTo(dialog.tabs()->widget(5)));
    CHECK(box->text() == QStringLiteral("Download the &images of Internet addresses (3)"));
    CHECK_FALSE(box->isChecked());
    CHECK(dialog.imagesText().isEmpty());

    SECTION("Off: the program does not connect, the descriptions stay") {
        dialog.accept();
        REQUIRE(dialog.isDone());
        CHECK(server.asked().isEmpty());
        CHECK(dialog.outcome().images == 0);
        CHECK(dialog.notesText().isEmpty());
        const QString text = mainText(dialog.outcome().firstText);
        CHECK(text.contains(QStringLiteral("[Mapa oazy]")));
        CHECK(text.contains(QStringLiteral("[Studnia]")));
        CHECK_FALSE(settings.get<bool>("import.webImages"));
    }

    SECTION("On: each address once, and the one that did not come after the result") {
        box->setChecked(true);
        dialog.accept();
        REQUIRE(dialog.isDone());
        QStringList asked = server.asked();
        asked.sort();
        CHECK(asked == QStringList({QStringLiteral("/map.png"), QStringLiteral("/well.png")}));
        CHECK(dialog.outcome().images == 1);
        REQUIRE(dialog.outcome().added.images.size() == 1);
        const QString source = dialog.outcome().added.images[0];
        QFile image(QDir(test::book().imagesBaseFolder()).filePath(source));
        REQUIRE(image.open(QIODevice::ReadOnly));
        CHECK(image.readAll() == server.picture());
        CHECK(QFileInfo(image.fileName()).fileName() == QStringLiteral("map.png"));
        const QString text = mainText(dialog.outcome().firstText);
        CHECK(text.count(source) == 2);
        CHECK_FALSE(text.contains(QStringLiteral("[Mapa oazy]")));
        CHECK(text.contains(QStringLiteral("[Studnia]")));
        const QStringList notes = dialog.notesText().split(QLatin1Char('\n'));
        REQUIRE(notes.size() == 2);
        CHECK(notes[0] == QStringLiteral(
                              "These images could not be downloaded; their descriptions are in "
                              "their places:"));
        CHECK(notes[1].startsWith(QStringLiteral("– ") + server.url(QStringLiteral("well.png")) +
                                  QStringLiteral(": ")));
        // The window's files stay as they were read; the next import starts with the option on
        CHECK(dialog.files().front().document->images.front().web);
        CHECK(settings.get<bool>("import.webImages"));
        dialog.undoImport();
    }
}

TEST_CASE("Import Text: a format of a plugin, its tab of options and its texts",
          "[gui][dialogs][import][plugins]") {
    SettingsKept kept;
    Novel novel;
    Files files;
    const QString path = files.write(QStringLiteral("Szklane miasto.fountain"), "scenes");
    const QString broken = files.write(QStringLiteral("broken.fountain"), "scenes");
    const core::BookTypeRegistry& types = core::ProjectManager::installedBookTypes();

    // File > Import > Other Formats: greyed out while no plugin imports a format
    gui::CommandRegistry commands;
    gui::Command other;
    other.id = "file.import.plugin";
    commands.registerCommand(other);
    BookImportCoordinator coordinator(test::book(), commands, {}, nullptr);
    coordinator.connectCommands();
    gui::Command* command = commands.getCommand("file.import.plugin");
    REQUIRE(command != nullptr);
    CHECK(command->canExecute());
    CHECK_FALSE(command->checkEnabled());
    CHECK(command->disabledReason() == QStringLiteral("No plugin imports other formats."));
    {
        ImportBookDialog dialog(Novel::dialogBook(), types, ImportFormat::Markdown, {}, {});
        CHECK(dialog.formatBox()->count() == 3);
    }

    RegisteredImporter registered;
    CHECK(command->checkEnabled());
    // Its formats with an id, a name and an extension, not the program's own
    const std::vector<gui::PluginImportFormat> formats = gui::pluginImportFormats();
    REQUIRE(formats.size() == 1);
    CHECK(formats[0].id == QStringLiteral("screenplay"));
    CHECK(formats[0].extensions ==
          QStringList({QStringLiteral(".fountain"), QStringLiteral(".spmd")}));
    CHECK(gui::importExtensions(ImportFormat::Plugin, QStringLiteral("screenplay")) ==
          formats[0].extensions);
    CHECK(gui::importerOf(QStringLiteral("markdown")) == nullptr);

    ImportBookDialog dialog(Novel::dialogBook(), types, QStringLiteral("screenplay"), {path},
                            intoBook());
    CHECK(dialog.formatBox()->count() == 4);
    CHECK(dialog.formatBox()->currentText() ==
          QStringLiteral("Screenplay (.fountain, .spmd) – plugin"));
    CHECK(dialog.pluginFormat() == QStringLiteral("screenplay"));
    CHECK(registered.importer->reads == 1);

    // The tabs: no styles, cleanups, annotations and images; the plugin's options
    QTabWidget* tabs = dialog.tabs();
    REQUIRE(tabs->count() == 7);
    CHECK_FALSE(tabs->isTabVisible(3));
    CHECK_FALSE(tabs->isTabVisible(4));
    CHECK_FALSE(tabs->isTabVisible(5));
    CHECK(tabs->isTabVisible(6));
    CHECK(tabs->tabText(6) == QStringLiteral("Screenplay"));
    CHECK(tabs->widget(6) == dialog.pluginPage(QStringLiteral("screenplay")));
    CHECK_FALSE(dialog.encodingBox()->isVisibleTo(&dialog));
    CHECK(keyProblems(dialog).toStdString().empty());
    checkFitsSmallScreen(dialog);

    // The division: the plugin's texts with their titles; the kinds in the list
    tabs->setCurrentIndex(2);
    CHECK(dialog.pluginNote()->isVisibleTo(&dialog));
    CHECK_FALSE(dialog.headingsButton()->isVisibleTo(&dialog));
    CHECK_FALSE(dialog.headingAsTitleBox()->isVisibleTo(&dialog));
    REQUIRE(dialog.rows().size() == 3);
    CHECK(dialog.rows()[0].kind == dialog.kinds().titlePage.reference());
    CHECK(dialog.rows()[1].title == QStringLiteral("INT. DOM – NOC"));
    CHECK(dialog.rows()[2].title == QStringLiteral("EXT. PUSTYNIA – DZIEŃ"));
    CHECK(dialog.rows()[1].kind == dialog.kinds().main.reference());

    // Its options read the file again when their tab goes to the back
    tabs->setCurrentIndex(6);
    auto* scenes = tabs->widget(6)->findChild<QCheckBox*>();
    REQUIRE(scenes != nullptr);
    scenes->setChecked(false);
    tabs->setCurrentIndex(2);
    CHECK(registered.importer->reads == 2);
    REQUIRE(dialog.rows().size() == 2);
    CHECK(dialog.rows()[1].title == QStringLiteral("Cały scenariusz"));
    tabs->setCurrentIndex(6);
    scenes->setChecked(true);
    tabs->setCurrentIndex(0);
    REQUIRE(dialog.rows().size() == 3);

    // The texts go to the book as the plugin wrote them
    dialog.setTarget(ImportBookDialog::Target::Book);
    REQUIRE(dialog.acceptButton()->isEnabled());
    const std::string target = core::SettingsManager::getInstance().get<std::string>(
        "import.markdown.emptyParagraphs");
    dialog.accept();
    REQUIRE(dialog.isDone());
    CHECK(novel.mainTitles().contains(QStringLiteral("INT. DOM – NOC")));
    CHECK(novel.mainTitles().contains(QStringLiteral("EXT. PUSTYNIA – DZIEŃ")));
    CHECK(core::SettingsManager::getInstance().get<std::string>(
              "import.markdown.emptyParagraphs") == target);
    dialog.undoImport();
    CHECK_FALSE(novel.mainTitles().contains(QStringLiteral("INT. DOM – NOC")));

    // A file the plugin cannot read: what it said under Details
    ImportBookDialog failed(Novel::dialogBook(), types, QStringLiteral("screenplay"), {broken},
                            intoBook());
    CHECK(failed.filesNote() ==
          QStringLiteral("The plugin could not read the file \"broken.fountain\"."));
    CHECK_FALSE(failed.acceptButton()->isEnabled());
    REQUIRE_FALSE(failed.files().empty());
    CHECK(failed.files().front().problem.detail.endsWith(
        QStringLiteral("line 3: no scene heading")));

    // The format of the plugin chosen in the list of a window of another format
    ImportBookDialog chosen(Novel::dialogBook(), types, ImportFormat::Markdown, {path},
                            intoBook());
    CHECK(chosen.tabs()->count() == 6);  // no tab of the plugin until its format is chosen
    chosen.setPluginFormat(QStringLiteral("screenplay"));
    CHECK(chosen.pluginFormat() == QStringLiteral("screenplay"));
    CHECK(chosen.rows().size() == 3);
    chosen.setFormat(ImportFormat::Markdown);
    CHECK(chosen.pluginFormat().isEmpty());
    CHECK_FALSE(chosen.tabs()->isTabVisible(chosen.tabs()->count() - 1));
    CHECK(chosen.tabs()->isTabVisible(3));
}
