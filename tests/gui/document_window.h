/// @file document_window.h
/// @brief For the tests of the commands of the documents: the main window's tabs and panels
/// with their coordinators, a book open until the test ends, and the windows the commands show

#pragma once

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
#include <QCoreApplication>
#include <QDialog>
#include <QFile>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QWindow>

#include <cstdio>
#include <functional>

namespace kalahari {
namespace test {

/// @brief The dialog shown now, if any
inline QDialog* shownDialog() {
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

/// @brief The title and the texts of @p dialog, in one line
inline QString textsOf(const QDialog& dialog) {
    QStringList texts{dialog.windowTitle()};
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        texts << label->text();
    }
    return texts.join(QLatin1Char(' '));
}

/// The ticks of 10 ms a test waits for the system to show a window before it answers the window
/// all the same, so that it never waits for ever
constexpr int SHOWN_TICKS = 200;

/// @brief Whether the system shows @p dialog on the screen
inline bool onScreen(const QDialog& dialog) {
    const QWindow* window = dialog.windowHandle();
    return window != nullptr && window->isExposed();
}

/// @brief The program's own window that saves a file, not the one of the system, until the
/// test ends: a test can choose the file in it on every system
class OwnFileDialogs {
public:
    OwnFileDialogs()
        : m_saved(QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs)) {
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    }
    ~OwnFileDialogs() { QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, m_saved); }
    OwnFileDialogs(const OwnFileDialogs&) = delete;
    OwnFileDialogs& operator=(const OwnFileDialogs&) = delete;

private:
    bool m_saved;
};

/// @brief Run @p command, accepting each dialog it shows with the button that does its job;
/// the title and the texts of each dialog go to @p said, one dialog a line
/// @return The number of dialogs the command showed
inline int accepting(const std::function<void()>& command, QStringList* said = nullptr) {
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
            said->append(textsOf(*dialog));
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
    gui::DocumentCoordinator documents{
        &window,
        tabs,
        navigatorPanel,
        properties,
        dashboard,
        &navigator,
        infoBar,
        &statusBar,
        []() {},
        [this]() { return !documents.unsavedDocumentNames().isEmpty(); },
        &window};

    Window() { navigator.refreshNavigator(); }

    /// The editors close before the coordinators, as in the main window: an editor tells the
    /// statistics of the writing, which the coordinators keep, that it closes. These are also
    /// the editors of the tabs closed meanwhile, deleted later.
    ~Window() {
        for (gui::EditorPanel* editor : window.findChildren<gui::EditorPanel*>()) {
            delete editor;
        }
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

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

} // namespace test
} // namespace kalahari
