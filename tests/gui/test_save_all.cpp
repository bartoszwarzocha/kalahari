/// @file test_save_all.cpp
/// @brief "Save All" saves the book and every open file outside it; closing the book asks only
/// about the changes of the book

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/editor/statistics_collector.h>  // members of DocumentCoordinator
#include <kalahari/editor/style_resolver.h>
#include <kalahari/gui/dialogs/kalahari_dialog.h>
#include <kalahari/gui/document_coordinator.h>
#include <kalahari/gui/navigator_coordinator.h>
#include <kalahari/gui/panels/dashboard_panel.h>
#include <kalahari/gui/panels/editor_panel.h>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/gui/panels/properties_panel.h>
#include <kalahari/gui/widgets/standalone_info_bar.h>

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>
#include <functional>

using namespace kalahari;

namespace {

/// @brief The dialog shown now, if any
QDialog* shownDialog() {
    if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
        return dialog;
    }
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        auto* dialog = qobject_cast<QDialog*>(widget);
        if (dialog && dialog->isVisible()) {
            return dialog;
        }
    }
    return nullptr;
}

/// @brief Run @p command, accepting each dialog it shows with the button that does its job;
/// the title and the texts of each dialog go to @p said, one dialog a line
/// @return The number of dialogs the command showed
int accepting(const std::function<void()>& command, QStringList* said = nullptr) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, said]() {
        QDialog* dialog = shownDialog();
        if (!dialog) {
            return;
        }
        ++shown;
        if (said) {
            QStringList texts{dialog->windowTitle()};
            for (const QLabel* label : dialog->findChildren<QLabel*>()) {
                texts << label->text();
            }
            said->append(texts.join(QLatin1Char(' ')));
        }
        // The program's own dialogs answer with the button that does their job
        if (auto* own = qobject_cast<gui::dialogs::KalahariDialog*>(dialog)) {
            own->acceptButton()->click();
        } else {
            dialog->accept();
        }
    });
    timer.start();
    command();
    return shown;
}

/// @brief Keeps the recent books of the test run as they were
class RecentBooks {
public:
    RecentBooks() : m_saved(core::RecentBooksManager::getInstance().getRecentFiles()) {}
    ~RecentBooks() {
        try {
            auto& recent = core::RecentBooksManager::getInstance();
            recent.clearRecentFiles();
            for (auto it = m_saved.crbegin(); it != m_saved.crend(); ++it) {
                recent.addRecentFile(*it);
            }
        } catch (...) {
            // A destructor may not throw: the later tests then find the books of this one
            std::fputs("RecentBooks: the recent books of the test run are lost\n", stderr);
        }
    }
    RecentBooks(const RecentBooks&) = delete;
    RecentBooks& operator=(const RecentBooks&) = delete;

private:
    QStringList m_saved;
};

/// @brief A new novel with a chapter of its own, open in ProjectManager until the test ends
struct OpenNovel {
    QTemporaryDir dir;
    core::ProjectManager& pm = core::ProjectManager::getInstance();
    QString chapterId;

    explicit OpenNovel(const QString& title) {
        REQUIRE(dir.isValid());
        pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        REQUIRE(pm.createProject(dir.path(), title, QStringLiteral("Author"), QStringLiteral("en"),
                                 false, QStringLiteral("kalahari.novel")));
        chapterId = pm.addElement(pm.chapterKindFor(core::BookPlace::Main),
                                  QStringLiteral("The Storm"), core::BookPlace::Main);
        REQUIRE_FALSE(chapterId.isEmpty());
    }
    ~OpenNovel() {
        try {
            pm.closeProject(false);
        } catch (...) {
            // A destructor may not throw: the later tests then find this book open
            std::fputs("OpenNovel: the book of the test is still open\n", stderr);
        }
    }
    OpenNovel(const OpenNovel&) = delete;
    OpenNovel& operator=(const OpenNovel&) = delete;

    /// The file of the chapter, as it is on the disk
    [[nodiscard]] QByteArray chapterFile() const {
        const core::ProjectElement* chapter = pm.findElement(chapterId);
        REQUIRE(chapter != nullptr);
        QFile file(pm.filePathOf(*chapter));
        REQUIRE(file.open(QIODevice::ReadOnly));
        return file.readAll();
    }
};

/// @brief The main window's tabs and panels, with the commands of the documents
struct Window {
    // Not the window's own, as in test_dock_layout.cpp (QTBUG-137524)
    QStatusBar statusBar;
    QMainWindow window;
    QTabWidget* tabs = new QTabWidget(&window);
    gui::NavigatorPanel* navigatorPanel = new gui::NavigatorPanel(&window);
    gui::PropertiesPanel* properties = new gui::PropertiesPanel(&window);
    gui::DashboardPanel* dashboard = new gui::DashboardPanel(&window);
    gui::StandaloneInfoBar* infoBar = new gui::StandaloneInfoBar(&window);
    gui::NavigatorCoordinator navigator{navigatorPanel, properties, tabs, &statusBar, &window};
    bool dirty = false;
    gui::DocumentCoordinator documents{
        &window,
        tabs,
        navigatorPanel,
        properties,
        dashboard,
        &navigator,
        infoBar,
        &statusBar,
        [this]() { return dirty; },
        [this](bool value) { dirty = value; },
        []() {},
        [this]() { return !documents.unsavedDocumentNames().isEmpty(); },
        &window};

    Window() { navigator.refreshNavigator(); }

    /// The editor of the tab with @p index
    [[nodiscard]] gui::EditorPanel* editor(int index) const {
        auto* editor = qobject_cast<gui::EditorPanel*>(tabs->widget(index));
        REQUIRE(editor != nullptr);
        return editor;
    }

    /// Open @p chapterId in a tab and write @p text in it
    void writeChapter(const QString& chapterId, const QString& text) {
        navigator.onElementSelected(chapterId, QStringLiteral("The Storm"));
        REQUIRE(tabs->count() > 0);
        editor(tabs->currentIndex())->setText(text);
        REQUIRE(navigator.isChapterDirty(chapterId));
    }

    /// Open the file @p path outside the book in a tab and write @p text in it
    gui::EditorPanel* writeFile(const QString& path, const QString& text) {
        documents.openStandaloneFile(path);
        gui::EditorPanel* file = editor(tabs->currentIndex());
        REQUIRE(file->property("standaloneFilePath").toString() == path);
        file->setText(text);
        REQUIRE(file->property("dirty").toBool());
        return file;
    }
};

/// Write @p text to a new file @p path
void writeText(const QString& path, const QByteArray& text) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(text) == text.size());
}

/// The text of the file @p path
QByteArray readText(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

} // namespace

TEST_CASE("Save All: the book and every open file outside it are saved", "[gui][save]") {
    OpenNovel novel(QStringLiteral("Save Test"));
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
    const QString ideas = QDir(files.path()).filePath(QStringLiteral("ideas.txt"));
    writeText(notes, "Notes");
    writeText(ideas, "Ideas");

    Window window;
    window.writeChapter(novel.chapterId, QStringLiteral("The storm came at night."));
    gui::EditorPanel* notesTab = window.writeFile(notes, QStringLiteral("Notes of the night"));
    window.documents.openStandaloneFile(ideas);  // open, and not changed

    CHECK(accepting([&] { window.documents.onSaveAll(); }) == 0);
    CHECK(window.statusBar.currentMessage() == QStringLiteral("All changes saved"));
    CHECK(novel.chapterFile().contains("The storm came at night."));
    CHECK_FALSE(window.navigator.isChapterDirty(novel.chapterId));
    CHECK(readText(notes) == "Notes of the night");
    CHECK_FALSE(notesTab->property("dirty").toBool());
    CHECK(readText(ideas) == "Ideas");
    CHECK(window.documents.unsavedDocumentNames().isEmpty());

    // Nothing left to save: the status bar says so
    window.statusBar.clearMessage();
    window.documents.onSaveAll();
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("Nothing to save: all changes are saved"));

    // Without a book, the files outside it are saved too
    novel.pm.closeProject(false);
    notesTab->setText(QStringLiteral("Notes of the morning"));
    window.documents.onSaveAll();
    CHECK(readText(notes) == "Notes of the morning");
    CHECK(window.statusBar.currentMessage() == QStringLiteral("All changes saved"));
}

TEST_CASE("Save All: a file that cannot be saved does not stop the others", "[gui][save]") {
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QDir root(files.path());
    REQUIRE(root.mkdir(QStringLiteral("gone")));
    const QString lost = root.filePath(QStringLiteral("gone/lost.txt"));
    const QString kept = root.filePath(QStringLiteral("kept.txt"));
    writeText(lost, "Lost");
    writeText(kept, "Kept");

    Window window;
    gui::EditorPanel* lostTab = window.writeFile(lost, QStringLiteral("Lost text"));
    gui::EditorPanel* keptTab = window.writeFile(kept, QStringLiteral("Kept text"));

    // The folder of the first file is gone: its window says so, and the second file is saved
    REQUIRE(QDir(root.filePath(QStringLiteral("gone"))).removeRecursively());
    QStringList said;
    CHECK(accepting([&] { window.documents.onSaveAll(); }, &said) == 1);
    INFO(said.join(QLatin1Char('\n')).toStdString());
    CHECK(lostTab->property("dirty").toBool());
    CHECK(readText(kept) == "Kept text");
    CHECK_FALSE(keptTab->property("dirty").toBool());
    CHECK(window.statusBar.currentMessage() != QStringLiteral("All changes saved"));
}

TEST_CASE("Opening another book asks only about the changes of the open book",
          "[gui][save]") {
    const RecentBooks recent;
    auto& pm = core::ProjectManager::getInstance();
    QTemporaryDir otherFolder;
    REQUIRE(otherFolder.isValid());
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.createProject(otherFolder.path(), QStringLiteral("Other Book"),
                             QStringLiteral("Author"), QStringLiteral("en"), false,
                             QStringLiteral("kalahari.novel")));
    const QString otherBook = pm.getManifestPath();
    pm.closeProject(false);

    OpenNovel novel(QStringLiteral("Open Book"));
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
    writeText(notes, "Notes");

    // The book has no changes; a file outside it has
    Window window;
    gui::EditorPanel* notesTab = window.writeFile(notes, QStringLiteral("Notes of the night"));

    // The question is whether to close the book, not whether to save it, and the file stays
    // open with its changes
    QStringList said;
    CHECK(accepting([&] { window.documents.onOpenRecentFile(otherBook); }, &said) == 1);
    INFO(said.join(QLatin1Char('\n')).toStdString());
    REQUIRE(said.size() == 1);
    CHECK(said.first().startsWith(QStringLiteral("Close Book? ")));
    CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(otherBook));
    CHECK(window.tabs->indexOf(notesTab) >= 0);
    CHECK(notesTab->property("dirty").toBool());
    CHECK(readText(notes) == "Notes");
}
